#include "libvirtterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VT* vt_new(size_t rows, size_t columns, VTCallback callback, void* data)
{
    VT* vt = calloc(1, sizeof(VT));
    vt->rows = rows;
    vt->columns = columns;
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

    vt->matrix[0] = (VTChar) { .ch = 'H', .attrib = (VTAttrib) { .bg_color = VT_BRIGHT_RED, .fg_color = VT_BRIGHT_GREEN } };
    vt->matrix[1] = (VTChar) { .ch = 'e', .attrib = DEFAULT_ATTR };
    vt->matrix[2] = (VTChar) { .ch = 'l', .attrib = DEFAULT_ATTR };
    vt->matrix[3] = (VTChar) { .ch = 'l', .attrib = DEFAULT_ATTR };
    vt->matrix[4] = (VTChar) { .ch = 'o', .attrib = DEFAULT_ATTR };

    // TODO - keep chars
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    (void) vt;
    (void) str;
    (void) str_sz;
}
