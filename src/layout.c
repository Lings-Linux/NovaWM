#include "novawm.h"

void novawm_layout_master_stack(struct novawm_server *srv) {
    struct novawm_monitor *m = &srv->mon;
    int bw = srv->cfg.border_width;
    int gi = srv->cfg.gaps_inner;
    int go = srv->cfg.gaps_outer;

    /* Count tiled clients */
    int tiled = 0;
    struct novawm_client *c;
    for (c = m->clients; c; c = c->next) {
        if (!c->floating && !c->fullscreen)
            tiled++;
    }

    if (tiled == 0)
        return;

    int x0 = m->x + go;
    int y0 = m->y + go;
    int ww = m->w - 2 * go;
    int wh = m->h - 2 * go;

    /* If master_factor is weird, clamp it */
    if (srv->cfg.master_factor < 0.05f) srv->cfg.master_factor = 0.05f;
    if (srv->cfg.master_factor > 0.95f) srv->cfg.master_factor = 0.95f;

    int master_h = (int)(wh * srv->cfg.master_factor);
    int stack_h = wh - master_h;

    int idx = 0;
    for (c = m->clients; c; c = c->next) {
        if (c->floating || c->fullscreen)
            continue;

        int nx, ny, nw, nh;

        if (idx == 0) {
            /* Master */
            nx = x0 + gi / 2;
            ny = y0 + gi / 2;
            nw = ww - gi;
            nh = master_h - gi;
        } else {
            int stack_idx = idx - 1;
            int stack_count = tiled - 1;
            int each = stack_count > 0 ? stack_h / stack_count : stack_h;
            int y = y0 + master_h + stack_idx * each;

            nx = x0 + gi / 2;
            ny = y + gi / 2;
            nw = ww - gi;
            nh = each - gi;
        }

        if (nw < 50) nw = 50;
        if (nh < 50) nh = 50;

        uint32_t values[4] = {
            (uint32_t)nx,
            (uint32_t)ny,
            (uint32_t)nw,
            (uint32_t)nh
        };

        xcb_configure_window(srv->conn, c->win,
                             XCB_CONFIG_WINDOW_X |
                             XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT,
                             values);

        /* write back so mouse drag uses correct values */
        c->x = nx;
        c->y = ny;
        c->w = nw;
        c->h = nh;

        idx++;
    }

    xcb_flush(srv->conn);
}