#include <string.h>
#include "wayland_decoration.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

#include "server-decoration-client-protocol.h"

static struct org_kde_kwin_server_decoration_manager* g_deco_manager = NULL;

static void registry_handle_global(void* data, struct wl_registry* registry,
        uint32_t name, const char* interface, uint32_t version) {
    (void) data;
    (void) version;

    if (strcmp(interface, "org_kde_kwin_server_decoration_manager") == 0) {
        g_deco_manager = wl_registry_bind(
            registry, name,
            &org_kde_kwin_server_decoration_manager_interface, 1);
        wl_proxy_set_queue((struct wl_proxy *) g_deco_manager, NULL);
    }
}

static void registry_handle_global_remove(void* data,
        struct wl_registry* registry, uint32_t name) {
    (void) data;
    (void) registry;
    (void) name;
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove,
};

static void ensure_manager(struct wl_display* display) {
    if (g_deco_manager != NULL) {
        return;
    }

    /* 使用独立事件队列查询全局，避免抢走 GTK 默认队列里的事件。 */
    struct wl_event_queue *queue = wl_display_create_queue(display);
    if (queue == NULL) {
        return;
    }

    struct wl_display* wrapped = wl_proxy_create_wrapper(display);
    if (wrapped == NULL) {
        wl_event_queue_destroy(queue);
        return;
    }
    wl_proxy_set_queue((struct wl_proxy*) wrapped, queue);

    struct wl_registry* registry = wl_display_get_registry(wrapped);
    wl_proxy_wrapper_destroy(wrapped);
    if (registry == NULL) {
        wl_event_queue_destroy(queue);
        return;
    }

    wl_registry_add_listener(registry, &registry_listener, NULL);

    /* 往返一次触发registry_handle_global */
    wl_display_roundtrip_queue(display, queue);

    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);
}

void gxde_force_client_side_decoration(GdkWindow* window) {
    if (window == NULL || !GDK_IS_WAYLAND_WINDOW(window)) {
        return;
    }

    GdkDisplay* gdk_display = gdk_window_get_display(window);
    if (gdk_display == NULL || !GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
        return;
    }

    struct wl_display* display = gdk_wayland_display_get_wl_display(
        gdk_display);
    struct wl_surface* surface = gdk_wayland_window_get_wl_surface(
        window);
    if (display == NULL || surface == NULL) {
        return;
    }

    ensure_manager(display);

    /* WM不支持org_kde_kwin_server_decoration_manager时交由原行为处理 */
    if (g_deco_manager == NULL) {
        return;
    }

    struct org_kde_kwin_server_decoration* deco =
        org_kde_kwin_server_decoration_manager_create(g_deco_manager, surface);

    if (deco == NULL) {
        return;
    }

    org_kde_kwin_server_decoration_request_mode(
        deco, ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT);
    wl_display_flush(display);
}

#else /* !GDK_WINDOWING_WAYLAND */

/* X11下走原逻辑，此patch不应起作用 */
void gxde_force_client_side_decoration(GdkWindow *window) {
    (void) window;
}

#endif
