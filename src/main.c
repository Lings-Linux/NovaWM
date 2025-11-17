#include "novawm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    struct novawm_server srv;
    memset(&srv, 0, sizeof srv);

    const char *cfg_path = novawm_get_config_path();
    if (!novawm_config_load(&srv.cfg, cfg_path)) {
        fprintf(stderr, "NovaWM: failed to load config %s\n", cfg_path);
    }

    if (!novawm_x11_init(&srv)) {
        fprintf(stderr, "NovaWM: failed to init X\n");
        return 1;
    }

    novawm_x11_grab_keys(&srv);
    novawm_run_autostart(&srv.cfg);

    fprintf(stderr, "NovaWM: running (config: %s)\n", cfg_path);
    novawm_x11_run(&srv);

    xcb_key_symbols_free(srv.keysyms);
    xcb_disconnect(srv.conn);
    return 0;
}