#include "novawm.h"
#include <stdlib.h>
#include <string.h>

static void apply_client_border(struct novawm_server *srv,
                                struct novawm_client *c,
                                bool focused) {
    uint32_t values[2];
    values[0] = focused ? srv->cfg.border_color_active
                        : srv->cfg.border_color_inactive;
    values[1] = srv->cfg.border_width;

    xcb_change_window_attributes(srv->conn, c->win, XCB_CW_BORDER_PIXEL, &values[0]);
    xcb_configure_window(srv->conn, c->win,
                         XCB_CONFIG_WINDOW_BORDER_WIDTH, &values[1]);
}

void novawm_arrange(struct novawm_server *srv) {
    novawm_layout_master_stack(srv);
    xcb_flush(srv->conn);
}

struct novawm_client *novawm_find_client(struct novawm_server *srv, xcb_window_t win) {
    struct novawm_client *c = srv->mon.clients;
    while (c) {
        if (c->win == win) return c;
        c = c->next;
    }
    return NULL;
}

void novawm_focus_client(struct novawm_server *srv, struct novawm_client *c) {
    if (!c) return;
    srv->mon.focused = c;

    xcb_set_input_focus(srv->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                        c->win, XCB_CURRENT_TIME);

    struct novawm_client *it = srv->mon.clients;
    while (it) {
        apply_client_border(srv, it, it == c);
        it = it->next;
    }
    xcb_flush(srv->conn);
}

void novawm_manage_window(struct novawm_server *srv, xcb_window_t win) {
    xcb_get_geometry_cookie_t gc = xcb_get_geometry(srv->conn, win);
    xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(srv->conn, gc, NULL);
    if (!gr) return;

    struct novawm_client *c = calloc(1, sizeof *c);
    c->win = win;
    c->x = gr->x;
    c->y = gr->y;
    c->w = gr->width;
    c->h = gr->height;
    c->floating = false;
    c->fullscreen = false;
    free(gr);

    /* Subscribe to events on this window */
    uint32_t mask = XCB_EVENT_MASK_ENTER_WINDOW |
                    XCB_EVENT_MASK_FOCUS_CHANGE |
                    XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_RELEASE |
                    XCB_EVENT_MASK_POINTER_MOTION |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    uint32_t val[] = { mask };
    xcb_change_window_attributes(srv->conn, win, XCB_CW_EVENT_MASK, val);

    /* Insert at head of client list */
    c->next = srv->mon.clients;
    srv->mon.clients = c;

    xcb_map_window(srv->conn, win);

    novawm_focus_client(srv, c);
    novawm_arrange(srv);
}

void novawm_unmanage_window(struct novawm_server *srv, struct novawm_client *c) {
    if (!c) return;

    if (srv->mon.clients == c) {
        srv->mon.clients = c->next;
    } else {
        struct novawm_client *p = srv->mon.clients;
        while (p && p->next != c) p = p->next;
        if (p) p->next = c->next;
    }

    if (srv->mon.focused == c)
        srv->mon.focused = srv->mon.clients;

    free(c);
    novawm_arrange(srv);
}

void novawm_kill_focused(struct novawm_server *srv) {
    struct novawm_client *c = srv->mon.focused;
    if (!c) return;
    xcb_kill_client(srv->conn, c->win);
    xcb_flush(srv->conn);
}

void novawm_toggle_floating(struct novawm_server *srv) {
    struct novawm_client *c = srv->mon.focused;
    if (!c) return;
    c->floating = !c->floating;
    novawm_arrange(srv);
}