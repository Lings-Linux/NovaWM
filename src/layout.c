#include "novawm.h"
#include <xcb/xcb.h>
#include <stdlib.h>

static void apply_client_geometry(struct novawm_server *srv,
                                  struct novawm_client *c,
                                  int x, int y, int w, int h) {
    struct novawm_workspace *ws = &srv->mon.ws[srv->mon.current_ws];

    uint32_t geom[4] = {
        (uint32_t)x,
        (uint32_t)y,
        (uint32_t)w,
        (uint32_t)h
    };

    xcb_configure_window(
        srv->conn,
        c->win,
        XCB_CONFIG_WINDOW_X |
        XCB_CONFIG_WINDOW_Y |
        XCB_CONFIG_WINDOW_WIDTH |
        XCB_CONFIG_WINDOW_HEIGHT,
        geom
    );

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;

    uint32_t bw = srv->cfg.border_width;
    xcb_configure_window(
        srv->conn,
        c->win,
        XCB_CONFIG_WINDOW_BORDER_WIDTH,
        &bw
    );

    uint32_t color = (c == ws->focused)
        ? srv->cfg.border_color_active
        : srv->cfg.border_color_inactive;

    xcb_change_window_attributes(
        srv->conn,
        c->win,
        XCB_CW_BORDER_PIXEL,
        &color
    );
}

/* Recursive dwindle tiling:
 * - arr[start] gets a slice of rect
 * - remaining clients recurse into leftover space
 * - splits alternate vertical / horizontal
 */
static void dwindle_place(struct novawm_server *srv,
                          struct novawm_workspace *ws,
                          struct novawm_client **arr,
                          int start, int end,
                          int x, int y, int w, int h,
                          int depth) {
    int n = end - start;
    if (n <= 0)
        return;

    int inner = srv->cfg.gaps_inner;
    if (w <= 0 || h <= 0)
        return;

    if (n == 1) {
        int gx = x + inner;
        int gy = y + inner;
        int gw = w - 2 * inner;
        int gh = h - 2 * inner;
        if (gw < 1) gw = 1;
        if (gh < 1) gh = 1;
        apply_client_geometry(srv, arr[start], gx, gy, gw, gh);
        return;
    }

    float factor = srv->cfg.master_factor;
    if (factor < 0.05f) factor = 0.05f;
    if (factor > 0.95f) factor = 0.95f;

    bool split_vert = (depth % 2 == 0);

    if (split_vert) {
        int w1 = (int)(w * factor);
        if (w1 < 1) w1 = 1;
        int w2 = w - w1;
        if (w2 < 1) w2 = 1;

        /* first client on the left */
        int gx = x + inner;
        int gy = y + inner;
        int gw = w1 - 2 * inner;
        int gh = h - 2 * inner;
        if (gw < 1) gw = 1;
        if (gh < 1) gh = 1;
        apply_client_geometry(srv, arr[start], gx, gy, gw, gh);

        /* rest on the right */
        int x2 = x + w1;
        dwindle_place(srv, ws, arr, start + 1, end,
                      x2, y, w2, h, depth + 1);
    } else {
        int h1 = (int)(h * factor);
        if (h1 < 1) h1 = 1;
        int h2 = h - h1;
        if (h2 < 1) h2 = 1;

        /* first client on the top */
        int gx = x + inner;
        int gy = y + inner;
        int gw = w - 2 * inner;
        int gh = h1 - 2 * inner;
        if (gw < 1) gw = 1;
        if (gh < 1) gh = 1;
        apply_client_geometry(srv, arr[start], gx, gy, gw, gh);

        /* rest at the bottom */
        int y2 = y + h1;
        dwindle_place(srv, ws, arr, start + 1, end,
                      x, y2, w, h2, depth + 1);
    }
}

void novawm_arrange(struct novawm_server *srv) {
    struct novawm_monitor   *m  = &srv->mon;
    struct novawm_workspace *ws = &m->ws[m->current_ws];

    /* Count tiled (non-floating) clients and collect them. */
    int tiled = 0;
    for (struct novawm_client *c = ws->clients; c; c = c->next) {
        if (!c->floating)
            tiled++;
    }

    int outer = srv->cfg.gaps_outer;

    int mx = m->x + outer;
    int my = m->y + outer;
    int mw = m->w - 2 * outer;
    int mh = m->h - 2 * outer;

    if (tiled <= 0) {
        /* No tiled clients – still update borders of floating ones */
        for (struct novawm_client *c = ws->clients; c; c = c->next) {
            uint32_t bw = srv->cfg.border_width;
            xcb_configure_window(
                srv->conn,
                c->win,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                &bw
            );
            uint32_t color = (c == ws->focused)
                ? srv->cfg.border_color_active
                : srv->cfg.border_color_inactive;
            xcb_change_window_attributes(
                srv->conn,
                c->win,
                XCB_CW_BORDER_PIXEL,
                &color
            );
        }
        xcb_flush(srv->conn);
        return;
    }

    /* Collect tiled clients into an array. */
    struct novawm_client *arr[tiled];
    int idx = 0;
    for (struct novawm_client *c = ws->clients; c; c = c->next) {
        if (!c->floating)
            arr[idx++] = c;
    }

    /* Make sure focused is first (Hyprland-style focus bias). */
    if (ws->focused) {
        int fi = -1;
        for (int i = 0; i < tiled; i++) {
            if (arr[i] == ws->focused) {
                fi = i;
                break;
            }
        }
        if (fi > 0) {
            struct novawm_client *tmp = arr[0];
            arr[0] = arr[fi];
            arr[fi] = tmp;
        }
    }

    /* Apply dwindle tiling to all tiled clients. */
    dwindle_place(srv, ws, arr, 0, tiled, mx, my, mw, mh, 0);

    /* update borders for floating clients as well */
    for (struct novawm_client *c = ws->clients; c; c = c->next) {
        if (!c->floating)
            continue;

        uint32_t bw = srv->cfg.border_width;
        xcb_configure_window(
            srv->conn,
            c->win,
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            &bw
        );
        uint32_t color = (c == ws->focused)
            ? srv->cfg.border_color_active
            : srv->cfg.border_color_inactive;
        xcb_change_window_attributes(
            srv->conn,
            c->win,
            XCB_CW_BORDER_PIXEL,
            &color
        );
    }

    xcb_flush(srv->conn);
}