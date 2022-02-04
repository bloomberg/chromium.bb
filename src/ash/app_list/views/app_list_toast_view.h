// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
class ImageView;
}  // namespace views

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A view specific to AppList that displays an icon, two labels and a button.
// The view can be built with all or some of the elements mentioned before, but
// always will have to include at least a title. The view has a grid-like layout
// with 3 columns where the icon will be on the first column, the button on the
// last column and the title and subtitle will share the middle column, with the
// title over the subtitle.
class ASH_EXPORT AppListToastView : public views::View {
 public:
  class Builder {
   public:
    explicit Builder(const std::u16string title);
    virtual ~Builder();

    std::unique_ptr<AppListToastView> Build();

    // Methods for setting vector icons for the toast.
    // Vector icons would change appearance with theming by default.
    // Nevertheless there might be a case when different icons need to be used
    // with dark/light mode (i.e. non-monochromatic icons) and a single icon is
    // not enough. For this case, use SetThemingIcons().
    Builder& SetIcon(const gfx::VectorIcon* icon);
    Builder& SetThemingIcons(const gfx::VectorIcon* dark_icon,
                             const gfx::VectorIcon* light_icon);

    Builder& SetSubtitle(const std::u16string subtitle);
    Builder& SetButton(std::u16string button_text,
                       views::Button::PressedCallback button_callback);
    Builder& SetStyleForTabletMode(bool style_for_tablet_mode);

   private:
    std::u16string title_;
    absl::optional<std::u16string> subtitle_;
    absl::optional<std::u16string> button_text_;
    const gfx::VectorIcon* icon_ = nullptr;
    const gfx::VectorIcon* dark_icon_ = nullptr;
    const gfx::VectorIcon* light_icon_ = nullptr;
    views::Button::PressedCallback button_callback_;
    bool has_button_ = false;
    bool style_for_tablet_mode_ = false;
  };

  explicit AppListToastView(const std::u16string title);
  AppListToastView(const AppListToastView&) = delete;
  AppListToastView& operator=(const AppListToastView&) = delete;
  ~AppListToastView() override;

  // views::View:
  gfx::Size GetMaximumSize() const override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;

  void SetButton(std::u16string button_text,
                 views::Button::PressedCallback button_callback);

  void SetIcon(const gfx::VectorIcon* icon);
  void SetThemingIcons(const gfx::VectorIcon* dark_icon,
                       const gfx::VectorIcon* light_icon);
  void SetTitle(const std::u16string title);
  void SetSubtitle(const std::u16string subtitle);

  // Styles the toast for display in tablet mode launcher UI - for example, adds
  // background blur, and sets rounded corners on the toast layer.
  void StyleForTabletMode();

  views::LabelButton* toast_button() const { return toast_button_; }

 private:
  // Attach the icon to the toast based on theming and available icons.
  void UpdateIconImage();
  // Creates an ImageView for the icon and inserts it in the toast view.
  void CreateIconView();

  // Vector icons to use with dark/light mode.
  const gfx::VectorIcon* dark_icon_ = nullptr;
  const gfx::VectorIcon* light_icon_ = nullptr;

  // Vector icon to use if there are not dark or light mode specific icons.
  const gfx::VectorIcon* default_icon_ = nullptr;

  // Whether the toast UI should be style for tablet mode app list UI.
  bool style_for_tablet_mode_ = false;

  // Toast icon view.
  views::ImageView* icon_ = nullptr;
  // Label with the main text for the toast.
  views::Label* title_label_ = nullptr;
  // Label with the subtext for the toast.
  views::Label* subtitle_label_ = nullptr;
  // The button for the toast.
  views::LabelButton* toast_button_ = nullptr;
  // Helper view to layout labels.
  views::View* label_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_TOAST_VIEW_H_
