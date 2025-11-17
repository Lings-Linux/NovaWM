#ifndef NOVAWM_H
#define NOVAWM_H

#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Modifier used for WM operations (mouse drag etc.) */
#define NOVAWM_MOD_MASK XCB_MOD_MASK_4  /* SUPER / Mod4 */

/* Limits */
#define NOVAWM_MAX_BINDS      64
#define NOVAWM_MAX_AUTOSTART  32

/* ==========================
 *  Config / binds
 * ========================== */

struct novawm_bind {
    uint16_t mods;        /* XCB_MOD_MASK_* */
    xcb_keysym_t keysym;  /* key symbol */
    char action[32];      /* "spawn", "killactive", ... */
    char arg[128];        /* command or extra arg */
};

struct novawm_config {
    float master_factor;              /* 0.05–0.95 */
    int border_width;
    uint32_t border_color_active;     /* RGB or X11 pixel */
    uint32_t border_color_inactive;

    int gaps_inner;                   /* px between windows */
    int gaps_outer;                   /* px to screen border */
    bool focus_follows_mouse;

    struct novawm_bind binds[NOVAWM_MAX_BINDS];
    int binds_len;

    char autostart[NOVAWM_MAX_AUTOSTART][128];
    int autostart_len;
};

bool novawm_config_load(struct novawm_config *cfg, const char *path);
void novawm_run_autostart(struct novawm_config *cfg);

/* ==========================
 *  Client / monitor / server
 * ========================== */

struct novawm_client {
    struct novawm_client *next;
    xcb_window_t win;
    int x, y, w, h;
    bool floating;
    bool fullscreen;
};

struct novawm_monitor {
    int x, y, w, h;
    struct novawm_client *clients;
    struct novawm_client *focused;
};

struct novawm_drag {
    bool active;
    bool resizing;
    struct novawm_client *client;
    int start_root_x, start_root_y;
    int start_x, start_y, start_w, start_h;
};

struct novawm_server {
    xcb_connection_t *conn;
    xcb_screen_t *screen;
    xcb_window_t root;

    xcb_key_symbols_t *keysyms;

    struct novawm_monitor mon;
    struct novawm_config cfg;
    struct novawm_drag drag;

    bool running;
};

/* x11.c */
bool novawm_x11_init(struct novawm_server *srv);
void novawm_x11_run(struct novawm_server *srv);
void novawm_x11_grab_keys(struct novawm_server *srv);

/* input.c */
void novawm_handle_key_press(struct novawm_server *srv, xcb_key_press_event_t *ev);
void novawm_handle_button_press(struct novawm_server *srv, xcb_button_press_event_t *ev);
void novawm_handle_button_release(struct novawm_server *srv, xcb_button_release_event_t *ev);
void novawm_handle_motion_notify(struct novawm_server *srv, xcb_motion_notify_event_t *ev);
void novawm_handle_enter_notify(struct novawm_server *srv, xcb_enter_notify_event_t *ev);

/* manage.c */
void novawm_manage_window(struct novawm_server *srv, xcb_window_t win);
void novawm_unmanage_window(struct novawm_server *srv, struct novawm_client *c);
struct novawm_client *novawm_find_client(struct novawm_server *srv, xcb_window_t win);
void novawm_focus_client(struct novawm_server *srv, struct novawm_client *c);
void novawm_arrange(struct novawm_server *srv);
void novawm_kill_focused(struct novawm_server *srv);
void novawm_toggle_floating(struct novawm_server *srv);

/* layout.c */
void novawm_layout_master_stack(struct novawm_server *srv);

/* util.c */
xcb_keysym_t novawm_keycode_to_keysym(struct novawm_server *srv, xcb_keycode_t code);
uint16_t novawm_clean_mods(uint16_t state);
uint16_t novawm_parse_mods(const char *mods_str);
const char *novawm_get_config_path(void);
void novawm_spawn(const char *cmd);

#endif /* NOVAWM_H */