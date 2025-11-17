#include "novawm.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

xcb_keysym_t novawm_keycode_to_keysym(struct novawm_server *srv, xcb_keycode_t code) {
    if (!srv->keysyms) return XCB_NO_SYMBOL;
    return xcb_key_symbols_get_keysym(srv->keysyms, code, 0);
}

/* Strip out weird modifiers; keep the basics */
uint16_t novawm_clean_mods(uint16_t state) {
    return state & (XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL |
                    XCB_MOD_MASK_1 | XCB_MOD_MASK_4 |
                    XCB_MOD_MASK_5 | XCB_MOD_MASK_LOCK);
}

/* Parse "SUPER SHIFT" etc. */
uint16_t novawm_parse_mods(const char *mods_str) {
    uint16_t mods = 0;
    if (!mods_str || !*mods_str) return 0;

    char buf[128];
    snprintf(buf, sizeof buf, "%s", mods_str);

    char *tok = strtok(buf, " ,+|");
    while (tok) {
        if (!strcasecmp(tok, "SHIFT"))
            mods |= XCB_MOD_MASK_SHIFT;
        else if (!strcasecmp(tok, "CTRL") || !strcasecmp(tok, "CONTROL"))
            mods |= XCB_MOD_MASK_CONTROL;
        else if (!strcasecmp(tok, "ALT") || !strcasecmp(tok, "MOD1"))
            mods |= XCB_MOD_MASK_1;
        else if (!strcasecmp(tok, "SUPER") || !strcasecmp(tok, "WIN") ||
                 !strcasecmp(tok, "MOD4"))
            mods |= XCB_MOD_MASK_4;
        tok = strtok(NULL, " ,+|");
    }
    return mods;
}

const char *novawm_get_config_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    static char path[512];
    snprintf(path, sizeof path, "%s/.config/novawm/novawm.conf", home);
    return path;
}

void novawm_spawn(const char *cmd) {
    if (!cmd || !*cmd) return;
    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
}
