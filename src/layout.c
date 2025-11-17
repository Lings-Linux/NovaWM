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

    int master_h = (int)(wh * srv->cfg.master_factor);
    int stack_h = wh - master_h;

    int idx = 0;
    for (c = m->clients; c; c = c->next) {
        if (c->floating || c->fullscreen)
            continue;

        uint32_t values[4];
        if (idx == 0) {
            /* Master */
            values[0] = x0 + gi / 2;
            values[1] = y0 + gi / 2;
            values[2] = ww - gi;
            values[3] = master_h - gi;
        } else {
            int stack_idx = idx - 1;
            int stack_count = tiled - 1;
            int each = stack_count > 0 ? stack_h / stack_count : stack_h;
            int y = y0 + master_h + stack_idx * each;

            values[0] = x0 + gi / 2;
            values[1] = y + gi / 2;
            values[2] = ww - gi;
            values[3] = each - gi;
        }

        /* Avoid negative sizes */
        if ((int)values[2] < 50) values[2] = 50;
        if ((int)values[3] < 50) values[3] = 50;

        xcb_configure_window(srv->conn, c->win,
                             XCB_CONFIG_WINDOW_X |
                             XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH |
                             XCB_CONFIG_WINDOW_HEIGHT,
                             values);
        idx++;
    }
}