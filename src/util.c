#include "novawm.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void novawm_spawn(const char *cmd) {
    if (!cmd || !*cmd) return;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        /* CHILD PROCESS */

        /* Force child to use the same DISPLAY as NovaWM */
        const char *disp = getenv("DISPLAY");
        if (!disp) disp = ":1";              /* safe fallback */
            setenv("DISPLAY", disp, 1);

        /* Also prevent KDE session variables from leaking */
        unsetenv("XDG_CURRENT_DESKTOP");
        unsetenv("DESKTOP_SESSION");

        setsid();
        execl("/bin/sh", "sh", "-c", cmd, NULL);

        perror("execl");
        _exit(1);
    }
}