#include "libvirtterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })
#define MAX(a,b) \
    ({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

typedef struct {
    VTKeys      key;
    const char* str;
} HidString;

static const HidString vt_strings[] = {
    { VT_ARROW_UP,    "\e[A "},
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

static const HidString vt_strings_ctrl[] = {
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

static const HidString vt_strings_shift[] = {
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


static const HidString vt_strings_ctrl_shift[] = {
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
    memset(vt->current_buffer, 0, sizeof vt->current_buffer);
    vt->buffer_mode = false;
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

    vt->matrix = malloc(sizeof(VTCell) * rows * columns);
    for (size_t i = 0; i < rows * columns; ++i)
        vt->matrix[i] = (VTCell) { .ch = ' ', .attrib = DEFAULT_ATTR };

    // TODO - keep chars when resizing
}

static void scroll_up(VT* vt)
{
    memmove(vt->matrix, &vt->matrix[vt->columns], vt->columns * (vt->rows - 1) * sizeof(VTCell));
    for (size_t i = 0; i < vt->columns; ++i) {
        vt->matrix[vt->columns * (vt->rows - 1) + i] = (VTCell) { .ch = ' ', .attrib = vt->current_attrib };
    }

    if (vt->config.on_scroll_up == VT_NOTIFY) {
        vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_SCROLL_UP, .scroll_up = { .count = 1 } });
    } else if (vt->config.update_events == VT_ROW_UPDATE) {
        for (size_t row = 0; row < vt->rows; ++row)
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_ROW_UPDATE, .row = { .row = row } });
    } else if (vt->config.update_events == VT_CELL_UPDATE) {
        // TODO - only update the cells that actually changed
        for (size_t row = 0; row < vt->rows; ++row) {
            for (size_t column = 0; column < vt->columns; ++column) {
                vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { .column = column, .row = row } });
            }
        }
    }
}

static void add_char(VT* vt, char c, size_t* row, size_t* column)
{
    *row = vt->cursor.row;
    *column = vt->cursor.column;

    switch (c) {
        case 10:
            ++vt->cursor.row;
            break;
        case 13:
            vt->cursor.column = 0;
            break;
        case '\b':
            vt->cursor.column = MAX(vt->cursor.column - 1, 0);
            break;
        case '\t':
            for (size_t i = 0; i < 8; ++i)
                add_char(vt, ' ', row, column);
            break;
        case 7:
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_BELL });
            break;
        default:
            vt->matrix[vt->cursor.row * vt->columns + vt->cursor.column] = (VTCell) {
                .ch = c,
                .attrib = vt->current_attrib,
            };
            ++vt->cursor.column;
    }

    if (vt->cursor.column == vt->columns) {
        vt->cursor.column = 0;
        ++vt->cursor.row;
    }

    if (vt->cursor.row == vt->rows) {
        scroll_up(vt);
        --vt->cursor.row;
        vt->cursor.column = 0;
    }
}

static bool match(const char* data, const char* pattern, int args[8])
{
    size_t i = 0;
    size_t argn = 0;
    for (const char* p = pattern; *p; ++p) {
        while (data[i] == ';')
            ++i;
        if (*p == '#') {
            char* endptr;
            args[argn++] = strtol(&data[i], &endptr, 10);
            i = endptr - data;
        } else if (*p != data[i] && data[i] != ';') {
            return false;
        } else {
            ++i;
        }
    }
    return i == strlen(data);
}

static bool parse_escape_sequence(VT* vt)
{
    int args[8] = {0};
    size_t len = strlen(vt->current_buffer);
    char last = vt->current_buffer[len - 1];
    if (isalpha(last) || len >= sizeof vt->current_buffer) {  // absolute cursor position
#define MATCH(pattern) if (match(vt->current_buffer, pattern, args))
        MATCH("[##H") {
            vt->cursor.row = MAX(args[0] - 1, 0);
            vt->cursor.column = MAX(args[1] - 1, 0);
        }
        else MATCH("A") vt->cursor.row = MAX(vt->cursor.row - 1, 0);
        else MATCH("C") vt->cursor.row = MIN(vt->cursor.column + 1, vt->columns - 1);
        else {
            fprintf(stderr, "Unknown escape sequence: ^[%s\n", vt->current_buffer);
        }
#undef MATCH
        return true;
    }
    return false;
}

void vt_write(VT* vt, const char* str, size_t str_sz)
{
    bool rows_updates[vt->rows];
    memset(rows_updates, 0, (sizeof rows_updates[0]) * vt->rows);

    for (size_t i = 0; i < str_sz; ++i) {
        if (vt->buffer_mode) {
            vt->current_buffer[strlen(vt->current_buffer)] = str[i];
            if (parse_escape_sequence(vt))
                vt->buffer_mode = false;
        } else {
            if (str[i] == '\e') {
                memset(vt->current_buffer, 0, sizeof vt->current_buffer);
                vt->buffer_mode = true;
            } else {  // show char in screen
                size_t row, column;
                add_char(vt, str[i], &row, &column);
                rows_updates[row] = true;
                if (vt->config.update_events == VT_CELL_UPDATE) {
                    vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .cell = { row, column } });
                }
            }
        }
    }

    if (vt->config.update_events == VT_ROW_UPDATE) {
        for (size_t i = 0; i < vt->rows; ++i)
            vt->push_event(vt, &(VTEvent) { .type = VT_EVENT_CELL_UPDATE, .row = { .row = i } });
    }
}

VTCell vt_char(VT* vt, size_t row, size_t column)
{
    VTCell ch = vt->matrix[row * vt->columns + column];
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
