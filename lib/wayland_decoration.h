#ifndef LIB_WAYLAND_DECORATION_H_
#define LIB_WAYLAND_DECORATION_H_

#include <gdk/gdk.h>

/*
 * 在Wayland下通过org_kde_kwin_server_decoration向WM声明自己是Client Side Decoration，
 * 从而避免双标题栏，在X11下走原有逻辑
 *
 * 注: 传入的 window 必须已经 realize
 */
void gxde_force_client_side_decoration(GdkWindow* window);

#endif /* LIB_WAYLAND_DECORATION_H_ */
