// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/multiline_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

// A simple View that hosts a RenderText object.
class MultilineExample::RenderTextView : public View {
 public:
  RenderTextView() : render_text_(gfx::RenderText::CreateInstance()) {
    render_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    render_text_->SetColor(SK_ColorBLACK);
    render_text_->SetMultiline(true);
    set_border(Border::CreateSolidBorder(2, SK_ColorGRAY));
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    View::OnPaint(canvas);
    render_text_->Draw(canvas);
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Turn off multiline mode to get the single-line text size, which is the
    // preferred size for this view.
    render_text_->SetMultiline(false);
    gfx::Size size(render_text_->GetContentWidth(),
                   render_text_->GetStringSize().height());
    size.Enlarge(GetInsets().width(), GetInsets().height());
    render_text_->SetMultiline(true);
    return size;
  }

  virtual int GetHeightForWidth(int w) OVERRIDE {
    // TODO(ckocagil): Why does this happen?
    if (w == 0)
      return View::GetHeightForWidth(w);
    gfx::Rect rect = render_text_->display_rect();
    rect.set_width(w - GetInsets().width());
    render_text_->SetDisplayRect(rect);
    return render_text_->GetStringSize().height() + GetInsets().height();
  }

  void SetText(const string16& new_contents) {
    // Color and style the text inside |test_range| to test colors and styles.
    gfx::Range test_range(1, 21);
    test_range.set_start(std::min(test_range.start(), new_contents.length()));
    test_range.set_end(std::min(test_range.end(), new_contents.length()));

    render_text_->SetText(new_contents);
    render_text_->SetColor(SK_ColorBLACK);
    render_text_->ApplyColor(0xFFFF0000, test_range);
    render_text_->SetStyle(gfx::DIAGONAL_STRIKE, false);
    render_text_->ApplyStyle(gfx::DIAGONAL_STRIKE, true, test_range);
    render_text_->SetStyle(gfx::UNDERLINE, false);
    render_text_->ApplyStyle(gfx::UNDERLINE, true, test_range);
    InvalidateLayout();
  }

 private:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(GetInsets());
    render_text_->SetDisplayRect(bounds);
  }

  scoped_ptr<gfx::RenderText> render_text_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextView);
};

MultilineExample::MultilineExample()
    : ExampleBase("Multiline RenderText"),
      render_text_view_(NULL),
      label_(NULL),
      textfield_(NULL),
      label_checkbox_(NULL) {
}

MultilineExample::~MultilineExample() {
}

void MultilineExample::CreateExampleView(View* container) {
  const char kTestString[] = "test string asdf 1234 test string asdf 1234 "
                             "test string asdf 1234 test string asdf 1234";

  render_text_view_ = new RenderTextView();
  render_text_view_->SetText(ASCIIToUTF16(kTestString));

  label_ = new Label();
  label_->SetText(ASCIIToUTF16(kTestString));
  label_->SetMultiLine(true);
  label_->set_border(Border::CreateSolidBorder(2, SK_ColorCYAN));

  label_checkbox_ = new Checkbox(ASCIIToUTF16("views::Label:"));
  label_checkbox_->SetChecked(true);
  label_checkbox_->set_listener(this);
  label_checkbox_->set_request_focus_on_press(false);

  textfield_ = new Textfield();
  textfield_->SetController(this);
  textfield_->SetText(ASCIIToUTF16(kTestString));

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

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
  layout->AddView(new Label(ASCIIToUTF16("Sample Text:")));
  layout->AddView(textfield_);
}

void MultilineExample::ContentsChanged(Textfield* sender,
                                       const string16& new_contents) {
  render_text_view_->SetText(new_contents);
  if (label_checkbox_->checked())
    label_->SetText(new_contents);
  container()->Layout();
  container()->SchedulePaint();
}

bool MultilineExample::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  return false;
}

void MultilineExample::ButtonPressed(Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, label_checkbox_);
  label_->SetText(label_checkbox_->checked() ? textfield_->text() : string16());
  container()->Layout();
  container()->SchedulePaint();
}

}  // namespace examples
}  // namespace views
