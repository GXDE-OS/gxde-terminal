#ifndef LIB_WAYLAND_BLUR_H_
#define LIB_WAYLAND_BLUR_H_

#include <gdk/gdk.h>

/*
 * 在 Wayland 下通过 org_kde_kwin_blur 协议为窗口设置“背景模糊”区域，
 * 对应 X11 上的 _KDE_NET_WM_BLUR_BEHIND_REGION 属性。
 *
 * gxde_set_blur_region: 启用模糊，region 为 surface 逻辑坐标（无需乘缩放）。
 * gxde_clear_blur:      取消模糊。
 *
 * 传入的 window 必须已经 realize。非 Wayland 后端为 no-op，可安全无条件调用。
 */

void gxde_set_blur_region(GdkWindow* window, int x, int y, int width,
    int height);

void gxde_clear_blur(GdkWindow* window);

#endif /* LIB_WAYLAND_BLUR_H_ */
