#ifndef NOVAWM_H
#define NOVAWM_H

#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* --- limits --- */

#define NOVAWM_MAX_BINDS     64
#define NOVAWM_MAX_AUTOSTART 32
#define NOVAWM_WORKSPACES    10

/* Super/Win as global modifier for mouse drag */
#define NOVAWM_MOD_MASK XCB_MOD_MASK_4

/* --- config / bindings --- */

struct novawm_bind {
    uint16_t     mods;
    xcb_keysym_t keysym;
    char         action[32];
    char         arg[256];
};

struct novawm_config {
    float    master_factor;
    int      border_width;
    uint32_t border_color_active;
    uint32_t border_color_inactive;
    int      gaps_inner;
    int      gaps_outer;
    bool     focus_follows_mouse;

    struct novawm_bind binds[NOVAWM_MAX_BINDS];
    int                binds_len;

    char autostart[NOVAWM_MAX_AUTOSTART][256];
    int  autostart_len;
};

/* --- client / workspace / monitor --- */

struct novawm_client {
    xcb_window_t win;
    int x, y, w, h;
    bool floating;
    int  ws;                    /* workspace index 0..NOVAWM_WORKSPACES-1 */
    bool ignore_unmap;          /* set when WM unmaps during workspace switch */
    struct novawm_client *next; /* next in workspace list */
};

struct novawm_workspace {
    struct novawm_client *clients;
    struct novawm_client *focused;
};

struct novawm_drag_state {
    bool active;
    bool resizing;
    struct novawm_client *client;
    int start_root_x, start_root_y;
    int start_x, start_y;
    int start_w, start_h;
};

struct novawm_monitor {
    int x, y, w, h;
    int current_ws;                         /* 0..NOVAWM_WORKSPACES-1 */
    struct novawm_workspace ws[NOVAWM_WORKSPACES];
};

/* --- main server --- */

struct novawm_server {
    xcb_connection_t  *conn;
    xcb_screen_t      *screen;
    xcb_window_t       root;
    xcb_key_symbols_t *keysyms;

    struct novawm_monitor    mon;
    struct novawm_config     cfg;
    struct novawm_drag_state drag;

    bool running;
};

/* --- config --- */

bool        novawm_config_load(struct novawm_config *cfg, const char *path);
const char *novawm_get_config_path(void);
void        novawm_run_autostart(struct novawm_config *cfg);
uint16_t    novawm_parse_mods(const char *s);

/* --- X11 backend --- */

bool novawm_x11_init(struct novawm_server *srv);
void novawm_x11_grab_keys(struct novawm_server *srv);
void novawm_x11_scan_existing(struct novawm_server *srv);
void novawm_x11_run(struct novawm_server *srv);

/* --- layout / manage --- */

void novawm_arrange(struct novawm_server *srv);

void novawm_manage_window(struct novawm_server *srv, xcb_window_t win);
void novawm_unmanage_window(struct novawm_server *srv, struct novawm_client *c);
struct novawm_client *novawm_find_client(struct novawm_server *srv,
                                         xcb_window_t win);

void novawm_focus_client(struct novawm_server *srv, struct novawm_client *c);
void novawm_toggle_floating(struct novawm_server *srv);
void novawm_kill_focused(struct novawm_server *srv);

/* --- input handlers --- */

void novawm_handle_key_press(struct novawm_server *srv,
                             xcb_key_press_event_t *ev);
void novawm_handle_button_press(struct novawm_server *srv,
                                xcb_button_press_event_t *ev);
void novawm_handle_button_release(struct novawm_server *srv,
                                  xcb_button_release_event_t *ev);
void novawm_handle_motion_notify(struct novawm_server *srv,
                                 xcb_motion_notify_event_t *ev);
void novawm_handle_enter_notify(struct novawm_server *srv,
                                xcb_enter_notify_event_t *ev);

/* --- util --- */

void         novawm_spawn(const char *cmd);
uint16_t     novawm_clean_mods(uint16_t state);
xcb_keysym_t novawm_keycode_to_keysym(struct novawm_server *srv,
                                      xcb_keycode_t code);

#endif /* NOVAWM_H */