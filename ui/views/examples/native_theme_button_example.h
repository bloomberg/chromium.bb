// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_NATIVE_THEME_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_NATIVE_THEME_BUTTON_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/native_theme.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/button/custom_button.h"
#include "views/controls/combobox/combobox_listener.h"
#include "views/native_theme_delegate.h"
#include "views/native_theme_painter.h"

namespace views {
class Combobox;
class NativeThemePainter;
}

namespace examples {

// A subclass of button to test native theme rendering.
class ExampleNativeThemeButton : public views::CustomButton,
                                 public views::NativeThemeDelegate,
                                 public views::ComboboxListener {
 public:
  ExampleNativeThemeButton(views::ButtonListener* listener,
                           views::Combobox* cb_part,
                           views::Combobox* cb_state);
  virtual ~ExampleNativeThemeButton();

  std::string MessWithState();

 private:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas)  OVERRIDE;

  // Overridden from views::ComboboxListener:
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;

  // Overridden from views::NativeThemePainter::Delegate:
  virtual gfx::NativeTheme::Part GetThemePart() const OVERRIDE;
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;
  virtual gfx::NativeTheme::State GetThemeState(
      gfx::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual const ui::Animation* GetThemeAnimation() const OVERRIDE;
  virtual gfx::NativeTheme::State GetBackgroundThemeState(
      gfx::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual gfx::NativeTheme::State GetForegroundThemeState(
      gfx::NativeTheme::ExtraParams* params) const OVERRIDE;

  void GetExtraParams(gfx::NativeTheme::ExtraParams* params) const;

  scoped_ptr<views::NativeThemePainter> painter_;
  views::Combobox* cb_part_;
  views::Combobox* cb_state_;
  int count_;
  bool is_checked_;
  bool is_indeterminate_;

  DISALLOW_COPY_AND_ASSIGN(ExampleNativeThemeButton);
};

// NativeThemeButtonExample shows how a View can use the NativeThemePainter
// to paints its background and get a native look.
class NativeThemeButtonExample : public ExampleBase,
                                 public views::ButtonListener {
 public:
  explicit NativeThemeButtonExample(ExamplesMain* main);
  virtual ~NativeThemeButtonExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The only control in this test.
  ExampleNativeThemeButton* button_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeButtonExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_NATIVE_THEME_BUTTON_EXAMPLE_H_
