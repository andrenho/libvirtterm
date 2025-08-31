#include "libvirtterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })
#define MAX(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

//
// INPUT KEYS
//

typedef struct {
    VTKeys      key;
    const char* str;
} InputKey;

static const InputKey vt_strings[] = {
    { VT_ARROW_UP,    "\e[A "},
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

//
// INITIALIZATION
//

VT* vt_new(int rows, int columns, VTCallback callback, VTConfig const* config, void* data)
{
    VT* vt = calloc(1, sizeof(VT));
    vt->rows = rows;
    vt->columns = columns;
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true };
    vt->config = *config;
    vt->current_attrib = DEFAULT_ATTR;
    vt->scroll_area_top = 1;
    vt->scroll_area_bottom = vt->rows;
    vt->push_event = callback;
    vt->data = data;
    memset(vt->current_buffer, 0, sizeof vt->current_buffer);
    vt->buffer_mode = false;
    vt_resize(vt, rows, columns);
    return vt;
}

void vt_free(VT* vt)
{
    if (vt)
        free(vt->matrix);
    free(vt);
}

//
// TERMINAL MANIPULATION
//

void vt_resize(VT* vt, int rows, int columns)
{
    vt->rows = rows;
    vt->columns = columns;

    free(vt->matrix);

    vt->matrix = malloc(sizeof(VTCell) * rows * columns);
    for (int i = 0; i < rows * columns; ++i)
        vt->matrix[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };

    // TODO - keep chars when resizing
}

static void clear_cells(VT* vt, int from, int to)
{
    bool rows_updates[vt->rows];
    memset(rows_updates, 0, (sizeof rows_updates[0]) * vt->rows);

    for (int i = from; i <= to; ++i) {
        int row = i / vt->columns;
        int column = i % vt->columns;
        vt->matrix[i].ch = ' ';
        if (vt->config.update_events == VT_CELL_UPDATE)
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { .row = row, .column = column } });
        rows_updates[row] = true;
    }

    if (vt->config.update_events == VT_ROW_UPDATE) {
        for (int i = 0; i < vt->rows; ++i)
            if (rows_updates[i])
                vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .row = { .row = i } });
    }
}

static void scroll(VT* vt, int amount)   // negative amount means scroll down
{
    VTCell* m = vt->matrix;
    VTCell* initial_cell = &m[(vt->scroll_area_top - 1) * vt->columns];
    int cell_count = (vt->scroll_area_bottom - vt->scroll_area_top) * vt->columns;
    int amount_cells = abs(amount * vt->columns);

    if (amount > 0) {
        // move cells
        memmove(initial_cell, initial_cell + amount_cells, cell_count * sizeof(VTCell));

        // clear cells
        int initial_cell_to_blank = (vt->scroll_area_bottom - 1) * vt->columns;
        clear_cells(vt, initial_cell_to_blank, initial_cell_to_blank + amount_cells - 1);

    } else if (amount < 0) {
        // move cells
        memmove(initial_cell + amount_cells, initial_cell, cell_count * sizeof(VTCell));

        // clear cells
        int initial_cell_to_blank = (vt->scroll_area_top - 1) * vt->columns;
        clear_cells(vt, initial_cell_to_blank, initial_cell_to_blank + amount_cells - 1);
    }

    if (vt->config.on_scroll == VT_NOTIFY) {
        vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_SCROLL_UP, .scroll = { .count = 1, .top_row = vt->scroll_area_top, .bottom_row = vt->scroll_area_bottom } });
    } if (vt->config.update_events == VT_ROW_UPDATE) {
        for (int row = vt->scroll_area_top - 1; row <= vt->scroll_area_bottom - 1; ++row)
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_ROW_UPDATE, .row = { .row = row } });
    } else if (vt->config.update_events == VT_CELL_UPDATE) {
        // TODO - only update the cells that actually changed
        for (int row = vt->scroll_area_top - 1; row <= vt->scroll_area_bottom - 1; ++row) {
            for (int column = 0; column < vt->columns; ++column) {
                vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { .column = column, .row = row } });
            }
        }
    }
}

//
// PARSE ESCAPE SEQUENCES
//

static void escape_seq_clear_cells(VT* vt, char mode, int parameter)
{
    int cursor = vt->cursor.row * vt->columns + vt->cursor.column;

    if (mode == 'J') {
        int last_cell = vt->rows * vt->columns - 1;
        switch (parameter) {
            case 0:  // cursor to end of screen
                clear_cells(vt, cursor, last_cell);
                break;
            case 1:  // start of screen to cursor
                clear_cells(vt, 0, cursor);
                break;
            case 2:  // all screen
                clear_cells(vt, 0, last_cell);
                break;
            case 3:
                break;
            default:
                fprintf(stderr, "Unsupported parameter for ESC[#J");
        }
    } else if (mode == 'K') {
        int start_of_line = vt->cursor.row * vt->columns;
        int end_of_line = (vt->cursor.row + 1) * vt->columns - 1;
        switch (parameter) {
            case 0:  // cursor to end of screen
                clear_cells(vt, cursor, end_of_line);
                break;
            case 1:  // start of screen to cursor
                clear_cells(vt, start_of_line, cursor);
                break;
            case 2:  // all screen
                clear_cells(vt, start_of_line, end_of_line);
                break;
            default:
                fprintf(stderr, "Unsupported parameter for ESC[#K");
        }
    }
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
        case 49: vt->current_attrib.bg_color = vt->config.default_fg_color; break;
    }
}

static bool match(const char* data, const char* pattern, int args[8], int* argn)
{
    int i = 0;

    *argn = 0;
    memset(args, 0, sizeof args[0] * 8);

    if (data[strlen(data) - 1] != pattern[strlen(pattern) - 1])  // fail fast
        return false;

    for (const char* p = pattern; *p; ++p) {
        while (data[i] == ';')
            ++i;
        if (*p == '#') {
            char* endptr;
            args[(*argn)++] = strtol(&data[i], &endptr, 10);
            i = endptr - data;
        } else if (*p != data[i] && data[i] != ';') {
            return false;
        } else {
            ++i;
        }
    }

    return i == strlen(data);
}

static bool parse_escape_sequence(VT* vt)
{
    int args[8] = {0};
    int argn;

    int len = strlen(vt->current_buffer);
    char last = vt->current_buffer[len - 1];
    if (isalpha(last) || len >= sizeof vt->current_buffer) {  // absolute cursor position
        // printf("%s\n", vt->current_buffer);
#define MATCH(pattern) (match(vt->current_buffer, pattern, args, &argn))
        if MATCH("[##H") {
            vt->cursor.row = MAX(args[0] - 1, 0);
            vt->cursor.column = MAX(args[1] - 1, 0);
        }
        else if MATCH("A") vt->cursor.row = MAX(vt->cursor.row - 1, 0);
        else if MATCH("C") vt->cursor.row = MIN(vt->cursor.column + 1, vt->columns - 1);
        else if MATCH("[#J") escape_seq_clear_cells(vt, 'J', args[0]);
        else if MATCH("[#K") escape_seq_clear_cells(vt, 'K', args[0]);
        else if MATCH("M") {
            if (vt->cursor.row == 0)
                scroll(vt, -1);
            vt->cursor.row = MAX(vt->cursor.row - 1, 0);
        }
        else if (MATCH("[##r") && args[0] <= args[1]) {
            vt->scroll_area_top = args[0];
            vt->scroll_area_bottom = args[1];
            vt->cursor.column = 0;
            vt->cursor.row = 0;
        }
        else if (MATCH("[##m")) {
            update_current_attrib(vt, 0);
            for (int i = 0; i < argn; ++i)
                update_current_attrib(vt, args[i]);
        }
        else if (MATCH("[?2004h") || MATCH("[?2004l")) /* TODO */ ;  // ignore for now
        else {
            fprintf(stderr, "Unknown escape sequence: ^[%s\n", vt->current_buffer);
        }
#undef MATCH
        return true;
    }
    return false;
}

//
// ADD TEXT TO TERMINAL
//

static void add_char(VT* vt, char c, int* row, int* column)
{
    *row = vt->cursor.row;
    *column = vt->cursor.column;

    switch (c) {
        case 10:
            ++vt->cursor.row;
            break;
        case 13:
            vt->cursor.column = 0;
            break;
        case '\b':
            vt->cursor.column = MAX(vt->cursor.column - 1, 0);
            break;
        case '\t':
            for (int i = 0; i < 8; ++i)
                add_char(vt, ' ', row, column);
            break;
        case 7:
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_BELL });
            break;
        default:
            vt->matrix[vt->cursor.row * vt->columns + vt->cursor.column] = (VTCell) {
                .ch = c,
                .attrib = vt->current_attrib,
            };
            ++vt->cursor.column;
    }

    if (vt->cursor.column == vt->columns) {
        vt->cursor.column = 0;
        ++vt->cursor.row;
    }

    if (vt->cursor.row == vt->scroll_area_bottom) {
        scroll(vt, 1);
        --vt->cursor.row;
        vt->cursor.column = 0;
    }
}


void vt_write(VT* vt, const char* str, int str_sz)
{
    bool rows_updates[vt->rows];
    memset(rows_updates, 0, (sizeof rows_updates[0]) * vt->rows);

    for (int i = 0; i < str_sz; ++i) {
        if (vt->buffer_mode) {
            vt->current_buffer[strlen(vt->current_buffer)] = str[i];
            if (parse_escape_sequence(vt))
                vt->buffer_mode = false;
        } else {
            if (str[i] == '\e') {
                memset(vt->current_buffer, 0, sizeof vt->current_buffer);
                vt->buffer_mode = true;
            } else {  // show char in screen
                int row, column;
                add_char(vt, str[i], &row, &column);
                rows_updates[row] = true;
                if (vt->config.update_events == VT_CELL_UPDATE) {
                    vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { row, column } });
                }
            }
        }
    }

    if (vt->config.update_events == VT_ROW_UPDATE) {
        for (int i = 0; i < vt->rows; ++i)
            if (rows_updates[i])
                vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .row = { .row = i } });
    }
}

//
// INFORMATION
//

VTCell vt_char(VT* vt, int row, int column)
{
    VTCell ch = vt->matrix[row * vt->columns + column];
    if (vt->cursor.visible && vt->cursor.row == row && vt->cursor.column == column && vt->config.automatic_cursor) {
        ch.attrib.bg_color = vt->config.cursor_color;
        ch.attrib.fg_color = vt->config.cursor_char_color;
    }
    return ch;
}

int vt_translate_key(VT* vt, uint16_t key, bool shift, bool ctrl, char* output, int max_sz)
{
    (void) vt;

    memset(output, 0, max_sz);

    if (key == '\r') {
        output[0] = '\n';
        return 1;
    }

#define CHECK_KBD(SHIFT, CTRL, ARRAY) \
    if (SHIFT && CTRL) \
        for (int i = 0; i < (sizeof ARRAY) / (sizeof ARRAY[0]); ++i) \
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
