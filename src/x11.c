#include "novawm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* splash is local to this file – no field needed in novawm_server */
static xcb_window_t novawm_splash = XCB_NONE;

/* --- small helpers --- */

static void
novawm_x11_show_splash(struct novawm_server *srv) {
    int w = 500;
    int h = 80;
    int x = (srv->mon.w - w) / 2;
    int y = (srv->mon.h - h) / 3;

    uint32_t mask =
        XCB_CW_BACK_PIXEL |
        XCB_CW_EVENT_MASK |
        XCB_CW_OVERRIDE_REDIRECT;

    uint32_t values[3];
    values[0] = srv->screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE;
    values[2] = 1;

    novawm_splash = xcb_generate_id(srv->conn);

    xcb_create_window(
        srv->conn,
        XCB_COPY_FROM_PARENT,
        novawm_splash,
        srv->root,
        x, y, w, h,
        1,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        srv->screen->root_visual,
        mask,
        values
    );

    xcb_map_window(srv->conn, novawm_splash);
    xcb_flush(srv->conn);
}

static void
novawm_x11_draw_splash(struct novawm_server *srv) {
    if (!novawm_splash)
        return;

    const char *msg =
        "If you see this inside Xephyr, NovaWM works properly (maybe...)";

    xcb_gcontext_t gc = xcb_generate_id(srv->conn);
    uint32_t gvals[2];
    gvals[0] = srv->screen->black_pixel;
    gvals[1] = srv->screen->white_pixel;

    xcb_create_gc(
        srv->conn, gc, novawm_splash,
        XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gvals
    );

    xcb_image_text_8(
        srv->conn,
        (uint8_t)strlen(msg),
        novawm_splash,
        gc,
        10, 40,
        msg
    );

    xcb_free_gc(srv->conn, gc);
    xcb_flush(srv->conn);
}

/* --- public X11 backend --- */

bool
novawm_x11_init(struct novawm_server *srv) {
    int screen_num = 0;

    const char *disp = getenv("DISPLAY");
    if (!disp || !*disp)
        disp = ":0";  /* fallback */

    fprintf(stderr, "novawm: connecting to DISPLAY=\"%s\"\n", disp);

    srv->conn = xcb_connect(disp, &screen_num);
    if (xcb_connection_has_error(srv->conn)) {
        fprintf(stderr, "novawm: cannot connect to X on %s\n", disp);
        return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(srv->conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);

    for (int i = 0; i < screen_num; i++)
        xcb_screen_next(&it);

    srv->screen = it.data;
    srv->root   = srv->screen->root;

    /* Try to become the WM */
    uint32_t mask =
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY      |
        XCB_EVENT_MASK_PROPERTY_CHANGE       |
        XCB_EVENT_MASK_BUTTON_PRESS          |
        XCB_EVENT_MASK_BUTTON_RELEASE        |
        XCB_EVENT_MASK_POINTER_MOTION        |
        XCB_EVENT_MASK_ENTER_WINDOW          |
        XCB_EVENT_MASK_KEY_PRESS;

    uint32_t values[] = { mask };

    xcb_void_cookie_t ck =
        xcb_change_window_attributes_checked(
            srv->conn, srv->root, XCB_CW_EVENT_MASK, values);

    xcb_generic_error_t *err = xcb_request_check(srv->conn, ck);
    if (err) {
        if (err->error_code == XCB_ACCESS) {
            fprintf(stderr,
                    "novawm: another window manager already owns this DISPLAY.\n");
        } else {
            fprintf(stderr,
                    "novawm: failed to select root events (error %d).\n",
                    err->error_code);
        }
        free(err);
        return false;
    }

    /* Monitor geometry */
    srv->mon.x = 0;
    srv->mon.y = 0;
    srv->mon.w = srv->screen->width_in_pixels;
    srv->mon.h = srv->screen->height_in_pixels;

    /* workspaces init */
    srv->mon.current_ws = 0;
    for (int i = 0; i < NOVAWM_WORKSPACES; i++) {
        srv->mon.ws[i].clients = NULL;
        srv->mon.ws[i].focused = NULL;
    }

    srv->drag.active = false;
    srv->drag.client = NULL;

    srv->keysyms = xcb_key_symbols_alloc(srv->conn);
    if (!srv->keysyms) {
        fprintf(stderr, "novawm: cannot alloc keysyms\n");
        return false;
    }

    novawm_splash = XCB_NONE;
    novawm_x11_show_splash(srv);

    fprintf(stderr, "novawm: X11 backend initialized, root=0x%08x\n",
            srv->root);

    return true;
}

void
novawm_x11_grab_keys(struct novawm_server *srv) {
    xcb_ungrab_key(srv->conn, XCB_GRAB_ANY, srv->root, XCB_MOD_MASK_ANY);

    for (int i = 0; i < srv->cfg.binds_len; i++) {
        struct novawm_bind *b = &srv->cfg.binds[i];
        if (b->keysym == XCB_NO_SYMBOL)
            continue;

        xcb_keycode_t *codes =
            xcb_key_symbols_get_keycode(srv->keysyms, b->keysym);
        if (!codes) continue;

        for (xcb_keycode_t *c = codes; *c != XCB_NO_SYMBOL; c++) {
            xcb_grab_key(
                srv->conn, 1, srv->root, b->mods, *c,
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
            );
        }
        free(codes);
    }

    xcb_flush(srv->conn);
}

void
novawm_x11_scan_existing(struct novawm_server *srv) {
    xcb_query_tree_cookie_t qc = xcb_query_tree(srv->conn, srv->root);
    xcb_query_tree_reply_t *qr = xcb_query_tree_reply(srv->conn, qc, NULL);
    if (!qr) return;

    int len = xcb_query_tree_children_length(qr);
    xcb_window_t *children = xcb_query_tree_children(qr);

    for (int i = 0; i < len; i++) {
        xcb_window_t w = children[i];

        xcb_get_window_attributes_cookie_t ac =
            xcb_get_window_attributes(srv->conn, w);

        xcb_get_window_attributes_reply_t *ar =
            xcb_get_window_attributes_reply(srv->conn, ac, NULL);

        if (!ar) continue;

        if (w != novawm_splash &&
            ar->map_state == XCB_MAP_STATE_VIEWABLE &&
            !ar->override_redirect) {
            novawm_manage_window(srv, w);
        }

        free(ar);
    }

    free(qr);
}

void
novawm_x11_run(struct novawm_server *srv) {
    srv->running = true;

    while (srv->running) {
        xcb_generic_event_t *ev = xcb_wait_for_event(srv->conn);
        if (!ev) break;

        uint8_t type = ev->response_type & ~0x80;

        switch (type) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *e =
                (xcb_map_request_event_t *)ev;
            novawm_manage_window(srv, e->window);
        } break;

        case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *e =
                (xcb_destroy_notify_event_t *)ev;
            if (novawm_splash && e->window == novawm_splash) {
                novawm_splash = XCB_NONE;
            } else {
                struct novawm_client *c =
                    novawm_find_client(srv, e->window);
                if (c) novawm_unmanage_window(srv, c);
            }
        } break;

        case XCB_UNMAP_NOTIFY: {
            xcb_unmap_notify_event_t *e =
                (xcb_unmap_notify_event_t *)ev;
            struct novawm_client *c =
                novawm_find_client(srv, e->window);
            if (c) {
                if (c->ignore_unmap) {
                    /* We caused this unmap (workspace switch) – keep client. */
                    c->ignore_unmap = false;
                } else {
                    /* Client unmapped itself; we keep it until DestroyNotify. */
                    /* no-op */
                }
            }
        } break;

        case XCB_CONFIGURE_REQUEST: {
            xcb_configure_request_event_t *e =
                (xcb_configure_request_event_t *)ev;

            uint32_t mask = 0;
            uint32_t vals[7];
            int i = 0;

            if (e->value_mask & XCB_CONFIG_WINDOW_X)
                vals[i] = e->x, mask |= XCB_CONFIG_WINDOW_X, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
                vals[i] = e->y, mask |= XCB_CONFIG_WINDOW_Y, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
                vals[i] = e->width, mask |= XCB_CONFIG_WINDOW_WIDTH, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
                vals[i] = e->height, mask |= XCB_CONFIG_WINDOW_HEIGHT, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                vals[i] = e->border_width, mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
                vals[i] = e->sibling, mask |= XCB_CONFIG_WINDOW_SIBLING, i++;
            if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
                vals[i] = e->stack_mode, mask |= XCB_CONFIG_WINDOW_STACK_MODE, i++;

            xcb_configure_window(
                srv->conn, e->window, mask, vals);
            xcb_flush(srv->conn);
        } break;

        case XCB_EXPOSE: {
            xcb_expose_event_t *e =
                (xcb_expose_event_t *)ev;

            if (novawm_splash && e->window == novawm_splash)
                novawm_x11_draw_splash(srv);
        } break;

        case XCB_KEY_PRESS:
            novawm_handle_key_press(
                srv, (xcb_key_press_event_t *)ev);
            break;

        case XCB_BUTTON_PRESS:
            novawm_handle_button_press(
                srv, (xcb_button_press_event_t *)ev);
            break;

        case XCB_BUTTON_RELEASE:
            novawm_handle_button_release(
                srv, (xcb_button_release_event_t *)ev);
            break;

        case XCB_MOTION_NOTIFY:
            novawm_handle_motion_notify(
                srv, (xcb_motion_notify_event_t *)ev);
            break;

        case XCB_ENTER_NOTIFY:
            novawm_handle_enter_notify(
                srv, (xcb_enter_notify_event_t *)ev);
            break;

        default:
            break;
        }

        free(ev);
    }
}