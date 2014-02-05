// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/text_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

// Number of columns in the view layout.
const int kNumColumns = 10;

const char kShortText[] = "Batman";
const char kMediumText[] = "The quick brown fox jumps over the lazy dog.";
const char kLongText[] =
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum.";
const char kAmpersandText[] =
    "The quick && &brown fo&x jumps over the lazy dog.";
const char kNewlineText[] =
    "The quick \nbrown fox jumps\n\n over the lazy dog.";

const char* kTextExamples[] = {
    "Short",
    "Medium",
    "Long",
    "Ampersands",
    "Newlines",
};

const char* kElidingBehaviors[] = {
    "Ellipsis",
    "None",
    "Fade Tail",
    "Fade Head",
};

const char* kPrefixOptions[] = {
    "Default",
    "Show",
    "Hide",
};

const char* kHorizontalAligments[] = {
    "Default",
    "Left",
    "Center",
    "Right",
};

// Toggles bit |flag| on |flags| based on state of |checkbox|.
void SetFlagFromCheckbox(Checkbox* checkbox, int* flags, int flag) {
  if (checkbox->checked())
    *flags |= flag;
  else
    *flags &= ~flag;
}

}  // namespace

// TextExample's content view, which is responsible for drawing a string with
// the specified style.
class TextExample::TextExampleView : public View {
 public:
  TextExampleView()
    : text_(ASCIIToUTF16(kShortText)),
      text_flags_(0),
      halo_(false),
      fade_(false),
      fade_mode_(gfx::Canvas::TruncateFadeTail) {
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    View::OnPaint(canvas);
    const gfx::Rect bounds = GetContentsBounds();

    if (fade_) {
      canvas->DrawFadeTruncatingStringRect(text_, fade_mode_, font_list_,
                                           SK_ColorDKGRAY, bounds);
    } else if (halo_) {
      canvas->DrawStringRectWithHalo(text_, font_list_, SK_ColorDKGRAY,
                                     SK_ColorWHITE, bounds, text_flags_);
    } else {
      canvas->DrawStringRectWithFlags(text_, font_list_, SK_ColorDKGRAY, bounds,
                                      text_flags_);
    }
  }

  int text_flags() const { return text_flags_; }
  void set_text_flags(int text_flags) { text_flags_ = text_flags; }

  const base::string16& text() const { return text_; }
  void set_text(const base::string16& text) { text_ = text; }

  bool halo() const { return halo_; }
  void set_halo(bool halo) { halo_ = halo; }

  bool fade() const { return fade_; }
  void set_fade(bool fade) { fade_ = fade; }

  gfx::Canvas::TruncateFadeMode fade_mode() const { return fade_mode_; }
  void set_fade_mode(gfx::Canvas::TruncateFadeMode mode) { fade_mode_ = mode; }

  int GetFontStyle() const {
    return font_list_.GetFontStyle();
  }
  void SetFontStyle(int style) {
    font_list_ = font_list_.DeriveWithStyle(style);
  }

 private:
  // The font used for drawing the text.
  gfx::FontList font_list_;

  // The text to draw.
  base::string16 text_;

  // Text flags for passing to |DrawStringRect()|.
  int text_flags_;

  // If |true|, specifies to call |DrawStringWithHalo()| instead of
  // |DrawStringRect()|.
  bool halo_;

  // If |true|, specifies to call |DrawFadeTruncatingString()| instead of
  // |DrawStringRect()|.
  bool fade_;

  // If |fade_| is |true|, fade mode parameter to |DrawFadeTruncatingString()|.
  gfx::Canvas::TruncateFadeMode fade_mode_;

  DISALLOW_COPY_AND_ASSIGN(TextExampleView);
};

TextExample::TextExample() : ExampleBase("Text Styles") {
}

TextExample::~TextExample() {
  // Remove all the views first as some reference models in
  // |example_combobox_model_|.
  container()->RemoveAllChildViews(true);
}

Checkbox* TextExample::AddCheckbox(GridLayout* layout, const char* name) {
  Checkbox* checkbox = new Checkbox(ASCIIToUTF16(name));
  checkbox->set_listener(this);
  layout->AddView(checkbox);
  return checkbox;
}

Combobox* TextExample::AddCombobox(GridLayout* layout,
                                   const char* name,
                                   const char** strings,
                                   int count) {
  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16(name)));
  ExampleComboboxModel* combobox_model = new ExampleComboboxModel(strings,
                                                                  count);
  example_combobox_model_.push_back(combobox_model);
  Combobox* combobox = new Combobox(combobox_model);
  combobox->SetSelectedIndex(0);
  combobox->set_listener(this);
  layout->AddView(combobox, kNumColumns - 1, 1);
  return combobox;
}

void TextExample::CreateExampleView(View* container) {
  text_view_ = new TextExampleView;
  text_view_->SetBorder(Border::CreateSolidBorder(1, SK_ColorGRAY));

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  layout->AddPaddingRow(0, 8);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0.1f, GridLayout::USE_PREF, 0, 0);
  for (int i = 0; i < kNumColumns - 1; i++)
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                          0.1f, GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 8);

  h_align_cb_ = AddCombobox(layout,
                            "H-Align",
                            kHorizontalAligments,
                            arraysize(kHorizontalAligments));
  eliding_cb_ = AddCombobox(layout,
                            "Eliding",
                            kElidingBehaviors,
                            arraysize(kElidingBehaviors));
  prefix_cb_ = AddCombobox(layout,
                           "Prefix",
                           kPrefixOptions,
                           arraysize(kPrefixOptions));
  text_cb_ = AddCombobox(layout,
                         "Example Text",
                         kTextExamples,
                         arraysize(kTextExamples));

  layout->StartRow(0, 0);
  multiline_checkbox_ = AddCheckbox(layout, "Multiline");
  break_checkbox_ = AddCheckbox(layout, "Character Break");
  halo_checkbox_ = AddCheckbox(layout, "Text Halo");
  bold_checkbox_ = AddCheckbox(layout, "Bold");
  italic_checkbox_ = AddCheckbox(layout, "Italic");
  underline_checkbox_ = AddCheckbox(layout, "Underline");

  layout->AddPaddingRow(0, 32);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, 16);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        1, GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 16);
  layout->StartRow(1, 1);
  layout->AddView(text_view_);

  layout->AddPaddingRow(0, 8);
}

void TextExample::ButtonPressed(Button* button, const ui::Event& event) {
  int flags = text_view_->text_flags();
  int style = text_view_->GetFontStyle();
  SetFlagFromCheckbox(multiline_checkbox_, &flags, gfx::Canvas::MULTI_LINE);
  SetFlagFromCheckbox(break_checkbox_, &flags, gfx::Canvas::CHARACTER_BREAK);
  SetFlagFromCheckbox(bold_checkbox_, &style, gfx::Font::BOLD);
  SetFlagFromCheckbox(italic_checkbox_, &style, gfx::Font::ITALIC);
  SetFlagFromCheckbox(underline_checkbox_, &style, gfx::Font::UNDERLINE);
  text_view_->set_halo(halo_checkbox_->checked());
  text_view_->set_text_flags(flags);
  text_view_->SetFontStyle(style);
  text_view_->SchedulePaint();
}

void TextExample::OnPerformAction(Combobox* combobox) {
  int text_flags = text_view_->text_flags();
  if (combobox == h_align_cb_) {
    text_flags &= ~(gfx::Canvas::TEXT_ALIGN_LEFT |
                    gfx::Canvas::TEXT_ALIGN_CENTER |
                    gfx::Canvas::TEXT_ALIGN_RIGHT);
    switch (combobox->selected_index()) {
      case 0:
        break;
      case 1:
        text_flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
        break;
      case 2:
        text_flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
        break;
      case 3:
        text_flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
        break;
    }
  } else if (combobox == text_cb_) {
    switch (combobox->selected_index()) {
      case 0:
        text_view_->set_text(ASCIIToUTF16(kShortText));
        break;
      case 1:
        text_view_->set_text(ASCIIToUTF16(kMediumText));
        break;
      case 2:
        text_view_->set_text(ASCIIToUTF16(kLongText));
        break;
      case 3:
        text_view_->set_text(ASCIIToUTF16(kAmpersandText));
        break;
      case 4:
        text_view_->set_text(ASCIIToUTF16(kNewlineText));
        break;
    }
  } else if (combobox == eliding_cb_) {
    switch (combobox->selected_index()) {
      case 0:
        text_flags &= ~gfx::Canvas::NO_ELLIPSIS;
        text_view_->set_fade(false);
        break;
      case 1:
        text_flags |= gfx::Canvas::NO_ELLIPSIS;
        text_view_->set_fade(false);
        break;
      case 2:
        text_view_->set_fade_mode(gfx::Canvas::TruncateFadeTail);
        text_view_->set_fade(true);
        break;
      case 3:
        text_view_->set_fade_mode(gfx::Canvas::TruncateFadeHead);
        text_view_->set_fade(true);
        break;
    }
  } else if (combobox == prefix_cb_) {
    text_flags &= ~(gfx::Canvas::SHOW_PREFIX | gfx::Canvas::HIDE_PREFIX);
    switch (combobox->selected_index()) {
      case 0:
        break;
      case 1:
        text_flags |= gfx::Canvas::SHOW_PREFIX;
        break;
      case 2:
        text_flags |= gfx::Canvas::HIDE_PREFIX;
        break;
    }
  }
  text_view_->set_text_flags(text_flags);
  text_view_->SchedulePaint();
}

}  // namespace examples
}  // namespace views
