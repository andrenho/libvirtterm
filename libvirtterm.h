#ifndef LIBVIRTTERM_H_
#define LIBVIRTTERM_H_

#include <stddef.h>

typedef enum VTColor {
    VT_BLACK = 0,
    VT_RED,
    VT_GREEN,
    VT_YELLOW,
    VT_BLUE,
    VT_MAGENTA,
    VT_CYAN,
    VT_WHITE,
    VT_BRIGHT_BLACK,
    VT_BRIGHT_RED,
    VT_BRIGHT_GREEN,
    VT_BRIGHT_YELLOW,
    VT_BRIGHT_BLUE,
    VT_BRIGHT_MAGENTA,
    VT_BRIGHT_CYAN,
    VT_BRIGHT_WHITE,
} VTColor;

typedef struct __attribute__((packed)) VTAttrib {
    VTColor bg_color : 4;
    VTColor fg_color : 4;
} VTAttrib;

#define DEFAULT_ATTR ((VTAttrib) { .bg_color = VT_BLACK, .fg_color = VT_WHITE })

typedef struct __attribute__((packed)) VTChar {
    char     ch;
    VTAttrib attrib;
} VTChar;

typedef enum VTEvent {
    VT_EVENT_UPDATE,
} VTEvent;

typedef struct VT VT;
typedef void (*VTCallback)(VT* vt, VTEvent* e);

typedef struct VT {
    size_t     rows;
    size_t     columns;
    VTCallback callback;
    void*      data;
    VTChar*    matrix;
} VT;

VT*  vt_new(size_t rows, size_t columns, VTCallback callback, void* data);
void vt_free(VT* vt);

void vt_resize(VT* vt, size_t rows, size_t columns);
void vt_write(VT* vt, const char* str, size_t str_sz);

#endif
