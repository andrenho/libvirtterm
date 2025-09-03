#define VT_DEBUG_SUPPORT 1
#include "../libvirtterm.c"

#include <assert.h>
#include <string.h>

#define R { vt_reset(vt); }                                    // reset
#define W(str) { vt_write(vt, str, strlen(str)); }             // write to screen
#define A(v) { assert(v); }                                    // assert
#define ACH(r, c, cmp) { A(vt_char(vt, r, c).ch == cmp); }     // assert char in r,c is cmp
#define ACU(r, c) { A(vt_cursor(vt).row == r && vt_cursor(vt).column == c); } // assert cursor is in r,c


int main()
{
    VTConfig config = VT_DEFAULT_CONFIG;
    // config.debug = VT_DEBUG_ALL_BYTES;
    VT* vt = vt_new(10, 20, &config, NULL);

    VTEvent e;

    // add single character
    R W("A") ACH(0, 0, 'A') ACU(0, 1)
      A(vt_next_event(vt, &e)) A(e.type == VT_EVENT_CELLS_UPDATED && e.cells.row_start == 0 && e.cells.row_end == 0)
      W("b") ACH(0, 1, 'b') ACU(0, 2)

    // test end of line, and skip to next line
    R W("0123456789012345678") ACH(0, 18, '8') ACU(0, 19)
      W("9") ACH(0, 19, '9') ACU(0, 19)
      W("x") ACH(1, 0, 'x') ACU(1, 1)
      W("y") ACH(1, 1, 'y') ACU(1, 2)

    // test return when in the last item
    R W("01234567890123456789\r") ACU(0, 0)
      W("\n") ACU(1, 0)

    // test of page and scroll
    R
    for (int i = 0; i < 10; ++i) {
        char buf[32]; sprintf(buf, "%02d 34567890123456789", i);
        W(buf) ACH(i, 1, i % 10 + '0')
    }
    ACH(0, 1, '0') ACU(9, 19)                         // no scroll for now
    W("x") ACH(0, 1, '1') ACH(9, 0, 'x') ACU(9, 1)   // scroll

    // escape sequence too long
    R W("\e012345678901234567890123456789012345") ACH(0, 0, '0')

    // escape sequence cursor right
    R W("a\e[2Cb") ACH(0, 0, 'a') ACH(0, 1, ' ') ACH(0, 2, ' ') ACH(0, 3, 'b')

    vt_free(vt);
}