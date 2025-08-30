#include "libvirtterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t     hid;
    const char* str;
} HidString;

static const HidString hid_strings[] = {
    { VT_ARROW_UP,    "\e[A "},
    { VT_ARROW_DOWN,  "\e[B" },
    { VT_ARROW_RIGHT, "\e[C" },
    { VT_ARROW_LEFT,  "\e[D" },
    { VT_HOME,        "\e[H" },
    { VT_END,         "\e[F" },
    { VT_INSERT,      "\e[2~" },
    { VT_BACKSPACE,   "\x7f" },
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

static const HidString hid_strings_ctrl[] = {
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

static const HidString hid_strings_shift[] = {
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


static const HidString hid_strings_ctrl_shift[] = {
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

VT* vt_new(size_t rows, size_t columns, VTCallback callback, VTConfig const* config, void* data)
{
    VT* vt = calloc(1, sizeof(VT));
    vt->rows = rows;
    vt->columns = columns;
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true };
    vt->config = *config;
    vt->current_attrib = DEFAULT_ATTR;
    vt->push_event = callback;
    vt->data = data;
    vt_resize(vt, rows, columns);
    return vt;
}

void vt_free(VT* vt)
{
    if (vt)
        free(vt->matrix);
    free(vt);
}

void vt_resize(VT* vt, size_t rows, size_t columns)
{
    vt->rows = rows;
    vt->columns = columns;

    free(vt->matrix);

    vt->matrix = malloc(sizeof(VTChar) * rows * columns);
    for (size_t i = 0; i < rows * columns; ++i)
        vt->matrix[i] = (VTChar) { .ch = ' ', .attrib = DEFAULT_ATTR };

    // TODO - keep chars when resizing
}

static void add_char(VT* vt, char c, size_t* row, size_t* column)
{
    *row = vt->cursor.row;
    *column = vt->cursor.column;

    vt->matrix[vt->cursor.row * vt->rows + vt->cursor.column] = (VTChar) {
        .ch = c,
        .attrib = vt->current_attrib,
    };

    ++vt->cursor.column;
    if (vt->cursor.column == vt->columns) {
        vt->cursor.column = 0;
        ++vt->cursor.row;
    }

    // TODO - scroll up
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    bool rows_updates[vt->rows];
    memset(rows_updates, 0, (sizeof rows_updates[0]) * vt->rows);

    for (size_t i = 0; i < str_sz; ++i) {
        size_t row, column;
        add_char(vt, str[i], &row, &column);
        rows_updates[row] = true;
        if (vt->config.update_events == VT_CELL_UPDATE) {
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { row, column } });
        }
    }

    if (vt->config.update_events == VT_ROW_UPDATE) {
        for (size_t i = 0; i < vt->rows; ++i)
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .row = i });
    }
}

VTChar vt_char(VT* vt, size_t row, size_t column)
{
    VTChar ch = vt->matrix[row * vt->columns + column];
    if (vt->cursor.visible && vt->cursor.row == row && vt->cursor.column == column && vt->config.automatic_cursor) {
        ch.attrib.bg_color = vt->config.cursor_color;
        ch.attrib.fg_color = vt->config.cursor_char_color;
    }
    return ch;
}

void vt_configure(VT* vt, VTConfig* config)
{
    vt->config = *config;
}

int vt_translate_key(VT* vt, uint16_t key, bool shift, bool ctrl, char* output, size_t max_sz)
{
    output[0] = (char) key;
    output[1] = 0;
    return 1;
}
