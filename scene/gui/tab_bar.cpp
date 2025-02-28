/*************************************************************************/
/*  tab_bar.cpp                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "tab_bar.h"

#include "core/object/message_queue.h"
#include "core/string/translation.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"

Size2 TabBar::get_minimum_size() const {
	Size2 ms;

	if (tabs.is_empty()) {
		return ms;
	}

	Ref<StyleBox> tab_unselected = get_theme_stylebox(SNAME("tab_unselected"));
	Ref<StyleBox> tab_selected = get_theme_stylebox(SNAME("tab_selected"));
	Ref<StyleBox> tab_disabled = get_theme_stylebox(SNAME("tab_disabled"));
	Ref<StyleBox> button_highlight = get_theme_stylebox(SNAME("button_highlight"));
	Ref<Texture2D> close = get_theme_icon(SNAME("close"));
	int hseparation = get_theme_constant(SNAME("hseparation"));

	int y_margin = MAX(MAX(tab_unselected->get_minimum_size().height, tab_selected->get_minimum_size().height), tab_disabled->get_minimum_size().height);

	for (int i = 0; i < tabs.size(); i++) {
		if (tabs[i].hidden) {
			continue;
		}

		int ofs = ms.width;

		Ref<StyleBox> style;
		if (tabs[i].disabled) {
			style = tab_disabled;
		} else if (current == i) {
			style = tab_selected;
		} else {
			style = tab_unselected;
		}
		ms.width += style->get_minimum_size().width;

		Ref<Texture2D> tex = tabs[i].icon;
		if (tex.is_valid()) {
			ms.height = MAX(ms.height, tex->get_size().height + y_margin);
			ms.width += tex->get_size().width + hseparation;
		}

		if (!tabs[i].text.is_empty()) {
			ms.width += tabs[i].size_text + hseparation;
		}
		ms.height = MAX(ms.height, tabs[i].text_buf->get_size().y + y_margin);

		bool close_visible = cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current);

		if (tabs[i].right_button.is_valid()) {
			Ref<Texture2D> rb = tabs[i].right_button;

			if (close_visible) {
				ms.width += button_highlight->get_minimum_size().width + rb->get_width();
			} else {
				ms.width += button_highlight->get_margin(SIDE_LEFT) + rb->get_width() + hseparation;
			}

			ms.height = MAX(ms.height, rb->get_height() + y_margin);
		}

		if (close_visible) {
			ms.width += button_highlight->get_margin(SIDE_LEFT) + close->get_width() + hseparation;

			ms.height = MAX(ms.height, close->get_height() + y_margin);
		}

		if (ms.width - ofs > style->get_minimum_size().width) {
			ms.width -= hseparation;
		}
	}

	if (clip_tabs) {
		ms.width = 0;
	}

	return ms;
}

void TabBar::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Point2 pos = mm->get_position();

		if (buttons_visible) {
			Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
			Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));

			if (is_layout_rtl()) {
				if (pos.x < decr->get_width()) {
					if (highlight_arrow != 1) {
						highlight_arrow = 1;
						update();
					}
				} else if (pos.x < incr->get_width() + decr->get_width()) {
					if (highlight_arrow != 0) {
						highlight_arrow = 0;
						update();
					}
				} else if (highlight_arrow != -1) {
					highlight_arrow = -1;
					update();
				}
			} else {
				int limit_minus_buttons = get_size().width - incr->get_width() - decr->get_width();
				if (pos.x > limit_minus_buttons + decr->get_width()) {
					if (highlight_arrow != 1) {
						highlight_arrow = 1;
						update();
					}
				} else if (pos.x > limit_minus_buttons) {
					if (highlight_arrow != 0) {
						highlight_arrow = 0;
						update();
					}
				} else if (highlight_arrow != -1) {
					highlight_arrow = -1;
					update();
				}
			}
		}

		_update_hover();
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_UP && !mb->is_command_pressed()) {
			if (scrolling_enabled && buttons_visible) {
				if (offset > 0) {
					offset--;
					_update_cache();
					update();
				}
			}
		}

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_DOWN && !mb->is_command_pressed()) {
			if (scrolling_enabled && buttons_visible) {
				if (missing_right && offset < tabs.size()) {
					offset++;
					_update_cache();
					update();
				}
			}
		}

		if (rb_pressing && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (rb_hover != -1) {
				emit_signal(SNAME("tab_button_pressed"), rb_hover);
			}

			rb_pressing = false;
			update();
		}

		if (cb_pressing && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (cb_hover != -1) {
				emit_signal(SNAME("tab_close_pressed"), cb_hover);
			}

			cb_pressing = false;
			update();
		}

		if (mb->is_pressed() && (mb->get_button_index() == MouseButton::LEFT || (select_with_rmb && mb->get_button_index() == MouseButton::RIGHT))) {
			Point2 pos = mb->get_position();

			if (buttons_visible) {
				Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
				Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));

				if (is_layout_rtl()) {
					if (pos.x < decr->get_width()) {
						if (missing_right) {
							offset++;
							_update_cache();
							update();
						}
						return;
					} else if (pos.x < incr->get_width() + decr->get_width()) {
						if (offset > 0) {
							offset--;
							_update_cache();
							update();
						}
						return;
					}
				} else {
					int limit = get_size().width - incr->get_width() - decr->get_width();
					if (pos.x > limit + decr->get_width()) {
						if (missing_right) {
							offset++;
							_update_cache();
							update();
						}
						return;
					} else if (pos.x > limit) {
						if (offset > 0) {
							offset--;
							_update_cache();
							update();
						}
						return;
					}
				}
			}

			if (tabs.is_empty()) {
				// Return early if there are no actual tabs to handle input for.
				return;
			}

			int found = -1;
			for (int i = offset; i <= max_drawn_tab; i++) {
				if (tabs[i].hidden) {
					continue;
				}

				if (tabs[i].rb_rect.has_point(pos)) {
					rb_pressing = true;
					update();
					return;
				}

				if (tabs[i].cb_rect.has_point(pos) && (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && i == current))) {
					cb_pressing = true;
					update();
					return;
				}

				if (pos.x >= get_tab_rect(i).position.x && pos.x < get_tab_rect(i).position.x + tabs[i].size_cache) {
					if (!tabs[i].disabled) {
						found = i;
					}
					break;
				}
			}

			if (found != -1) {
				set_current_tab(found);

				if (mb->get_button_index() == MouseButton::RIGHT) {
					// Right mouse button clicked.
					emit_signal(SNAME("tab_rmb_clicked"), found);
				}

				emit_signal(SNAME("tab_clicked"), found);
			}
		}
	}
}

void TabBar::_shape(int p_tab) {
	Ref<Font> font = get_theme_font(SNAME("font"));
	int font_size = get_theme_font_size(SNAME("font_size"));

	tabs.write[p_tab].xl_text = atr(tabs[p_tab].text);
	tabs.write[p_tab].text_buf->clear();
	tabs.write[p_tab].text_buf->set_width(-1);
	if (tabs[p_tab].text_direction == Control::TEXT_DIRECTION_INHERITED) {
		tabs.write[p_tab].text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		tabs.write[p_tab].text_buf->set_direction((TextServer::Direction)tabs[p_tab].text_direction);
	}

	tabs.write[p_tab].text_buf->add_string(tabs[p_tab].xl_text, font, font_size, tabs[p_tab].opentype_features, !tabs[p_tab].language.is_empty() ? tabs[p_tab].language : TranslationServer::get_singleton()->get_tool_locale());
}

void TabBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			update();
		} break;

		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			for (int i = 0; i < tabs.size(); ++i) {
				_shape(i);
			}

			[[fallthrough]];
		}
		case NOTIFICATION_RESIZED: {
			int ofs_old = offset;
			int max_old = max_drawn_tab;

			_update_cache();
			_ensure_no_over_offset();

			if (scroll_to_selected && (offset != ofs_old || max_drawn_tab != max_old)) {
				ensure_tab_visible(current);
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (tabs.is_empty()) {
				return;
			}

			Ref<StyleBox> tab_unselected = get_theme_stylebox(SNAME("tab_unselected"));
			Ref<StyleBox> tab_selected = get_theme_stylebox(SNAME("tab_selected"));
			Ref<StyleBox> tab_disabled = get_theme_stylebox(SNAME("tab_disabled"));
			Color font_selected_color = get_theme_color(SNAME("font_selected_color"));
			Color font_unselected_color = get_theme_color(SNAME("font_unselected_color"));
			Color font_disabled_color = get_theme_color(SNAME("font_disabled_color"));
			Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
			Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));
			Ref<Texture2D> incr_hl = get_theme_icon(SNAME("increment_highlight"));
			Ref<Texture2D> decr_hl = get_theme_icon(SNAME("decrement_highlight"));

			bool rtl = is_layout_rtl();
			Vector2 size = get_size();
			int limit_minus_buttons = size.width - incr->get_width() - decr->get_width();

			int ofs = tabs[offset].ofs_cache;

			// Draw unselected tabs in the back.
			for (int i = offset; i <= max_drawn_tab; i++) {
				if (tabs[i].hidden) {
					continue;
				}

				if (i != current) {
					Ref<StyleBox> sb;
					Color col;

					if (tabs[i].disabled) {
						sb = tab_disabled;
						col = font_disabled_color;
					} else if (i == current) {
						sb = tab_selected;
						col = font_selected_color;
					} else {
						sb = tab_unselected;
						col = font_unselected_color;
					}

					_draw_tab(sb, col, i, rtl ? size.width - ofs - tabs[i].size_cache : ofs);
				}

				ofs += tabs[i].size_cache;
			}

			// Draw selected tab in the front, but only if it's visible.
			if (current >= offset && current <= max_drawn_tab && !tabs[current].hidden) {
				Ref<StyleBox> sb = tabs[current].disabled ? tab_disabled : tab_selected;
				float x = rtl ? size.width - tabs[current].ofs_cache - tabs[current].size_cache : tabs[current].ofs_cache;

				_draw_tab(sb, font_selected_color, current, x);
			}

			if (buttons_visible) {
				int vofs = (get_size().height - incr->get_size().height) / 2;

				if (rtl) {
					if (missing_right) {
						draw_texture(highlight_arrow == 1 ? decr_hl : decr, Point2(0, vofs));
					} else {
						draw_texture(decr, Point2(0, vofs), Color(1, 1, 1, 0.5));
					}

					if (offset > 0) {
						draw_texture(highlight_arrow == 0 ? incr_hl : incr, Point2(incr->get_size().width, vofs));
					} else {
						draw_texture(incr, Point2(incr->get_size().width, vofs), Color(1, 1, 1, 0.5));
					}
				} else {
					if (offset > 0) {
						draw_texture(highlight_arrow == 0 ? decr_hl : decr, Point2(limit_minus_buttons, vofs));
					} else {
						draw_texture(decr, Point2(limit_minus_buttons, vofs), Color(1, 1, 1, 0.5));
					}

					if (missing_right) {
						draw_texture(highlight_arrow == 1 ? incr_hl : incr, Point2(limit_minus_buttons + decr->get_size().width, vofs));
					} else {
						draw_texture(incr, Point2(limit_minus_buttons + decr->get_size().width, vofs), Color(1, 1, 1, 0.5));
					}
				}
			}
		} break;
	}
}

void TabBar::_draw_tab(Ref<StyleBox> &p_tab_style, Color &p_font_color, int p_index, float p_x) {
	RID ci = get_canvas_item();
	bool rtl = is_layout_rtl();

	Color font_outline_color = get_theme_color(SNAME("font_outline_color"));
	int outline_size = get_theme_constant(SNAME("outline_size"));
	int hseparation = get_theme_constant(SNAME("hseparation"));

	Rect2 sb_rect = Rect2(p_x, 0, tabs[p_index].size_cache, get_size().height);
	p_tab_style->draw(ci, sb_rect);

	p_x += rtl ? tabs[p_index].size_cache - p_tab_style->get_margin(SIDE_LEFT) : p_tab_style->get_margin(SIDE_LEFT);

	Size2i sb_ms = p_tab_style->get_minimum_size();

	// Draw the icon.
	Ref<Texture2D> icon = tabs[p_index].icon;
	if (icon.is_valid()) {
		icon->draw(ci, Point2i(rtl ? p_x - icon->get_width() : p_x, p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - icon->get_height()) / 2));

		p_x = rtl ? p_x - icon->get_width() - hseparation : p_x + icon->get_width() + hseparation;
	}

	// Draw the text.
	if (!tabs[p_index].text.is_empty()) {
		Point2i text_pos = Point2i(rtl ? p_x - tabs[p_index].size_text : p_x,
				p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - tabs[p_index].text_buf->get_size().y) / 2);

		if (outline_size > 0 && font_outline_color.a > 0) {
			tabs[p_index].text_buf->draw_outline(ci, text_pos, outline_size, font_outline_color);
		}
		tabs[p_index].text_buf->draw(ci, text_pos, p_font_color);

		p_x = rtl ? p_x - tabs[p_index].size_text - hseparation : p_x + tabs[p_index].size_text + hseparation;
	}

	// Draw and calculate rect of the right button.
	if (tabs[p_index].right_button.is_valid()) {
		Ref<StyleBox> style = get_theme_stylebox(SNAME("button_highlight"));
		Ref<Texture2D> rb = tabs[p_index].right_button;

		Rect2 rb_rect;
		rb_rect.size = style->get_minimum_size() + rb->get_size();
		rb_rect.position.x = rtl ? p_x - rb_rect.size.width : p_x;
		rb_rect.position.y = p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - (rb_rect.size.y)) / 2;

		tabs.write[p_index].rb_rect = rb_rect;

		if (rb_hover == p_index) {
			if (rb_pressing) {
				get_theme_stylebox(SNAME("button_pressed"))->draw(ci, rb_rect);
			} else {
				style->draw(ci, rb_rect);
			}
		}

		rb->draw(ci, Point2i(rb_rect.position.x + style->get_margin(SIDE_LEFT), rb_rect.position.y + style->get_margin(SIDE_TOP)));

		p_x = rtl ? rb_rect.position.x : rb_rect.position.x + rb_rect.size.width;
	}

	// Draw and calculate rect of the close button.
	if (cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && p_index == current)) {
		Ref<StyleBox> style = get_theme_stylebox(SNAME("button_highlight"));
		Ref<Texture2D> cb = get_theme_icon(SNAME("close"));

		Rect2 cb_rect;
		cb_rect.size = style->get_minimum_size() + cb->get_size();
		cb_rect.position.x = rtl ? p_x - cb_rect.size.width : p_x;
		cb_rect.position.y = p_tab_style->get_margin(SIDE_TOP) + ((sb_rect.size.y - sb_ms.y) - (cb_rect.size.y)) / 2;

		tabs.write[p_index].cb_rect = cb_rect;

		if (!tabs[p_index].disabled && cb_hover == p_index) {
			if (cb_pressing) {
				get_theme_stylebox(SNAME("button_pressed"))->draw(ci, cb_rect);
			} else {
				style->draw(ci, cb_rect);
			}
		}

		cb->draw(ci, Point2i(cb_rect.position.x + style->get_margin(SIDE_LEFT), cb_rect.position.y + style->get_margin(SIDE_TOP)));
	}
}

void TabBar::set_tab_count(int p_count) {
	if (p_count == tabs.size()) {
		return;
	}

	ERR_FAIL_COND(p_count < 0);
	tabs.resize(p_count);

	if (p_count == 0) {
		offset = 0;
		max_drawn_tab = 0;
		current = 0;
		previous = 0;
	} else {
		offset = MIN(offset, p_count - 1);
		max_drawn_tab = MIN(max_drawn_tab, p_count - 1);
		current = MIN(current, p_count - 1);

		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
	}

	update();
	update_minimum_size();
	notify_property_list_changed();
}

int TabBar::get_tab_count() const {
	return tabs.size();
}

void TabBar::set_current_tab(int p_current) {
	ERR_FAIL_INDEX(p_current, get_tab_count());

	previous = current;
	current = p_current;

	if (current == previous) {
		emit_signal(SNAME("tab_selected"), current);
		return;
	}

	emit_signal(SNAME("tab_selected"), current);

	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();

	emit_signal(SNAME("tab_changed"), p_current);
}

int TabBar::get_current_tab() const {
	return current;
}

int TabBar::get_previous_tab() const {
	return previous;
}

int TabBar::get_hovered_tab() const {
	return hover;
}

int TabBar::get_tab_offset() const {
	return offset;
}

bool TabBar::get_offset_buttons_visible() const {
	return buttons_visible;
}

void TabBar::set_tab_title(int p_tab, const String &p_title) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].text = p_title;

	_shape(p_tab);
	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

String TabBar::get_tab_title(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), "");
	return tabs[p_tab].text;
}

void TabBar::set_tab_text_direction(int p_tab, Control::TextDirection p_text_direction) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);

	if (tabs[p_tab].text_direction != p_text_direction) {
		tabs.write[p_tab].text_direction = p_text_direction;
		_shape(p_tab);
		update();
	}
}

Control::TextDirection TabBar::get_tab_text_direction(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Control::TEXT_DIRECTION_INHERITED);
	return tabs[p_tab].text_direction;
}

void TabBar::clear_tab_opentype_features(int p_tab) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].opentype_features.clear();

	_shape(p_tab);
	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

void TabBar::set_tab_opentype_feature(int p_tab, const String &p_name, int p_value) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	int32_t tag = TS->name_to_tag(p_name);
	if (!tabs[p_tab].opentype_features.has(tag) || (int)tabs[p_tab].opentype_features[tag] != p_value) {
		tabs.write[p_tab].opentype_features[tag] = p_value;

		_shape(p_tab);
		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
		update();
		update_minimum_size();
	}
}

int TabBar::get_tab_opentype_feature(int p_tab, const String &p_name) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), -1);

	int32_t tag = TS->name_to_tag(p_name);
	if (!tabs[p_tab].opentype_features.has(tag)) {
		return -1;
	}
	return tabs[p_tab].opentype_features[tag];
}

void TabBar::set_tab_language(int p_tab, const String &p_language) {
	ERR_FAIL_INDEX(p_tab, tabs.size());

	if (tabs[p_tab].language != p_language) {
		tabs.write[p_tab].language = p_language;
		_shape(p_tab);
		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
		update();
		update_minimum_size();
	}
}

String TabBar::get_tab_language(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), "");
	return tabs[p_tab].language;
}

void TabBar::set_tab_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].icon = p_icon;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

Ref<Texture2D> TabBar::get_tab_icon(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture2D>());
	return tabs[p_tab].icon;
}

void TabBar::set_tab_disabled(int p_tab, bool p_disabled) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].disabled = p_disabled;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

bool TabBar::is_tab_disabled(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), false);
	return tabs[p_tab].disabled;
}

void TabBar::set_tab_hidden(int p_tab, bool p_hidden) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].hidden = p_hidden;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

bool TabBar::is_tab_hidden(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), false);
	return tabs[p_tab].hidden;
}

void TabBar::set_tab_button_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_tab, tabs.size());
	tabs.write[p_tab].right_button = p_icon;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

Ref<Texture2D> TabBar::get_tab_button_icon(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Ref<Texture2D>());
	return tabs[p_tab].right_button;
}

void TabBar::_update_hover() {
	if (!is_inside_tree()) {
		return;
	}

	ERR_FAIL_COND(tabs.is_empty());

	const Point2 &pos = get_local_mouse_position();
	// Test hovering to display right or close button.
	int hover_now = -1;
	int hover_buttons = -1;
	for (int i = offset; i <= max_drawn_tab; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		Rect2 rect = get_tab_rect(i);
		if (rect.has_point(pos)) {
			hover_now = i;
		}

		if (tabs[i].rb_rect.has_point(pos)) {
			rb_hover = i;
			cb_hover = -1;
			hover_buttons = i;
		} else if (!tabs[i].disabled && tabs[i].cb_rect.has_point(pos)) {
			cb_hover = i;
			rb_hover = -1;
			hover_buttons = i;
		}

		if (hover_buttons != -1) {
			update();
			break;
		}
	}

	if (hover != hover_now) {
		hover = hover_now;

		if (hover != -1) {
			emit_signal(SNAME("tab_hovered"), hover);
		}
	}

	if (hover_buttons == -1) { // No hover.
		int rb_hover_old = rb_hover;
		int cb_hover_old = cb_hover;

		rb_hover = hover_buttons;
		cb_hover = hover_buttons;

		if (rb_hover != rb_hover_old || cb_hover != cb_hover_old) {
			update();
		}
	}
}

void TabBar::_update_cache() {
	if (tabs.is_empty()) {
		return;
	}

	Ref<StyleBox> tab_disabled = get_theme_stylebox(SNAME("tab_disabled"));
	Ref<StyleBox> tab_unselected = get_theme_stylebox(SNAME("tab_unselected"));
	Ref<StyleBox> tab_selected = get_theme_stylebox(SNAME("tab_selected"));
	Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
	Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));

	int limit = get_size().width;
	int limit_minus_buttons = limit - incr->get_width() - decr->get_width();

	int w = 0;

	max_drawn_tab = tabs.size() - 1;

	for (int i = 0; i < tabs.size(); i++) {
		tabs.write[i].text_buf->set_width(-1);
		tabs.write[i].size_text = Math::ceil(tabs[i].text_buf->get_size().x);
		tabs.write[i].size_cache = get_tab_width(i);

		if (max_width > 0 && tabs[i].size_cache > max_width) {
			int size_textless = tabs[i].size_cache - tabs[i].size_text;
			int mw = MAX(size_textless, max_width);

			tabs.write[i].size_text = MAX(mw - size_textless, 1);
			tabs.write[i].text_buf->set_width(tabs[i].size_text);
			tabs.write[i].size_cache = size_textless + tabs[i].size_text;
		}

		if (i < offset || i > max_drawn_tab) {
			tabs.write[i].ofs_cache = 0;
			continue;
		}

		tabs.write[i].ofs_cache = w;

		if (tabs[i].hidden) {
			continue;
		}

		w += tabs[i].size_cache;

		// Check if all tabs would fit inside the area.
		if (clip_tabs && i > offset && (w > limit || (offset > 0 && w > limit_minus_buttons))) {
			tabs.write[i].ofs_cache = 0;

			w -= tabs[i].size_cache;
			max_drawn_tab = i - 1;

			while (w > limit_minus_buttons && max_drawn_tab > offset) {
				tabs.write[max_drawn_tab].ofs_cache = 0;

				if (!tabs[max_drawn_tab].hidden) {
					w -= tabs[max_drawn_tab].size_cache;
				}

				max_drawn_tab--;
			}
		}
	}

	missing_right = max_drawn_tab < tabs.size() - 1;
	buttons_visible = offset > 0 || missing_right;

	if (tab_alignment == ALIGNMENT_LEFT) {
		_update_hover();
		return;
	}

	if (tab_alignment == ALIGNMENT_CENTER) {
		w = ((buttons_visible ? limit_minus_buttons : limit) - w) / 2;
	} else if (tab_alignment == ALIGNMENT_RIGHT) {
		w = (buttons_visible ? limit_minus_buttons : limit) - w;
	}

	for (int i = offset; i <= max_drawn_tab; i++) {
		tabs.write[i].ofs_cache = w;

		if (!tabs[i].hidden) {
			w += tabs[i].size_cache;
		}
	}

	_update_hover();
}

void TabBar::_on_mouse_exited() {
	rb_hover = -1;
	cb_hover = -1;
	hover = -1;
	highlight_arrow = -1;
	update();
}

void TabBar::add_tab(const String &p_str, const Ref<Texture2D> &p_icon) {
	Tab t;
	t.text = p_str;
	t.text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	t.icon = p_icon;
	tabs.push_back(t);

	_shape(tabs.size() - 1);
	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();

	if (tabs.size() == 1 && is_inside_tree()) {
		emit_signal(SNAME("tab_changed"), 0);
	}
}

void TabBar::clear_tabs() {
	if (tabs.is_empty()) {
		return;
	}

	tabs.clear();
	offset = 0;
	max_drawn_tab = 0;
	current = 0;
	previous = 0;

	update();
	update_minimum_size();
	notify_property_list_changed();
}

void TabBar::remove_tab(int p_idx) {
	ERR_FAIL_INDEX(p_idx, tabs.size());
	tabs.remove_at(p_idx);

	bool is_tab_changing = current == p_idx && !tabs.is_empty();

	if (current >= p_idx && current > 0) {
		current--;
	}

	if (tabs.is_empty()) {
		offset = 0;
		max_drawn_tab = 0;
		previous = 0;
	} else {
		offset = MIN(offset, tabs.size() - 1);
		max_drawn_tab = MIN(max_drawn_tab, tabs.size() - 1);

		_update_cache();
		_ensure_no_over_offset();
		if (scroll_to_selected) {
			ensure_tab_visible(current);
		}
	}

	update();
	update_minimum_size();
	notify_property_list_changed();

	if (is_tab_changing && is_inside_tree()) {
		emit_signal(SNAME("tab_changed"), current);
	}
}

Variant TabBar::get_drag_data(const Point2 &p_point) {
	if (!drag_to_rearrange_enabled) {
		return Control::get_drag_data(p_point); // Allow stuff like TabContainer to override it.
	}

	int tab_over = get_tab_idx_at_point(p_point);
	if (tab_over < 0) {
		return Variant();
	}

	HBoxContainer *drag_preview = memnew(HBoxContainer);

	if (!tabs[tab_over].icon.is_null()) {
		TextureRect *tf = memnew(TextureRect);
		tf->set_texture(tabs[tab_over].icon);
		tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		drag_preview->add_child(tf);
	}

	Label *label = memnew(Label(tabs[tab_over].xl_text));
	drag_preview->add_child(label);

	set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = "tab_element";
	drag_data["tab_element"] = tab_over;
	drag_data["from_path"] = get_path();

	return drag_data;
}

bool TabBar::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	if (!drag_to_rearrange_enabled) {
		return Control::can_drop_data(p_point, p_data); // Allow stuff like TabContainer to override it.
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	if (String(d["type"]) == "tab_element") {
		NodePath from_path = d["from_path"];
		NodePath to_path = get_path();
		if (from_path == to_path) {
			return true;
		} else if (get_tabs_rearrange_group() != -1) {
			// Drag and drop between other TabBars.
			Node *from_node = get_node(from_path);
			TabBar *from_tabs = Object::cast_to<TabBar>(from_node);
			if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
				return true;
			}
		}
	}

	return false;
}

void TabBar::drop_data(const Point2 &p_point, const Variant &p_data) {
	if (!drag_to_rearrange_enabled) {
		Control::drop_data(p_point, p_data); // Allow stuff like TabContainer to override it.
		return;
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == "tab_element") {
		int tab_from_id = d["tab_element"];
		int hover_now = get_tab_idx_at_point(p_point);
		NodePath from_path = d["from_path"];
		NodePath to_path = get_path();

		if (from_path == to_path) {
			if (hover_now < 0) {
				hover_now = get_tab_count() - 1;
			}

			move_tab(tab_from_id, hover_now);
			emit_signal(SNAME("active_tab_rearranged"), hover_now);
			set_current_tab(hover_now);
		} else if (get_tabs_rearrange_group() != -1) {
			// Drag and drop between Tabs.

			Node *from_node = get_node(from_path);
			TabBar *from_tabs = Object::cast_to<TabBar>(from_node);

			if (from_tabs && from_tabs->get_tabs_rearrange_group() == get_tabs_rearrange_group()) {
				if (tab_from_id >= from_tabs->get_tab_count()) {
					return;
				}

				Tab moving_tab = from_tabs->tabs[tab_from_id];
				if (hover_now < 0) {
					hover_now = get_tab_count();
				}

				from_tabs->remove_tab(tab_from_id);
				tabs.insert(hover_now, moving_tab);

				if (tabs.size() > 1) {
					if (current >= hover_now) {
						current++;
					}
					if (previous >= hover_now) {
						previous++;
					}
				}

				set_current_tab(hover_now);
				update_minimum_size();

				if (tabs.size() == 1) {
					emit_signal(SNAME("tab_selected"), 0);
					emit_signal(SNAME("tab_changed"), 0);
				}
			}
		}
	}
}

int TabBar::get_tab_idx_at_point(const Point2 &p_point) const {
	int hover_now = -1;
	for (int i = offset; i <= max_drawn_tab; i++) {
		Rect2 rect = get_tab_rect(i);
		if (rect.has_point(p_point)) {
			hover_now = i;
		}
	}

	return hover_now;
}

void TabBar::set_tab_alignment(AlignmentMode p_alignment) {
	ERR_FAIL_INDEX(p_alignment, ALIGNMENT_MAX);
	tab_alignment = p_alignment;

	_update_cache();
	update();
}

TabBar::AlignmentMode TabBar::get_tab_alignment() const {
	return tab_alignment;
}

void TabBar::set_clip_tabs(bool p_clip_tabs) {
	if (clip_tabs == p_clip_tabs) {
		return;
	}
	clip_tabs = p_clip_tabs;

	if (!clip_tabs) {
		offset = 0;
		max_drawn_tab = 0;
	}

	_update_cache();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

bool TabBar::get_clip_tabs() const {
	return clip_tabs;
}

void TabBar::move_tab(int p_from, int p_to) {
	if (p_from == p_to) {
		return;
	}

	ERR_FAIL_INDEX(p_from, tabs.size());
	ERR_FAIL_INDEX(p_to, tabs.size());

	Tab tab_from = tabs[p_from];
	tabs.remove_at(p_from);
	tabs.insert(p_to, tab_from);

	if (current == p_from) {
		current = p_to;
	} else if (current > p_from && current <= p_to) {
		current--;
	} else if (current < p_from && current >= p_to) {
		current++;
	}

	if (previous == p_from) {
		previous = p_to;
	} else if (previous > p_from && previous >= p_to) {
		previous--;
	} else if (previous < p_from && previous <= p_to) {
		previous++;
	}

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	notify_property_list_changed();
}

int TabBar::get_tab_width(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, tabs.size(), 0);

	Ref<StyleBox> tab_unselected = get_theme_stylebox(SNAME("tab_unselected"));
	Ref<StyleBox> tab_selected = get_theme_stylebox(SNAME("tab_selected"));
	Ref<StyleBox> tab_disabled = get_theme_stylebox(SNAME("tab_disabled"));
	int hseparation = get_theme_constant(SNAME("hseparation"));

	Ref<StyleBox> style;

	if (tabs[p_idx].disabled) {
		style = tab_disabled;
	} else if (current == p_idx) {
		style = tab_selected;
	} else {
		style = tab_unselected;
	}
	int x = style->get_minimum_size().width;

	Ref<Texture2D> tex = tabs[p_idx].icon;
	if (tex.is_valid()) {
		x += tex->get_width() + hseparation;
	}

	if (!tabs[p_idx].text.is_empty()) {
		x += tabs[p_idx].size_text + hseparation;
	}

	bool close_visible = cb_displaypolicy == CLOSE_BUTTON_SHOW_ALWAYS || (cb_displaypolicy == CLOSE_BUTTON_SHOW_ACTIVE_ONLY && p_idx == current);

	if (tabs[p_idx].right_button.is_valid()) {
		Ref<StyleBox> btn_style = get_theme_stylebox(SNAME("button_highlight"));
		Ref<Texture2D> rb = tabs[p_idx].right_button;

		if (close_visible) {
			x += btn_style->get_minimum_size().width + rb->get_width();
		} else {
			x += btn_style->get_margin(SIDE_LEFT) + rb->get_width() + hseparation;
		}
	}

	if (close_visible) {
		Ref<StyleBox> btn_style = get_theme_stylebox(SNAME("button_highlight"));
		Ref<Texture2D> cb = get_theme_icon(SNAME("close"));
		x += btn_style->get_margin(SIDE_LEFT) + cb->get_width() + hseparation;
	}

	if (x > style->get_minimum_size().width) {
		x -= hseparation;
	}

	return x;
}

void TabBar::_ensure_no_over_offset() {
	if (!is_inside_tree() || !buttons_visible) {
		return;
	}

	Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
	Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));
	int limit_minus_buttons = get_size().width - incr->get_width() - decr->get_width();

	int prev_offset = offset;

	int total_w = tabs[max_drawn_tab].ofs_cache + tabs[max_drawn_tab].size_cache - tabs[offset].ofs_cache;
	for (int i = offset; i > 0; i--) {
		if (tabs[i - 1].hidden) {
			continue;
		}

		total_w += tabs[i - 1].size_cache;

		if (total_w < limit_minus_buttons) {
			offset--;
		} else {
			break;
		}
	}

	if (prev_offset != offset) {
		_update_cache();
		update();
	}
}

void TabBar::ensure_tab_visible(int p_idx) {
	if (!is_inside_tree() || !buttons_visible) {
		return;
	}
	ERR_FAIL_INDEX(p_idx, tabs.size());

	if (tabs[p_idx].hidden || (p_idx >= offset && p_idx <= max_drawn_tab)) {
		return;
	}

	if (p_idx < offset) {
		offset = p_idx;
		_update_cache();
		update();

		return;
	}

	Ref<Texture2D> incr = get_theme_icon(SNAME("increment"));
	Ref<Texture2D> decr = get_theme_icon(SNAME("decrement"));
	int limit_minus_buttons = get_size().width - incr->get_width() - decr->get_width();

	int total_w = tabs[max_drawn_tab].ofs_cache - tabs[offset].ofs_cache;
	for (int i = max_drawn_tab; i <= p_idx; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		total_w += tabs[i].size_cache;
	}

	int prev_offset = offset;

	for (int i = offset; i < p_idx; i++) {
		if (tabs[i].hidden) {
			continue;
		}

		if (total_w > limit_minus_buttons) {
			total_w -= tabs[i].size_cache;
			offset++;
		} else {
			break;
		}
	}

	if (prev_offset != offset) {
		_update_cache();
		update();
	}
}

Rect2 TabBar::get_tab_rect(int p_tab) const {
	ERR_FAIL_INDEX_V(p_tab, tabs.size(), Rect2());
	if (is_layout_rtl()) {
		return Rect2(get_size().width - tabs[p_tab].ofs_cache - tabs[p_tab].size_cache, 0, tabs[p_tab].size_cache, get_size().height);
	} else {
		return Rect2(tabs[p_tab].ofs_cache, 0, tabs[p_tab].size_cache, get_size().height);
	}
}

void TabBar::set_tab_close_display_policy(CloseButtonDisplayPolicy p_policy) {
	ERR_FAIL_INDEX(p_policy, CLOSE_BUTTON_MAX);
	cb_displaypolicy = p_policy;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

TabBar::CloseButtonDisplayPolicy TabBar::get_tab_close_display_policy() const {
	return cb_displaypolicy;
}

void TabBar::set_max_tab_width(int p_width) {
	ERR_FAIL_COND(p_width < 0);
	max_width = p_width;

	_update_cache();
	_ensure_no_over_offset();
	if (scroll_to_selected) {
		ensure_tab_visible(current);
	}
	update();
	update_minimum_size();
}

int TabBar::get_max_tab_width() const {
	return max_width;
}

void TabBar::set_scrolling_enabled(bool p_enabled) {
	scrolling_enabled = p_enabled;
}

bool TabBar::get_scrolling_enabled() const {
	return scrolling_enabled;
}

void TabBar::set_drag_to_rearrange_enabled(bool p_enabled) {
	drag_to_rearrange_enabled = p_enabled;
}

bool TabBar::get_drag_to_rearrange_enabled() const {
	return drag_to_rearrange_enabled;
}

void TabBar::set_tabs_rearrange_group(int p_group_id) {
	tabs_rearrange_group = p_group_id;
}

int TabBar::get_tabs_rearrange_group() const {
	return tabs_rearrange_group;
}

void TabBar::set_scroll_to_selected(bool p_enabled) {
	scroll_to_selected = p_enabled;
	if (p_enabled) {
		ensure_tab_visible(current);
	}
}

bool TabBar::get_scroll_to_selected() const {
	return scroll_to_selected;
}

void TabBar::set_select_with_rmb(bool p_enabled) {
	select_with_rmb = p_enabled;
}

bool TabBar::get_select_with_rmb() const {
	return select_with_rmb;
}

bool TabBar::_set(const StringName &p_name, const Variant &p_value) {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("tab_") && components[0].trim_prefix("tab_").is_valid_int()) {
		int tab_index = components[0].trim_prefix("tab_").to_int();
		String property = components[1];
		if (property == "title") {
			set_tab_title(tab_index, p_value);
			return true;
		} else if (property == "icon") {
			set_tab_icon(tab_index, p_value);
			return true;
		} else if (components[1] == "disabled") {
			set_tab_disabled(tab_index, p_value);
			return true;
		}
	}
	return false;
}

bool TabBar::_get(const StringName &p_name, Variant &r_ret) const {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("tab_") && components[0].trim_prefix("tab_").is_valid_int()) {
		int tab_index = components[0].trim_prefix("tab_").to_int();
		String property = components[1];
		if (property == "title") {
			r_ret = get_tab_title(tab_index);
			return true;
		} else if (property == "icon") {
			r_ret = get_tab_icon(tab_index);
			return true;
		} else if (components[1] == "disabled") {
			r_ret = is_tab_disabled(tab_index);
			return true;
		}
	}
	return false;
}

void TabBar::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < tabs.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, vformat("tab_%d/title", i)));

		PropertyInfo pi = PropertyInfo(Variant::OBJECT, vformat("tab_%d/icon", i), PROPERTY_HINT_RESOURCE_TYPE, "Texture2D");
		pi.usage &= ~(get_tab_icon(i).is_null() ? PROPERTY_USAGE_STORAGE : 0);
		p_list->push_back(pi);

		pi = PropertyInfo(Variant::BOOL, vformat("tab_%d/disabled", i));
		pi.usage &= ~(!is_tab_disabled(i) ? PROPERTY_USAGE_STORAGE : 0);
		p_list->push_back(pi);
	}
}

void TabBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tab_count", "count"), &TabBar::set_tab_count);
	ClassDB::bind_method(D_METHOD("get_tab_count"), &TabBar::get_tab_count);
	ClassDB::bind_method(D_METHOD("set_current_tab", "tab_idx"), &TabBar::set_current_tab);
	ClassDB::bind_method(D_METHOD("get_current_tab"), &TabBar::get_current_tab);
	ClassDB::bind_method(D_METHOD("get_previous_tab"), &TabBar::get_previous_tab);
	ClassDB::bind_method(D_METHOD("set_tab_title", "tab_idx", "title"), &TabBar::set_tab_title);
	ClassDB::bind_method(D_METHOD("get_tab_title", "tab_idx"), &TabBar::get_tab_title);
	ClassDB::bind_method(D_METHOD("set_tab_text_direction", "tab_idx", "direction"), &TabBar::set_tab_text_direction);
	ClassDB::bind_method(D_METHOD("get_tab_text_direction", "tab_idx"), &TabBar::get_tab_text_direction);
	ClassDB::bind_method(D_METHOD("set_tab_opentype_feature", "tab_idx", "tag", "values"), &TabBar::set_tab_opentype_feature);
	ClassDB::bind_method(D_METHOD("get_tab_opentype_feature", "tab_idx", "tag"), &TabBar::get_tab_opentype_feature);
	ClassDB::bind_method(D_METHOD("clear_tab_opentype_features", "tab_idx"), &TabBar::clear_tab_opentype_features);
	ClassDB::bind_method(D_METHOD("set_tab_language", "tab_idx", "language"), &TabBar::set_tab_language);
	ClassDB::bind_method(D_METHOD("get_tab_language", "tab_idx"), &TabBar::get_tab_language);
	ClassDB::bind_method(D_METHOD("set_tab_icon", "tab_idx", "icon"), &TabBar::set_tab_icon);
	ClassDB::bind_method(D_METHOD("get_tab_icon", "tab_idx"), &TabBar::get_tab_icon);
	ClassDB::bind_method(D_METHOD("set_tab_button_icon", "tab_idx", "icon"), &TabBar::set_tab_button_icon);
	ClassDB::bind_method(D_METHOD("get_tab_button_icon", "tab_idx"), &TabBar::get_tab_button_icon);
	ClassDB::bind_method(D_METHOD("set_tab_disabled", "tab_idx", "disabled"), &TabBar::set_tab_disabled);
	ClassDB::bind_method(D_METHOD("is_tab_disabled", "tab_idx"), &TabBar::is_tab_disabled);
	ClassDB::bind_method(D_METHOD("set_tab_hidden", "tab_idx", "hidden"), &TabBar::set_tab_hidden);
	ClassDB::bind_method(D_METHOD("is_tab_hidden", "tab_idx"), &TabBar::is_tab_hidden);
	ClassDB::bind_method(D_METHOD("remove_tab", "tab_idx"), &TabBar::remove_tab);
	ClassDB::bind_method(D_METHOD("add_tab", "title", "icon"), &TabBar::add_tab, DEFVAL(""), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("get_tab_idx_at_point", "point"), &TabBar::get_tab_idx_at_point);
	ClassDB::bind_method(D_METHOD("set_tab_alignment", "alignment"), &TabBar::set_tab_alignment);
	ClassDB::bind_method(D_METHOD("get_tab_alignment"), &TabBar::get_tab_alignment);
	ClassDB::bind_method(D_METHOD("set_clip_tabs", "clip_tabs"), &TabBar::set_clip_tabs);
	ClassDB::bind_method(D_METHOD("get_clip_tabs"), &TabBar::get_clip_tabs);
	ClassDB::bind_method(D_METHOD("get_tab_offset"), &TabBar::get_tab_offset);
	ClassDB::bind_method(D_METHOD("get_offset_buttons_visible"), &TabBar::get_offset_buttons_visible);
	ClassDB::bind_method(D_METHOD("ensure_tab_visible", "idx"), &TabBar::ensure_tab_visible);
	ClassDB::bind_method(D_METHOD("get_tab_rect", "tab_idx"), &TabBar::get_tab_rect);
	ClassDB::bind_method(D_METHOD("move_tab", "from", "to"), &TabBar::move_tab);
	ClassDB::bind_method(D_METHOD("set_tab_close_display_policy", "policy"), &TabBar::set_tab_close_display_policy);
	ClassDB::bind_method(D_METHOD("get_tab_close_display_policy"), &TabBar::get_tab_close_display_policy);
	ClassDB::bind_method(D_METHOD("set_max_tab_width", "width"), &TabBar::set_max_tab_width);
	ClassDB::bind_method(D_METHOD("get_max_tab_width"), &TabBar::get_max_tab_width);
	ClassDB::bind_method(D_METHOD("set_scrolling_enabled", "enabled"), &TabBar::set_scrolling_enabled);
	ClassDB::bind_method(D_METHOD("get_scrolling_enabled"), &TabBar::get_scrolling_enabled);
	ClassDB::bind_method(D_METHOD("set_drag_to_rearrange_enabled", "enabled"), &TabBar::set_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("get_drag_to_rearrange_enabled"), &TabBar::get_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("set_tabs_rearrange_group", "group_id"), &TabBar::set_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("get_tabs_rearrange_group"), &TabBar::get_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("set_scroll_to_selected", "enabled"), &TabBar::set_scroll_to_selected);
	ClassDB::bind_method(D_METHOD("get_scroll_to_selected"), &TabBar::get_scroll_to_selected);
	ClassDB::bind_method(D_METHOD("set_select_with_rmb", "enabled"), &TabBar::set_select_with_rmb);
	ClassDB::bind_method(D_METHOD("get_select_with_rmb"), &TabBar::get_select_with_rmb);

	ADD_SIGNAL(MethodInfo("tab_selected", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_changed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_clicked", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_rmb_clicked", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_close_pressed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_button_pressed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_hovered", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("active_tab_rearranged", PropertyInfo(Variant::INT, "idx_to")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_tab", PROPERTY_HINT_RANGE, "-1,4096,1", PROPERTY_USAGE_EDITOR), "set_current_tab", "get_current_tab");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_tab_alignment", "get_tab_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_tabs"), "set_clip_tabs", "get_clip_tabs");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_close_display_policy", PROPERTY_HINT_ENUM, "Show Never,Show Active Only,Show Always"), "set_tab_close_display_policy", "get_tab_close_display_policy");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tab_width", PROPERTY_HINT_RANGE, "0,99999,1"), "set_max_tab_width", "get_max_tab_width");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scrolling_enabled"), "set_scrolling_enabled", "get_scrolling_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_to_rearrange_enabled"), "set_drag_to_rearrange_enabled", "get_drag_to_rearrange_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tabs_rearrange_group"), "set_tabs_rearrange_group", "get_tabs_rearrange_group");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_to_selected"), "set_scroll_to_selected", "get_scroll_to_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "select_with_rmb"), "set_select_with_rmb", "get_select_with_rmb");

	ADD_ARRAY_COUNT("Tabs", "tab_count", "set_tab_count", "get_tab_count", "tab_");

	BIND_ENUM_CONSTANT(ALIGNMENT_LEFT);
	BIND_ENUM_CONSTANT(ALIGNMENT_CENTER);
	BIND_ENUM_CONSTANT(ALIGNMENT_RIGHT);
	BIND_ENUM_CONSTANT(ALIGNMENT_MAX);

	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_NEVER);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ACTIVE_ONLY);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_SHOW_ALWAYS);
	BIND_ENUM_CONSTANT(CLOSE_BUTTON_MAX);
}

TabBar::TabBar() {
	set_size(Size2(get_size().width, get_minimum_size().height));
	connect("mouse_exited", callable_mp(this, &TabBar::_on_mouse_exited));
}
