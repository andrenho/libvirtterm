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

typedef struct VT {
    // terminal configuration
    INT        rows;
    INT        columns;
    VTConfig   config;

    // user provided pointers
    VTCallback push_event;
    void*      data;

    // terminal state
    VTCell*    matrix;
    VTCursor   cursor;
    VTAttrib   current_attrib;
    INT        scroll_area_top;
    INT        scroll_area_bottom;
    bool       acs_mode;

    // escape sequence parsing
    char       esc_buffer[32];
    bool*      rows_updated;
} VT;

//
// TERMINAL CAPABILITIES
//

#pragma region Terminal Capabilities

// special characters
#define BELL        7
#define BACKSPACE   '\b'
#define CR          '\r'
#define LF          '\n'
#define TAB         '\t'

#pragma endregion

//
// INITIALIZATION
//

#pragma region Initialization

VT* vt_new(INT rows, INT columns, VTCallback callback, VTConfig const* config, void* data)
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
    memset(vt->esc_buffer, 0, sizeof vt->esc_buffer);
    vt_resize(vt, rows, columns);
    return vt;
}

void vt_free(VT* vt)
{
    if (vt)
        free(vt->matrix);
    free(vt);
}

void vt_reset(VT* vt)
{
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true };
    vt->current_attrib = DEFAULT_ATTR;
    vt->scroll_area_top = 1;
    vt->scroll_area_bottom = vt->rows;
    for (int i = 0; i < vt->rows * vt->columns; ++i)
        vt->matrix[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };
}

void vt_resize(VT* vt, INT rows, INT columns)
{
    vt->rows = rows;
    vt->columns = columns;

    free(vt->matrix);

    vt->matrix = malloc(sizeof(VTCell) * rows * columns);
    vt->rows_updated = calloc(sizeof(bool), rows);
    for (int i = 0; i < rows * columns; ++i)
        vt->matrix[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };

    // TODO - keep chars when resizing
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
    vt->matrix[row * vt->columns + column] = (VTCell) {
        .ch = c,
        .attrib = vt->current_attrib,
    };

    if (vt->config.update_events == VT_ROW_UPDATE)
        vt->rows_updated[row] = true;
    else if (vt->config.update_events == VT_CELL_UPDATE)
        vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { .row = row, .column = column } });
}

#pragma endregion

//
// CURSOR MOVEMENT
//

#pragma region Cursor Movement

static void vt_cursor_advance(VT* vt, int rows, int columns)
{
    vt->cursor.row += rows;
    vt->cursor.column += columns;
}

static void vt_cursor_to_bol(VT* vt)
{
    vt->cursor.column = 0;
}

static void vt_cursor_tab(VT* vt)
{
    vt->cursor.column = ((vt->cursor.column / 8) + 1) * 8;
}

#pragma endregion

//
// SCROLLING
//

#pragma region Scrolling

static void vt_scroll_based_on_cursor(VT* vt)
{
    if (vt->cursor.column == vt->columns) {
        vt->cursor.column = 0;
        ++vt->cursor.row;
    }

    // TODO - scroll bottom
}

#pragma endregion

//
// ESCAPE SEQUENCES
//

#pragma region Escape Sequences

static void start_escape_seq(VT* vt, char c)
{
    // TODO
}

static bool parse_escape_seq(VT* vt)
{
    // TODO
    return false;
}

static void end_escape_seq(VT* vt)
{
    // TODO
}

static void vt_add_escape_char(VT* vt, char c)
{
    size_t len = strlen(vt->esc_buffer);
    if (len == sizeof vt->esc_buffer - 1) {
        end_escape_seq(vt);
        return;
    }

    vt->esc_buffer[len] = c;

    if (parse_escape_seq(vt))
        end_escape_seq(vt);
}

#pragma endregion

//
// BASIC OPERATION
//

#pragma region Basic operation

static void vt_add_regular_char(VT* vt, CHAR c)
{
    vt_scroll_based_on_cursor(vt);
    vt_set_ch(vt, vt->cursor.row, vt->cursor.column, c);
    vt_cursor_advance(vt, 0, 1);
}

static void vt_add_char(VT* vt, CHAR c)
{
    switch (c) {
        case CR:
            vt_cursor_to_bol(vt);
            break;
        case BACKSPACE:
            vt_cursor_advance(vt, 0, -1);
            break;
        case TAB:
            vt_cursor_tab(vt);
            break;
        case BELL:
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_BELL });
            break;
        default:
            vt_add_regular_char(vt, c);
    }
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    // parse characters
    for (size_t i = 0; i < str_sz; ++i) {
        CHAR c = str[i];
#ifdef VT_DEBUG_SUPPORT
        if (vt->config.debug >= VT_DEBUG_ALL_BYTES)
            printf("%c      %d  0x%02x\n", c >= 32 && c < 127 ? c : ' ', c, c);
#endif
        if (!vt->esc_buffer[0])   // not parsing escape sequence
            vt_add_char(vt, c);
        else
            vt_add_escape_char(vt, c);
    }

    // send row events
    if (vt->config.update_events == VT_ROW_UPDATE)
        for (INT row = 0; row < vt->rows; ++row)
            if (vt->rows_updated[row])
                vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_ROW_UPDATE, .row = row });
    memset(vt->rows_updated, 0, vt->rows);
}

#pragma endregion

//
// INFORMATION
//

#pragma region Information

VTCell vt_char(VT* vt, INT row, INT column)
{
    VTCell ch = vt->matrix[row * vt->columns + column];

    if (vt->cursor.visible && vt->cursor.row == row && vt->config.automatic_cursor && (
        vt->cursor.column == column || (column == vt->columns - 1 && vt->cursor.column == vt->columns)
    )) {
        ch.attrib.bg_color = vt->config.cursor_color;
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

// https://man7.org/linux/man-pages/man4/console_codes.4.html