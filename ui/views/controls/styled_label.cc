// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <vector>

#include "base/string_util.h"
#include "ui/base/text/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {

namespace {

// Calculates the height of a line of text. Currently returns the height of
// a label.
int CalculateLineHeight() {
  Label label;
  return label.GetPreferredSize().height();
}

}  // namespace

bool StyledLabel::LinkRange::operator<(
    const StyledLabel::LinkRange& other) const {
  // Intentionally reversed so the priority queue is sorted by smallest first.
  return range.start() > other.range.start();
}

StyledLabel::StyledLabel(const string16& text, StyledLabelListener* listener)
    : text_(text),
      listener_(listener) {}

StyledLabel::~StyledLabel() {}

void StyledLabel::SetText(const string16& text) {
  text_ = text;
  calculated_size_ = gfx::Size();
  link_ranges_ = std::priority_queue<LinkRange>();
  RemoveAllChildViews(true);
  PreferredSizeChanged();
}

void StyledLabel::AddLink(const ui::Range& range) {
  DCHECK(!range.is_reversed());
  DCHECK(!range.is_empty());
  DCHECK(ui::Range(0, text_.size()).Contains(range));
  link_ranges_.push(LinkRange(range));
  calculated_size_ = gfx::Size();
  PreferredSizeChanged();
}

gfx::Insets StyledLabel::GetInsets() const {
  gfx::Insets insets = View::GetInsets();
  const gfx::Insets focus_border_padding(1, 1, 1, 1);
  insets += focus_border_padding;
  return insets;
}

int StyledLabel::GetHeightForWidth(int w) {
  if (w != calculated_size_.width())
    calculated_size_ = gfx::Size(w, CalculateAndDoLayout(w, true));

  return calculated_size_.height();
}

void StyledLabel::Layout() {
  CalculateAndDoLayout(GetLocalBounds().width(), false);
}

void StyledLabel::LinkClicked(Link* source, int event_flags) {
  listener_->StyledLabelLinkClicked(link_targets_[source], event_flags);
}

int StyledLabel::CalculateAndDoLayout(int width, bool dry_run) {
  if (!dry_run) {
    RemoveAllChildViews(true);
    link_targets_.clear();
  }

  width -= GetInsets().width();
  if (width <= 0 || text_.empty())
    return 0;

  const int line_height = CalculateLineHeight();
  // The index of the line we're on.
  int line = 0;
  // The x position (in pixels) of the line we're on, relative to content
  // bounds.
  int x = 0;

  string16 remaining_string = text_;
  std::priority_queue<LinkRange> link_ranges = link_ranges_;

  // Iterate over the text, creating a bunch of labels and links and laying them
  // out in the appropriate positions.
  while (!remaining_string.empty()) {
    // Don't put whitespace at beginning of a line.
    if (x == 0)
      TrimWhitespace(remaining_string, TRIM_LEADING, &remaining_string);

    ui::Range range(ui::Range::InvalidRange());
    if (!link_ranges.empty())
      range = link_ranges.top().range;

    const gfx::Rect chunk_bounds(x, 0, width - x, 2 * line_height);
    std::vector<string16> substrings;
    ui::ElideRectangleText(remaining_string,
                           gfx::Font(),
                           chunk_bounds.width(),
                           chunk_bounds.height(),
                           ui::IGNORE_LONG_WORDS,
                           &substrings);

    string16 chunk = substrings[0];
    if (chunk.empty()) {
      // Nothing fit on this line. Start a new line. If x is 0, there's no room
      // for anything. Just abort.
      if (x == 0)
        break;

      x = 0;
      line++;
      continue;
    }

    scoped_ptr<View> view;
    const size_t position = text_.size() - remaining_string.size();
    if (position >= range.start()) {
      // This chunk is a link.
      if (chunk.size() < range.length() && x != 0) {
        // Don't wrap links. Try to fit them entirely on one line.
        x = 0;
        line++;
        continue;
      }

      chunk = chunk.substr(0, range.length());
      Link* link = new Link(chunk);
      link->set_listener(this);
      if (!dry_run)
        link_targets_[link] = range;
      view.reset(link);
      link_ranges.pop();
    } else {
      // This chunk is normal text.
      if (position + chunk.size() > range.start())
        chunk = chunk.substr(0, range.start() - position);

      Label* label = new Label(chunk);
      // Give the label a focus border so that its preferred size matches
      // links' preferred sizes.
      label->SetHasFocusBorder(true);
      view.reset(label);
    }

    // Lay out the views to overlap by 1 pixel to compensate for their border
    // spacing. Otherwise, "<a>link</a>," will render as "link ,".
    const int overlap = 1;
    const gfx::Size view_size = view->GetPreferredSize();
    DCHECK_EQ(line_height, view_size.height() - 2 * overlap);
    if (!dry_run) {
      view->SetBoundsRect(gfx::Rect(
          gfx::Point(GetInsets().left() + x - overlap,
                     GetInsets().top() + line * line_height - overlap),
          view_size));
      AddChildView(view.release());
    }
    x += view_size.width() - 2 * overlap;

    remaining_string = remaining_string.substr(chunk.size());
  }

  return (line + 1) * line_height + GetInsets().height();
}

}  // namespace views
