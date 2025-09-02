#include "../libvirtterm.c"

#include <assert.h>
#include <string.h>

#define R { vt_reset(vt); }                                    // reset
#define W(str) { R vt_write(vt, str, strlen(str)); }           // write to screen
#define A(v) { assert(v); }                                    // assert
#define ACH(r, c, cmp) { A(vt_char(vt, r, c).ch == cmp); }     // assert char in r,c is cmp
#define ACU(r, c) { A(vt_cursor(vt).row == r && vt_cursor(vt).column == c); } // assert cursor is in r,c

void callback(VT* vt, VTEvent* e)
{
    (void) vt;
    (void) e;
}

int main()
{
    VTConfig config = VT_DEFAULT_CONFIG;
    VT* vt = vt_new(80, 24, callback, &config, NULL);

    // add single character
    W("A") ACH(0, 0, 'A') ACU(0, 1)

    vt_free(vt);
}