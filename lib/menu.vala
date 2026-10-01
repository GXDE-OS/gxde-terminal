/* -*- Mode: Vala; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *               2011 ~ 2018 Wang Yong
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Wang Yong <wangyong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Gtk;

namespace Menu {
    [CCode (cheader_filename = "wayland_blur.h", cname = "gxde_set_blur_region")]
    private extern void wayland_set_blur_region (Gdk.Window window, int x, int y, int width, int height, int radius);
    [CCode (cheader_filename = "wayland_blur.h", cname = "gxde_blur_available")]
    private extern bool wayland_blur_available (Gdk.Display display);

    public class MenuItem : Object {
        public string menu_item_id;
        public string menu_item_text;
        public bool menu_item_checkable;
        public bool menu_item_checked;
        public bool menu_item_sensitive = true;
        public List<MenuItem> menu_item_submenu;

        public MenuItem(string item_id, string item_text, bool checkable = false, bool checked = false, bool sensitive = true) {
            menu_item_id = item_id;
            menu_item_text = item_text;
            menu_item_checkable = checkable;
            menu_item_checked = checked;
            menu_item_sensitive = sensitive;

            menu_item_submenu = new List<MenuItem>();
        }

        public void add_submenu_item(MenuItem item) {
            menu_item_submenu.append(item);
        }
    }

    // 用 GTK 菜单复刻 deepin-menu 的外观，尺寸与颜色取自 dstyle 插件 dlight2/ddark2 与 DTK 的 DMenuEffect。
    public class Menu : Object {
        private const int RADIUS = 8;
        private const int ITEM_HEIGHT = 22;
        private const int CHECK_X = 10;
        private const int TEXT_X = 27;
        private const int TEXT_RIGHT_PADDING = 44;
        private const int ARROW_RIGHT_MARGIN = 6;
        private const double BLUR_BACKGROUND_ALPHA = 0.45;

        private static Gtk.CssProvider? css_provider = null;
        private static HashTable<string, Cairo.Surface?> icon_cache = null;

        private Gtk.Menu? gtk_menu = null;
        private bool is_dark = false;
        private bool blur_enabled = false;
        private int scale_factor = 1;

        public signal void click_item(string item_id);
        public signal void destroy();

        public Menu() {}

        // 在指针处弹出（右键菜单、键盘菜单键）。
        public void popup_at_pointer(List<MenuItem> menu_content, Gtk.Widget attach_widget, Gdk.Event? trigger_event) {
            prepare(menu_content, attach_widget);

            var toplevel_window = attach_widget.get_toplevel().get_window();
            Gdk.Device? pointer = trigger_event != null ? trigger_event.get_device() : null;
            if (pointer == null) {
                pointer = attach_widget.get_display().get_default_seat().get_pointer();
            }
            int x, y;
            toplevel_window.get_device_position(pointer, out x, out y, null);
            popup_at_toplevel_rect(toplevel_window, { x, y, 1, 1 }, Gdk.Gravity.NORTH_WEST, Gdk.Gravity.NORTH_WEST, trigger_event);
        }

        // 贴着控件弹出，例如标题栏菜单按钮。
        public void popup_at_widget(List<MenuItem> menu_content, Gtk.Widget widget, Gdk.Gravity widget_anchor, Gdk.Gravity menu_anchor, Gdk.Event? trigger_event) {
            prepare(menu_content, widget);

            var toplevel = widget.get_toplevel();
            int x, y;
            widget.translate_coordinates(toplevel, 0, 0, out x, out y);
            popup_at_toplevel_rect(toplevel.get_window(), { x, y, widget.get_allocated_width(), widget.get_allocated_height() }, widget_anchor, menu_anchor, trigger_event);
        }

        // 在相对 widget 的 (x, y) 处弹出，用于键盘触发等没有指针事件的场景。
        public void popup_at_rect(List<MenuItem> menu_content, Gtk.Widget widget, int x, int y, Gdk.Event? trigger_event) {
            prepare(menu_content, widget);

            var toplevel = widget.get_toplevel();
            int window_x, window_y;
            widget.translate_coordinates(toplevel, x, y, out window_x, out window_y);
            popup_at_toplevel_rect(toplevel.get_window(), { window_x, window_y, 1, 1 }, Gdk.Gravity.NORTH_WEST, Gdk.Gravity.NORTH_WEST, trigger_event);
        }

        // 统一以顶层窗口为锚点：EventBox、VTE 等控件有自己的子 GdkWindow，
        // Wayland 下 GDK 不会把子窗口坐标换算到顶层 surface，直接用 GTK 的 popup_at_widget/pointer 会错位。
        private void popup_at_toplevel_rect(Gdk.Window toplevel_window, Gdk.Rectangle rect, Gdk.Gravity rect_anchor, Gdk.Gravity menu_anchor, Gdk.Event? trigger_event) {
            gtk_menu.popup_at_rect(toplevel_window, rect, rect_anchor, menu_anchor, trigger_event);
        }

        private void prepare(List<MenuItem> menu_content, Gtk.Widget attach_widget) {
            is_dark = detect_dark_style();
            blur_enabled = wayland_blur_available(attach_widget.get_display());
            scale_factor = attach_widget.get_scale_factor();
            load_css(attach_widget.get_screen());

            gtk_menu = create_menu(menu_content);
            gtk_menu.attach_to_widget(attach_widget, null);
            gtk_menu.deactivate.connect(() => {
                    // deactivate 先于菜单项的 activate 发出，延后收尾以保证 click_item 先送达。
                    GLib.Idle.add(() => {
                            finish();
                            return false;
                        });
                });
            gtk_menu.show_all();
        }

        private void finish() {
            if (gtk_menu == null) {
                return;
            }

            gtk_menu.destroy();
            gtk_menu = null;
            destroy();
        }

        private bool detect_dark_style() {
            // deepin-menu 跟随 QT_STYLE_OVERRIDE（dlight2/ddark2），没有时退回 GTK 主题名判断。
            string? qt_style = Environment.get_variable("QT_STYLE_OVERRIDE");
            if (qt_style != null && qt_style != "") {
                return qt_style.down().contains("dark");
            }

            string theme_name = Gtk.Settings.get_default().gtk_theme_name ?? "";
            return theme_name.down().contains("dark");
        }

        private Gtk.Menu create_menu(List<MenuItem> menu_content) {
            var menu = new Gtk.Menu();
            // 勾选标记由菜单项自己绘制，不需要 GTK 预留 toggle 列。
            menu.reserve_toggle_size = false;
            menu.get_style_context().add_class("gxde-menu");
            menu.get_style_context().add_class(is_dark ? "dark" : "light");
            if (blur_enabled) {
                menu.get_style_context().add_class("blur");
            }

            var toplevel = menu.get_toplevel();
            toplevel.get_style_context().add_class("gxde-menu-window");
            menu.size_allocate.connect_after((w, a) => {
                    update_surface(menu);
                });
            toplevel.map.connect_after((w) => {
                    update_surface(menu);
                });

            foreach (unowned MenuItem menu_item in menu_content) {
                menu.append(create_menu_item(menu_item));
            }

            return menu;
        }

        private void update_surface(Gtk.Menu menu) {
            var toplevel = menu.get_toplevel();
            var window = toplevel.get_window();
            if (window == null) {
                return;
            }

            int x, y;
            menu.translate_coordinates(toplevel, 0, 0, out x, out y);
            int width = menu.get_allocated_width();
            int height = menu.get_allocated_height();

            // 把阴影声明给合成器，让定位和贴边避让只按可见面板计算，
            // 否则靠近屏幕边缘时菜单会多让出一个阴影宽度，与按钮对不齐。
            window.set_shadow_width(x, toplevel.get_allocated_width() - x - width,
                                    y, toplevel.get_allocated_height() - y - height);

            if (blur_enabled) {
                wayland_set_blur_region(window, x, y, width, height, RADIUS);
            }
        }

        private Gtk.MenuItem create_menu_item(MenuItem menu_item) {
            if (menu_item.menu_item_text == "") {
                return new Gtk.SeparatorMenuItem();
            }

            var item = new Gtk.MenuItem();
            var box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 0);

            // 勾选标记位于 x=10，文字从 x=27 开始，与 dstyle 的 drawMenuItemControl 一致。
            var check_image = new Gtk.Image();
            check_image.set_size_request(TEXT_X - CHECK_X, -1);
            check_image.halign = Gtk.Align.START;
            check_image.xalign = 0;
            check_image.margin_start = CHECK_X;
            box.pack_start(check_image, false, false, 0);

            var label = new Gtk.Label(menu_item.menu_item_text);
            label.halign = Gtk.Align.START;
            label.margin_end = TEXT_RIGHT_PADDING;
            box.pack_start(label, true, true, 0);

            var arrow_image = new Gtk.Image();
            arrow_image.margin_end = ARROW_RIGHT_MARGIN;
            box.pack_end(arrow_image, false, false, 0);

            item.add(box);
            item.set_size_request(-1, ITEM_HEIGHT);
            item.sensitive = menu_item.menu_item_sensitive;

            bool has_submenu = menu_item.menu_item_submenu.length() > 0;
            bool show_check = menu_item.menu_item_checkable && menu_item.menu_item_checked;

            if (has_submenu) {
                item.set_submenu(create_menu(menu_item.menu_item_submenu));
            } else {
                string item_id = menu_item.menu_item_id;
                item.activate.connect(() => {
                        click_item(item_id);
                    });
            }

            // 根据高亮/禁用状态切换图标，对应 dstyle 的 _normal/_selected/_disabled。
            item.state_flags_changed.connect((w, previous) => {
                    update_item_icons(item, check_image, arrow_image, show_check, has_submenu);
                });
            update_item_icons(item, check_image, arrow_image, show_check, has_submenu);

            return item;
        }

        private void update_item_icons(Gtk.Widget item, Gtk.Image check_image, Gtk.Image arrow_image, bool show_check, bool has_submenu) {
            var flags = item.get_state_flags();
            string state = "normal";
            if (Gtk.StateFlags.INSENSITIVE in flags) {
                state = "disabled";
            } else if (Gtk.StateFlags.PRELIGHT in flags) {
                state = "selected";
            }

            check_image.set_from_surface(show_check ? load_icon("check", state, scale_factor) : null);
            arrow_image.set_from_surface(has_submenu ? load_icon("arrow-right", state, scale_factor) : null);
        }

        private Cairo.Surface? load_icon(string name, string state, int scale) {
            if (icon_cache == null) {
                icon_cache = new HashTable<string, Cairo.Surface?>(str_hash, str_equal);
            }

            string style_name = is_dark ? "dark" : "light";
            string key = "%s/%s_%s@%d".printf(style_name, name, state, scale);
            if (icon_cache.contains(key)) {
                return icon_cache.get(key);
            }

            Cairo.Surface? surface = null;
            // 与 dstyle 相同的查找顺序：先找对应状态，找不到退回 _normal。
            foreach (string icon_state in new string[] { state, "normal" }) {
                foreach (string format in new string[] { "svg", "png" }) {
                    string path = Utils.get_image_path("menu/%s/%s_%s.%s".printf(style_name, name, icon_state, format));
                    if (!FileUtils.test(path, FileTest.EXISTS)) {
                        continue;
                    }

                    try {
                        int width, height;
                        Gdk.Pixbuf.get_file_info(path, out width, out height);
                        var pixbuf = new Gdk.Pixbuf.from_file_at_scale(path, width * scale, height * scale, true);
                        surface = Gdk.cairo_surface_create_from_pixbuf(pixbuf, scale, null);
                    } catch (GLib.Error e) {
                        warning("Load menu icon %s failed: %s", path, e.message);
                    }

                    if (surface != null) {
                        break;
                    }
                }

                if (surface != null) {
                    break;
                }
            }

            icon_cache.insert(key, surface);
            return surface;
        }

        private void load_css(Gdk.Screen screen) {
            if (css_provider == null) {
                css_provider = new Gtk.CssProvider();
                Gtk.StyleContext.add_provider_for_screen(screen, css_provider, Gtk.STYLE_PROVIDER_PRIORITY_USER);
            }

            try {
                css_provider.load_from_data(get_menu_css());
            } catch (GLib.Error e) {
                warning("Something bad happened with CSS load %s", e.message);
            }
        }

        // 取 DTK 的 Qt 字体设置，使菜单文字与 deepin-menu 一致。
        private string get_font_css() {
            var key_file = new KeyFile();
            try {
                key_file.load_from_file(Path.build_filename(Environment.get_user_config_dir(), "deepin", "qt-theme.ini"), KeyFileFlags.NONE);
                string font_css = "";
                if (key_file.has_key("Theme", "Font")) {
                    font_css += "font-family: \"%s\";".printf(key_file.get_string("Theme", "Font"));
                }
                if (key_file.has_key("Theme", "FontSize")) {
                    font_css += "font-size: %spt;".printf(key_file.get_string("Theme", "FontSize"));
                }
                return font_css;
            } catch (GLib.Error e) {
                return "";
            }
        }

        private string get_menu_css() {
            string font_css = get_font_css();

            return @"
            window.gxde-menu-window,
            window.gxde-menu-window.background {
                background-color: transparent;
            }
            window.gxde-menu-window decoration,
            window.gxde-menu-window decoration:backdrop {
                margin: 0;
                border-radius: $(RADIUS)px;
                box-shadow: 0 3px 14px rgba(0, 0, 0, 0.18), 0 0 0 1px rgba(0, 0, 0, 0.08);
            }
            menu.gxde-menu {
                border: none;
                border-radius: $(RADIUS)px;
                margin: 0;
                /* 上下留白不小于圆角半径，首末项高亮才不会顶进圆角里 */
                padding: $(RADIUS)px 0;
            }
            menu.gxde-menu.light { background-color: rgb(255, 255, 255); }
            menu.gxde-menu.light.blur { background-color: rgba(255, 255, 255, $(BLUR_BACKGROUND_ALPHA)); }
            menu.gxde-menu.dark { background-color: #202020; }
            menu.gxde-menu.dark.blur { background-color: rgba(32, 32, 32, $(BLUR_BACKGROUND_ALPHA)); }
            menu.gxde-menu menuitem {
                min-height: $(ITEM_HEIGHT)px;
                padding: 0;
                margin: 0;
                border: none;
                border-radius: 0;
                background: none;
                box-shadow: none;
                $font_css
            }
            menu.gxde-menu menuitem label {
                color: inherit;
                $font_css
            }
            menu.gxde-menu menuitem arrow {
                -gtk-icon-source: none;
                min-width: 0;
                min-height: 0;
                margin: 0;
                padding: 0;
            }
            menu.gxde-menu separator {
                min-height: 1px;
                margin: 2px 5px;
                padding: 0;
            }
            menu.gxde-menu.light menuitem { color: rgb(25, 25, 25); }
            menu.gxde-menu.light menuitem:disabled { color: rgba(0, 0, 0, 0.4); }
            menu.gxde-menu.light menuitem:hover { background-color: #2ca7f8; color: #ffffff; }
            menu.gxde-menu.light separator { background-color: rgba(0, 0, 0, 0.1); }
            menu.gxde-menu.dark menuitem { color: #ffffff; }
            menu.gxde-menu.dark menuitem:disabled { color: rgba(255, 255, 255, 0.4); }
            menu.gxde-menu.dark menuitem:hover { background-color: #61b5f8; color: #ffffff; }
            menu.gxde-menu.dark separator { background-color: rgba(255, 255, 255, 0.1); }
            ";
        }
    }
}
