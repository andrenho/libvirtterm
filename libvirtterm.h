#ifndef LIBVIRTTERM_H_
#define LIBVIRTTERM_H_

#include <stdbool.h>
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

typedef enum { VT_NO_UPDATES, VT_CELL_UPDATE, VT_ROW_UPDATE } VTUpdateEvents;

typedef struct VTConfig {
    VTColor        default_fg_color;       // some default colors
    VTColor        default_bg_color;
    VTColor        cursor_color;
    VTColor        cursor_char_color;
    bool           automatic_cursor;       // if true, control cursor from within libvirtterm, else sends cursor updates as events
    VTUpdateEvents update_events;          // when a cell changes, send updates per cell, per line, or none at all
} VTConfig;

#define VT_DEFAULT_CONFIG (VTConfig) {      \
    .default_bg_color = VT_BLACK,           \
    .default_fg_color = VT_WHITE,           \
    .cursor_color = VT_BRIGHT_GREEN,        \
    .cursor_char_color = VT_BLACK,          \
    .automatic_cursor = true,               \
    .update_events = VT_NO_UPDATES,         \
}

typedef struct __attribute__((packed)) VTAttrib {
    bool bold        : 1;
    bool dim         : 1;
    bool underline   : 1;
    bool blink       : 1;   // automatically managed
    bool reverse     : 1;   //       "          "
    bool invisible   : 1;   //       "          "
    VTColor bg_color : 4;
    VTColor fg_color : 4;
} VTAttrib;

#define DEFAULT_ATTR ((VTAttrib) { .bold = false, .dim = false, .underline = false, .blink = false, .reverse = false, .invisible = false, .bg_color = vt->config.default_bg_color, .fg_color = vt->config.default_fg_color })

typedef struct __attribute__((packed)) VTChar {
    char     ch;
    VTAttrib attrib;
} VTChar;

typedef enum VTEventType {
    VT_EVENT_CELL_UPDATE,
    VT_EVENT_ROW_UPDATE,
} VTEventType;

typedef struct VTEvent {
    VTEventType type;
    union {
        struct {
            size_t row;
            size_t column;
        } cell;
        struct {
            size_t row;
        } row;
    };
} VTEvent;

typedef struct VT VT;
typedef void (*VTCallback)(VT* vt, VTEvent* e);

typedef struct VTCursor {
    bool   visible;
    size_t row;
    size_t column;
} VTCursor;

typedef struct VT {
    size_t     rows;
    size_t     columns;
    VTCursor   cursor;
    VTConfig   config;
    VTAttrib   current_attrib;
    VTCallback push_event;
    void*      data;
    VTChar*    matrix;
} VT;

VT*  vt_new(size_t rows, size_t columns, VTCallback callback, VTConfig const* config, void* data);
void vt_free(VT* vt);

VTChar vt_char(VT* vt, size_t row, size_t column);

void vt_resize(VT* vt, size_t rows, size_t columns);
void vt_write(VT* vt, const char* str, size_t str_sz);

void vt_configure(VT* vt, VTConfig* config);

#endif
