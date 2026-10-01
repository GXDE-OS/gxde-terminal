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

using Cairo;
using Draw;
using GLib;
using Gee;
using Gtk;
using Utils;
using Widgets;

namespace Widgets {
    public class Tabbar : Gtk.DrawingArea {
        public Gdk.RGBA tab_split_dark_color;
        public Gdk.RGBA tab_split_light_color;
        public HashMap<int, bool> tab_highlight_map;
        private Cairo.ImageSurface add_hover_dark_surface;
        private Cairo.ImageSurface add_hover_light_surface;
        private Cairo.ImageSurface add_normal_dark_surface;
        private Cairo.ImageSurface add_normal_light_surface;
        private Cairo.ImageSurface add_press_dark_surface;
        private Cairo.ImageSurface add_press_light_surface;
        private Cairo.ImageSurface close_hover_surface;
        private Cairo.ImageSurface close_normal_surface;
        private Cairo.ImageSurface close_press_surface;
        private bool draw_hover = false;
        private bool is_button_press = false;
        private bool is_dragging_tab = false;
        private bool press_on_tab_body = false;
        private int drag_tab_index = -1;
        private int drag_tab_id = -1;
        private int drag_start_x = 0;
        private const int DRAG_THRESHOLD = 4;
        private HashMap<int, double?> tab_current_x = new HashMap<int, double?>();
        private HashMap<int, double?> tab_target_x = new HashMap<int, double?>();
        private bool tab_animating = false;
        private double tab_anim_last_time = 0;
        private double drag_grab_offset = 0;
        private const double TAB_ANIM_TAU = 0.05;
        private const double TAB_ANIM_THRESHOLD = 0.5;
        private double draw_scale = 1.0;
        private int add_button_width = 50;
        private int button_press_x = 0;
        private int button_press_y = 0;
        private int close_button_padding_x = 28;
        private int hover_x = 0;
        private int tab_min_width = 80;
        private int tab_split_width = 1;
        private int text_padding_x = 20;
        public ArrayList<int> tab_list;
        public Gdk.RGBA hover_arrow_color;
        public Gdk.RGBA inactive_arrow_color;
        public Gdk.RGBA tab_text_color;
        public Gdk.RGBA text_active_color;
        public Gdk.RGBA text_dark_color;
        public Gdk.RGBA text_highlight_color;
        public Gdk.RGBA text_hover_dark_color;
        public Gdk.RGBA text_hover_light_color;
        public Gdk.RGBA text_light_color;
        private Gdk.RGBA tabbar_bg_color = Gdk.RGBA();
        public HashMap<int, string> tab_name_map;
        public Pango.FontDescription font_description;
        public bool allowed_add_tab = true;
        private Menu.Menu context_menu;
        public int font_size = 11;
        public int height = Constant.TITLEBAR_HEIGHT;
        public int hover_clip_right_offset = 6;
        public int min_tab_width = 70;
        public int tab_index = 0;

        public bool is_pressing_tab {
            get { return press_on_tab_body; }
        }

        public signal void press_tab(int tab_index, int tab_id);
        public signal void update_tab_underline(int x, int width);
        public signal void close_tab(int tab_index, int tab_id);
        public signal void new_tab();

        public Tabbar() {
            Intl.bindtextdomain(GETTEXT_PACKAGE, "/usr/share/locale");

            add_events (Gdk.EventMask.BUTTON_PRESS_MASK
                        | Gdk.EventMask.BUTTON_RELEASE_MASK
                        | Gdk.EventMask.POINTER_MOTION_MASK
                        | Gdk.EventMask.LEAVE_NOTIFY_MASK);

            tab_list = new ArrayList<int>();
            tab_name_map = new HashMap<int, string>();
            tab_highlight_map = new HashMap<int, bool>();

            font_description = new Pango.FontDescription();
            font_description.set_size((int)(font_size * Pango.SCALE));

            set_size_request(-1, height);

            close_normal_surface = Utils.create_image_surface("tab_close_normal.svg");
            close_hover_surface = Utils.create_image_surface("tab_close_hover.svg");
            close_press_surface = Utils.create_image_surface("tab_close_press.svg");

            add_normal_dark_surface = Utils.create_image_surface("tab_add_dark_normal.svg");
            add_hover_dark_surface = Utils.create_image_surface("tab_add_dark_hover.svg");
            add_press_dark_surface = Utils.create_image_surface("tab_add_dark_press.svg");
            add_normal_light_surface = Utils.create_image_surface("tab_add_light_normal.svg");
            add_hover_light_surface = Utils.create_image_surface("tab_add_light_hover.svg");
            add_press_light_surface = Utils.create_image_surface("tab_add_light_press.svg");

            inactive_arrow_color = Utils.hex_to_rgba("#393937");
            hover_arrow_color = Utils.hex_to_rgba("#494943");
            text_hover_dark_color = Utils.hex_to_rgba("#ffffff");
            text_hover_light_color = Utils.hex_to_rgba("#000000");
            text_dark_color = Utils.hex_to_rgba("#ffffff", 0.8);
            text_light_color = Utils.hex_to_rgba("#000000", 0.8);
            text_highlight_color = Utils.hex_to_rgba("#ff9600");
            tab_split_dark_color = Utils.hex_to_rgba("#ffffff", 0.05);
            tab_split_light_color = Utils.hex_to_rgba("#000000", 0.05);
            text_active_color = Gdk.RGBA();
            tab_text_color = Gdk.RGBA();

            draw.connect(on_draw);
            configure_event.connect(on_configure);
            button_press_event.connect(on_button_press);
            button_release_event.connect(on_button_release);
            motion_notify_event.connect(on_motion_notify);
            leave_notify_event.connect(on_leave_notify);
        }

        public void init(WorkspaceManager workspace_manager, Widgets.ConfigWindow window) {
            press_tab.connect((t, tab_index, tab_id) => {
                    unhighlight_tab(tab_id);
                    workspace_manager.switch_workspace(tab_id);
                });

            close_tab.connect((t, tab_index, tab_id) => {
                    Widgets.Workspace focus_workspace = workspace_manager.workspace_map.get(tab_id);
                    if (focus_workspace.has_active_term()) {
                        ConfirmDialog dialog;
                        dialog = Widgets.create_running_confirm_dialog(window);

                        dialog.confirm.connect((d) => {
                                destroy_tab(tab_index);
                                workspace_manager.remove_workspace(tab_id);
                            });
                    } else {
                        destroy_tab(tab_index);
                        workspace_manager.remove_workspace(tab_id);
                    }
                });

            new_tab.connect((t) => {
                    workspace_manager.new_workspace_with_current_directory();
                });
        }

        public void reset() {
            tab_list = new ArrayList<int>();
            tab_name_map = new HashMap<int, string>();
            tab_current_x.clear();
            tab_target_x.clear();
            tab_index = 0;
        }

        public void add_tab(string tab_name, int tab_id) {
            tab_list.add(tab_id);
            tab_name_map.set(tab_id, tab_name);

            reconcile_tab_positions(true);
        }

        public void rename_tab(int tab_id, string tab_name) {
            tab_name_map.set(tab_id, tab_name);

            if (is_focus_tab(tab_id)) {
                update_window_title(tab_name);
            }

            reconcile_tab_positions(false);
        }

        public void highlight_tab(int tab_id) {
            if (!tab_highlight_map.has_key(tab_id)) {
                tab_highlight_map.set(tab_id, true);

                queue_draw();
            }
        }

        public void unhighlight_tab(int tab_id) {
            if (tab_highlight_map.has_key(tab_id)) {
                tab_highlight_map.unset(tab_id);

                queue_draw();
            }
        }

        public bool is_focus_tab(int tab_id) {
            int? index = tab_list.index_of(tab_id);
            if (index != null) {
                return tab_index == index;
            } else {
                return false;
            }
        }

        public void select_next_tab() {
            var index = tab_index + 1;
            if (index >= tab_list.size) {
                index = 0;
            }
            switch_tab(index);
        }

        public void select_previous_tab() {
            var index = tab_index - 1;
            if (index < 0) {
                index = tab_list.size - 1;
            }
            switch_tab(index);
        }

        public void select_first_tab() {
            switch_tab(0);
        }

        public void select_end_tab() {
            var index = 0;
            if (tab_list.size == 0) {
                index = 0;
            } else {
                index = tab_list.size - 1;
            }
            switch_tab(index);
        }

        public void select_nth_tab(int index) {
            switch_tab(index);
        }

        public void select_tab_with_id(int tab_id) {
            switch_tab(tab_list.index_of(tab_id));
        }

        public void close_current_tab() {
            close_nth_tab(tab_index);
        }

        public void close_nth_tab(int index) {
            if (tab_list.size > 0) {
                var tab_id = tab_list.get(index);
                close_tab(index, tab_id);
            }
        }

        public void close_other_tabs(int keep_index) {
            if (tab_list.size < 2) {
                return;
            }

            int keep_id = tab_list.get(keep_index);
            var ids_to_close = new ArrayList<int>();
            foreach (int id in tab_list) {
                if (id != keep_id) {
                    ids_to_close.add(id);
                }
            }

            foreach (int id in ids_to_close) {
                int index = tab_list.index_of(id);
                if (index >= 0) {
                    close_tab(index, id);
                }
            }
        }

        private void show_tab_menu(Gdk.Event event, int tab_index, int tab_id) {
            var menu_content = new GLib.List<Menu.MenuItem>();
            menu_content.append(new Menu.MenuItem("close_tab", _("Close tab")));
            menu_content.append(new Menu.MenuItem("close_other_tabs", _("Close other tabs"), false, false, tab_list.size >= 2));
            menu_content.append(new Menu.MenuItem("rename_tab", _("Rename tab")));

            context_menu = new Menu.Menu();
            context_menu.click_item.connect((item_id) => handle_tab_menu_click(item_id, tab_index, tab_id));
            context_menu.destroy.connect(() => { context_menu = null; });
            context_menu.popup_at_pointer(menu_content, this, event);
        }

        private void handle_tab_menu_click(string item_id, int tab_index, int tab_id) {
            switch (item_id) {
                case "close_tab":
                    close_nth_tab(tab_index);
                    break;
                case "close_other_tabs":
                    close_other_tabs(tab_index);
                    break;
                case "rename_tab":
                    rename_tab_dialog(tab_index, tab_id);
                    break;
            }
        }

        private void rename_tab_dialog(int tab_index, int tab_id) {
            var window = (Widgets.ConfigWindow) get_toplevel();
            string current_name = tab_name_map.get(tab_id);

            var rename_dialog = new Widgets.RenameDialog(
                _("Rename tab"),
                current_name,
                _("Cancel"),
                _("Rename")
            );
            rename_dialog.transient_for_window(window);
            rename_dialog.rename.connect((w, new_title) => {
                rename_tab(tab_id, new_title.strip());
            });
        }

        public void destroy_tab(int index) {
            var tab_id = tab_list.get(index);

            tab_list.remove_at(index);
            tab_name_map.unset(tab_id);
            tab_current_x.unset(tab_id);
            tab_target_x.unset(tab_id);

            if (tab_list.size == 0) {
                tab_index = 0;
            } else if (tab_index >= tab_list.size) {
                tab_index = tab_list.size - 1;
            }

            reconcile_tab_positions(true);
        }

        public bool on_configure(Gtk.Widget widget, Gdk.EventConfigure event) {
            reconcile_tab_positions(false);

            return false;
        }

        public bool on_button_press(Gtk.Widget widget, Gdk.EventButton event) {
            is_button_press = true;

            event.device.get_position(null, out button_press_x, out button_press_y);

            if (event.button == Gdk.BUTTON_SECONDARY) {
                int tab_index = get_tab_index_at_x((int) event.x);
                if (tab_index != -1) {
                    show_tab_menu(event, tab_index, tab_list.get(tab_index));
                    return true;
                }
            }

            return false;
        }

        public bool on_button_release(Gtk.Widget widget, Gdk.EventButton event) {
            is_button_press = false;

            if (is_action_mouse_button(event)) {
                int button_release_x, button_release_y;
                event.device.get_position(null, out button_release_x, out button_release_y);

                if (button_release_x == button_press_x && button_release_y == button_press_y) {
                    var release_x = (int)event.x;

                    Gtk.Allocation alloc;
                    widget.get_allocation(out alloc);

                    int draw_x = 0;
                    int counter = 0;
                    foreach (int tab_id in tab_list) {
                        int name_width, name_height;
                        get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                        int tab_width = get_tab_width(name_width);

                        if (release_x > draw_x && release_x < draw_x + tab_width) {
                            if (release_x > draw_x && release_x < draw_x + tab_width - get_tab_close_button_padding()) {
                                if(is_left_button(event)) {
                                    select_nth_tab(counter);
    
                                    press_tab(counter, tab_id);
                                    return false;
                                }

                                if(is_mouse_wheel(event)) {
                                    close_nth_tab(counter);
                                    return false;
                                }                                
                            } else if (release_x > draw_x + tab_width - get_tab_close_button_padding()) {
                                close_nth_tab(counter);
                                return false;
                            }
                        }

                        draw_x += tab_width;

                        counter++;
                    }

                    if (release_x > draw_x && release_x < draw_x + add_button_width) {
                        new_tab();
                    }

                    queue_draw();
                }
            }

            return false;
        }

        public int is_at_tab_close_button(int x) {
            Gtk.Allocation alloc;
            this.get_allocation(out alloc);

            int draw_x = 0;
            int counter = 0;
            foreach (int tab_id in tab_list) {
                int name_width, name_height;
                get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                int tab_width = get_tab_width(name_width);

                if (x > draw_x && x < draw_x + tab_width) {
                    if (x > draw_x + tab_width - get_tab_close_button_padding()) {
                        return counter;
                    }
                }

                draw_x += tab_width;

                counter++;
            }

            return -1;
        }

        public int get_tab_index_at_x(int x) {
            int draw_x = 0;
            int counter = 0;
            foreach (int tab_id in tab_list) {
                int name_width, name_height;
                get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                int tab_width = get_tab_width(name_width);

                if (x > draw_x && x < draw_x + tab_width - get_tab_close_button_padding()) {
                    return counter;
                }

                draw_x += tab_width;
                counter++;
            }

            return -1;
        }

        public bool try_start_tab_drag(int x) {
            drag_start_x = x;
            drag_tab_index = get_tab_index_at_x(x);
            press_on_tab_body = drag_tab_index != -1;
            if (press_on_tab_body) {
                drag_tab_id = tab_list.get(drag_tab_index);

                double left = 0;
                for (int i = 0; i < drag_tab_index; i++) {
                    int nw, nh;
                    get_text_size(tab_name_map.get(tab_list.get(i)), out nw, out nh);
                    left += get_tab_width(nw);
                }
                drag_grab_offset = x - left;
            } else {
                drag_tab_id = -1;
            }
            is_dragging_tab = false;

            return press_on_tab_body;
        }

        public void end_tab_drag() {
            if (is_dragging_tab && drag_tab_id != -1) {
                int nw, nh;
                get_text_size(tab_name_map.get(drag_tab_id), out nw, out nh);
                double w = get_tab_width(nw);
                double left = clamp_drag_left(hover_x - drag_grab_offset, w);
                tab_current_x.set(drag_tab_id, left);
            }
            is_dragging_tab = false;
            press_on_tab_body = false;
            drag_tab_index = -1;
            drag_tab_id = -1;
            start_tab_animation();
            queue_draw();
        }

        public void reorder_dragged_tab(int x) {
            if (drag_tab_id == -1) {
                return;
            }

            int? current = tab_list.index_of(drag_tab_id);
            if (current == null) {
                return;
            }

            is_dragging_tab = true;

            tab_list.remove_at((int) current);

            int draw_x = 0;
            int target = tab_list.size;
            for (int i = 0; i < tab_list.size; i++) {
                int tab_id = tab_list.get(i);
                string? name = tab_name_map.get(tab_id);
                int name_width, name_height;
                get_text_size(name != null ? name : "", out name_width, out name_height);
                int tab_width = get_tab_width(name_width);

                if (x < draw_x + tab_width / 2) {
                    target = i;
                    break;
                }

                draw_x += tab_width;
            }

            tab_list.insert(target, drag_tab_id);

            tab_index = (int) tab_list.index_of(drag_tab_id);

            reconcile_tab_positions(true);
        }

        public bool on_motion_notify(Gtk.Widget widget, Gdk.EventMotion event) {
            draw_hover = true;
            hover_x = (int) event.x;

            if (press_on_tab_body && drag_tab_id != -1) {
                int dx = (int) event.x - drag_start_x;
                if (!is_dragging_tab && (dx < 0 ? -dx : dx) < DRAG_THRESHOLD) {
                    queue_draw();
                    return false;
                }

                is_dragging_tab = true;
                reorder_dragged_tab((int) event.x);
            }

            queue_draw();

            return false;
        }

        public bool on_leave_notify(Gtk.Widget widget, Gdk.EventCrossing event) {
            draw_hover = false;
            hover_x = 0;

            queue_draw();

            return false;
        }

        public void update_tab_scale() {
            Gtk.Allocation alloc;
            this.get_allocation(out alloc);

            int tab_width = 0;
            foreach (int tab_id in tab_list) {
                int name_width, name_height;
                get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);

                tab_width += get_tab_render_width(name_width);
            }

            if (tab_width + add_button_width > alloc.width) {
                // FIXME: I know 0.97 is magic number, this number avoid add_button render out of area of tabbar.
                // Welcome to fix this.
                draw_scale = (double) alloc.width / (tab_width + add_button_width) * 0.97;
            } else {
                draw_scale = 1.0;
            }
        }

        public bool on_draw(Gtk.Widget widget, Cairo.Context cr) {
            Gtk.Allocation alloc;
            widget.get_allocation(out alloc);

            tabbar_bg_color = get_style_context().get_background_color(Gtk.StateFlags.NORMAL);

            bool is_light_theme = ((Widgets.ConfigWindow) get_toplevel()).is_light_theme();

            update_tab_targets();

            // Draw tab splitters at each tab's animated position.
            foreach (int tab_id in tab_list) {
                if (is_dragging_tab && tab_id == drag_tab_id) {
                    continue;
                }

                double drawn_x = get_drawn_x(tab_id);
                if (is_light_theme) {
                    Utils.set_context_color(cr, tab_split_light_color);
                } else {
                    Utils.set_context_color(cr, tab_split_dark_color);
                }
                Draw.draw_rectangle(cr, (int) drawn_x, 0, tab_split_width, height);
            }

            try {
                text_active_color = Utils.hex_to_rgba(((Widgets.ConfigWindow) this.get_toplevel()).config.config_file.get_string("theme", "tab"));
            } catch (Error e) {
                print("Tabbar draw: %s\n", e.message);
            }

            int max_tab_width = 0;
            int max_tab_height = 0;
            foreach (int tab_id in tab_list) {
                int name_width, name_height;
                var layout = get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                int tab_width = get_tab_width(name_width);

                max_tab_height = int.max(max_tab_height, name_height);
                max_tab_width = int.max(max_tab_width, tab_width);

                if (is_dragging_tab && tab_id == drag_tab_id) {
                    continue;
                }

                bool is_active = (tab_index == tab_list.index_of(tab_id));
                draw_tab_body(cr, tab_id, (int) get_drawn_x(tab_id), tab_width, alloc, is_light_theme, layout, max_tab_height, is_active, false);
            }

            // Draw the dragged tab on top so it can follow the cursor smoothly.
            if (is_dragging_tab) {
                int tab_id = drag_tab_id;
                int name_width, name_height;
                var layout = get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                int tab_width = get_tab_width(name_width);
                bool is_active = (tab_index == tab_list.index_of(tab_id));
                draw_tab_body(cr, tab_id, (int) get_drawn_x(tab_id), tab_width, alloc, is_light_theme, layout, max_tab_height, is_active, true);
            }

            // Don't allowed add tab when scale too small.
            allowed_add_tab = max_tab_width > min_tab_width || draw_scale >= 1.0;

            int draw_x = 0;
            if (tab_list.size > 0) {
                int last_id = tab_list.get(tab_list.size - 1);
                int lnw, lnh;
                get_text_size(tab_name_map.get(last_id), out lnw, out lnh);
                int last_tab_width = get_tab_width(lnw);
                draw_x = (int) ((tab_target_x.get(last_id) ?? 0.0) + last_tab_width);
            }

            if (hover_x > draw_x && hover_x < draw_x + add_button_width) {
                if (is_button_press) {
                    if (is_light_theme) {
                        Draw.draw_surface(cr, add_press_light_surface, draw_x, 0, 0, height);
                    } else {
                        Draw.draw_surface(cr, add_press_dark_surface, draw_x, 0, 0, height);
                    }
                } else if (draw_hover) {
                    if (is_light_theme) {
                        Draw.draw_surface(cr, add_hover_light_surface, draw_x, 0, 0, height);
                    } else {
                        Draw.draw_surface(cr, add_hover_dark_surface, draw_x, 0, 0, height);
                    }
                }
            } else {
                if (is_light_theme) {
                    Draw.draw_surface(cr, add_normal_light_surface, draw_x, 0, 0, height);
                } else {
                    Draw.draw_surface(cr, add_normal_dark_surface, draw_x, 0, 0, height);
                }
            }

            return true;
        }

        private double clamp_drag_left(double raw_left, double w) {
            double layout_right = w;
            if (tab_list.size > 0) {
                int last_id = tab_list.get(tab_list.size - 1);
                int lnw, lnh;
                get_text_size(tab_name_map.get(last_id), out lnw, out lnh);
                double last_w = get_tab_width(lnw);
                double? t = tab_target_x.get(last_id);
                layout_right = (t ?? 0.0) + last_w;
            }
            double right = double.max(0, layout_right - w);
            return double.max(0, double.min(raw_left, right));
        }

        private double get_drawn_x(int tab_id) {
            if (is_dragging_tab && tab_id == drag_tab_id) {
                int name_width, name_height;
                get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                double w = get_tab_width(name_width);
                return clamp_drag_left(hover_x - drag_grab_offset, w);
            }

            double? current = tab_current_x.get(tab_id);
            if (current == null) {
                double? target = tab_target_x.get(tab_id);
                double t = target ?? 0.0;
                tab_current_x.set(tab_id, t);
                return t;
            }
            return current;
        }

        private void update_tab_targets() {
            double x = 0;
            foreach (int tab_id in tab_list) {
                int name_width, name_height;
                get_text_size(tab_name_map.get(tab_id), out name_width, out name_height);
                double tw = get_tab_width(name_width);
                tab_target_x.set(tab_id, x);
                if (!tab_current_x.has_key(tab_id)) {
                    tab_current_x.set(tab_id, x);
                }
                x += tw;
            }
        }

        private void reconcile_tab_positions(bool animate) {
            update_tab_scale();
            update_tab_targets();

            if (animate) {
                start_tab_animation();
            } else {
                foreach (int tab_id in tab_list) {
                    double? t = tab_target_x.get(tab_id);
                    tab_current_x.set(tab_id, t ?? 0.0);
                }
            }

            queue_draw();
        }

        private void start_tab_animation() {
            update_tab_targets();
            if (!tab_animating) {
                tab_animating = true;
                tab_anim_last_time = 0;
                add_tick_callback(on_tab_anim_frame);
            }
        }

        private bool on_tab_anim_frame(Gtk.Widget widget, Gdk.FrameClock frame_clock) {
            double now = (double) frame_clock.get_frame_time() / 1000000.0;
            double dt = now - tab_anim_last_time;
            tab_anim_last_time = now;
            if (dt <= 0) {
                dt = 0.016;
            } else if (dt > 0.05) {
                dt = 0.05;
            }

            double k = 1.0 - Math.exp(-dt / TAB_ANIM_TAU);
            bool active = false;
            foreach (int tab_id in tab_list) {
                if (is_dragging_tab && tab_id == drag_tab_id) {
                    continue;
                }

                double? target = tab_target_x.get(tab_id);
                if (target == null) {
                    continue;
                }
                double tgt = target ?? 0.0;

                double current = tab_current_x.get(tab_id) ?? 0.0;
                double next = current + (tgt - current) * k;
                if (Math.fabs(next - tgt) > TAB_ANIM_THRESHOLD) {
                    active = true;
                } else {
                    next = tgt;
                }
                tab_current_x.set(tab_id, next);
            }

            queue_draw();

            if (!active) {
                tab_animating = false;
                return false;
            }
            return true;
        }

        private void draw_tab_body(Cairo.Context cr, int tab_id, int dx, int tab_width,
                                   Gtk.Allocation alloc, bool is_light_theme,
                                   Pango.Layout layout, int max_tab_height,
                                   bool is_active, bool is_dragged) {
            if (tab_highlight_map.has_key(tab_id)) {
                tab_text_color = text_highlight_color;
            } else {
                tab_text_color = is_light_theme ? text_light_color : text_dark_color;
            }

            if (is_dragged) {
                // Opaque background so the dragged tab covers the tabs beneath it
                // instead of letting their text overlap with the dragged tab.
                tabbar_bg_color.alpha = 1.0;
                Utils.set_context_color(cr, tabbar_bg_color);
                Draw.draw_rectangle(cr, dx, 0, tab_width, height);
                tab_text_color = text_active_color;
            } else if (is_active) {
                tab_text_color = text_active_color;
            } else if (draw_hover && hover_x > dx && hover_x < dx + tab_width) {
                cr.save();
                clip_rectangle(cr, dx, 0, tab_width + 1, height);
                Utils.set_context_color(cr, is_light_theme ? tab_split_light_color : tab_split_dark_color);
                Draw.draw_rectangle(cr, dx, 0, tab_width + 1, height);
                cr.restore();
                tab_text_color = is_light_theme ? text_hover_light_color : text_hover_dark_color;
            } else {
                cr.set_source_rgba(0, 0, 0, 0);
                Draw.draw_rectangle(cr, dx, 0, tab_width, height);
            }

            if (is_active || is_dragged) {
                cr.save();
                clip_rectangle(cr, dx, 0, tab_width, height);
                update_tab_underline(dx + 1, tab_width - 1);
                cr.restore();
            }

            if (!is_dragged && draw_hover) {
                if (hover_x > dx && hover_x < dx + tab_width) {
                    if (hover_x > dx + tab_width - get_tab_close_button_padding()) {
                        if (is_button_press) {
                            Draw.draw_surface(cr, close_press_surface, dx + tab_width - get_tab_close_button_padding(), 0, 0, height);
                        } else {
                            Draw.draw_surface(cr, close_hover_surface, dx + tab_width - get_tab_close_button_padding(), 0, 0, height);
                        }
                    } else {
                        Draw.draw_surface(cr, close_normal_surface, dx + tab_width - get_tab_close_button_padding(), 0, 0, height);
                    }
                }
            }

            cr.save();
            clip_rectangle(cr, dx + get_tab_text_padding(), 0, tab_width - get_tab_text_padding() * 2, height);

            Utils.set_context_color(cr, tab_text_color);
            bool is_hover = draw_hover && !is_dragged && hover_x > dx && hover_x < dx + tab_width;
            int text_render_y = (alloc.height - max_tab_height) / 2;
            if (is_hover) {
                cr.rectangle(dx, text_render_y, tab_width - get_tab_close_button_padding() - hover_clip_right_offset, height);
                cr.clip();
            }
            Draw.draw_layout(cr, layout, dx + get_tab_text_padding(), text_render_y);
            cr.restore();
        }

        public int get_tab_render_width(int name_width) {
            return int.max(name_width + get_tab_text_padding() * 2, tab_min_width);
        }

        public int get_tab_width(int name_width) {
            return int.max((int) ((name_width + get_tab_text_padding() * 2) * draw_scale), (int) (tab_min_width * draw_scale));
        }

        public int get_tab_text_padding() {
            return text_padding_x;
        }

        public int get_tab_close_button_padding() {
            return close_button_padding_x;
        }

        public void switch_tab(int new_index) {
            tab_index = new_index;

            press_tab(tab_index, tab_list.get(tab_index));

            queue_draw();
        }

        public Pango.Layout get_text_size(string text, out int width, out int height) {
            var layout = create_pango_layout(text);
            layout.set_font_description(font_description);
            layout.get_pixel_size(out width, out height);

            return layout;
        }

        public void update_window_title(string title) {
            ((Gtk.Window) get_toplevel()).set_title("%s - %s".printf(title, _("Deepin Terminal")));
        }
    }
}
