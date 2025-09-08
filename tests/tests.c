#include "../libvirtterm.c"

#include <assert.h>
#include <string.h>

#define R { vt_reset(vt); }                                    // reset
#define W(str) { vt_step(vt, str, strlen(str)); }              // write to screen
#define A(v) { assert(v); }                                    // assert
#define ACH(r, c, cmp) { A(vt_char(vt, r, c).ch == cmp); }     // assert char in r,c is cmp
#define ACU(r, c) { A(vt_cursor(vt).row == r && vt_cursor(vt).column == c); } // assert cursor is in r,c
#define CMP(r, c, txt) { for (size_t i = 0; i < strlen(txt); ++i) A(vt->matrix[r * vt->columns + c + i].ch == txt[i]); }  // assert if screen text is this
#define P { vt_print(vt); }


[[maybe_unused]] static void vt_print(VT* vt)
{
    printf("+--------------------+\n");
    for (INT row = 0; row < vt->rows; ++row) {
        printf("|");
        for (INT column = 0; column < vt->columns; ++column)
            printf("%c", vt_char(vt, row, column).ch);
        printf("|\n");
    }
    printf("+--------------------+\n");
}

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

    // escape sequence cursor right
    R W("a\e[2Cb") ACH(0, 0, 'a') ACH(0, 1, ' ') ACH(0, 2, ' ') ACH(0, 3, 'b')

    // escape sequence too long
    R W("\e012345678901234567890123456789012345") ACH(0, 0, '0')

    // vt_memset
    R vt_memset_ch(vt, 1, 1, 3, 6, 'x');
    ACH(1, 2, ' ') ACH(1, 3, 'x') ACH(1, 6, 'x') ACH(1, 7, ' ')

    R vt_memset_ch(vt, 1, 1, 0, 19, 'y');
    ACH(0, 19, ' ') ACH(1, 0, 'y') ACH(1, 19, 'y') ACH(2, 0, ' ')

    R vt_memset_ch(vt, 1, 2, 18, 2, 'z');
    ACH(1, 17, ' ') ACH(1, 18, 'z') ACH(1, 19, 'z') ACH(2, 0, 'z') ACH(2, 2, 'z') ACH(2, 3, ' ')

    // vt_memmove
    R W("  aabbbbaa")
    vt_memmove(vt, 0, 0, 4, 6, 0, 5);
    CMP(0, 0, "  aabbbbabbb")

    R W("  aabbbbaa")
    vt_memmove(vt, 0, 0, 4, 6, 2, 2);
    CMP(2, 6, "bbb")

    R W("  xx")
    vt_memmove(vt, 0, 0, 2, 3, 0, -2);
    CMP(0, 2, "xx")

    R W("\e[3;1H  xx")
    vt_memmove(vt, 2, 2, 2, 3, -2, 0);
    CMP(0, 0, "  xx")

    // vertical scroll
    R W("aaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbb")  // 2 lines
    for (int i = 0; i < 8; ++i) {
        char buf[21] = {0}; memset(buf, i + '0', 20);
        W(buf)
    }
    ACH(0, 0, 'a') ACH(0, 19, 'a') ACH(9, 0, '7') ACH(9, 19, '7')
    vt_scroll_vertical(vt, 0, 9, 2);
    ACH(0, 0, '0') ACH(0, 19, '0') ACH(7, 0, '7') ACH(7, 19, '7') ACH(9, 0, ' ') ACH(9, 19, ' ')

    R W("aaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbb")  // 2 lines
    for (int i = 0; i < 8; ++i) {
        char buf[21] = {0}; memset(buf, i + '0', 20);
        W(buf)
    }
    vt_scroll_vertical(vt, 0, 9, -2);
    ACH(0, 0, ' ') ACH(0, 19, ' ') ACH(1, 0, ' ') ACH(2, 0, 'a') ACH(2, 19, 'a') ACH(9, 0, '5') ACH(9, 19, '5')

    // horizontal scroll
    R W("0123456789abcdefghij")
    vt_scroll_horizontal(vt, 0, 3, 2);
    CMP(0, 0, "012  3456789abcdefgh") ACH(1, 0, ' ')

    R W("0123456789abcdefghij")
    vt_scroll_horizontal(vt, 0, 3, -2);
    CMP(0, 0, "01256789abcdefghij  ") ACH(1, 0, ' ')

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

    // page scroll up
    R W("0123456789abcdefghij\EH")


    vt_free(vt);
}