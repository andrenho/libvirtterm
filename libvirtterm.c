#include "libvirtterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VT* vt_new(size_t rows, size_t columns, VTCallback callback, void* data)
{
    VT* vt = calloc(1, sizeof(VT));
    vt->rows = rows;
    vt->columns = columns;
    vt->cursor = (VTCursor) { .column = 0, .row = 0, .visible = true };
    vt->callback = callback;
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
        vt->matrix[i] = (VTChar) { .ch = ' ' };

    // TODO - keep chars when resizing
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    (void) vt;
    (void) str;
    (void) str_sz;
}

VTChar vt_char(VT* vt, size_t row, size_t column)
{

}
