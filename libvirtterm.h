#ifndef LIBVIRTTERM_H_
#define LIBVIRTTERM_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define CHAR uint8_t
#define INT  int16_t

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

typedef enum { VT_NO_DEBUG, VT_DEBUG_ERRORS_ONLY, VT_DEBUG_ALL_ESCAPE_SEQUENCES, VT_DEBUG_ALL_BYTES } VTDebug;

typedef struct VTConfig {
    VTColor          default_fg_color;       // some default colors
    VTColor          default_bg_color;
    VTColor          cursor_color;
    VTColor          blinking_cursor_color;
    VTColor          cursor_char_color;
    bool             automatic_cursor;       // if true, control cursor from within libvirtterm, else sends cursor updates as events
    bool             bold_is_bright;         // true = bold is also bright color
    bool             blink_cursor;
    uint16_t         blink_ms;
    CHAR             acs_chars[32];          // see https://en.wikipedia.org/wiki/DEC_Special_Graphics (0x60 ~ 0x7e)
    VTDebug          debug;
} VTConfig;

#define VT_DEFAULT_CONFIG (VTConfig) {              \
    .default_bg_color = VT_BLACK,                   \
    .default_fg_color = VT_WHITE,                   \
    .cursor_color = VT_BRIGHT_GREEN,                \
    .blinking_cursor_color = VT_BRIGHT_YELLOW,      \
    .cursor_char_color = VT_BLACK,                  \
    .automatic_cursor = true,                       \
    .bold_is_bright = true,                         \
    .blink_cursor = false,                          \
    .blink_ms = 600,                                \
    .acs_chars = "+#????o#??+++++~---_++++|<>*!fo", \
    .debug = VT_DEBUG_ERRORS_ONLY,                  \
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
    bool invisible   : 1;
    bool italic      : 1;
    bool dirty       : 1;
    // TODO - add charset
    VTColor bg_color : 4;
    VTColor fg_color : 4;
} VTAttrib;

#define DEFAULT_ATTR ((VTAttrib) { \
    .bold = false, .dim = false, .underline = false, .blink = false, .reverse = false, .invisible = false, .italic = false, .dirty = false,\
    .bg_color = vt->config.default_bg_color, .fg_color = vt->config.default_fg_color \
})

typedef struct __attribute__((packed)) VTCell {
    CHAR     ch;
    VTAttrib attrib;
} VTCell;

//
// Events
//

typedef enum VTEventType {
    VT_EVENT_CELLS_UPDATED,
    VT_EVENT_CURSOR_MOVED,
    VT_EVENT_BELL,
    VT_EVENT_TEXT_RECEIVED,
} VTEventType;

typedef enum VTTextReceivedType {
    VTT_NOT_RECEIVING, VTT_WINDOW_TITLE_UPDATED, VTT_DIRECTORY_HINT_UPDATED,
} VTTextReceivedType;

typedef struct VTEvent {
    VTEventType type;
    union {
        struct {
            INT row_start;
            INT row_end;
            INT column_start;
            INT column_end;
        } cells;
        struct {
            VTTextReceivedType type;
            const char*        text;
        } text_received;
    };
    struct VTEvent* _next;
} VTEvent;

//
// Mouse
//

typedef enum VTMouseButton {
    VTM_LEFT=0, VTM_MIDDLE=1, VTM_RIGHT=2, VTM_SCROLL_UP=3, VTM_SCROLL_DOWN=4, VTM_MAX=5 /* do not use */
} VTMouseButton;

typedef enum VTMouseModifier {
    VTM_SHIFT=4, VTM_ALT=8, VTM_CTRL=16,
} VTMouseModifier;

typedef struct VTMouseState {
    INT             row;
    INT             column;
    bool            button[VTM_MAX];
    VTMouseModifier mod;
} VTMouseState;

//
// Terminal
//

typedef struct VT VT;

typedef struct VTCursor {
    INT  row;
    INT  column;
    bool visible;
    bool blinking;
} VTCursor;

//
// Functions
//

// initialization
VT*  vt_new(INT rows, INT columns, VTConfig const* config);
void vt_free(VT* vt);

// events
bool vt_next_event(VT* vt, VTEvent* e);

// operations
void vt_step(VT* vt, const char* new_text, size_t new_text_sz);
void vt_reset(VT* vt);
void vt_resize(VT* vt, INT rows, INT columns);

// information
VTCell vt_char(VT* vt, INT row, INT column);
int    vt_translate_key(VT* vt, uint16_t key, bool shift, bool ctrl, char* output, size_t max_sz);
int    vt_translate_updated_mouse_state(VT* vt, VTMouseState state, char* output, size_t max_sz);

#define CURSOR_NOT_VISIBLE -1
VTCursor vt_cursor(VT* vt);

INT vt_rows(VT* vt);
INT vt_columns(VT* vt);

#endif
