// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/text_example.h"

#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/layout/grid_layout.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/view.h"

namespace {

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

const char* kTextExamples[] = {
    "Short",
    "Medium",
    "Long",
    "Ampersands",
};

const char* kElidingBehaviors[] = {
    "Ellipsis",
    "None",
#if defined(OS_WIN)
    "Fade Tail",
    "Fade Head",
    "Fade Head and Tail",
#endif
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

const char* kVerticalAlignments[] = {
    "Default",
    "Top",
    "Middle",
    "Bottom",
};

}  // namespace

namespace examples {

// TextExample's content view, which is responsible for drawing a string with
// the specified style.
class TextExample::TextExampleView : public views::View {
 public:
  TextExampleView()
    : font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      text_(ASCIIToUTF16(kShortText)),
      text_flags_(0),
      fade_(false),
      fade_mode_(gfx::CanvasSkia::TruncateFadeTail) {
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
#if defined(OS_WIN)
    if (fade_) {
      gfx::Rect rect(0, 0, width(), height());
      size_t characters_to_truncate_from_head =
          gfx::CanvasSkia::TruncateFadeHeadAndTail ? 10 : 0;
      canvas->AsCanvasSkia()->DrawFadeTruncatingString(text_, fade_mode_,
          characters_to_truncate_from_head, font_, SK_ColorDKGRAY, rect);
      return;
    }
#endif

    canvas->DrawStringInt(text_, font_, SK_ColorDKGRAY,
                          0, 0, width(), height(), text_flags_);
  }

  int text_flags() const { return text_flags_; }
  void set_text_flags(int text_flags) { text_flags_ = text_flags; }

  const string16& text() const { return text_; }
  void set_text(const string16& text) { text_ = text; }

  bool fade() const { return fade_; }
  void set_fade(bool fade) { fade_ = fade; }

  gfx::CanvasSkia::TruncateFadeMode fade_mode() const { return fade_mode_; }
  void set_fade_mode(gfx::CanvasSkia::TruncateFadeMode fade_mode) {
    fade_mode_ = fade_mode;
  }

 private:
  // The font used for drawing the text.
  gfx::Font font_;

  // The text to draw.
  string16 text_;

  // Text flags for passing to |DrawStringInt()|.
  int text_flags_;

  // If |true|, specifies to call |DrawFadeTruncatingString()| instead of
  // |DrawStringInt()|.
  bool fade_;

  // If |fade_| is |true|, fade mode parameter to |DrawFadeTruncatingString()|.
  gfx::CanvasSkia::TruncateFadeMode fade_mode_;

  DISALLOW_COPY_AND_ASSIGN(TextExampleView);
};

TextExample::TextExample(ExamplesMain* main)
    : ExampleBase(main, "Text Styles") {
}

TextExample::~TextExample() {
}

views::Combobox* TextExample::AddCombobox(views::GridLayout* layout,
                                          const char* name,
                                          const char** strings,
                                          int count) {
  layout->StartRow(0, 0);
  layout->AddView(new views::Label(ASCIIToUTF16(name)));
  views::Combobox* combo_box =
      new views::Combobox(new ExampleComboboxModel(strings, count));
  combo_box->SetSelectedItem(0);
  combo_box->set_listener(this);
  layout->AddView(combo_box);
  return combo_box;
}

void TextExample::CreateExampleView(views::View* container) {
  text_view_ = new TextExampleView;

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  layout->AddPaddingRow(0, 8);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        0.1f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.9f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 8);

  h_align_cb_ = AddCombobox(layout,
                            "H-Align",
                            kHorizontalAligments,
                            arraysize(kHorizontalAligments));
  v_align_cb_ = AddCombobox(layout,
                            "V-Align",
                            kVerticalAlignments,
                            arraysize(kVerticalAlignments));
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
  multiline_checkbox_ = new views::Checkbox(ASCIIToUTF16("Multiline"));
  multiline_checkbox_->set_listener(this);
  layout->AddView(multiline_checkbox_);
  break_checkbox_ = new views::Checkbox(ASCIIToUTF16("Character Break"));
  break_checkbox_->set_listener(this);
  layout->AddView(break_checkbox_);

  layout->AddPaddingRow(0, 32);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, 16);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 16);
  layout->StartRow(1, 1);
  layout->AddView(text_view_);

  layout->AddPaddingRow(0, 8);
}

void TextExample::ButtonPressed(views::Button* button,
                                const views::Event& event) {
  int text_flags = text_view_->text_flags();
  if (button == multiline_checkbox_) {
    if (multiline_checkbox_->checked())
      text_flags |= gfx::Canvas::MULTI_LINE;
    else
      text_flags &= ~gfx::Canvas::MULTI_LINE;
  } else if (button == break_checkbox_) {
    if (break_checkbox_->checked())
      text_flags |= gfx::Canvas::CHARACTER_BREAK;
    else
      text_flags &= ~gfx::Canvas::CHARACTER_BREAK;
  }
  text_view_->set_text_flags(text_flags);
  text_view_->SchedulePaint();
}

void TextExample::ItemChanged(views::Combobox* combo_box,
                              int prev_index,
                              int new_index) {
  int text_flags = text_view_->text_flags();
  if (combo_box == h_align_cb_) {
    text_flags &= ~(gfx::Canvas::TEXT_ALIGN_LEFT |
                    gfx::Canvas::TEXT_ALIGN_CENTER |
                    gfx::Canvas::TEXT_ALIGN_RIGHT);
    switch (new_index) {
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
  } else if (combo_box == v_align_cb_) {
    text_flags &= ~(gfx::Canvas::TEXT_VALIGN_TOP |
                    gfx::Canvas::TEXT_VALIGN_MIDDLE |
                    gfx::Canvas::TEXT_VALIGN_BOTTOM);
    switch (new_index) {
      case 0:
        break;
      case 1:
        text_flags |= gfx::Canvas::TEXT_VALIGN_TOP;
        break;
      case 2:
        text_flags |= gfx::Canvas::TEXT_VALIGN_MIDDLE;
        break;
      case 3:
        text_flags |= gfx::Canvas::TEXT_VALIGN_BOTTOM;
        break;
    }
  } else if (combo_box == text_cb_) {
    switch (new_index) {
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
    }
  } else if (combo_box == eliding_cb_) {
    switch (new_index) {
      case 0:
        text_flags &= ~gfx::Canvas::NO_ELLIPSIS;
        text_view_->set_fade(false);
        break;
      case 1:
        text_flags |= gfx::Canvas::NO_ELLIPSIS;
        text_view_->set_fade(false);
        break;
#if defined(OS_WIN)
      case 2:
        text_view_->set_fade_mode(gfx::CanvasSkia::TruncateFadeTail);
        text_view_->set_fade(true);
        break;
      case 3:
        text_view_->set_fade_mode(gfx::CanvasSkia::TruncateFadeHead);
        text_view_->set_fade(true);
        break;
      case 4:
        text_view_->set_fade_mode(gfx::CanvasSkia::TruncateFadeHeadAndTail);
        text_view_->set_fade(true);
        break;
#endif
    }
  } else if (combo_box == prefix_cb_) {
    text_flags &= ~(gfx::Canvas::SHOW_PREFIX | gfx::Canvas::HIDE_PREFIX);
    switch (new_index) {
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
