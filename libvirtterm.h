#ifndef LIBVIRTTERM_H_
#define LIBVIRTTERM_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//
// Keys
//

typedef enum VTKeys {
    VT_ESC = 0x100,
    VT_F1,
    VT_F2,
    VT_F3,
    VT_F4,
    VT_F5,
    VT_F6,
    VT_F7,
    VT_F8,
    VT_F9,
    VT_F10,
    VT_F11,
    VT_F12,
    VT_INSERT,
    VT_DELETE,
    VT_HOME,
    VT_END,
    VT_PAGE_UP,
    VT_PAGE_DOWN,
    VT_ARROW_UP,
    VT_ARROW_DOWN,
    VT_ARROW_LEFT,
    VT_ARROW_RIGHT,
    VT_BACKSPACE,
    VT_TAB,
} VTKeys;

//
// Colors
//

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

//
// Config
//

typedef enum { VT_NO_UPDATES, VT_CELL_UPDATE, VT_ROW_UPDATE } VTUpdateEvents;
typedef enum { VT_NOTIFY, VT_REFRESH } VTScrollAction;

typedef struct VTConfig {
    VTColor          default_fg_color;       // some default colors
    VTColor          default_bg_color;
    VTColor          cursor_color;
    VTColor          cursor_char_color;
    bool             automatic_cursor;       // if true, control cursor from within libvirtterm, else sends cursor updates as events
    VTUpdateEvents   update_events;          // when a cell changes, send updates per cell, per line, or none at all
    VTScrollAction on_scroll;
} VTConfig;

#define VT_DEFAULT_CONFIG (VTConfig) {      \
    .default_bg_color = VT_BLACK,           \
    .default_fg_color = VT_WHITE,           \
    .cursor_color = VT_BRIGHT_GREEN,        \
    .cursor_char_color = VT_BLACK,          \
    .automatic_cursor = true,               \
    .update_events = VT_NO_UPDATES,         \
    .on_scroll = VT_REFRESH,                \
}

//
// Cells
//

typedef struct __attribute__((packed)) VTAttrib {
    bool bold        : 1;
    bool dim         : 1;
    bool underline   : 1;
    bool blink       : 1;   // automatically managed
    bool reverse     : 1;   //       "          "
    bool invisible   : 1;   //       "          "
    int  padding     : 2;
    VTColor bg_color : 4;
    VTColor fg_color : 4;
} VTAttrib;

#define DEFAULT_ATTR ((VTAttrib) { .bold = false, .dim = false, .underline = false, .blink = false, .reverse = false, .invisible = false, .bg_color = vt->config.default_bg_color, .fg_color = vt->config.default_fg_color })

typedef struct __attribute__((packed)) VTCell {
    char     ch;
    VTAttrib attrib;
} VTCell;

//
// Events
//

typedef enum VTEventType {
    VT_EVENT_CELL_UPDATE,
    VT_EVENT_ROW_UPDATE,
    VT_EVENT_SCROLL_UP,
    VT_EVENT_BELL,
} VTEventType;

typedef struct VTEvent {
    VTEventType type;
    union {
        struct {
            int row;
            int column;
        } cell;
        struct {
            int row;
        } row;
        struct {
            int count;
            int top_row;
            int bottom_row;
        } scroll;
    };
} VTEvent;

//
// Terminal
//

typedef struct VT VT;
typedef void (*VTCallback)(VT* vt, VTEvent* e);

typedef struct VTCursor {
    bool   visible;
    int    row;
    int    column;
} VTCursor;

typedef struct VT {
    int        rows;
    int        columns;
    VTCursor   cursor;
    VTConfig   config;
    VTAttrib   current_attrib;
    VTCallback push_event;
    void*      data;
    char       current_buffer[32];
    bool       buffer_mode;
    int        scroll_area_top;
    int        scroll_area_bottom;
    VTCell*    matrix;
} VT;

//
// Functions
//

VT*  vt_new(int rows, int columns, VTCallback callback, VTConfig const* config, void* data);
void vt_free(VT* vt);

VTCell vt_char(VT* vt, int row, int column);

void vt_resize(VT* vt, int rows, int columns);
void vt_write(VT* vt, const char* str, int str_sz);
int  vt_translate_key(VT* vt, uint16_t key, bool shift, bool ctrl, char* output, int max_sz);

#endif
