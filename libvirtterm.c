#include "libvirtterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma region MIN/MAX
#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })
#define MAX(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })
#pragma endregion

#define ACS_MIN 0x2b

typedef enum { VTM_NO, VTM_CLICKS, VTM_DRAG, VTM_ALL } VTMouseTracking;

typedef struct VT {
    // terminal configuration
    INT        rows;
    INT        columns;
    VTConfig   config;

    // terminal state
    VTCell*            matrix;
    VTCell*            matrix_copy;
    VTCursor           cursor;
    VTCursor           cursor_saved;
    VTAttrib           current_attrib;
    INT                scroll_area_top;
    INT                scroll_area_bottom;
    bool               acs_mode;
    bool               insert_mode;
    bool               cursor_app_mode;
    VTTextReceivedType receiving_text;
    CHAR               last_char;
    char*              last_text_received;
    VTMouseTracking    mouse_tracking;
    bool               sgr_mouse_mode;
    VTMouseButton      last_mouse_button;
    VTMouseModifier    last_mouse_mod;
    INT                last_mouse_row;
    INT                last_mouse_column;

    // escape sequence parsing
    char       esc_buffer[32];

    // events
    VTEvent*   event_queue_start;
    VTEvent*   event_queue_end;
} VT;

static void vt_add_char(VT* vt, CHAR c);
static void vt_free_event_queue(VT*);


//
// INITIALIZATION
//

#pragma region Initialization

VT* vt_new(INT rows, INT columns, VTConfig const* config)
{
    VT* vt = calloc(1, sizeof(VT));
    vt->rows = rows;
    vt->columns = columns;
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true, .blinking = false };
    vt->cursor_saved = vt->cursor;
    vt->config = *config;
    vt->current_attrib = DEFAULT_ATTR;
    vt->scroll_area_top = 0;
    vt->scroll_area_bottom = vt->rows - 1;
    vt->event_queue_start = NULL;
    vt->event_queue_end = NULL;
    vt->acs_mode = false;
    vt->insert_mode = false;
    vt->cursor_app_mode = false;
    vt->receiving_text = VTT_NOT_RECEIVING;
    vt->last_text_received = NULL;
    vt->mouse_tracking = VTM_NO;
    vt->sgr_mouse_mode = false;
    vt->last_mouse_button = VTM_RELEASE;
    vt->last_mouse_mod = 0;
    vt->last_mouse_row = 0;
    vt->last_mouse_column = 0;
    memset(vt->esc_buffer, 0, sizeof vt->esc_buffer);

    vt->matrix = malloc(rows * columns * sizeof(VTCell));
    vt->matrix_copy = malloc(rows * columns * sizeof(VTCell));
    for (INT i = 0; i < rows * columns; ++i)
        vt->matrix[i] = vt->matrix_copy[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };

    return vt;
}

void vt_free(VT* vt)
{
    if (vt) {
        vt_free_event_queue(vt);
        free(vt->last_text_received);
        free(vt->matrix);
        free(vt->matrix_copy);
    }
    free(vt);
}

void vt_reset(VT* vt)
{
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true, .blinking = false };
    vt->cursor_saved = vt->cursor;
    vt->current_attrib = DEFAULT_ATTR;
    vt->scroll_area_top = 0;
    vt->scroll_area_bottom = vt->rows - 1;
    vt->acs_mode = false;
    vt->insert_mode = false;
    vt->cursor_app_mode = false;
    vt->receiving_text = VTT_NOT_RECEIVING;
    free(vt->last_text_received);
    vt->last_text_received = NULL;
    vt->mouse_tracking = VTM_NO;
    vt->sgr_mouse_mode = false;
    vt->last_mouse_button = VTM_RELEASE;
    vt->last_mouse_mod = 0;
    vt->last_mouse_row = 0;
    vt->last_mouse_column = 0;
    for (int i = 0; i < vt->rows * vt->columns; ++i)
        vt->matrix[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };
    memcpy(vt->matrix_copy, vt->matrix, vt->columns * vt->rows * sizeof(VTCell));
}

void vt_resize(VT* vt, INT rows, INT columns)
{
    if (!vt->matrix)
        return;

    /*
    INT init_row_new = 0, init_row_old = 0;
    if (rows > vt->rows)
        init_row_new = rows - vt->rows;
    else
        init_row_old = vt->rows - rows;

    VTCell* old_matrix[] = { vt->matrix, vt->matrix_copy };
    */
    VTCell* new_matrix[2];
    for (size_t i = 0; i < 2; ++i) {
        new_matrix[i] = malloc(sizeof(VTCell) * rows * columns);
        // clear new matrix
        for (INT j = 0; j < rows * columns; ++j)
            new_matrix[i][j] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };
        /*
        // copy characters from old to new, taking resize into account
        for (INT new_row = rows - 1, old_row = vt->rows - 1; new_row >= 0 && old_row >= 0; --old_row, --new_row)
            for (INT column = 0; column < MIN(columns, vt->columns); ++column)
                new_matrix[i][new_row * columns + column] = old_matrix[i][old_row * vt->columns + column];
        */
    }

    vt->cursor.column = 0;
    vt->cursor.row = 0;
    free(vt->matrix);
    free(vt->matrix_copy);
    vt->matrix = new_matrix[0];
    vt->matrix_copy = new_matrix[1];

    vt->rows = rows;
    vt->columns = columns;
}

#pragma endregion

//
// EVENTS
//

#pragma region Events

static void vt_add_event(VT* vt, VTEvent* event)
{
    VTEvent* new_event = malloc(sizeof(VTEvent));
    memcpy(new_event, event, sizeof(VTEvent));

    new_event->_next = NULL;
    if (vt->event_queue_start == NULL)
        vt->event_queue_start = new_event;
    if (vt->event_queue_end)
        vt->event_queue_end->_next = new_event;
    vt->event_queue_end = new_event;
}

bool vt_next_event(VT* vt, VTEvent* e)
{
    if (vt->event_queue_start == NULL)
        return false;

    VTEvent* event_to_remove = vt->event_queue_start;
    if (e)
        memcpy(e, event_to_remove, sizeof(VTEvent));
    vt->event_queue_start = event_to_remove->_next;

    if (vt->event_queue_start == NULL)
        vt->event_queue_end = NULL;

    free(event_to_remove);

    return true;
}

static void vt_free_event_queue(VT* vt)
{
    while (vt_next_event(vt, NULL));
}

static void vt_beep(VT* vt)
{
    vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_BELL });
}

#pragma endregion

//
// UPDATES TO TERMINAL MATRIX
//

#pragma region Updates to Terminal Matrix

static void vt_set_ch(VT* vt, INT row, INT column, CHAR c)
{
    row = MAX(MIN(row, vt->rows - 1), 0);
    column = MAX(MIN(column, vt->columns - 1), 0);

    if (row * vt->columns + column >= vt->rows * vt->columns && vt->config.debug >= VT_DEBUG_ERRORS_ONLY) {
        fprintf(stderr, "vt_set_ch: trying write data outside of screen bounds");
        return;
    }

    vt->matrix[row * vt->columns + column] = (VTCell) {
        .ch = c,
        .attrib = vt->current_attrib,
    };
}

static void vt_memset_ch(VT* vt, INT row_start, INT row_end, INT column_start, INT column_end, CHAR c)
{
    row_start = MIN(MAX(row_start, 0), vt->rows - 1);
    row_end = MIN(MAX(row_end, 0), vt->rows - 1);
    column_start = MIN(MAX(column_start, 0), vt->columns - 1);
    column_end = MIN(MAX(column_end, 0), vt->columns - 1);

    if ((row_start > row_end || row_start == row_end) && column_start > column_end)
        return;

    INT start = row_start * vt->columns + column_start;
    INT end = row_end * vt->columns + column_end;

    for (INT i = start; i <= end; ++i) {
        if (i >= vt->rows * vt->columns && vt->config.debug >= VT_DEBUG_ERRORS_ONLY) {
            fprintf(stderr, "vt_memset_ch: trying write data outside of screen bounds");
            return;
        }
        vt->matrix[i] = (VTCell) { .ch = c, .attrib = vt->current_attrib };
    }
}

static void vt_memmove(VT* vt, INT row_start, INT row_end, INT column_start, INT column_end, INT n_rows, INT n_columns)
{
    row_start = MIN(MAX(row_start, 0), vt->rows - 1);
    row_end = MIN(MAX(row_end, 0), vt->rows - 1);
    column_start = MIN(MAX(column_start, 0), vt->columns - 1);
    column_end = MIN(MAX(column_end, 0), vt->columns - 1);

    if (row_start > row_end || column_start > column_end)
        return;

    INT start = row_start * vt->columns + column_start;
    INT end = row_end * vt->columns + column_end;
    INT dest = (row_start + n_rows) * vt->columns + (column_start + n_columns);
    INT size = end - start + 1;
    INT past_the_end = vt->columns * vt->rows;

    if ((dest + size < 0 || start + size < 0) && vt->config.debug >= VT_DEBUG_ERRORS_ONLY) {
        fprintf(stderr, "vt_memmove: trying to move data before screen start");
        return;
    }

    if ((dest + size > past_the_end || start + size > past_the_end) && vt->config.debug >= VT_DEBUG_ERRORS_ONLY) {
        fprintf(stderr, "vt_memmove: trying to move data past of screen end");
        return;
    }

    memmove(&vt->matrix[dest], &vt->matrix[start], size * sizeof(VTCell));
}

#pragma endregion

//
// CURSOR MOVEMENT
//

#pragma region Cursor Movement

static void reframe_cursor(VT* vt)
{
    vt->cursor.row = MIN(MAX(vt->cursor.row, 0), vt->rows);
    vt->cursor.column = MIN(MAX(vt->cursor.column, 0), vt->columns);
}

static void vt_cursor_advance(VT* vt, int rows, int columns)
{
    vt->cursor.row += rows;
    vt->cursor.column += columns;
    reframe_cursor(vt);
    vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_CURSOR_MOVED });
}

static void vt_move_cursor_to(VT* vt, INT row, INT column)
{
    vt->cursor.row = row;
    vt->cursor.column = column;
    reframe_cursor(vt);
    vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_CURSOR_MOVED });
}

static void vt_cursor_to_bol(VT* vt)
{
    vt->cursor.column = 0;
    reframe_cursor(vt);
    vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_CURSOR_MOVED });
}

static void vt_cursor_tab(VT* vt)
{
    vt->cursor.column = ((vt->cursor.column / 8) + 1) * 8;
    reframe_cursor(vt);
    vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_CURSOR_MOVED });
}

#pragma endregion

//
// SCROLLING
//

#pragma region Scrolling

static void vt_scroll_vertical(VT* vt, INT top_row, INT bottom_row, INT rows_forward)
{
    if (rows_forward == 0)
        return;

    if (rows_forward > 0) {
        vt_memmove(vt, top_row + rows_forward, bottom_row, 0, vt->columns - 1, -rows_forward, 0);
        vt_memset_ch(vt, bottom_row - rows_forward + 1, bottom_row, 0, vt->columns - 1, ' ');
    } else {
        vt_memmove(vt, top_row, bottom_row + rows_forward, 0, vt->columns - 1, -rows_forward, 0);
        vt_memset_ch(vt, top_row, top_row - rows_forward - 1, 0, vt->columns - 1, ' ');
    }

    // report events
    vt_add_event(vt, &(VTEvent) {
        .type = VT_EVENT_CELLS_UPDATED,
        .cells = { .row_start = top_row, .row_end = bottom_row, .column_start = 0, .column_end = vt->columns },
    });
}

static void vt_scroll_horizontal(VT* vt, INT row, INT column, INT columns_forward)
{
    if (columns_forward == 0)
        return;

    if (columns_forward > 0) {
        vt_memmove(vt, row, row, column, vt->columns - 1 - columns_forward, 0, columns_forward);
        vt_memset_ch(vt, row, row, column, column + columns_forward - 1, ' ');
    } else if (columns_forward < 0) {
        vt_memmove(vt, row, row, column - columns_forward, vt->columns - 1, 0, columns_forward);
        vt_memset_ch(vt, row, row, vt->columns + columns_forward, vt->columns - 1, ' ');
    }

    // report events
    vt_add_event(vt, &(VTEvent) {
        .type = VT_EVENT_CELLS_UPDATED,
        .cells = { .row_start = row, .row_end = row, .column_start = column, .column_end = vt->columns - 1 },
    });
}

static void vt_scroll_based_on_cursor(VT* vt)
{
    if (vt->cursor.column >= vt->columns) {
        vt->cursor.column = 0;
        ++vt->cursor.row;
    }

    if (vt->cursor.row > vt->scroll_area_bottom) {
        vt_scroll_vertical(vt, vt->scroll_area_top, vt->scroll_area_bottom, 1);
        vt_cursor_advance(vt, -1, 0);
    }
}

static void vt_set_scoll_area(VT* vt, INT top, INT bottom)
{
    if (top == 0 && bottom == 0) {
        vt->scroll_area_top = 0;
        vt->scroll_area_bottom = vt->rows - 1;
    } else {
        vt->scroll_area_top = top;
        vt->scroll_area_bottom = bottom;
    }
    vt_move_cursor_to(vt, 0, 0);
}

#pragma endregion

//
// ESCAPE SEQUENCES
//

#pragma region Escape Sequences

static void vt_start_escape_seq(VT* vt, char c)
{
    vt->esc_buffer[0] = c;
}

static void vt_start_receiving_text(VT* vt, VTTextReceivedType r)
{
    free(vt->last_text_received);
    vt->last_text_received = NULL;
    vt->receiving_text = r;
}

static void update_current_attrib(VT* vt, int arg)
{
    switch (arg) {
        case 0:
            vt->current_attrib.bold = false;
            vt->current_attrib.dim = false;
            vt->current_attrib.underline = false;
            vt->current_attrib.blink = false;
            vt->current_attrib.reverse = false;
            vt->current_attrib.invisible = false;
            vt->current_attrib.italic = false;
            vt->current_attrib.fg_color = vt->config.default_fg_color;
            vt->current_attrib.bg_color = vt->config.default_bg_color;
            break;
        case 1: vt->current_attrib.bold = true; break;
        case 2: vt->current_attrib.dim = true; break;
        case 3: vt->current_attrib.italic = true; break;
        case 4: vt->current_attrib.underline = true; break;
        case 5:
        case 6:
            vt->current_attrib.blink = true;
            break;
        case 7: vt->current_attrib.reverse = true; break;
        case 8: vt->current_attrib.invisible = true; break;
        case 22:
            vt->current_attrib.bold = false;
            vt->current_attrib.dim = false;
            break;
        case 23: vt->current_attrib.italic = false; break;
        case 24: vt->current_attrib.underline = false; break;
        case 25: vt->current_attrib.blink = false; break;
        case 27: vt->current_attrib.reverse = false; break;
        case 28: vt->current_attrib.invisible = false; break;
        case 30: vt->current_attrib.fg_color = VT_BLACK; break;
        case 31: vt->current_attrib.fg_color = VT_RED; break;
        case 32: vt->current_attrib.fg_color = VT_GREEN; break;
        case 33: vt->current_attrib.fg_color = VT_YELLOW; break;
        case 34: vt->current_attrib.fg_color = VT_BLUE; break;
        case 35: vt->current_attrib.fg_color = VT_MAGENTA; break;
        case 36: vt->current_attrib.fg_color = VT_CYAN; break;
        case 37: vt->current_attrib.fg_color = VT_WHITE; break;
        case 39: vt->current_attrib.fg_color = vt->config.default_fg_color; break;
        case 40: vt->current_attrib.bg_color = VT_BLACK; break;
        case 41: vt->current_attrib.bg_color = VT_RED; break;
        case 42: vt->current_attrib.bg_color = VT_GREEN; break;
        case 43: vt->current_attrib.bg_color = VT_YELLOW; break;
        case 44: vt->current_attrib.bg_color = VT_BLUE; break;
        case 45: vt->current_attrib.bg_color = VT_MAGENTA; break;
        case 46: vt->current_attrib.bg_color = VT_CYAN; break;
        case 47: vt->current_attrib.bg_color = VT_WHITE; break;
        case 49: vt->current_attrib.bg_color = vt->config.default_bg_color; break;
        case 90: vt->current_attrib.fg_color = VT_BRIGHT_BLACK; break;
        case 91: vt->current_attrib.fg_color = VT_BRIGHT_RED; break;
        case 92: vt->current_attrib.fg_color = VT_BRIGHT_GREEN; break;
        case 93: vt->current_attrib.fg_color = VT_BRIGHT_YELLOW; break;
        case 94: vt->current_attrib.fg_color = VT_BRIGHT_BLUE; break;
        case 95: vt->current_attrib.fg_color = VT_BRIGHT_MAGENTA; break;
        case 96: vt->current_attrib.fg_color = VT_BRIGHT_CYAN; break;
        case 97: vt->current_attrib.fg_color = VT_BRIGHT_WHITE; break;
        case 100: vt->current_attrib.bg_color = VT_BRIGHT_BLACK; break;
        case 101: vt->current_attrib.bg_color = VT_BRIGHT_RED; break;
        case 102: vt->current_attrib.bg_color = VT_BRIGHT_GREEN; break;
        case 103: vt->current_attrib.bg_color = VT_BRIGHT_YELLOW; break;
        case 104: vt->current_attrib.bg_color = VT_BRIGHT_BLUE; break;
        case 105: vt->current_attrib.bg_color = VT_BRIGHT_MAGENTA; break;
        case 106: vt->current_attrib.bg_color = VT_BRIGHT_CYAN; break;
        case 107: vt->current_attrib.bg_color = VT_BRIGHT_WHITE; break;
    }
}

static void escape_seq_clear_cells(VT* vt, char mode, int parameter)
{
    if (mode == 'J') {
        switch (parameter) {
            case 0:  // cursor to end of screen
                vt_memset_ch(vt, vt->cursor.row, vt->rows - 1, vt->cursor.column, vt->columns - 1, ' ');
            break;
            case 1:  // start of screen to cursor
                vt_memset_ch(vt, 0, vt->cursor.row, 0, vt->cursor.column, ' ');
            break;
            case 2:  // all screen
                vt_memset_ch(vt, 0, vt->rows - 1, 0, vt->columns - 1, ' ');
            // TODO - send event
            break;
            case 3:
                break;
            default:
                if (vt->config.debug >= VT_DEBUG_ERRORS_ONLY)
                    fprintf(stderr, "Unsupported parameter for ESC[#J");
        }
    } else if (mode == 'K') {
        switch (parameter) {
            case 0:  // cursor to end of line
                vt_memset_ch(vt, vt->cursor.row, vt->cursor.row, vt->cursor.column, vt->columns - 1, ' ');
            break;
            case 1:  // start of line to cursor
                vt_memset_ch(vt, vt->cursor.row, vt->cursor.row, 0, vt->cursor.column, ' ');
            break;
            case 2:  // all line
                vt_memset_ch(vt, vt->cursor.row, vt->cursor.row, 0, vt->columns - 1, ' ');
            break;
            default:
        }
    }
}

static void xterm_escape_seq(VT* vt, char mode, INT arg)
{
    (void) mode;

    bool enable = (mode == 'h');

    switch (arg) {
        case 1:  // application mode
            vt->cursor_app_mode = enable;
            break;
        case 3:  // ignore for now - configuration
            break;
        case 12:
            vt->cursor.blinking = enable;
            break;
        case 25:
            vt->cursor.visible = enable;
            break;
        case 69:  // ignore for now - margins
            break;
        case 1000:
            if (enable) vt->mouse_tracking = VTM_CLICKS; else vt->mouse_tracking = VTM_NO;
            break;
        case 1002:
            if (enable) vt->mouse_tracking = VTM_DRAG; else vt->mouse_tracking = VTM_NO;
            break;
        case 1003:
            if (enable) vt->mouse_tracking = VTM_ALL; else vt->mouse_tracking = VTM_NO;
            break;
        case 1006:
            vt->mouse_tracking = enable;
            break;
        case 1049:  // alternate screen buffer
            if (enable) {
                memcpy(vt->matrix_copy, vt->matrix, vt->columns * vt->rows * sizeof(VTCell));
                vt->cursor_saved = vt->cursor;
            } else {
                memcpy(vt->matrix, vt->matrix_copy, vt->columns * vt->rows * sizeof(VTCell));
                vt->cursor = vt->cursor_saved;
            }
            break;
        case 2004:  // ignore for now - Bracketed Paste Mode
            break;
        default:
            if (vt->config.debug >= VT_DEBUG_ERRORS_ONLY)
                fprintf(stderr, "Invalid/unsupported xterm escape sequence: (ESC)%s\n", &vt->esc_buffer[1]);
    }
}

static bool match_escape_seq(VT* vt, const char* data, const char* pattern, INT args[8], int* argn)
{
    int i = 0;

    *argn = 0;
    memset(args, 0, sizeof args[0] * 8);

    // fail fast if last char doesn't match
    char last_char_of_pattern = pattern[strlen(pattern) - 1];
    if (data[strlen(data) - 1] != last_char_of_pattern)
        return false;

    for (const char* p = pattern; *p; ++p) {
        if (*p == '%') {                                            // matches a number - converts it and skip to next non-number char
            char* endptr;
            args[(*argn)++] = strtol(&data[i], &endptr, 10);  // convert number
            i = endptr - data;
            while (data[i] == ';')                                  // skip ';' separators
                ++i;
        } else if (*p != data[i] && data[i] != ';') {               // match failed (ignore ';' as it's not part of the match)
            return false;
        } else {                                                    // character matches - skip to the next char
            ++i;
        }
    }

    if (i == (int) strlen(data)) {                                  // did we match the whole input?
        if (vt->config.debug >= VT_DEBUG_ALL_ESCAPE_SEQUENCES) {
            printf("\e[0;35m\\e%s\e[0m", &data[1]);
            fflush(stdout);
        }
        return true;
    } else {
        return false;
    }
}

static bool parse_escape_seq(VT* vt)
{
#define T return true;
#define N(n) ((n) == 0 ? 1 : (n))
#define MATCH(pattern) match_escape_seq(vt, vt->esc_buffer, pattern, args, &argn)

    if (strcmp(vt->esc_buffer, "\e]0;") == 0) { vt_start_receiving_text(vt, VTT_WINDOW_TITLE_UPDATED); T }
    if (strcmp(vt->esc_buffer, "\e]7;") == 0) { vt_start_receiving_text(vt, VTT_DIRECTORY_HINT_UPDATED); T }

    size_t sz = strlen(vt->esc_buffer);
    char last_char = vt->esc_buffer[sz - 1];
    if (last_char == '\e' || last_char == ';' || last_char == '[')   // skip on the easy cases
        return false;

    INT args[8];
    int argn;

    if (MATCH("\e[?%%h"))       { xterm_escape_seq(vt, 'h', args[0]); if (args[1] != 0) xterm_escape_seq(vt, 'h', args[1]); T }
    if (MATCH("\e[?%%l"))       { xterm_escape_seq(vt, 'l', args[0]); if (args[1] != 0) xterm_escape_seq(vt, 'l', args[1]); T }
    if (MATCH("\e[%%%t"))       { T }    // do nothing for now (Xterm extension)
    if (MATCH("\e="))           { T }    // do nothing - not used anymore - keypad related
    if (MATCH("\e>"))           { T }    // do nothing - not used anymore - keypad related
    if (MATCH("\e[%@"))         { vt_scroll_horizontal(vt, vt->cursor.row, vt->cursor.column, N(args[0])); T }
    if (MATCH("\e[%A"))         { vt_cursor_advance(vt, -N(args[0]), 0); T }
    if (MATCH("\e[%B"))         { vt_cursor_advance(vt, N(args[0]), 0); T }
    if (MATCH("\e[%C"))         { vt_cursor_advance(vt, 0, N(args[0])); T }
    if (MATCH("\e[%D"))         { vt_cursor_advance(vt, 0, -N(args[0])); T }
    if (MATCH("\e[%E"))         { vt_cursor_advance(vt, args[0] - 1, -vt->cursor.column); T }
    if (MATCH("\e[%F"))         { vt_cursor_advance(vt, args[0] + 1, -vt->cursor.column); T }
    if (MATCH("\e[%G"))         { vt_move_cursor_to(vt, vt->cursor.row, args[0] - 1); T }
    if (MATCH("\e[%%H"))        { vt_move_cursor_to(vt, args[0] - 1, args[1] - 1); T }
    if (MATCH("\e[%K"))         { escape_seq_clear_cells(vt, 'K', args[0]); T }
    if (MATCH("\e[%J"))         { escape_seq_clear_cells(vt, 'J', args[0]); T }
    if (MATCH("\e[%L"))         { vt_scroll_vertical(vt, vt->cursor.row, vt->scroll_area_bottom, -N(args[0])); T }
    if (MATCH("\e[%M"))         { vt_scroll_vertical(vt, vt->cursor.row, vt->scroll_area_bottom, N(args[0])); T }
    if (MATCH("\e[%P"))         { vt_scroll_horizontal(vt, vt->cursor.row, vt->cursor.column, -N(args[0])); T }
    if (MATCH("\e[%X"))         { for (INT i = 0; i < N(args[0]); ++i) vt_add_char(vt, ' '); T }
    if (MATCH("\e[%a"))         { vt_cursor_advance(vt, 0, N(args[0])); T }
    if (MATCH("\e[%d"))         { vt_move_cursor_to(vt, args[0] - 1, vt->cursor.column); T }
    if (MATCH("\e[%e"))         { vt_cursor_advance(vt, N(args[0]), 0); T }
    if (MATCH("\e[%%f"))        { vt_move_cursor_to(vt, args[0] - 1, args[1] - 1); T }
    if (MATCH("\e[%%r"))        { vt_set_scoll_area(vt, N(args[0]) - 1, N(args[1]) - 1); T }
    if (MATCH("\e[%b"))         { INT n = N(args[0]); for (INT i = 0; i < n; ++i) vt_add_char(vt, vt->last_char); T }
    if (MATCH("\e[4h"))         { vt->insert_mode = true; T }
    if (MATCH("\e[4l"))         { vt->insert_mode = false; T }

    if (MATCH("\e(0"))          { vt->acs_mode = true; T }
    if (MATCH("\e(B"))          { vt->acs_mode = false; T }

    if (MATCH("\e7"))           { vt->cursor_saved = vt->cursor; }
    if (MATCH("\e8"))           { vt->cursor = vt->cursor_saved; }
    if (MATCH("\ec"))           { vt_reset(vt); T }
    if (MATCH("\e[!p"))         { T }  // soft reset

    if (MATCH("\eM")) {
        if (vt->cursor.row == 0)
            vt_scroll_vertical(vt, vt->scroll_area_top, vt->scroll_area_bottom, -1);
        else
            vt_cursor_advance(vt, -1, 0);
        T
    }

    if (MATCH("\e[%%%m")) {
        for (int i = 0; i < argn; ++i)
            if (i == 0 || (i > 0 && args[i] != 0))
                update_current_attrib(vt, args[i]);
        T
    }

#undef MATCH
#undef T

    return false;
}

static void end_escape_seq(VT* vt)
{
    memset(vt->esc_buffer, 0, sizeof vt->esc_buffer);
}

static void cancel_escape_seq(VT* vt)
{
    char copy_buf[sizeof vt->esc_buffer];
    memcpy(copy_buf, vt->esc_buffer, sizeof vt->esc_buffer);
    if (vt->config.debug >= VT_DEBUG_ERRORS_ONLY)
        fprintf(stderr, "Invalid escape sequence: (ESC)%s\n", &copy_buf[1]);
    vt_beep(vt);
    end_escape_seq(vt);
    vt_write(vt, &copy_buf[1], strlen(copy_buf) - 1);
}

static void vt_add_escape_char(VT* vt, char c)
{
    size_t len = strlen(vt->esc_buffer);
    if (len == sizeof vt->esc_buffer - 1) {   // parsing of the escape sequence failed, send back to regular parser
        cancel_escape_seq(vt);
        return;
    }

    vt->esc_buffer[len] = c;

    if (parse_escape_seq(vt)) {
        end_escape_seq(vt);
    } else if (isalpha(vt->esc_buffer[strlen(vt->esc_buffer) - 1])) {
        if (vt->config.debug >= VT_DEBUG_ERRORS_ONLY)
            fprintf(stderr, "Escape sequence not recognized: (ESC)%s\n", &vt->esc_buffer[1]);
        end_escape_seq(vt);
    }
}

#pragma endregion

//
// BASIC OPERATION
//

#pragma region Basic operation

static CHAR translate_acs_char(VT* vt, CHAR c)
{
    if (c >= 0x60 && c <= 0x7e)
        return vt->config.acs_chars[c - 0x60];
    return c;
}

static void vt_add_regular_char(VT* vt, CHAR c)
{
    if (vt->acs_mode)
        c = translate_acs_char(vt, c);

    vt_scroll_based_on_cursor(vt);
    if (vt->insert_mode)
        vt_scroll_horizontal(vt, vt->cursor.row, vt->cursor.column, 1);
    vt_set_ch(vt, vt->cursor.row, vt->cursor.column, c);
    vt_add_event(vt, &(VTEvent) {
        .type = VT_EVENT_CELLS_UPDATED,
        .cells = { .row_start = vt->cursor.row, .row_end = vt->cursor.row, .column_start = vt->cursor.column, .column_end = vt->cursor.column }
    });
    vt_cursor_advance(vt, 0, 1);
}

static void vt_add_char(VT* vt, CHAR c)
{
    if (vt->config.debug >= VT_DEBUG_ALL_BYTES) {
        if (c >= 32 && c < 127)
            printf("%c", c);
        else if (c != '\e')
            printf("\e[0;36m[%02X]\e[0m", c);
        if (c == 10 || c == 13)
            printf("\n");
        fflush(stdout);
    }

    switch (c) {
        case '\r':  // CR
            vt_cursor_to_bol(vt);
            break;
        case '\n':  // LF
            vt_cursor_advance(vt, 1, 0);
            if (vt->cursor.row > vt->scroll_area_bottom) {
                vt_scroll_vertical(vt, vt->scroll_area_top, vt->scroll_area_bottom, 1);
                vt_cursor_advance(vt, -1, 0);
            }
            break;
        case '\b':
            vt_cursor_advance(vt, 0, -1);
            break;
        case '\t':
            vt_cursor_tab(vt);
            break;
        case 7: // BELL
            vt_beep(vt);
            break;
        case '\e':
            vt_start_escape_seq(vt, c);
            break;
        default:
            vt_add_regular_char(vt, c);
    }

    if (c != '\e')
        vt->last_char = c;
}

static void vt_add_to_window_title(VT* vt, CHAR c)
{
    if (vt->config.debug >= VT_DEBUG_ALL_BYTES) {
        if (c >= 32 && c < 127)
            printf("\e[0;34m%c\e[0m", c);
        else
            printf("\e[0;34m[%02X]\e[0m", c);
        fflush(stdout);
    }

    if (vt->last_text_received == NULL) {
        vt->last_text_received = calloc(1, 2);
        vt->last_text_received[0] = (char) c;
    } else {
        size_t len = strlen(vt->last_text_received);
        vt->last_text_received = realloc(vt->last_text_received, len + 2);
        vt->last_text_received[len] = (char) c;
        vt->last_text_received[len + 1] = 0;
    }

    size_t len = strlen(vt->last_text_received);
    bool end = false;
    if (vt->last_text_received[len - 1] == 0x7) {
        vt->last_text_received[len - 1] = '\0';
        end = true;
    } else if (len >= 2 && vt->last_text_received[len - 2] == '\e' && vt->last_text_received[len -1] == '\\') {
        vt->last_text_received[len - 2] = '\0';
        end = true;
    }

    if (end) {
        vt_add_event(vt, &(VTEvent) { .type = VT_EVENT_TEXT_RECEIVED, .text_received = {
            .type = vt->receiving_text, .text = strdup(vt->last_text_received) } });
        vt->receiving_text = VTT_NOT_RECEIVING;
    }
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    // parse characters
    for (size_t i = 0; i < str_sz; ++i) {
        CHAR c = str[i];
        if (vt->receiving_text != VTT_NOT_RECEIVING)
            vt_add_to_window_title(vt, c);
        else if (!vt->esc_buffer[0])   // not parsing escape sequence
            vt_add_char(vt, c);
        else
            vt_add_escape_char(vt, c);
    }
}

#pragma endregion

//
// INFORMATION
//

#pragma region Information

VTCell vt_char(VT* vt, INT row, INT column)
{
    if (row * vt->columns + column >= vt->rows * vt->columns && vt->config.debug >= VT_DEBUG_ERRORS_ONLY) {
        fprintf(stderr, "vt_char: trying read data outside of screen bounds");
        return (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };
    }

    VTCell ch = vt->matrix[row * vt->columns + column];

    if (vt->cursor.visible && vt->cursor.row == row && vt->config.automatic_cursor &&
            (vt->cursor.column == column || (column == vt->columns - 1 && vt->cursor.column == vt->columns))) {
        ch.attrib.bg_color = vt->cursor.blinking ? vt->config.blinking_cursor_color : vt->config.cursor_color;
        ch.attrib.fg_color = vt->config.cursor_char_color;
    }

    if (ch.attrib.bold && vt->config.bold_is_bright && ch.attrib.fg_color < 8)
        ch.attrib.fg_color += 8;

    if (ch.attrib.reverse) {
        VTColor swp = ch.attrib.fg_color;
        ch.attrib.fg_color = ch.attrib.bg_color;
        ch.attrib.bg_color = swp;
    }

    return ch;
}

INT vt_rows(VT* vt)
{
    return vt->rows;
}

INT vt_columns(VT* vt)
{
    return vt->columns;
}

VTCursor vt_cursor(VT* vt)
{
    VTCursor cursor = vt->cursor;
    cursor.row = MIN(MAX(cursor.row, 0), vt->rows - 1);
    cursor.column = MIN(MAX(cursor.column, 0), vt->columns - 1);
    return cursor;
}

#pragma endregion

//
// KEY TRANSLATION
//

#pragma region Key Translation

typedef struct {
    VTKeys      key;
    const char* str;
} InputKey;

static const InputKey vt_strings[] = {
    { VT_ARROW_UP,    "\e[A" },
    { VT_ARROW_DOWN,  "\e[B" },
    { VT_ARROW_RIGHT, "\e[C" },
    { VT_ARROW_LEFT,  "\e[D" },
    { VT_HOME,        "\e[H" },
    { VT_END,         "\e[F" },
    { VT_INSERT,      "\e[2~" },
    { VT_BACKSPACE,   "\b" },
    { VT_TAB,         "\t" },
    { VT_PAGE_UP,     "\e[5~" },
    { VT_PAGE_DOWN,   "\e[6~" },
    { VT_F1,          "\eOP" },
    { VT_F2,          "\eOQ" },
    { VT_F3,          "\eOR" },
    { VT_F4,          "\eOS" },
    { VT_F5,          "\e[15~" },
    { VT_F6,          "\e[17~" },
    { VT_F7,          "\e[18~" },
    { VT_F8,          "\e[19~" },
    { VT_F9,          "\e[20~" },
    { VT_F10,         "\e[21~" },
    { VT_F11,         "\e[23~" },
    { VT_F12,         "\e[24~" },
};

static const InputKey vt_strings_ctrl[] = {
    { VT_ARROW_UP,    "\e[1;5A" },
    { VT_ARROW_DOWN,  "\e[1;5B" },
    { VT_ARROW_RIGHT, "\e[1;5C" },
    { VT_ARROW_LEFT,  "\e[1;5D" },
    { VT_HOME,        "\e[1;5H" },
    { VT_END,         "\e[1;5F" },
    { VT_INSERT,      "\e[2;5~" },
    { VT_BACKSPACE,   "\b" },
    { VT_TAB,         "\t" },
    { VT_PAGE_UP,     "\e[5;5~" },
    { VT_PAGE_DOWN,   "\e[6;5~" },
    { VT_F1,          "\e[1;5P" },
    { VT_F2,          "\e[1;5Q" },
    { VT_F3,          "\e[1;5R" },
    { VT_F4,          "\e[1;5S" },
    { VT_F5,          "\e[15;5~" },
    { VT_F6,          "\e[17;5~" },
    { VT_F7,          "\e[18;5~" },
    { VT_F8,          "\e[19;5~" },
    { VT_F9,          "\e[20;5~" },
    { VT_F10,         "\e[21;5~" },
    { VT_F11,         "\e[23;5~" },
    { VT_F12,         "\e[24;5~" },
};

static const InputKey vt_strings_shift[] = {
    { VT_ARROW_UP,    "\e[1;2A" },
    { VT_ARROW_DOWN,  "\e[1;2B" },
    { VT_ARROW_RIGHT, "\e[1;2C" },
    { VT_ARROW_LEFT,  "\e[1;2D" },
    { VT_HOME,        "\e[1;2H" },
    { VT_END,         "\e[1;2F" },
    { VT_INSERT,      "\e[2;5~" },  // TODO
    { VT_BACKSPACE,   "\x7f" },
    { VT_TAB,         "\e[Z" },
    { VT_PAGE_UP,     "\e[5~" },
    { VT_PAGE_DOWN,   "\e[6~" },
    { VT_F1,          "\e[1;2P" },
    { VT_F2,          "\e[1;2Q" },
    { VT_F3,          "\e[1;2R" },
    { VT_F4,          "\e[1;2S" },
    { VT_F5,          "\e[15;2~" },
    { VT_F6,          "\e[17;2~" },
    { VT_F7,          "\e[18;2~" },
    { VT_F8,          "\e[19;2~" },
    { VT_F9,          "\e[20;2~" },
    { VT_F10,         "\e[21;2~" },
    { VT_F11,         "\e[23;2~" },
    { VT_F12,         "\e[24;2~" },
};


static const InputKey vt_strings_ctrl_shift[] = {
    { VT_ARROW_UP,    "\e[1;6A" },
    { VT_ARROW_DOWN,  "\e[1;6B" },
    { VT_ARROW_RIGHT, "\e[1;6C" },
    { VT_ARROW_LEFT,  "\e[1;6D" },
    { VT_HOME,        "\e[1;6H" },
    { VT_END,         "\e[1;6F" },
    { VT_INSERT,      "\e[2;6~" },
    { VT_BACKSPACE,   "\b" },
    { VT_TAB,         "\t" },
    { VT_PAGE_UP,     "\e[5;6~" },
    { VT_PAGE_DOWN,   "\e[6;6~" },
    { VT_F1,          "\e[1;6P" },
    { VT_F2,          "\e[1;6Q" },
    { VT_F3,          "\e[1;6R" },
    { VT_F4,          "\e[1;6S" },
    { VT_F5,          "\e[15;6~" },
    { VT_F6,          "\e[17;6~" },
    { VT_F7,          "\e[18;6~" },
    { VT_F8,          "\e[19;6~" },
    { VT_F9,          "\e[20;6~" },
    { VT_F10,         "\e[21;6~" },
    { VT_F11,         "\e[23;6~" },
    { VT_F12,         "\e[24;6~" },
};


int vt_translate_key(VT* vt, uint16_t key, bool shift, bool ctrl, char* output, size_t max_sz)
{
    (void) vt;

    memset(output, 0, max_sz);

    if (key == '\r') {
        output[0] = '\n';
        return 1;
    }

    if (vt->cursor_app_mode) {
        switch (key) {
            case VT_ARROW_UP:    return sprintf(output, "\eOA");
            case VT_ARROW_DOWN:  return sprintf(output, "\eOB");
            case VT_ARROW_RIGHT: return sprintf(output, "\eOC");
            case VT_ARROW_LEFT:  return sprintf(output, "\eOD");
        }
    }

#define CHECK_KBD(SHIFT, CTRL, ARRAY) \
    if (SHIFT && CTRL) \
        for (size_t i = 0; i < (sizeof ARRAY) / (sizeof ARRAY[0]); ++i) \
            if (key == ARRAY[i].key) \
                return snprintf(output, max_sz, "%s", ARRAY[i].str);

    CHECK_KBD(!shift, !ctrl, vt_strings)
    CHECK_KBD(shift,  !ctrl, vt_strings_shift)
    CHECK_KBD(!shift, ctrl,  vt_strings_ctrl)
    CHECK_KBD(shift,  ctrl,  vt_strings_ctrl_shift)
#undef CHECK_KBD

    if (key != 0) {
        if (ctrl) {
            if (key >= 'A' && key <= '_')
                output[0] = key - 'A' + 1;
            else if (key >= 'a' && key <= 'z')
                output[0] = key - 'a' + 1;
        } else {
            output[0] = (char) key;
        }
    }

    return 1;
}

#pragma endregion

//
// MOUSE TRANSLATION
//

#pragma region Mouse Translation


static int generate_mouse_sequence(VT* vt, INT row, INT column, VTMouseButton button, bool down, VTMouseModifier mod, bool is_move, char* output, size_t max_sz)
{
    if (vt->sgr_mouse_mode) {
        // TODO - what about dragging?
        int m = button + mod + (is_move ? 32 : 0);
        return snprintf(output, max_sz, "\e<%d;%d;%d%c", m, column + 1, row + 1, down ? 'M' : 'm');
    } else {
        if (down)
            button = VTM_RELEASE;
        if (is_move)
            button = vt->last_mouse_button;
        int m = button + mod + (is_move ? 32 : 0);
        if (row >= 223 || column >= 223)
            return 0;
        return snprintf(output, max_sz, "\e[M%c%c%c", m + 32, column + 33, row + 33);
    }
}

int vt_translate_mouse_move(VT* vt, INT row, INT column, char* output, size_t max_sz)
{
    bool same_pos = false;
    if (row == vt->last_mouse_row && column == vt->last_mouse_column)
        same_pos = true;

    vt->last_mouse_row = row;
    vt->last_mouse_column = column;

    if (!same_pos && (vt->mouse_tracking == VTM_ALL || (vt->mouse_tracking == VTM_DRAG && vt->last_mouse_button != VTM_RELEASE)))
        return generate_mouse_sequence(vt, row, column, vt->last_mouse_button, false, vt->last_mouse_mod, true, output, max_sz);

    return 0;
}

int vt_translate_mouse_click(VT* vt, INT row, INT column, VTMouseButton button, bool down, VTMouseModifier mod, char* output, size_t max_sz)
{
    if (vt->mouse_tracking == VTM_NO)
        return 0;
    int r = generate_mouse_sequence(vt, row, column, button, down, mod, false, output, max_sz);
    vt->last_mouse_button = down ? button : VTM_RELEASE;
    vt->last_mouse_mod = mod;
    return r;
}

#pragma endregion

// https://man7.org/linux/man-pages/man4/console_codes.4.html
