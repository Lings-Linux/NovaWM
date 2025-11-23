#include "novawm.h"
#include <stdio.h>

int main(void) {
    struct novawm_server srv;

    const char *cfg_path = novawm_get_config_path();
    novawm_config_load(&srv.cfg, cfg_path);

    if (!novawm_x11_init(&srv))
        return 1;

    novawm_x11_grab_keys(&srv);
    novawm_x11_scan_existing(&srv);

    novawm_run_autostart(&srv.cfg);

    novawm_x11_run(&srv);

    return 0;
}