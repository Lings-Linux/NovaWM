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

                                      /* Count tiled (non-floating) clients */
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
                                          /* still update borders of floating clients */
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

                                      /* simple vertical tiling:
                                       * each tiled client gets a vertical slot, full width
                                       */
                                      int total_height = mh - inner * (tiled + 1);
                                      if (total_height < 1) total_height = mh;
                                      int slot_h = total_height / tiled;
                                      if (slot_h < 1) slot_h = 1;

                                      int y = my + inner;

                                      for (struct novawm_client *c = ws->clients; c; c = c->next) {
                                          if (c->floating)
                                              continue;

                                          int x = mx + inner;
                                          int w = mw - 2 * inner;
                                          int h = slot_h;

                                          apply_client_geometry(srv, c, x, y, w, h);
                                          y += slot_h + inner;
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