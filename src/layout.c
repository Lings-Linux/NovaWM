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

/* master + stack layout:
 * - focused client is master
 * - master takes master_factor of width on the left
 * - others are stacked vertically on the right
 */
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
    int inner = srv->cfg.gaps_inner;

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

    /* Ensure focused client is master (index 0). */
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

    float factor = srv->cfg.master_factor;
    if (factor < 0.05f) factor = 0.05f;
    if (factor > 0.95f) factor = 0.95f;

    /* If only one tiled client → full area. */
    if (tiled == 1) {
        int x = mx + inner;
        int y = my + inner;
        int w = mw - 2 * inner;
        int h = mh - 2 * inner;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        apply_client_geometry(srv, arr[0], x, y, w, h);
    } else {
        /* Master on the left, stack on the right. */
        int master_w = (int)(mw * factor);
        if (master_w < 1) master_w = 1;
        int stack_w = mw - master_w - inner;
        if (stack_w < 1) stack_w = 1;

        /* Master rect */
        int mx0 = mx + inner;
        int my0 = my + inner;
        int mw0 = master_w - inner;
        int mh0 = mh - 2 * inner;
        if (mw0 < 1) mw0 = 1;
        if (mh0 < 1) mh0 = 1;

        apply_client_geometry(srv, arr[0], mx0, my0, mw0, mh0);

        /* Stack rect */
        int sx = mx + master_w + inner;
        int sy = my + inner;
        int sw = stack_w - inner;
        int sh = mh - 2 * inner;
        if (sw < 1) sw = 1;
        if (sh < 1) sh = 1;

        int stack_count = tiled - 1;
        int slot_h = (stack_count > 0) ? (sh - (inner * (stack_count - 1))) / stack_count : sh;
        if (slot_h < 1) slot_h = 1;

        int ty = sy;
        for (int i = 1; i < tiled; i++) {
            int th = slot_h;
            apply_client_geometry(srv, arr[i], sx, ty, sw, th);
            ty += th + inner;
        }
    }

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