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

                                      /* Collect tiled clients into an array so ordering is stable. */
                                      struct novawm_client *arr[tiled];
                                      int idx = 0;
                                      for (struct novawm_client *c = ws->clients; c; c = c->next) {
                                          if (!c->floating)
                                              arr[idx++] = c;
                                      }

                                      /* Remaining rectangle for the dwindle / spiral. */
                                      int rx = mx + inner;
                                      int ry = my + inner;
                                      int rw = mw - 2 * inner;
                                      int rh = mh - 2 * inner;
                                      if (rw < 1) rw = 1;
                                      if (rh < 1) rh = 1;

                                      float factor = srv->cfg.master_factor;
                                      if (factor < 0.05f) factor = 0.05f;
                                      if (factor > 0.95f) factor = 0.95f;

                                      for (int i = 0; i < tiled; i++) {
                                          struct novawm_client *c = arr[i];

                                          /* Last client gets whatever is left. */
                                          if (i == tiled - 1) {
                                              apply_client_geometry(srv, c, rx, ry, rw, rh);
                                              break;
                                          }

                                          bool split_vert = (i % 2 == 0);  /* even: vertical, odd: horizontal */

                                          if (split_vert) {
                                              int cw = (int)(rw * factor);
                                              if (cw < 1) cw = 1;
                                              int remw = rw - cw - inner;
                                              if (remw < 1) remw = 1;

                                              /* client gets left part */
                                              apply_client_geometry(srv, c, rx, ry, cw, rh);

                                              /* remaining rect becomes right part */
                                              rx = rx + cw + inner;
                                              rw = remw;
                                          } else {
                                              int ch = (int)(rh * factor);
                                              if (ch < 1) ch = 1;
                                              int remh = rh - ch - inner;
                                              if (remh < 1) remh = 1;

                                              /* client gets top part */
                                              apply_client_geometry(srv, c, rx, ry, rw, ch);

                                              /* remaining rect becomes bottom part */
                                              ry = ry + ch + inner;
                                              rh = remh;
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