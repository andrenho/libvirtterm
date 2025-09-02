#include "libvirtterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    int        rows;
    int        columns;
    VTCursor   cursor;
    VTConfig   config;
    VTAttrib   current_attrib;
    VTCallback push_event;
    void*      data;
    char       current_buffer[32];
    int        scroll_area_top;
    int        scroll_area_bottom;
    bool       acs_mode;
    VTCell*    matrix;
} VT;

//
// INITIALIZATION
//

#pragma region Initialization

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

#pragma endregion

//
// BASIC OPERATION
//

#pragma region Basic operation

void vt_write(VT* vt, const char* str, int str_sz)
{
}

#pragma endregion

//
// INFORMATION
//

#pragma region Information

VTCell vt_char(VT* vt, int row, int column)
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

size_t vt_rows(VT* vt)
{
    return vt->rows;
}

size_t vt_columns(VT* vt)
{
    return vt->columns;
}

VTCursor vt_cursor(VT* vt)
{
    return vt->cursor;
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