// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/multiline_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/events/event.h"
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
  explicit RenderTextView(gfx::RenderText* render_text)
      : render_text_(render_text) {
    render_text_->SetColor(SK_ColorBLACK);
    set_border(Border::CreateSolidBorder(2, SK_ColorGRAY));
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    View::OnPaint(canvas);
    render_text_->Draw(canvas);
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(0,
        render_text_->font_list().GetHeight() + GetInsets().height());
  }

  void SetText(const string16& new_contents) {
    render_text_->SetText(new_contents);
    SchedulePaint();
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
      textfield_(NULL) {
}

MultilineExample::~MultilineExample() {
}

void MultilineExample::CreateExampleView(View* container) {
  const char kTestString[] = "test string asdf 1234 test string asdf 1234 "
                             "test string asdf 1234 test string asdf 1234";

  gfx::RenderText* render_text = gfx::RenderText::CreateInstance();
  render_text->SetText(ASCIIToUTF16(kTestString));
  render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  render_text_view_ = new RenderTextView(render_text);

  label_ = new Label();
  label_->SetText(ASCIIToUTF16(kTestString));
  label_->SetMultiLine(true);
  label_->set_border(Border::CreateSolidBorder(2, SK_ColorCYAN));

  textfield_ = new Textfield();
  textfield_->SetController(this);
  textfield_->SetText(ASCIIToUTF16(kTestString));

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
      0.0f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
      1.0f, GridLayout::FIXED, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("gfx::RenderText:")));
  layout->AddView(render_text_view_);

  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("views::Label:")));
  layout->AddView(label_);

  layout->StartRow(0, 0);
  layout->AddView(new Label(ASCIIToUTF16("Sample Text:")));
  layout->AddView(textfield_);
}

void MultilineExample::ContentsChanged(Textfield* sender,
                                       const string16& new_contents) {
  render_text_view_->SetText(new_contents);
  label_->SetText(new_contents);
}

bool MultilineExample::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  return false;
}

}  // namespace examples
}  // namespace views
