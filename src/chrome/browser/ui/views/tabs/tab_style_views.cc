// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style_views.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "cc/paint/paint_record.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace {

// Opacity of the active tab background painted over inactive selected tabs.
constexpr float kSelectedTabOpacity = 0.75f;

// Tab style implementation for the GM2 refresh (Chrome 69).
class GM2TabStyle : public TabStyleViews {
 public:
  explicit GM2TabStyle(Tab* tab);

 protected:
  // TabStyle:
  SkPath GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const override;
  gfx::Insets GetContentsInsets() const override;
  float GetZValue() const override;
  TabStyle::TabColors CalculateColors() const override;
  void PaintTab(gfx::Canvas* canvas, const SkPath& clip) const override;
  void SetHoverLocation(const gfx::Point& location) override;
  void ShowHover(ShowHoverStyle style) override;
  void HideHover(HideHoverStyle style) override;

 private:
  // Gets the bounds for the leading and trailing separators for a tab.
  SeparatorBounds GetSeparatorBounds(float scale) const;

  // Returns the opacities of the separators. If |for_layout| is true, returns
  // the "layout" opacities, which ignore the effects of surrounding tabs' hover
  // effects and consider only the current tab's state.
  SeparatorOpacities GetSeparatorOpacities(bool for_layout) const;

  // Returns whether we shoould extend the hit test region for Fitts' Law.
  bool ShouldExtendHitTest() const;

  // Returns whether the hover animation is being shown.
  bool IsHoverActive() const;

  // Returns the progress (0 to 1) of the hover animation.
  double GetHoverAnimationValue() const;

  // Returns the opacity of the hover effect that should be drawn, which may not
  // be the same as GetHoverAnimationValue.
  float GetHoverOpacity() const;

  // Gets the throb value. A value of 0 indicates no throbbing.
  float GetThrobValue() const;

  // Returns the thickness of the stroke drawn around the top and sides of the
  // tab. Only active tabs may have a stroke, and not in all cases. If there
  // is no stroke, returns 0. If |should_paint_as_active| is true, the tab is
  // treated as an active tab regardless of its true current state.
  int GetStrokeThickness(bool should_paint_as_active = false) const;

  bool ShouldPaintTabBackgroundColor(TabState active_state, int fill_id) const;

  SkColor GetTabBackgroundColor(TabState active_state) const;

  // Painting helper functions:
  void PaintInactiveTabBackground(gfx::Canvas* canvas,
                                  const SkPath& clip) const;
  void PaintTabBackground(gfx::Canvas* canvas,
                          TabState active_state,
                          int fill_id,
                          int y_inset,
                          const SkPath* clip) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              TabState active_state,
                              bool paint_hover_effect,
                              int fill_id,
                              int y_inset) const;
  void PaintBackgroundStroke(gfx::Canvas* canvas,
                             TabState active_state,
                             SkColor stroke_color) const;
  void PaintSeparators(gfx::Canvas* canvas) const;

  // Given a tab of width |width|, returns the radius to use for the corners.
  static float GetTopCornerRadiusForWidth(int width);

  // Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
  // during painting.
  static gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                        float scale,
                                        int stroke_thickness);

  const Tab* const tab_;

  std::unique_ptr<GlowHoverController> hover_controller_;

  DISALLOW_COPY_AND_ASSIGN(GM2TabStyle);
};

void DrawHighlight(gfx::Canvas* canvas,
                   const SkPoint& p,
                   SkScalar radius,
                   SkColor color) {
  const SkColor colors[2] = {color, SkColorSetA(color, 0)};
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      p, radius, colors, nullptr, 2, SkTileMode::kClamp));
  canvas->sk_canvas()->drawRect(
      SkRect::MakeXYWH(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
      flags);
}

// Updates a target value, returning true if it changed.
template <class T>
bool UpdateValue(T* dest, const T& src) {
  if (*dest == src)
    return false;
  *dest = src;
  return true;
}

// GM2TabStyle -----------------------------------------------------------------

GM2TabStyle::GM2TabStyle(Tab* tab)
    : tab_(tab),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? new GlowHoverController(tab)
                            : nullptr) {}

SkPath GM2TabStyle::GetPath(PathType path_type,
                            float scale,
                            bool force_active,
                            RenderUnits render_units) const {
  const int stroke_thickness = GetStrokeThickness(force_active);

  // We'll do the entire path calculation in aligned pixels.
  // TODO(dfried): determine if we actually want to use |stroke_thickness| as
  // the inset in this case.
  gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab_->bounds(), scale, stroke_thickness);

  if (path_type == PathType::kInteriorClip) {
    // When there is a separator, animate the clip to account for it, in sync
    // with the separator's fading.
    // TODO(pkasting): Consider crossfading the favicon instead of animating
    // the clip, especially if other children get crossfaded.
    const auto opacities = GetSeparatorOpacities(true);
    constexpr float kChildClipPadding = 2.5f;
    aligned_bounds.Inset(gfx::InsetsF(0.0f, kChildClipPadding + opacities.left,
                                      0.0f,
                                      kChildClipPadding + opacities.right));
  }

  // Calculate the corner radii. Note that corner radius is based on original
  // tab width (in DIP), not our new, scaled-and-aligned bounds.
  const float radius = GetTopCornerRadiusForWidth(tab_->width()) * scale;
  float top_radius = radius;
  float bottom_radius = radius;

  // Compute |extension| as the width outside the separators.  This is a fixed
  // value equal to the normal corner radius.
  const float extension = GetCornerRadius() * scale;

  // Calculate the bounds of the actual path.
  const float left = aligned_bounds.x();
  const float right = aligned_bounds.right();
  float tab_top = aligned_bounds.y();
  float tab_left = left + extension;
  float tab_right = right - extension;

  // Overlap the toolbar below us so that gaps don't occur when rendering at
  // non-integral display scale factors.
  const float extended_bottom = aligned_bounds.bottom();
  const float bottom_extension =
      GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
  float tab_bottom = extended_bottom - bottom_extension;

  // Path-specific adjustments:
  const float stroke_adjustment = stroke_thickness * scale;
  if (path_type == PathType::kInteriorClip) {
    // Inside of the border runs |stroke_thickness| inside the outer edge.
    tab_left += stroke_adjustment;
    tab_right -= stroke_adjustment;
    tab_top += stroke_adjustment;
    top_radius -= stroke_adjustment;
  } else if (path_type == PathType::kFill || path_type == PathType::kBorder) {
    tab_left += 0.5f * stroke_adjustment;
    tab_right -= 0.5f * stroke_adjustment;
    tab_top += 0.5f * stroke_adjustment;
    top_radius -= 0.5f * stroke_adjustment;
    tab_bottom -= 0.5f * stroke_adjustment;
    bottom_radius -= 0.5f * stroke_adjustment;
  } else if (path_type == PathType::kHitTest ||
             path_type == PathType::kExteriorClip) {
    // Outside border needs to draw its bottom line a stroke width above the
    // bottom of the tab, to line up with the stroke that runs across the rest
    // of the bottom of the tab bar (when strokes are enabled).
    tab_bottom -= stroke_adjustment;
    bottom_radius -= stroke_adjustment;
  }
  const bool extend_to_top =
      (path_type == PathType::kHitTest) && ShouldExtendHitTest();

  // When the radius shrinks, it leaves a gap between the bottom corners and the
  // edge of the tab. Make sure we account for this - and for any adjustment we
  // may have made to the location of the tab!
  const float corner_gap = (right - tab_right) - bottom_radius;

  SkPath path;

  if (path_type == PathType::kInteriorClip) {
    // Clip path is a simple rectangle.
    path.addRect(tab_left, tab_top, tab_right, tab_bottom);
  } else if (path_type == PathType::kHighlight) {
    // The path is a round rect inset by the focus ring thickness. The
    // radius is also adjusted by the inset.
    const float inset = views::PlatformStyle::kFocusHaloThickness +
                        views::PlatformStyle::kFocusHaloInset;
    SkRRect rrect = SkRRect::MakeRectXY(
        SkRect::MakeLTRB(tab_left + inset, tab_top + inset, tab_right - inset,
                         tab_bottom - inset),
        radius - inset, radius - inset);
    path.addRRect(rrect);
  } else {
    // We will go clockwise from the lower left. We start in the overlap region,
    // preventing a gap between toolbar and tabstrip.
    // TODO(dfried): verify that the we actually want to start the stroke for
    // the exterior path outside the region; we might end up rendering an
    // extraneous descending pixel on displays with odd scaling and nonzero
    // stroke width.

    // Start with the left side of the shape.

    // Draw everything left of the bottom-left corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┏━╯         ╰─┐
    path.moveTo(left, extended_bottom);
    path.lineTo(left, tab_bottom);
    path.lineTo(left + corner_gap, tab_bottom);

    // Draw the bottom-left arc.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╝         ╰─┐
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, tab_left, tab_bottom - bottom_radius);

    // Draw the ascender and top arc, if present.
    if (extend_to_top) {
      //   ┎─────────╮
      //   ┃ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_left, tab_top);
    } else {
      //   ╔─────────╮
      //   ┃ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_left, tab_top + top_radius);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPath::kCW_Direction, tab_left + top_radius, tab_top);
    }

    // Draw the top crossbar and top-right curve, if present.
    if (extend_to_top) {
      //   ┌━━━━━━━━━┑
      //   │ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_right, tab_top);

    } else {
      //   ╭━━━━━━━━━╗
      //   │ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_right - top_radius, tab_top);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPath::kCW_Direction, tab_right, tab_top + top_radius);
    }

    // Draw the descender and bottom-right arc.
    //   ╭─────────╮
    //   │ Content ┃
    // ┌─╯         ╚─┐
    path.lineTo(tab_right, tab_bottom - bottom_radius);
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, right - corner_gap, tab_bottom);

    // Draw everything right of the bottom-right corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╯         ╰━┓
    path.lineTo(right, tab_bottom);
    path.lineTo(right, extended_bottom);

    if (path_type != PathType::kBorder)
      path.close();
  }

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(tab_->origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  // Possibly convert back to DIPs.
  if (render_units == RenderUnits::kDips && scale != 1.0f)
    path.transform(SkMatrix::MakeScale(1.f / scale));

  return path;
}

gfx::Insets GM2TabStyle::GetContentsInsets() const {
  const int stroke_thickness = GetStrokeThickness();
  const int horizontal_inset = GetContentsHorizontalInsetSize();
  return gfx::Insets(
      stroke_thickness, horizontal_inset,
      stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
      horizontal_inset);
}

float GM2TabStyle::GetZValue() const {
  // This will return values so that inactive tabs can be sorted in the
  // following order:
  //
  // o Unselected tabs, in ascending hover animation value order.
  // o The single unselected tab being hovered by the mouse, if present.
  // o Selected tabs, in ascending hover animation value order.
  // o The single selected tab being hovered by the mouse, if present.
  //
  // Representing the above groupings is accomplished by adding a "weight" to
  // the current hover animation value.
  //
  // 0.0 == z-value         Unselected/non hover animating.
  // 0.0 <  z-value <= 1.0  Unselected/hover animating.
  // 2.0 <= z-value <= 3.0  Unselected/mouse hovered tab.
  // 4.0 == z-value         Selected/non hover animating.
  // 4.0 <  z-value <= 5.0  Selected/hover animating.
  // 6.0 <= z-value <= 7.0  Selected/mouse hovered tab.
  //
  // This function doesn't handle active tabs, as they are normally painted by a
  // different code path (with z-value infinity).
  float sort_value = GetHoverAnimationValue();
  if (tab_->IsSelected())
    sort_value += 4.f;
  if (tab_->mouse_hovered())
    sort_value += 2.f;
  return sort_value;
}

TabStyle::TabColors GM2TabStyle::CalculateColors() const {
  const ui::ThemeProvider* theme_provider = tab_->GetThemeProvider();

  // These ratios are calculated from the default Chrome theme colors.
  // Active/inactive are the contrast ratios of the close X against the tab
  // background. Hovered/pressed are the contrast ratios of the highlight circle
  // against the tab background.
  constexpr float kMinimumActiveContrastRatio = 6.05f;
  constexpr float kMinimumInactiveContrastRatio = 4.61f;
  constexpr float kMinimumHoveredContrastRatio = 5.02f;
  constexpr float kMinimumPressedContrastRatio = 4.41f;

  // In some cases, inactive tabs may have background more like active tabs than
  // inactive tabs, so colors should be adapted to ensure appropriate contrast.
  // In particular, text should have plenty of contrast in all cases, so switch
  // to using foreground color designed for active tabs if the tab looks more
  // like an active tab than an inactive tab.
  float expected_opacity = 0.0f;
  if (tab_->IsActive()) {
    expected_opacity = 1.0f;
  } else if (tab_->IsSelected()) {
    expected_opacity = kSelectedTabOpacity;
  } else if (tab_->mouse_hovered()) {
    expected_opacity = GetHoverOpacity();
  }
  const SkColor bg_color = color_utils::AlphaBlend(
      GetTabBackgroundColor(TAB_ACTIVE), GetTabBackgroundColor(TAB_INACTIVE),
      expected_opacity);

  SkColor title_color = tab_->controller()->GetTabForegroundColor(
      expected_opacity > 0.5f ? TAB_ACTIVE : TAB_INACTIVE, bg_color);
  title_color = color_utils::BlendForMinContrast(title_color, bg_color).color;

  const SkColor base_hovered_color = theme_provider->GetColor(
      ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_HOVER);
  const SkColor base_pressed_color = theme_provider->GetColor(
      ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_PRESSED);

  const auto get_color_for_contrast_ratio =
      [](SkColor fg_color, SkColor bg_color, float contrast_ratio) {
        return color_utils::BlendForMinContrast(bg_color, bg_color, fg_color,
                                                contrast_ratio)
            .color;
      };

  const SkColor generated_icon_color = get_color_for_contrast_ratio(
      title_color, bg_color,
      tab_->IsActive() ? kMinimumActiveContrastRatio
                       : kMinimumInactiveContrastRatio);
  const SkColor generated_hovered_color = get_color_for_contrast_ratio(
      base_hovered_color, bg_color, kMinimumHoveredContrastRatio);
  const SkColor generated_pressed_color = get_color_for_contrast_ratio(
      base_pressed_color, bg_color, kMinimumPressedContrastRatio);

  const SkColor generated_hovered_icon_color =
      color_utils::BlendForMinContrast(title_color, generated_hovered_color)
          .color;
  const SkColor generated_pressed_icon_color =
      color_utils::BlendForMinContrast(title_color, generated_pressed_color)
          .color;

  return {bg_color,
          title_color,
          generated_icon_color,
          generated_hovered_icon_color,
          generated_pressed_icon_color,
          generated_hovered_color,
          generated_pressed_color};
}

void GM2TabStyle::PaintTab(gfx::Canvas* canvas, const SkPath& clip) const {
  int active_tab_fill_id = 0;
  int active_tab_y_inset = 0;
  if (tab_->GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
    active_tab_y_inset = GetStrokeThickness(true);
  }

  if (tab_->IsActive()) {
    PaintTabBackground(canvas, TAB_ACTIVE, active_tab_fill_id,
                       active_tab_y_inset, nullptr /* clip */);
  } else {
    PaintInactiveTabBackground(canvas, clip);

    const float throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(gfx::ToRoundedInt(throb_value * 0xff),
                             tab_->GetLocalBounds());
      PaintTabBackground(canvas, TAB_ACTIVE, active_tab_fill_id,
                         active_tab_y_inset, nullptr /* clip */);
      canvas->Restore();
    }
  }
}

void GM2TabStyle::SetHoverLocation(const gfx::Point& location) {
  if (hover_controller_)
    hover_controller_->SetLocation(location);
}

void GM2TabStyle::ShowHover(ShowHoverStyle style) {
  if (!hover_controller_)
    return;

  if (style == ShowHoverStyle::kSubtle) {
    hover_controller_->SetSubtleOpacityScale(
        tab_->controller()->GetHoverOpacityForRadialHighlight());
  }
  hover_controller_->Show(style);
}

void GM2TabStyle::HideHover(HideHoverStyle style) {
  if (hover_controller_)
    hover_controller_->Hide(style);
}

TabStyle::SeparatorBounds GM2TabStyle::GetSeparatorBounds(float scale) const {
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab_->bounds(), scale, GetStrokeThickness());
  const int corner_radius = GetCornerRadius() * scale;
  gfx::SizeF separator_size(GetSeparatorSize());
  separator_size.Scale(scale);

  SeparatorBounds separator_bounds;

  separator_bounds.leading =
      gfx::RectF(aligned_bounds.x() + corner_radius,
                 aligned_bounds.y() +
                     (aligned_bounds.height() - separator_size.height()) / 2,
                 separator_size.width(), separator_size.height());

  separator_bounds.trailing = separator_bounds.leading;
  separator_bounds.trailing.set_x(aligned_bounds.right() -
                                  (corner_radius + separator_size.width()));

  gfx::PointF origin(tab_->bounds().origin());
  origin.Scale(scale);
  separator_bounds.leading.Offset(-origin.x(), -origin.y());
  separator_bounds.trailing.Offset(-origin.x(), -origin.y());

  return separator_bounds;
}

TabStyle::SeparatorOpacities GM2TabStyle::GetSeparatorOpacities(
    bool for_layout) const {
  // Something should visually separate tabs from each other and any adjacent
  // new tab button.  Normally, active and hovered tabs draw distinct shapes
  // (via different background colors) and thus need no separators, while
  // background tabs need separators between them.
  float leading_opacity, trailing_opacity;
  if (tab_->IsActive()) {
    leading_opacity = trailing_opacity = 0;
  } else {
    const Tab* subsequent_tab = tab_->controller()->GetAdjacentTab(tab_, 1);
    const Tab* previous_tab = tab_->controller()->GetAdjacentTab(tab_, -1);

    // Fade out the intervening separator while this tab or an adjacent tab is
    // hovered, which prevents sudden opacity changes when scrubbing the mouse
    // across the tabstrip. If that adjacent tab is active, don't consider its
    // hover animation value, otherwise the separator on this tab will disappear
    // while that tab is being dragged.
    auto adjacent_hover_value = [for_layout](const Tab* tab) {
      if (for_layout || !tab || tab->IsActive())
        return 0.f;
      auto* tab_style = static_cast<const GM2TabStyle*>(tab->tab_style());
      return float{tab_style->GetHoverAnimationValue()};
    };
    const float hover_value = GetHoverAnimationValue();
    trailing_opacity =
        1.f - std::max(hover_value, adjacent_hover_value(subsequent_tab));
    leading_opacity =
        1.f - std::max(hover_value, adjacent_hover_value(previous_tab));

    if (tab_->IsSelected()) {
      // Since this tab is selected, its shape will be visible against adjacent
      // unselected tabs, so remove the separator in those cases.
      if (previous_tab && !previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && !subsequent_tab->IsSelected())
        trailing_opacity = 0;
    } else if (tab_->controller()->HasVisibleBackgroundTabShapes()) {
      // Since this tab is unselected, adjacent selected tabs will normally
      // paint atop it, covering the separator.  But if the user drags those
      // selected tabs away, the exposed region looks like the window frame; and
      // since background tab shapes are visible, there should be no separator.
      // TODO(pkasting): https://crbug.com/876599  When a tab is animating
      // into this gap, we should adjust its separator opacities as well.
      if (previous_tab && previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && subsequent_tab->IsSelected())
        trailing_opacity = 0;
    }
  }

  // For the first or (when tab shapes are visible) last tab in the strip, fade
  // the leading or trailing separator based on how close to the target bounds
  // this tab is.  In the steady state, this hides the leading separator; it
  // fades out the separators as tabs animate into these positions, after they
  // pass by the other tabs; and it snaps the separators to full visibility
  // immediately when animating away from these positions, which seems
  // desirable.
  const gfx::Rect target_bounds =
      tab_->controller()->GetTabAnimationTargetBounds(tab_);
  const int tab_width = std::max(tab_->width(), target_bounds.width());
  const float target_opacity =
      float{std::min(std::abs(tab_->x() - target_bounds.x()), tab_width)} /
      tab_width;
  if (tab_->controller()->IsFirstVisibleTab(tab_))
    leading_opacity = target_opacity;
  if (tab_->controller()->IsLastVisibleTab(tab_) &&
      tab_->controller()->HasVisibleBackgroundTabShapes())
    trailing_opacity = target_opacity;

  // Return the opacities in physical order, rather than logical.
  if (base::i18n::IsRTL())
    std::swap(leading_opacity, trailing_opacity);
  return {leading_opacity, trailing_opacity};
}

bool GM2TabStyle::ShouldExtendHitTest() const {
  const views::Widget* widget = tab_->GetWidget();
  return widget->IsMaximized() || widget->IsFullscreen();
}

bool GM2TabStyle::IsHoverActive() const {
  if (!hover_controller_)
    return false;
  return hover_controller_->ShouldDraw();
}

double GM2TabStyle::GetHoverAnimationValue() const {
  if (!hover_controller_)
    return 0.0;
  return hover_controller_->GetAnimationValue();
}

float GM2TabStyle::GetHoverOpacity() const {
  // Opacity boost varies on tab width.  The interpolation is nonlinear so
  // that most tabs will fall on the low end of the opacity range, but very
  // narrow tabs will still stand out on the high end.
  const float range_start = float{GetStandardWidth()};
  const float range_end = float{GetMinimumInactiveWidth()};
  const float value_in_range = float{tab_->width()};
  const float t = base::ClampToRange(
      (value_in_range - range_start) / (range_end - range_start), 0.0f, 1.0f);
  return tab_->controller()->GetHoverOpacityForTab(t * t);
}

float GM2TabStyle::GetThrobValue() const {
  const bool is_selected = tab_->IsSelected();
  double val = is_selected ? kSelectedTabOpacity : 0;

  if (IsHoverActive()) {
    constexpr float kSelectedTabThrobScale = 0.95f - kSelectedTabOpacity;
    const float opacity = GetHoverOpacity();
    const float offset =
        is_selected ? (kSelectedTabThrobScale * opacity) : opacity;
    val += GetHoverAnimationValue() * offset;
  }

  return val;
}

int GM2TabStyle::GetStrokeThickness(bool should_paint_as_active) const {
  return (tab_->IsActive() || should_paint_as_active)
             ? tab_->controller()->GetStrokeThickness()
             : 0;
}

bool GM2TabStyle::ShouldPaintTabBackgroundColor(TabState active_state,
                                                int fill_id) const {
  // In the active case, always paint the tab background. The fill image may be
  // transparent.
  if (active_state == TAB_ACTIVE)
    return true;

  // In the inactive case, the fill image is guaranteed to be opaque, so it's
  // not necessary to paint the background when there is one.
  if (fill_id)
    return false;

  return tab_->GetThemeProvider()->GetDisplayProperty(
      ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
}

SkColor GM2TabStyle::GetTabBackgroundColor(TabState active_state) const {
  SkColor color = tab_->controller()->GetTabBackgroundColor(active_state);

  base::Optional<SkColor> group_color = tab_->GetGroupColor();
  if (group_color.has_value()) {
    if (tab_->IsActive()) {
      color = group_color.value();
    } else {
      // Tint with group color. With a dark scheme, the tint needs a higher
      // contrast to stand out effectively.
      const float target_contrast = color_utils::IsDark(color) ? 1.8f : 1.2f;
      color = color_utils::BlendForMinContrast(
                  color, color, group_color.value(), target_contrast)
                  .color;
    }
  }

  return color;
}

void GM2TabStyle::PaintInactiveTabBackground(gfx::Canvas* canvas,
                                             const SkPath& clip) const {
  bool has_custom_image;
  int fill_id = tab_->controller()->GetBackgroundResourceId(&has_custom_image);
  if (!has_custom_image)
    fill_id = 0;

  PaintTabBackground(canvas, TAB_INACTIVE, fill_id, 0,
                     tab_->controller()->MaySetClip() ? &clip : nullptr);
}

void GM2TabStyle::PaintTabBackground(gfx::Canvas* canvas,
                                     TabState active_state,
                                     int fill_id,
                                     int y_inset,
                                     const SkPath* clip) const {
  // |y_inset| is only set when |fill_id| is being used.
  DCHECK(!y_inset || fill_id);

  const SkColor stroke_color =
      tab_->controller()->GetToolbarTopSeparatorColor();
  const bool paint_hover_effect =
      active_state == TAB_INACTIVE && IsHoverActive();
  const float stroke_thickness = GetStrokeThickness(active_state == TAB_ACTIVE);

  PaintTabBackgroundFill(canvas, active_state, paint_hover_effect, fill_id,
                         y_inset);
  if (stroke_thickness > 0) {
    gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
    if (clip)
      canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
    PaintBackgroundStroke(canvas, active_state, stroke_color);
  }

  PaintSeparators(canvas);
}

void GM2TabStyle::PaintTabBackgroundFill(gfx::Canvas* canvas,
                                         TabState active_state,
                                         bool paint_hover_effect,
                                         int fill_id,
                                         int y_inset) const {
  const SkPath fill_path = GetPath(PathType::kFill, canvas->image_scale(),
                                   active_state == TAB_ACTIVE);
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  if (ShouldPaintTabBackgroundColor(active_state, fill_id)) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetTabBackgroundColor(active_state));
    canvas->DrawRect(gfx::ScaleToEnclosingRect(tab_->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    canvas->TileImageInt(*tab_->GetThemeProvider()->GetImageSkiaNamed(fill_id),
                         tab_->GetMirroredX() + tab_->background_offset(), 0, 0,
                         y_inset, tab_->width(), tab_->height());
  }

  if (paint_hover_effect) {
    SkPoint hover_location(gfx::PointToSkPoint(hover_controller_->location()));
    hover_location.scale(SkFloatToScalar(scale));
    const SkScalar kMinHoverRadius = 16;
    const SkScalar radius =
        std::max(SkFloatToScalar(tab_->width() / 4.f), kMinHoverRadius);
    DrawHighlight(canvas, hover_location, radius * scale,
                  SkColorSetA(GetTabBackgroundColor(TAB_ACTIVE),
                              hover_controller_->GetAlpha()));
  }
}

void GM2TabStyle::PaintBackgroundStroke(gfx::Canvas* canvas,
                                        TabState active_state,
                                        SkColor stroke_color) const {
  SkPath outer_path =
      GetPath(TabStyle::PathType::kBorder, canvas->image_scale(),
              active_state == TAB_ACTIVE);
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(stroke_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(GetStrokeThickness(active_state == TAB_ACTIVE) * scale);
  canvas->DrawPath(outer_path, flags);
}

void GM2TabStyle::PaintSeparators(gfx::Canvas* canvas) const {
  const auto separator_opacities = GetSeparatorOpacities(false);
  if (!separator_opacities.left && !separator_opacities.right)
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  TabStyle::SeparatorBounds separator_bounds = GetSeparatorBounds(scale);

  const SkColor separator_base_color =
      tab_->controller()->GetTabSeparatorColor();
  const auto separator_color = [separator_base_color](float opacity) {
    return SkColorSetA(separator_base_color,
                       gfx::Tween::IntValueBetween(opacity, SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE));
  };

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(separator_color(separator_opacities.left));
  canvas->DrawRect(separator_bounds.leading, flags);
  flags.setColor(separator_color(separator_opacities.right));
  canvas->DrawRect(separator_bounds.trailing, flags);
}

// static
float GM2TabStyle::GetTopCornerRadiusForWidth(int width) {
  // Get the width of the top of the tab by subtracting the width of the outer
  // corners.
  const int ideal_radius = GetCornerRadius();
  const int top_width = width - ideal_radius * 2;

  // To maintain a round-rect appearance, ensure at least one third of the top
  // of the tab is flat.
  const float radius = top_width / 3.f;
  return base::ClampToRange<float>(radius, 0, ideal_radius);
}

// static
gfx::RectF GM2TabStyle::ScaleAndAlignBounds(const gfx::Rect& bounds,
                                            float scale,
                                            int stroke_thickness) {
  // Convert to layout bounds.  We must inset the width such that the right edge
  // of one tab's layout bounds is the same as the left edge of the next tab's;
  // this way the two tabs' separators will be drawn at the same coordinate.
  gfx::RectF aligned_bounds(bounds);
  const int corner_radius = GetCornerRadius();
  // Note: This intentionally doesn't subtract TABSTRIP_TOOLBAR_OVERLAP from the
  // bottom inset, because we want to pixel-align the bottom of the stroke, not
  // the bottom of the overlap.
  gfx::InsetsF layout_insets(stroke_thickness, corner_radius, stroke_thickness,
                             corner_radius + GetSeparatorSize().width());
  aligned_bounds.Inset(layout_insets);

  // Scale layout bounds from DIP to px.
  aligned_bounds.Scale(scale);

  // Snap layout bounds to nearest pixels so we get clean lines.
  const float x = std::round(aligned_bounds.x());
  const float y = std::round(aligned_bounds.y());
  // It's important to round the right edge and not the width, since rounding
  // both x and width would mean the right edge would accumulate error.
  const float right = std::round(aligned_bounds.right());
  const float bottom = std::round(aligned_bounds.bottom());
  aligned_bounds = gfx::RectF(x, y, right - x, bottom - y);

  // Convert back to full bounds.  It's OK that the outer corners of the curves
  // around the separator may not be snapped to the pixel grid as a result.
  aligned_bounds.Inset(-layout_insets.Scale(scale));
  return aligned_bounds;
}

}  // namespace

// TabStyle --------------------------------------------------------------------

TabStyleViews::~TabStyleViews() = default;

// static
std::unique_ptr<TabStyleViews> TabStyleViews::CreateForTab(Tab* tab) {
  return std::make_unique<GM2TabStyle>(tab);
}

// static
int TabStyleViews::GetMinimumActiveWidth() {
  return TabCloseButton::GetWidth() + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyleViews::GetMinimumInactiveWidth() {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  return kInteriorWidth - GetSeparatorSize().width() + GetTabOverlap();
}
