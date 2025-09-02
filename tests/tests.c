#define VT_DEBUG_SUPPORT 1
#include "../libvirtterm.c"

#include <assert.h>
#include <string.h>

#define R { vt_reset(vt); }                                    // reset
#define W(str) { vt_write(vt, str, strlen(str)); }             // write to screen
#define A(v) { assert(v); }                                    // assert
#define ACH(r, c, cmp) { A(vt_char(vt, r, c).ch == cmp); }     // assert char in r,c is cmp
#define ACU(r, c) { A(vt_cursor(vt).row == r && vt_cursor(vt).column == c); } // assert cursor is in r,c
#define LEV(t) { A(last_event.type == t); }                    // assert last event

static VTEvent last_event;

void callback(VT* vt, VTEvent* e)
{
    (void) vt;
    (void) e;
    last_event = *e;
}

int main()
{
    VTConfig config = VT_DEFAULT_CONFIG;
    config.update_events = VT_CELL_UPDATE;
    config.debug = VT_DEBUG_ALL_BYTES;
    VT* vt = vt_new(20, 10, callback, &config, NULL);

    // add single character
    R W("A") ACH(0, 0, 'A') ACU(0, 1) LEV(VT_EVENT_CELL_UPDATE) A(last_event.cell.row == 0) A(last_event.cell.column == 0)
      W("b") ACH(0, 1, 'b') ACU(0, 2)
    // TODO - check events

    vt_free(vt);
}