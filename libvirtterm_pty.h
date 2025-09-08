#ifndef LIBVIRTTERM_PTY_H
#define LIBVIRTTERM_PTY_H

#define _XOPEN_SOURCE 700
#include "libvirtterm.h"

typedef struct VTPTY VTPTY;

typedef enum VTPTYStatus { VTP_CONTINUE, VTP_CLOSE, VTP_ERROR } VTPTYStatus;

VTPTY*      vtpty_new(VT* vt, size_t input_buffer_size);
void        vtpty_close(VTPTY* p);

VTPTYStatus vtpty_keypress(VTPTY* p, uint16_t key, bool shift, bool ctrl);
VTPTYStatus vtpty_process(VTPTY* p);

void        vtpty_resize(VTPTY* p, int rows, int columns);

VTPTYStatus vtpty_update_mouse_state(VTPTY* p, VTMouseState state);

const char* vtpty_name(VTPTY* p);

#endif //LIBVIRTTERM_PTY_H
