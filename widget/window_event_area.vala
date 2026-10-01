/* -*- Mode: Vala; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *               2011 ~ 2018 Wang Yong
 *               2019 ~ 2020 Gary Wang
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 *             Gary Wang <wzc782970009@gmail.com>
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
using Widgets;

namespace Widgets {
    public class WindowEventArea : Gtk.EventBox {
        public FilterDoubleClick? filter_double_click_callback = null;
        public FilterDoubleClick? filter_move_callback = null;
        public Widgets.Tabbar? drag_tabbar = null;
        private bool is_tab_drag_candidate = false;
        private bool is_tab_dragging = false;
        public Gtk.Container drawing_area;
        public Gtk.Widget? child_before_leave;
        public bool is_double_clicked = false;
        public bool is_press = false;
        public double press_x = 0;
        public double press_y = 0;
        public int double_clicked_max_delay = 150;

        public delegate bool FilterDoubleClick(int x, int y);

        public WindowEventArea(Gtk.Container area) {
            drawing_area = area;

            visible_window = false;

            add_events(Gdk.EventMask.BUTTON_PRESS_MASK
                       | Gdk.EventMask.BUTTON_RELEASE_MASK
                       | Gdk.EventMask.POINTER_MOTION_MASK
                       | Gdk.EventMask.LEAVE_NOTIFY_MASK);

            leave_notify_event.connect((w, e) => {
                    if (child_before_leave != null) {
                        var e2 = e.copy();
                        e2.crossing.window = child_before_leave.get_window();

                        child_before_leave.get_window().ref();
                        ((Gdk.Event*) e2)->put();

                        child_before_leave = null;
                    }

                    return false;
            });

            motion_notify_event.connect((w, e) => {
                    var child = get_child_at_pos(drawing_area, (int) e.x, (int) e.y);
                    child_before_leave = child;

                    if (child != null) {
                        int x, y;
                        drawing_area.translate_coordinates(child, (int) e.x, (int) e.y, out x, out y);

                        Gdk.EventMotion* event;
                        event = (Gdk.EventMotion) new Gdk.Event(Gdk.EventType.MOTION_NOTIFY);
                        event->window = child.get_window();
                        event->send_event = 1;
                        event->time = e.time;
                        event->x = x;
                        event->y = y;
                        event->x_root = e.x_root;
                        event->y_root = e.y_root;
                        event->state = e.state;
                        event->is_hint = e.is_hint;
                        event->device = e.device;
                        event->axes = e.axes;
                        ((Gdk.Event*) event)->put();
                    }

                    // Backup driver: while a tab drag is in progress, reorder the
                    // tabbar based on the current pointer position. The tabbar's own
                    // motion handler also does this; keeping this guarantees the
                    // reorder works even if forwarded motion is not delivered.
                    if (is_tab_drag_candidate && drag_tabbar != null && is_press) {
                        int lx, ly;
                        this.translate_coordinates(drag_tabbar, (int) e.x, (int) e.y, out lx, out ly);
                        drag_tabbar.reorder_dragged_tab(lx);
                        is_tab_dragging = true;
                    }

                    return true;
                });

            button_press_event.connect((w, e) => {
                    is_press = true;
                    is_tab_drag_candidate = false;
                    is_tab_dragging = false;

                    e.device.get_position(null, out press_x, out press_y);

                    GLib.Timeout.add(10, () => {
                        // blumia: should use begin_move_drag instead of send event to X
                        //         so it should also works under wayland :)
                        if (is_press) {
                            int pointer_x, pointer_y;
                            e.device.get_position(null, out pointer_x, out pointer_y);

                            if (pointer_x != press_x || pointer_y != press_y) {
                                // Block window movement when the user is dragging a tab
                                // in the tabbar so that the gesture reorders tabs instead.
                                if (is_tab_drag_candidate || (filter_move_callback != null && filter_move_callback((int) e.x, (int) e.y))) {
                                    return false;
                                }
                                Utils.move_window(this, e);
                                return false;
                            } else {
                                return true;
                            }
                        } else {
                            return false;
                        }
                    });


                    var child = get_child_at_pos(drawing_area, (int) e.x, (int) e.y);
                    if (child != null) {
                        int x, y;
                        drawing_area.translate_coordinates(child, (int) e.x, (int) e.y, out x, out y);

                        Gdk.EventButton* event;
                        event = (Gdk.EventButton) new Gdk.Event(Gdk.EventType.BUTTON_PRESS);
                        event->window = child.get_window();
                        event->send_event = 1;
                        event->time = e.time;
                        event->x = x;
                        event->y = y;
                        event->x_root = e.x_root;
                        event->y_root = e.y_root;
                        event->state = e.state;
                        event->device = e.device;
                        event->button = e.button;
                        ((Gdk.Event*) event)->put();
                    }

                    // Mark a potential tab drag if the press started on a tab body.
                    if (drag_tabbar != null) {
                        int lx, ly;
                        this.translate_coordinates(drag_tabbar, (int) e.x, (int) e.y, out lx, out ly);
                        is_tab_drag_candidate = drag_tabbar.try_start_tab_drag(lx);
                    }

                    if (e.type == Gdk.EventType.BUTTON_PRESS) {
                        is_double_clicked = true;

                        // Add timeout to avoid long-long-long time double clicked to cause toggle maximize action.
                        GLib.Timeout.add(double_clicked_max_delay, () => {
                                is_double_clicked = false;

                                return false;
                            });
                    } else if (e.type == Gdk.EventType.2BUTTON_PRESS) {
                        if (is_double_clicked && Utils.is_left_button(e)) {
                            if (filter_double_click_callback == null || !filter_double_click_callback((int) e.x, (int) e.y)) {
                                if (this.get_toplevel().get_type().is_a(typeof(Widgets.Window))) {
                                    ((Widgets.Window) this.get_toplevel()).toggle_max();
                                }
                            }
                        }
                    }

                    return true;
                });

            button_release_event.connect((w, e) => {
                    is_press = false;

                    // Finalize a tab drag if one was in progress.
                    if (drag_tabbar != null) {
                        drag_tabbar.end_tab_drag();
                    }
                    is_tab_drag_candidate = false;
                    is_tab_dragging = false;

                    var child = get_child_at_pos(drawing_area, (int) e.x, (int) e.y);
                    if (child != null) {
                        int x, y;
                        drawing_area.translate_coordinates(child, (int) e.x, (int) e.y, out x, out y);

                        Gdk.EventButton* event;
                        event = (Gdk.EventButton) new Gdk.Event(Gdk.EventType.BUTTON_RELEASE);
                        event->window = child.get_window();
                        event->send_event = 1;
                        event->time = e.time;
                        event->x = x;
                        event->y = y;
                        event->x_root = e.x_root;
                        event->y_root = e.y_root;
                        event->state = e.state;
                        event->device = e.device;
                        event->button = e.button;
                        ((Gdk.Event*) event)->put();
                    }

                    return true;
                });
        }

        public Gtk.Widget? get_child_at_pos(Gtk.Container container, int x, int y) {
            if (container.get_children().length() > 0) {
                foreach (Gtk.Widget child in container.get_children()) {
                    Gtk.Allocation child_rect;
                    child.get_allocation(out child_rect);

                    int child_x, child_y;
                    child.translate_coordinates(container, 0, 0, out child_x, out child_y);

                    if (x >= child_x && x <= child_x + child_rect.width && y >= child_y && y <= child_y + child_rect.height) {
                        if (child.get_type().is_a(typeof(Gtk.Container))) {
                            return get_child_at_pos((Gtk.Container) child, x - child_x, y - child_y);
                        } else {
                            return child;
                        }
                    }
                }
            }

            return null;
        }
    }
}
