#include "novawm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <X11/keysym.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
    return s;
}

/* Minimal string→keysym mapper */
static xcb_keysym_t parse_keysym_name(const char *name) {
    if (!name || !*name) return XCB_NO_SYMBOL;

    char buf[64];
    snprintf(buf, sizeof buf, "%s", name);
    char *s = trim(buf);

    /* Single char: letters / digits */
    if (strlen(s) == 1) {
        unsigned char c = (unsigned char)s[0];
        if (isalpha(c)) return (xcb_keysym_t)tolower(c);
        if (isdigit(c)) return (xcb_keysym_t)c;
    }

    if (!strcasecmp(s, "Return"))   return XK_Return;
    if (!strcasecmp(s, "Escape"))   return XK_Escape;
    if (!strcasecmp(s, "Tab"))      return XK_Tab;
    if (!strcasecmp(s, "Space"))    return XK_space;
    if (!strcasecmp(s, "Backspace"))return XK_BackSpace;
    if (!strcasecmp(s, "Left"))     return XK_Left;
    if (!strcasecmp(s, "Right"))    return XK_Right;
    if (!strcasecmp(s, "Up"))       return XK_Up;
    if (!strcasecmp(s, "Down"))     return XK_Down;
    if (!strcasecmp(s, "F1"))       return XK_F1;
    if (!strcasecmp(s, "F2"))       return XK_F2;
    if (!strcasecmp(s, "F3"))       return XK_F3;
    if (!strcasecmp(s, "F4"))       return XK_F4;
    if (!strcasecmp(s, "F5"))       return XK_F5;
    if (!strcasecmp(s, "F6"))       return XK_F6;
    if (!strcasecmp(s, "F7"))       return XK_F7;
    if (!strcasecmp(s, "F8"))       return XK_F8;
    if (!strcasecmp(s, "F9"))       return XK_F9;
    if (!strcasecmp(s, "F10"))      return XK_F10;
    if (!strcasecmp(s, "F11"))      return XK_F11;
    if (!strcasecmp(s, "F12"))      return XK_F12;

    /* Fallback: take first char */
    return (xcb_keysym_t)s[0];
}

bool novawm_config_load(struct novawm_config *cfg, const char *path) {
    memset(cfg, 0, sizeof *cfg);

    /* Defaults */
    cfg->master_factor = 0.6f;
    cfg->border_width = 2;
    cfg->border_color_active = 0x00ff00;
    cfg->border_color_inactive = 0x333333;
    cfg->gaps_inner = 5;
    cfg->gaps_outer = 10;
    cfg->focus_follows_mouse = false;
    cfg->binds_len = 0;
    cfg->autostart_len = 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        /* No config is fine, we keep defaults */
        return true;
    }

    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (!*s || *s == '#')
            continue;

        if (!strncmp(s, "master_factor", 13)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->master_factor = strtof(trim(eq+1), NULL);
            if (cfg->master_factor < 0.05f) cfg->master_factor = 0.05f;
            if (cfg->master_factor > 0.95f) cfg->master_factor = 0.95f;
            continue;
        }

        if (!strncmp(s, "border_width", 12)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->border_width = atoi(trim(eq+1));
            if (cfg->border_width < 0) cfg->border_width = 0;
            continue;
        }

        if (!strncmp(s, "border_color_active", 19)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->border_color_active = strtoul(trim(eq+1), NULL, 0);
            continue;
        }

        if (!strncmp(s, "border_color_inactive", 21)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->border_color_inactive = strtoul(trim(eq+1), NULL, 0);
            continue;
        }

        if (!strncmp(s, "gaps_inner", 10)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->gaps_inner = atoi(trim(eq+1));
            if (cfg->gaps_inner < 0) cfg->gaps_inner = 0;
            continue;
        }

        if (!strncmp(s, "gaps_outer", 10)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            cfg->gaps_outer = atoi(trim(eq+1));
            if (cfg->gaps_outer < 0) cfg->gaps_outer = 0;
            continue;
        }

        if (!strncmp(s, "focus_follows_mouse", 19)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            char *val = trim(eq+1);
            cfg->focus_follows_mouse =
            (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || !strcmp(val, "1"));
            continue;
        }

        if (!strncmp(s, "exec-once", 9)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            if (cfg->autostart_len >= NOVAWM_MAX_AUTOSTART) continue;
            snprintf(cfg->autostart[cfg->autostart_len++],
                     sizeof cfg->autostart[0],
                     "%s", trim(eq+1));
            continue;
        }

        if (!strncmp(s, "bind", 4)) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            if (cfg->binds_len >= NOVAWM_MAX_BINDS) continue;

            char *rhs = trim(eq + 1);
            char *mods = strtok(rhs, ",");
            char *key = strtok(NULL, ",");
            char *action = strtok(NULL, ",");
            char *arg = strtok(NULL, "");

            if (!mods || !key || !action) continue;

            struct novawm_bind *b = &cfg->binds[cfg->binds_len++];
            memset(b, 0, sizeof *b);

            b->mods = novawm_parse_mods(trim(mods));
            b->keysym = parse_keysym_name(trim(key));

            snprintf(b->action, sizeof b->action, "%s", trim(action));
            if (arg) snprintf(b->arg, sizeof b->arg, "%s", trim(arg));
            continue;
        }
    }

    fclose(f);
    return true;
}

void novawm_run_autostart(struct novawm_config *cfg) {
    for (int i = 0; i < cfg->autostart_len; i++) {
        novawm_spawn(cfg->autostart[i]);
    }
}

/* ------ Helpers we were missing ------ */

/* Path to config:
 *  $XDG_CONFIG_HOME/novawm/novawm.conf
 * or
 *  ~/.config/novawm/novawm.conf
 */
const char *novawm_get_config_path(void) {
    static char buf[512];

    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(buf, sizeof(buf), "%s/novawm/novawm.conf", xdg);
        return buf;
    }

    const char *home = getenv("HOME");
    if (!home || !*home) {
        snprintf(buf, sizeof(buf), ".novawm.conf");
        return buf;
    }

    snprintf(buf, sizeof(buf), "%s/.config/novawm/novawm.conf", home);
    return buf;
}

/* Parse "SUPER, SHIFT", "ALT+CTRL", etc into XCB modifier mask. */
uint16_t novawm_parse_mods(const char *s) {
    if (!s) return 0;

    uint16_t mods = 0;
    char buf[128];

    /* copy + lowercase */
    size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) {
        char c = s[n];
        buf[n] = (char)tolower((unsigned char)c);
        n++;
    }
    buf[n] = '\0';

    const char *delims = " +\t,";
    char *tok = strtok(buf, delims);
    while (tok) {
        if (!strcmp(tok, "shift")) {
            mods |= XCB_MOD_MASK_SHIFT;
        } else if (!strcmp(tok, "ctrl") || !strcmp(tok, "control")) {
            mods |= XCB_MOD_MASK_CONTROL;
        } else if (!strcmp(tok, "alt") || !strcmp(tok, "mod1")) {
            mods |= XCB_MOD_MASK_1;   /* Alt usually Mod1 */
        } else if (!strcmp(tok, "super") ||
            !strcmp(tok, "win")   ||
            !strcmp(tok, "logo")  ||
            !strcmp(tok, "mod4")) {
            mods |= XCB_MOD_MASK_4;   /* Super usually Mod4 */
            }
            tok = strtok(NULL, delims);
    }

    return mods;
}