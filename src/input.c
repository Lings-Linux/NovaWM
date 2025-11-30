#include "novawm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_keysyms.h>

static struct novawm_workspace *current_ws(struct novawm_server *srv) {
    return &srv->mon.ws[srv->mon.current_ws];
}

/* ------ Actions ------ */

static void action_spawn(struct novawm_server *srv, const char *arg) {
    (void)srv;
    if (arg && *arg)
        novawm_spawn(arg);
}

static void action_kill(struct novawm_server *srv, const char *arg) {
    (void)arg;
    novawm_kill_focused(srv);
}

static void focus_move(struct novawm_server *srv, int dir) {
    struct novawm_workspace *ws = current_ws(srv);
    struct novawm_client *c = ws->focused;
    if (!c) return;

    if (dir > 0) {
        if (c->next)
            novawm_focus_client(srv, c->next);
        else
            novawm_focus_client(srv, ws->clients); /* wrap */
    } else {
        struct novawm_client *p = NULL, *it = ws->clients;
        while (it && it != c) {
            p = it;
            it = it->next;
        }
        if (p)
            novawm_focus_client(srv, p);
        else {
            /* go to last */
            it = ws->clients;
            if (!it) return;
            while (it->next) it = it->next;
            novawm_focus_client(srv, it);
        }
    }
}

static void action_focusnext(struct novawm_server *srv, const char *arg) {
    (void)arg;
    focus_move(srv, +1);
}

static void action_focusprev(struct novawm_server *srv, const char *arg) {
    (void)arg;
    focus_move(srv, -1);
}

static void action_togglefloating(struct novawm_server *srv, const char *arg) {
    (void)arg;
    novawm_toggle_floating(srv);
}

static void action_grow(struct novawm_server *srv, const char *arg) {
    (void)arg;
    srv->cfg.master_factor += 0.05f;
    if (srv->cfg.master_factor > 0.95f) srv->cfg.master_factor = 0.95f;
    novawm_arrange(srv);
}

static void action_shrink(struct novawm_server *srv, const char *arg) {
    (void)arg;
    srv->cfg.master_factor -= 0.05f;
    if (srv->cfg.master_factor < 0.05f) srv->cfg.master_factor = 0.05f;
    novawm_arrange(srv);
}

static void action_quit(struct novawm_server *srv, const char *arg) {
    (void)arg;
    srv->running = false;
}

/* workspace switch: action "workspace", arg "1".."10" */
static void action_workspace(struct novawm_server *srv, const char *arg) {
    if (!arg || !*arg) return;
    int idx = atoi(arg);
    if (idx <= 0 || idx > NOVAWM_WORKSPACES) return;
    idx--; /* to 0-based */

    if (srv->mon.current_ws == idx)
        return;

    srv->mon.current_ws = idx;

    /* map current, unmap others */
    for (int i = 0; i < NOVAWM_WORKSPACES; i++) {
        struct novawm_client *c = srv->mon.ws[i].clients;
        while (c) {
            if (i == idx) {
                /* show windows on the new workspace */
                xcb_map_window(srv->conn, c->win);
            } else {
                /* hide windows from other workspaces – we now ignore UnmapNotify */
                xcb_unmap_window(srv->conn, c->win);
            }
            c = c->next;
        }
    }

    struct novawm_workspace *ws = &srv->mon.ws[idx];
    if (ws->focused)
        novawm_focus_client(srv, ws->focused);
    else if (ws->clients)
        novawm_focus_client(srv, ws->clients);

    novawm_arrange(srv);
}

static void dispatch_action(struct novawm_server *srv,
                            const char *action, const char *arg) {
    if (!strcmp(action, "spawn"))               action_spawn(srv, arg);
    else if (!strcmp(action, "killactive"))     action_kill(srv, arg);
    else if (!strcmp(action, "focusnext"))      action_focusnext(srv, arg);
    else if (!strcmp(action, "focusprev"))      action_focusprev(srv, arg);
    else if (!strcmp(action, "togglefloating")) action_togglefloating(srv, arg);
    else if (!strcmp(action, "grow"))           action_grow(srv, arg);
    else if (!strcmp(action, "shrink"))         action_shrink(srv, arg);
    else if (!strcmp(action, "quit"))           action_quit(srv, arg);
    else if (!strcmp(action, "workspace"))      action_workspace(srv, arg);
}

/* ------ Keyboard ------ */

void novawm_handle_key_press(struct novawm_server *srv,
                             xcb_key_press_event_t *ev) {
    uint16_t mods = novawm_clean_mods(ev->state);
    xcb_keysym_t ks = novawm_keycode_to_keysym(srv, ev->detail);
    if (ks == XCB_NO_SYMBOL) return;

    for (int i = 0; i < srv->cfg.binds_len; i++) {
        struct novawm_bind *b = &srv->cfg.binds[i];
        if (b->mods == mods && b->keysym == ks) {
            dispatch_action(srv, b->action, b->arg);
            break;
        }
    }
}

/* ------ Mouse ------ */

void novawm_handle_button_press(struct novawm_server *srv,
                                xcb_button_press_event_t *ev) {
    xcb_window_t win = ev->event;
    if (win == srv->root)
        return;

    struct novawm_client *c = novawm_find_client(srv, win);
    if (!c) return;

    /* Click-to-focus */
    novawm_focus_client(srv, c);

    /* Only start drag with MOD pressed */
    if (!(ev->state & NOVAWM_MOD_MASK))
        return;

    /* when we start dragging, treat the window as floating */
    c->floating = true;

    srv->drag.active = true;
    srv->drag.client = c;
    srv->drag.start_root_x = ev->root_x;
    srv->drag.start_root_y = ev->root_y;
    srv->drag.start_x = c->x;
    srv->drag.start_y = c->y;
    srv->drag.start_w = c->w;
    srv->drag.start_h = c->h;
    srv->drag.resizing = (ev->detail == 3); /* 1 = left, 3 = right */
}

void novawm_handle_button_release(struct novawm_server *srv,
                                  xcb_button_release_event_t *ev) {
    (void)ev;
    srv->drag.active = false;
}

void novawm_handle_motion_notify(struct novawm_server *srv,
                                 xcb_motion_notify_event_t *ev) {
    if (!srv->drag.active || !srv->drag.client)
        return;

    int dx = ev->root_x - srv->drag.start_root_x;
    int dy = ev->root_y - srv->drag.start_root_y;

    struct novawm_client *c = srv->drag.client;

    if (!srv->drag.resizing) {
        /* Move */
        int nx = srv->drag.start_x + dx;
        int ny = srv->drag.start_y + dy;
        uint32_t values[2] = { (uint32_t)nx, (uint32_t)ny };
        xcb_configure_window(srv->conn, c->win,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                             values);
        c->x = nx;
        c->y = ny;
    } else {
        /* Resize */
        int nw = srv->drag.start_w + dx;
        int nh = srv->drag.start_h + dy;
        if (nw < 50) nw = 50;
        if (nh < 50) nh = 50;
        uint32_t values[2] = { (uint32_t)nw, (uint32_t)nh };
        xcb_configure_window(srv->conn, c->win,
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             values);
        c->w = nw;
        c->h = nh;
    }

    xcb_flush(srv->conn);
}

void novawm_handle_enter_notify(struct novawm_server *srv,
                                xcb_enter_notify_event_t *ev) {
    if (!srv->cfg.focus_follows_mouse)
        return;

    xcb_window_t win = ev->event;
    if (win == srv->root)
        return;

    struct novawm_client *c = novawm_find_client(srv, win);
    if (c)
        novawm_focus_client(srv, c);
}

/* --- helpers implemented here for now --- */

uint16_t novawm_clean_mods(uint16_t state) {
    const uint16_t allowed =
        XCB_MOD_MASK_SHIFT |
        XCB_MOD_MASK_CONTROL |
        XCB_MOD_MASK_1 |
        XCB_MOD_MASK_4;
    return state & allowed;
}

xcb_keysym_t novawm_keycode_to_keysym(struct novawm_server *srv,
                                      xcb_keycode_t code) {
    if (!srv || !srv->keysyms)
        return XCB_NO_SYMBOL;
    return xcb_key_symbols_get_keysym(srv->keysyms, code, 0);
}