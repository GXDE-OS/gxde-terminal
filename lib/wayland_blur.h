#ifndef LIB_WAYLAND_BLUR_H_
#define LIB_WAYLAND_BLUR_H_

#include <gdk/gdk.h>

/*
 * 在 Wayland 下通过 org_kde_kwin_blur 协议为窗口设置“背景模糊”区域，
 * 对应 X11 上的 _KDE_NET_WM_BLUR_BEHIND_REGION 属性。
 *
 * gxde_set_blur_region: 启用模糊，region 为 surface 逻辑坐标（无需乘缩放），
 *                       radius > 0 时四角按该半径做圆角，避免模糊区域在圆角外露出方角。
 * gxde_clear_blur:      取消模糊。
 * gxde_blur_available:  查询合成器是否支持模糊。
 *
 * 传入的 window 必须已经 realize。非 Wayland 后端为 no-op，可安全无条件调用。
 */

void gxde_set_blur_region(GdkWindow* window, int x, int y, int width,
    int height, int radius);

void gxde_clear_blur(GdkWindow* window);

/* 当前合成器是否支持背景模糊（Wayland 下有 kwin blur 或 Treeland 时返回 1，X11 恒为 0）。 */
int gxde_blur_available(GdkDisplay* display);

#endif /* LIB_WAYLAND_BLUR_H_ */
