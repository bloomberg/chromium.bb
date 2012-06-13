// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_result_view.h"

#include "ui/app_list/search_result.h"
#include "ui/app_list/search_result_list_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/image_view.h"

namespace {

const int kPreferredWidth = 300;
const int kPreferredHeight = 52;
const int kIconDimension = 32;
const int kIconPadding = 14;
const int kIconViewWidth = kIconDimension + 2 * kIconPadding;
const int kTextTrailPadding = kIconPadding;
const int kBorderSize = 1;

const SkColor kBorderColor = SkColorSetARGB(0x0F, 0, 0, 0);

const SkColor kDefaultTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kDimmedTextColor = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kURLTextColor = SkColorSetRGB(0x00, 0x99, 0x33);

const SkColor kSelectedBorderColor = kBorderColor;
const SkColor kSelectedBackgroundColor = SkColorSetARGB(0x0F, 0x4D, 0x90, 0xFE);
const SkColor kHoverAndPushedColor = SkColorSetARGB(0x05, 0, 0, 0);

// A non-interactive image view to display result icon.
class IconView : public views::ImageView {
 public:
  IconView() : ImageView() {}
  virtual ~IconView() {}

 private:
  // views::View overrides:
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(IconView);
};

// Creates a RenderText of given |text| and |styles|. Callers takes ownership
// of returned RenderText.
gfx::RenderText* CreateRenderText(const string16& text,
                                  const app_list::SearchResult::Tags& tags) {
  gfx::RenderText* render_text = gfx::RenderText::CreateRenderText();
  render_text->SetText(text);

  gfx::StyleRange default_style;
  default_style.foreground = kDefaultTextColor;
  render_text->set_default_style(default_style);
  render_text->ApplyDefaultStyle();

  for (app_list::SearchResult::Tags::const_iterator it = tags.begin();
       it != tags.end();
       ++it) {
    // NONE means default style so do nothing.
    if (it->styles == app_list::SearchResult::Tag::NONE)
      continue;

    gfx::StyleRange style;
    style.range = it->range;

    if (it->styles & app_list::SearchResult::Tag::MATCH)
      style.font_style = gfx::Font::BOLD;
    if (it->styles & app_list::SearchResult::Tag::URL)
      style.foreground = kURLTextColor;
    if (it->styles & app_list::SearchResult::Tag::DIM)
      style.foreground = kDimmedTextColor;

    render_text->ApplyStyleRange(style);
  }

  return render_text;
}

}  // namespace

namespace app_list {

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

SearchResultView::SearchResultView(SearchResultListView* list_view,
                                   views::ButtonListener* listener)
    : views::CustomButton(listener),
      result_(NULL),
      list_view_(list_view),
      icon_(NULL) {
  icon_ = new IconView;
  AddChildView(icon_);
}

SearchResultView::~SearchResultView() {
  if (result_)
    result_->RemoveObserver(this);
}

void SearchResultView::SetResult(SearchResult* result) {
  if (result_)
    result_->RemoveObserver(this);

  result_ = result;

  if (result_)
    result_->AddObserver(this);

  icon_->SetImage(result_ ? result_->icon() : gfx::ImageSkia());
  UpdateTitleText();
  UpdateDetailsText();
  SchedulePaint();
}

void SearchResultView::UpdateTitleText() {
  if (!result_ || result_->title().empty()) {
    title_text_.reset();
  } else {
    title_text_.reset(CreateRenderText(result_->title(),
                                       result_->title_tags()));
  }
}

void SearchResultView::UpdateDetailsText() {
  if (!result_ || result_->details().empty()) {
    details_text_.reset();
  } else {
    details_text_.reset(CreateRenderText(result_->details(),
                                         result_->details_tags()));
  }
}

std::string SearchResultView::GetClassName() const {
  return kViewClassName;
}

gfx::Size SearchResultView::GetPreferredSize() {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchResultView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_bounds(rect);
  icon_bounds.set_width(kIconViewWidth);
  icon_bounds.Inset(kIconPadding, (rect.height() - kIconDimension) / 2);
  icon_->SetBoundsRect(icon_bounds.Intersect(rect));
}

void SearchResultView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect content_rect(rect);
  content_rect.set_height(rect.height() - kBorderSize);

  bool selected = list_view_->IsResultViewSelected(this);
  if (selected) {
    canvas->FillRect(content_rect, kSelectedBackgroundColor);
  } else if (state() == BS_HOT || state() == BS_PUSHED) {
    canvas->FillRect(content_rect, kHoverAndPushedColor);
  }

  gfx::Rect border_bottom = rect.Subtract(content_rect);
  canvas->FillRect(border_bottom,
                   selected ? kSelectedBorderColor : kBorderColor);

  gfx::Rect text_bounds(rect);
  text_bounds.set_x(kIconViewWidth);
  text_bounds.set_width(rect.width() - kIconViewWidth - kTextTrailPadding);

  if (title_text_.get() && details_text_.get()) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Size details_size(text_bounds.width(),
                           details_text_->GetStringSize().height());
    int total_height = title_size.height() + + details_size.height();
    int y = text_bounds.y() + (text_bounds.height() - total_height) / 2;

    title_text_->SetDisplayRect(gfx::Rect(gfx::Point(text_bounds.x(), y),
                                          title_size));
    title_text_->Draw(canvas);

    y += title_size.height();
    details_text_->SetDisplayRect(gfx::Rect(gfx::Point(text_bounds.x(), y),
                                            details_size));
    details_text_->Draw(canvas);
  } else if (title_text_.get()) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    title_text_->SetDisplayRect(text_bounds.Center(title_size));
    title_text_->Draw(canvas);
  }
}

void SearchResultView::OnIconChanged() {
  SchedulePaint();
}

}  // namespace app_list
