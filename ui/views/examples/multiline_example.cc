// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/multiline_example.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/gfx/render_text.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

gfx::Range ClampRange(gfx::Range range, uint32_t max) {
  range.set_start(std::min(range.start(), max));
  range.set_end(std::min(range.end(), max));
  return range;
}

// A Label with a clamped preferred width to demonstrate wrapping.
class PreferredSizeLabel : public Label {
 public:
  PreferredSizeLabel() = default;
  ~PreferredSizeLabel() override = default;

  // Label:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(50, Label::CalculatePreferredSize().height());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferredSizeLabel);
};

}  // namespace

// A simple View that hosts a RenderText object.
class MultilineExample::RenderTextView : public View {
 public:
  RenderTextView() : render_text_(gfx::RenderText::CreateHarfBuzzInstance()) {
    render_text_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    render_text_->SetColor(SK_ColorBLACK);
    render_text_->SetMultiline(true);
    SetBorder(CreateSolidBorder(2, SK_ColorGRAY));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);
    render_text_->Draw(canvas);
  }

  gfx::Size CalculatePreferredSize() const override {
    // Turn off multiline mode to get the single-line text size, which is the
    // preferred size for this view.
    render_text_->SetMultiline(false);
    gfx::Size size(render_text_->GetContentWidth(),
                   render_text_->GetStringSize().height());
    size.Enlarge(GetInsets().width(), GetInsets().height());
    render_text_->SetMultiline(true);
    return size;
  }

  int GetHeightForWidth(int w) const override {
    // TODO(ckocagil): Why does this happen?
    if (w == 0)
      return View::GetHeightForWidth(w);
    const gfx::Rect old_rect = render_text_->display_rect();
    gfx::Rect rect = old_rect;
    rect.set_width(w - GetInsets().width());
    render_text_->SetDisplayRect(rect);
    int height = render_text_->GetStringSize().height() + GetInsets().height();
    render_text_->SetDisplayRect(old_rect);
    return height;
  }

  void SetText(const base::string16& new_contents) {
    // Color and style the text inside |test_range| to test colors and styles.
    const size_t range_max = new_contents.length();
    gfx::Range color_range = ClampRange(gfx::Range(1, 21), range_max);
    gfx::Range bold_range = ClampRange(gfx::Range(4, 10), range_max);
    gfx::Range italic_range = ClampRange(gfx::Range(7, 13), range_max);

    render_text_->SetText(new_contents);
    render_text_->SetColor(SK_ColorBLACK);
    render_text_->ApplyColor(0xFFFF0000, color_range);
    render_text_->SetStyle(gfx::TEXT_STYLE_UNDERLINE, false);
    render_text_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, true, color_range);
    render_text_->ApplyStyle(gfx::TEXT_STYLE_ITALIC, true, italic_range);
    render_text_->ApplyWeight(gfx::Font::Weight::BOLD, bold_range);
    InvalidateLayout();
  }

  void SetMaxLines(int max_lines) {
    render_text_->SetMaxLines(max_lines);
    render_text_->SetElideBehavior(max_lines ? gfx::ELIDE_TAIL : gfx::NO_ELIDE);
  }

 private:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(GetInsets());
    render_text_->SetDisplayRect(bounds);
  }

  std::unique_ptr<gfx::RenderText> render_text_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextView);
};

MultilineExample::MultilineExample() : ExampleBase("Multiline RenderText") {}

MultilineExample::~MultilineExample() = default;

void MultilineExample::CreateExampleView(View* container) {
  const base::string16 kTestString = base::WideToUTF16(L"qwerty"
      L"\x627\x644\x631\x626\x64A\x633\x64A\x629"
      L"asdfgh");

  render_text_view_ = new RenderTextView();
  render_text_view_->SetText(kTestString);

  label_ = new PreferredSizeLabel();
  label_->SetText(kTestString);
  label_->SetMultiLine(true);
  label_->SetBorder(CreateSolidBorder(2, SK_ColorCYAN));

  label_checkbox_ = new Checkbox(ASCIIToUTF16("views::Label:"), this);
  label_checkbox_->SetChecked(true);
  label_checkbox_->set_request_focus_on_press(false);

  elision_checkbox_ = new Checkbox(ASCIIToUTF16("elide text?"), this);
  elision_checkbox_->SetChecked(false);
  elision_checkbox_->set_request_focus_on_press(false);

  textfield_ = new Textfield();
  textfield_->set_controller(this);
  textfield_->SetText(kTestString);

  GridLayout* layout = container->SetLayoutManager(
      std::make_unique<views::GridLayout>(container));

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
      0.0f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
      1.0f, GridLayout::FIXED, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("gfx::RenderText:")));
  layout->AddView(render_text_view_);

  layout->StartRow(0, 0);
  layout->AddView(label_checkbox_);
  layout->AddView(label_);

  layout->StartRow(0, 0);
  layout->AddView(elision_checkbox_);

  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Sample Text:")));
  layout->AddView(textfield_);
}

void MultilineExample::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  render_text_view_->SetText(new_contents);
  if (label_checkbox_->GetChecked())
    label_->SetText(new_contents);
  container()->InvalidateLayout();
  container()->SchedulePaint();
}

void MultilineExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == label_checkbox_) {
    label_->SetText(label_checkbox_->GetChecked() ? textfield_->text()
                                                  : base::string16());
  } else if (sender == elision_checkbox_) {
    render_text_view_->SetMaxLines(elision_checkbox_->GetChecked() ? 3 : 0);
  }
  container()->InvalidateLayout();
  container()->SchedulePaint();
}

}  // namespace examples
}  // namespace views
