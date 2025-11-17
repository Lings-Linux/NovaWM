#include "novawm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool novawm_x11_init(struct novawm_server *srv) {
    int screen_num;
    srv->conn = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(srv->conn)) {
        fprintf(stderr, "novawm: cannot connect to X\n");
        return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(srv->conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; i++)
        xcb_screen_next(&it);
    srv->screen = it.data;
    srv->root = srv->screen->root;

    /* Select events on root, but check for another WM (BadAccess) */
    uint32_t mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY      |
                    XCB_EVENT_MASK_PROPERTY_CHANGE       |
                    XCB_EVENT_MASK_BUTTON_PRESS          |
                    XCB_EVENT_MASK_BUTTON_RELEASE        |
                    XCB_EVENT_MASK_POINTER_MOTION        |
                    XCB_EVENT_MASK_ENTER_WINDOW          |
                    XCB_EVENT_MASK_KEY_PRESS;
    uint32_t values[] = { mask };
    xcb_void_cookie_t ck = xcb_change_window_attributes_checked(
        srv->conn, srv->root, XCB_CW_EVENT_MASK, values);
    xcb_generic_error_t *err = xcb_request_check(srv->conn, ck);
    if (err) {
        if (err->error_code == XCB_ACCESS) {
            fprintf(stderr, "novawm: another window manager is already running.\n");
        } else {
            fprintf(stderr, "novawm: failed to select root events (error %d).\n",
                    err->error_code);
        }
        free(err);
        return false;
    }

    /* Single monitor: full screen */
    srv->mon.x = 0;
    srv->mon.y = 0;
    srv->mon.w = srv->screen->width_in_pixels;
    srv->mon.h = srv->screen->height_in_pixels;
    srv->mon.clients = NULL;
    srv->mon.focused = NULL;

    srv->drag.active = false;
    srv->drag.client = NULL;

    srv->keysyms = xcb_key_symbols_alloc(srv->conn);
    if (!srv->keysyms) {
        fprintf(stderr, "novawm: cannot alloc keysyms\n");
        return false;
    }

    return true;
}

void novawm_x11_grab_keys(struct novawm_server *srv) {
    xcb_ungrab_key(srv->conn, XCB_GRAB_ANY, srv->root, XCB_MOD_MASK_ANY);

    for (int i = 0; i < srv->cfg.binds_len; i++) {
        struct novawm_bind *b = &srv->cfg.binds[i];
        if (b->keysym == XCB_NO_SYMBOL)
            continue;

        xcb_keycode_t *codes = xcb_key_symbols_get_keycode(srv->keysyms, b->keysym);
        if (!codes) continue;
        for (xcb_keycode_t *c = codes; *c != XCB_NO_SYMBOL; c++) {
            xcb_grab_key(srv->conn,
                         1,
                         srv->root,
                         b->mods,
                         *c,
                         XCB_GRAB_MODE_ASYNC,
                         XCB_GRAB_MODE_ASYNC);
        }
        free(codes);
    }
    xcb_flush(srv->conn);
}

void novawm_x11_run(struct novawm_server *srv) {
    srv->running = true;

    while (srv->running) {
        xcb_generic_event_t *ev = xcb_wait_for_event(srv->conn);
        if (!ev) break;

        uint8_t type = ev->response_type & ~0x80;

        switch (type) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
            novawm_manage_window(srv, e->window);
        } break;

        case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
            struct novawm_client *c = novawm_find_client(srv, e->window);
            if (c) novawm_unmanage_window(srv, c);
        } break;

        case XCB_UNMAP_NOTIFY: {
            xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
            struct novawm_client *c = novawm_find_client(srv, e->window);
            if (c) novawm_unmanage_window(srv, c);
        } break;

        case XCB_CONFIGURE_REQUEST: {
            xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
            uint32_t mask = 0;
            uint32_t vals[7];
            int i = 0;
            if (e->value_mask & XCB_CONFIG_WINDOW_X) { mask |= XCB_CONFIG_WINDOW_X; vals[i++] = e->x; }
            if (e->value_mask & XCB_CONFIG_WINDOW_Y) { mask |= XCB_CONFIG_WINDOW_Y; vals[i++] = e->y; }
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) { mask |= XCB_CONFIG_WINDOW_WIDTH; vals[i++] = e->width; }
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) { mask |= XCB_CONFIG_WINDOW_HEIGHT; vals[i++] = e->height; }
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) { mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH; vals[i++] = e->border_width; }
            if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) { mask |= XCB_CONFIG_WINDOW_SIBLING; vals[i++] = e->sibling; }
            if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) { mask |= XCB_CONFIG_WINDOW_STACK_MODE; vals[i++] = e->stack_mode; }

            xcb_configure_window(srv->conn, e->window, mask, vals);
            xcb_flush(srv->conn);
        } break;

        case XCB_KEY_PRESS: {
            novawm_handle_key_press(srv, (xcb_key_press_event_t*)ev);
        } break;

        case XCB_BUTTON_PRESS: {
            novawm_handle_button_press(srv, (xcb_button_press_event_t*)ev);
        } break;

        case XCB_BUTTON_RELEASE: {
            novawm_handle_button_release(srv, (xcb_button_release_event_t*)ev);
        } break;

        case XCB_MOTION_NOTIFY: {
            novawm_handle_motion_notify(srv, (xcb_motion_notify_event_t*)ev);
        } break;

        case XCB_ENTER_NOTIFY: {
            novawm_handle_enter_notify(srv, (xcb_enter_notify_event_t*)ev);
        } break;

        default:
            break;
        }

        free(ev);
    }
}