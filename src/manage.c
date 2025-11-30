#include "novawm.h"
#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>

struct novawm_client *novawm_find_client(struct novawm_server *srv,
                                         xcb_window_t win) {
    for (int ws = 0; ws < NOVAWM_WORKSPACES; ws++) {
        struct novawm_client *c = srv->mon.ws[ws].clients;
        while (c) {
            if (c->win == win)
                return c;
            c = c->next;
        }
    }
    return NULL;
}

void novawm_focus_client(struct novawm_server *srv, struct novawm_client *c) {
    if (!c)
        return;

    /* focus per current workspace */
    struct novawm_workspace *ws = &srv->mon.ws[srv->mon.current_ws];
    if (ws->focused == c)
        return;

    ws->focused = c;

    uint32_t values[1] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(
        srv->conn,
        c->win,
        XCB_CONFIG_WINDOW_STACK_MODE,
        values
    );

    xcb_set_input_focus(
        srv->conn,
        XCB_INPUT_FOCUS_POINTER_ROOT,
        c->win,
        XCB_CURRENT_TIME
    );

    novawm_arrange(srv);
}

void novawm_manage_window(struct novawm_server *srv, xcb_window_t win) {
    /* get attributes */
    xcb_get_window_attributes_cookie_t ac =
        xcb_get_window_attributes(srv->conn, win);
    xcb_get_window_attributes_reply_t *ar =
        xcb_get_window_attributes_reply(srv->conn, ac, NULL);
    if (!ar)
        return;

    if (ar->override_redirect) {
        free(ar);
        return;
    }

    free(ar);

    struct novawm_client *c = calloc(1, sizeof *c);
    if (!c)
        return;

    c->win = win;
    c->floating = false;
    c->ws = srv->mon.current_ws;
    c->ignore_unmap = false;
    c->next = NULL;

    struct novawm_workspace *ws = &srv->mon.ws[c->ws];

    /* insert at head of workspace list */
    c->next = ws->clients;
    ws->clients = c;

    /* ensure window is mapped and we receive enter events */
    uint32_t mask = XCB_EVENT_MASK_ENTER_WINDOW |
                    XCB_EVENT_MASK_FOCUS_CHANGE |
                    XCB_EVENT_MASK_PROPERTY_CHANGE;
    uint32_t val = mask;
    xcb_change_window_attributes(
        srv->conn,
        win,
        XCB_CW_EVENT_MASK,
        &val
    );
    xcb_map_window(srv->conn, win);

    novawm_focus_client(srv, c);
}

void novawm_unmanage_window(struct novawm_server *srv, struct novawm_client *c) {
    if (!c)
        return;

    int ws_idx = c->ws;
    if (ws_idx < 0 || ws_idx >= NOVAWM_WORKSPACES)
        ws_idx = srv->mon.current_ws;

    struct novawm_workspace *ws = &srv->mon.ws[ws_idx];

    struct novawm_client **pp = &ws->clients;
    while (*pp && *pp != c) {
        pp = &(*pp)->next;
    }
    if (*pp == c) {
        *pp = c->next;
    }

    if (ws->focused == c)
        ws->focused = ws->clients;

    free(c);

    novawm_arrange(srv);
}

void novawm_toggle_floating(struct novawm_server *srv) {
    struct novawm_workspace *ws = &srv->mon.ws[srv->mon.current_ws];
    struct novawm_client *c = ws->focused;
    if (!c)
        return;

    c->floating = !c->floating;
    novawm_arrange(srv);
}

void novawm_kill_focused(struct novawm_server *srv) {
    struct novawm_workspace *ws = &srv->mon.ws[srv->mon.current_ws];
    struct novawm_client *c = ws->focused;
    if (!c)
        return;

    xcb_kill_client(srv->conn, c->win);
    xcb_flush(srv->conn);
}