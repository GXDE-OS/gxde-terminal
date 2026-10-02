#include <math.h>
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

/* wl_region 只支持矩形，圆角部分按行拆成1像素高的横条
 * 原来还要自己拼.png
 */
static void add_rounded_rect(struct wl_region* region, int x, int y,
        int width, int height, int radius) {
    if (radius > width / 2) {
        radius = width / 2;
    }

    if (radius > height / 2) {
        radius = height / 2;
    }

    if (radius <= 0) {
        wl_region_add(region, x, y, width, height);
        return;
    }

    for (int i = 0; i < radius; i++) {
        double dy = radius - i - 0.5;
        int inset = (int) lround(radius - sqrt((double) radius * radius - dy * dy));
        wl_region_add(region, x + inset, y + i, width - inset * 2, 1);
        wl_region_add(region, x + inset, y + height - 1 - i, width - inset * 2, 1);
    }
    wl_region_add(region, x, y + radius, width, height - radius * 2);
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

static void wl_set_blur_region(GdkWindow* window, int x, int y, int width,
        int height, int radius) {
    struct wl_display* display;
    struct wl_surface* surface;
    if (!resolve_window(window, &display, &surface)) {
        return;
    }

    ensure_globals(display);

    /* 优先 KDE：按区域模糊，可以把圆角外的部分排除掉。 */
    if (g_kde_blur_manager != NULL && g_compositor != NULL) {
        struct org_kde_kwin_blur* blur = get_or_create_kde_blur(window, surface);
        if (blur == NULL) {
            return;
        }

        struct wl_region* region = wl_compositor_create_region(g_compositor);
        if (region == NULL) {
            return;
        }
        add_rounded_rect(region, x, y, width, height, radius);

        org_kde_kwin_blur_set_region(blur, region);
        org_kde_kwin_blur_commit(blur);
        wl_region_destroy(region);

        wl_display_flush(display);
        return;
    }

    /* 回退 Treeland：整窗背景模糊，不支持区域。 */
    if (g_treeland_manager != NULL) {
        struct treeland_personalization_window_context_v1* ctx =
            get_or_create_treeland_ctx(window, surface);
        if (ctx != NULL) {
            treeland_personalization_window_context_v1_set_blend_mode(
                ctx,
                TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_BLUR);
            treeland_personalization_window_context_v1_set_round_corner_radius(
                ctx, radius);
            wl_display_flush(display);
        }
    }
}

static void wl_clear_blur(GdkWindow* window) {
    struct wl_display* display;
    struct wl_surface* surface;
    if (!resolve_window(window, &display, &surface)) {
        return;
    }

    ensure_globals(display);

    /* KDE：移除缓存的 blur 对象并 unset。 */
    if (g_kde_blur_manager != NULL && g_compositor != NULL) {
        g_object_set_data(G_OBJECT(window), GXDE_KDE_BLUR_KEY, NULL);
        org_kde_kwin_blur_manager_unset(g_kde_blur_manager, surface);
        wl_display_flush(display);
        return;
    }

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
    }
}

static int wl_blur_available(GdkDisplay* gdk_display) {
    if (gdk_display == NULL || !GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
        return 0;
    }

    struct wl_display* display = gdk_wayland_display_get_wl_display(
        gdk_display);
    if (display == NULL) {
        return 0;
    }

    ensure_globals(display);

    return (g_kde_blur_manager != NULL && g_compositor != NULL)
        || g_treeland_manager != NULL;
}

#endif /* GDK_WINDOWING_WAYLAND */

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

static int x11_list_contains_atom(Display* xdisplay, Window window,
        Atom list_atom, Atom target) {
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char* data = NULL;
    int found = 0;

    if (XGetWindowProperty(xdisplay, window, list_atom, 0, 4096, False,
            XA_ATOM, &type, &format, &count, &remaining, &data) == Success
            && data != NULL) {
        Atom* atoms = (Atom*) data;
        for (unsigned long i = 0; i < count; i++) {
            if (atoms[i] == target) {
                found = 1;
                break;
            }
        }
    }

    if (data != NULL) {
        XFree(data);
    }
    return found;
}

static int x11_blur_available(GdkDisplay* gdk_display) {
    if (!gdk_screen_is_composited(gdk_display_get_default_screen(gdk_display))) {
        return 0;
    }

    Display* xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
    Window root = DefaultRootWindow(xdisplay);

    Atom net_supported = XInternAtom(xdisplay, "_NET_SUPPORTED", False);
    Atom deepin_blur = XInternAtom(xdisplay, "_NET_WM_DEEPIN_BLUR_REGION_ROUNDED", False);
    if (x11_list_contains_atom(xdisplay, root, net_supported, deepin_blur)) {
        return 1;
    }

    Atom kde_blur = XInternAtom(xdisplay, "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    int count = 0;
    Atom* properties = XListProperties(xdisplay, root, &count);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (properties[i] == kde_blur) {
            found = 1;
            break;
        }
    }
    if (properties != NULL) {
        XFree(properties);
    }
    return found;
}

static void x11_set_blur_region(GdkWindow* window, int x, int y, int width,
        int height, int radius) {
    Display* xdisplay = GDK_WINDOW_XDISPLAY(window);
    Window xid = GDK_WINDOW_XID(window);
    int scale = gdk_window_get_scale_factor(window);

    long deepin_data[6] = { x * scale, y * scale, width * scale, height * scale,
                            radius * scale, radius * scale };
    long kde_data[4] = { x * scale, y * scale, width * scale, height * scale };

    XChangeProperty(xdisplay, xid,
        XInternAtom(xdisplay, "_NET_WM_DEEPIN_BLUR_REGION_ROUNDED", False),
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*) deepin_data, 6);
    XChangeProperty(xdisplay, xid,
        XInternAtom(xdisplay, "_KDE_NET_WM_BLUR_BEHIND_REGION", False),
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*) kde_data, 4);
    XFlush(xdisplay);
}

static void x11_clear_blur(GdkWindow* window) {
    Display* xdisplay = GDK_WINDOW_XDISPLAY(window);
    Window xid = GDK_WINDOW_XID(window);

    XDeleteProperty(xdisplay, xid,
        XInternAtom(xdisplay, "_NET_WM_DEEPIN_BLUR_REGION_ROUNDED", False));
    XDeleteProperty(xdisplay, xid,
        XInternAtom(xdisplay, "_KDE_NET_WM_BLUR_BEHIND_REGION", False));
    XFlush(xdisplay);
}
#endif /* GDK_WINDOWING_X11 */

void gxde_set_blur_region(GdkWindow* window, int x, int y, int width,
        int height, int radius) {
    if (window == NULL) {
        return;
    }
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_WINDOW(window)) {
        x11_set_blur_region(window, x, y, width, height, radius);
        return;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    wl_set_blur_region(window, x, y, width, height, radius);
#endif
}

void gxde_clear_blur(GdkWindow* window) {
    if (window == NULL) {
        return;
    }
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_WINDOW(window)) {
        x11_clear_blur(window);
        return;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    wl_clear_blur(window);
#endif
}

int gxde_blur_available(GdkDisplay* display) {
    if (display == NULL) {
        return 0;
    }
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(display)) {
        return x11_blur_available(display);
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    return wl_blur_available(display);
#else
    return 0;
#endif
}
