#include "libvirtterm_pty.h"

#ifdef __APPLE__
# include <util.h>
#else
# include <pty.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct VTPTY {
    int    master_pty;
    size_t input_buffer_size;
    VT*    vt;
    char   pty_name[1024];
} VTPTY;

VTPTY* vtpty_new(VT* vt, size_t input_buffer_size)
{
    VTPTY* p = calloc(1, sizeof(VTPTY));
    p->master_pty = -1;
    p->input_buffer_size = input_buffer_size;
    p->vt = vt;

    char pty_name[1024];
    struct winsize ws = { vt_rows(vt), vt_columns(vt), 0, 0 };
    pid_t pid = forkpty(&p->master_pty, pty_name, NULL, &ws);
    if (pid == 0) {
        setenv("LC_ALL", "en_US.ISO-8859-1", 1);
        setenv("TERM", "xterm", 1);
        char *shell_path = getenv("SHELL");
        if (shell_path)
            execl(shell_path, shell_path, NULL);
        else
            execl("/bin/sh", "sh", NULL);
        perror("execl");
        exit(1);
    }

    int flags = fcntl(p->master_pty, F_GETFL, 0);
    fcntl(p->master_pty, F_SETFL, flags | O_NONBLOCK);

    return p;
}

void vtpty_close(VTPTY* p)
{
    close(p->master_pty);
    free(p);
}

VTPTYStatus vtpty_keypress(VTPTY* p, uint16_t key, bool shift, bool ctrl)
{
    char buf[16];
    int n = vt_translate_key(p->vt, key, shift, ctrl, buf, sizeof buf);
    if (buf[0] != 0) {
        int r = write(p->master_pty, buf, n);
        if (r == 0)
            return VTP_CLOSE;
        if (r < 0)
            return VTP_ERROR;
    }
    return VTP_CONTINUE;
}

VTPTYStatus vtpty_process(VTPTY* p)
{
    char buf[p->input_buffer_size];
    int n = read(p->master_pty, buf, sizeof(buf));
    if (n > 0) {
        vt_write(p->vt, buf, n);
        return VTP_CONTINUE;
    }

    if (n == 0)
        return VTP_CLOSE;

    return VTP_ERROR;
}

void vtpty_resize(VTPTY* p, int rows, int columns)
{
    struct winsize ws = { rows, columns, 0, 0 };
    if (ioctl(p->master_pty, TIOCSWINSZ, &ws) == -1) {
        perror("ioctl(TIOCSWINSZ)");
        return;
    }

    vt_resize(p->vt, rows, columns);
}

const char* vtpty_name(VTPTY* p)
{
    return p->pty_name;
}
