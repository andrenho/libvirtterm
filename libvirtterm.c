#include "libvirtterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    VTChar ch = vt->matrix[row * vt->rows + column];
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

int vt_translate_key(VT* vt, uint16_t key, char* output, size_t max_sz)
{
    output[0] = (char) key;
    output[1] = 0;
    return 1;
}
