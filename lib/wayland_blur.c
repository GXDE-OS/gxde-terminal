#include <string.h>
#include "wayland_blur.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

#include "blur-client-protocol.h"
#include "treeland-personalization-manager-v1-client-protocol.h"

#define GXDE_KDE_BLUR_KEY "gxde-kwin-blur"
#define GXDE_TREELAND_CTX_KEY "gxde-treeland-winctx"

/* Treeland走treeland_personalization_manager_v1 */
static struct treeland_personalization_manager_v1* g_treeland_manager = NULL;

/* KWin/Wlcom用org_kde_kwin_blur的区域模糊。 */
static struct org_kde_kwin_blur_manager* g_kde_blur_manager = NULL;
static struct wl_compositor* g_compositor = NULL;

static int g_globals_queried = 0;

static void registry_handle_global(void* data, struct wl_registry* registry,
        uint32_t name, const char* interface, uint32_t version) {

    (void) data;
    (void) version;

    if (strcmp(interface, "treeland_personalization_manager_v1") == 0) {
        g_treeland_manager = wl_registry_bind(
            registry, name,
            &treeland_personalization_manager_v1_interface, 1);
        wl_proxy_set_queue((struct wl_proxy*) g_treeland_manager, NULL);
    } else if (strcmp(interface, "org_kde_kwin_blur_manager") == 0) {
        g_kde_blur_manager = wl_registry_bind(
            registry, name,
            &org_kde_kwin_blur_manager_interface, 1);
        wl_proxy_set_queue((struct wl_proxy*) g_kde_blur_manager, NULL);
    } else if (strcmp(interface, "wl_compositor") == 0) {
        g_compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, 1);
        wl_proxy_set_queue((struct wl_proxy*) g_compositor, NULL);
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

static void ensure_globals(struct wl_display* display) {
    if (g_globals_queried) {
        return;
    }
    g_globals_queried = 1;

    struct wl_event_queue* queue = wl_display_create_queue(display);
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

    /* 触发registry_handle_global */
    wl_display_roundtrip_queue(display, queue);

    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);
}

static struct treeland_personalization_window_context_v1*
get_or_create_treeland_ctx(GdkWindow* window, struct wl_surface* surface) {
    struct treeland_personalization_window_context_v1* ctx =
        g_object_get_data(G_OBJECT(window), GXDE_TREELAND_CTX_KEY);
    if (ctx != NULL) {
        return ctx;
    }

    ctx = treeland_personalization_manager_v1_get_window_context(
        g_treeland_manager, surface);
    if (ctx == NULL) {
        return NULL;
    }

    g_object_set_data_full(G_OBJECT(window), GXDE_TREELAND_CTX_KEY, ctx,
        (GDestroyNotify) treeland_personalization_window_context_v1_destroy);
    return ctx;
}

static struct org_kde_kwin_blur* get_or_create_kde_blur(GdkWindow* window,
        struct wl_surface* surface) {
    struct org_kde_kwin_blur* blur =
        g_object_get_data(G_OBJECT(window), GXDE_KDE_BLUR_KEY);
    if (blur != NULL) {
        return blur;
    }

    blur = org_kde_kwin_blur_manager_create(g_kde_blur_manager, surface);
    if (blur == NULL) {
        return NULL;
    }
    g_object_set_data_full(G_OBJECT(window), GXDE_KDE_BLUR_KEY, blur,
        (GDestroyNotify) org_kde_kwin_blur_release);
    return blur;
}

static int resolve_window(GdkWindow* window, struct wl_display** display_out,
        struct wl_surface** surface_out) {
    if (window == NULL || !GDK_IS_WAYLAND_WINDOW(window)) {
        return 0;
    }

    GdkDisplay* gdk_display = gdk_window_get_display(window);
    if (gdk_display == NULL || !GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
        return 0;
    }

    struct wl_display* display = gdk_wayland_display_get_wl_display(
        gdk_display);
    struct wl_surface* surface = gdk_wayland_window_get_wl_surface(window);
    if (display == NULL || surface == NULL) {
        return 0;
    }

    *display_out = display;
    *surface_out = surface;
    return 1;
}

void gxde_set_blur_region(GdkWindow* window, int x, int y, int width,
        int height) {
    struct wl_display* display;
    struct wl_surface* surface;
    if (!resolve_window(window, &display, &surface)) {
        return;
    }

    ensure_globals(display);

    /* 优先 Treeland：整窗背景模糊，不需要区域。 */
    if (g_treeland_manager != NULL) {
        struct treeland_personalization_window_context_v1* ctx =
            get_or_create_treeland_ctx(window, surface);
        if (ctx != NULL) {
            treeland_personalization_window_context_v1_set_blend_mode(
                ctx,
                TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_BLUR);
            wl_display_flush(display);
        }
        return;
    }

    /* 回退KDE: 按内容区域模糊。 */
    if (g_kde_blur_manager == NULL || g_compositor == NULL) {
        return;
    }

    struct org_kde_kwin_blur* blur = get_or_create_kde_blur(window, surface);
    if (blur == NULL) {
        return;
    }

    struct wl_region* region = wl_compositor_create_region(g_compositor);
    if (region == NULL) {
        return;
    }
    wl_region_add(region, x, y, width, height);

    org_kde_kwin_blur_set_region(blur, region);
    org_kde_kwin_blur_commit(blur);
    wl_region_destroy(region);

    wl_display_flush(display);
}

void gxde_clear_blur(GdkWindow* window) {
    struct wl_display* display;
    struct wl_surface* surface;
    if (!resolve_window(window, &display, &surface)) {
        return;
    }

    ensure_globals(display);

    /* Treeland: blend mode -> 透明。 */
    if (g_treeland_manager != NULL) {
        struct treeland_personalization_window_context_v1* ctx =
            get_or_create_treeland_ctx(window, surface);
        if (ctx != NULL) {
            treeland_personalization_window_context_v1_set_blend_mode(
                ctx,
                TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_TRANSPARENT);
            wl_display_flush(display);
        }
        return;
    }

    /* KDE：移除缓存的 blur 对象并 unset。 */
    g_object_set_data(G_OBJECT(window), GXDE_KDE_BLUR_KEY, NULL);
    if (g_kde_blur_manager != NULL) {
        org_kde_kwin_blur_manager_unset(g_kde_blur_manager, surface);
        wl_display_flush(display);
    }
}

#else /* !GDK_WINDOWING_WAYLAND */

/* X11下依旧走原逻辑 本patch不应该生效 */
void gxde_set_blur_region(GdkWindow* window, int x, int y, int width,
        int height) {
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
}

void gxde_clear_blur(GdkWindow* window) {
    (void) window;
}

#endif
