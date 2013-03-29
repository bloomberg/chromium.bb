// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_STYLED_LABEL_H_
#define UI_VIEWS_CONTROLS_STYLED_LABEL_H_

#include <map>
#include <queue>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/base/range/range.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

namespace views {

class Link;
class StyledLabelListener;

// A class which can apply mixed styles to a block of text. Currently, text is
// always multiline, and the only style that may be applied is linkifying ranges
// of text.
class VIEWS_EXPORT StyledLabel : public View, public LinkListener {
 public:
  StyledLabel(const string16& text, StyledLabelListener* listener);
  virtual ~StyledLabel();

  // Sets the text to be displayed, and clears any previous styling.
  void SetText(const string16& text);

  // Marks the given range within |text_| as a link.
  void AddLink(const ui::Range& range);

  // View implementation:
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual int GetHeightForWidth(int w) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // LinkListener implementation:
  virtual void LinkClicked(Link* source, int event_flags) OVERRIDE;

 private:
  struct LinkRange {
    explicit LinkRange(const ui::Range& range) : range(range) {}
    ~LinkRange() {}

    bool operator<(const LinkRange& other) const;

    ui::Range range;
  };

  // Calculates how to layout child views, creates them and sets their size
  // and position. |width| is the horizontal space, in pixels, that the view
  // has to work with. If |dry_run| is true, the view hierarchy is not touched.
  // The return value is the height in pixels.
  int CalculateAndDoLayout(int width, bool dry_run);

  // The text to display.
  string16 text_;

  // The listener that will be informed of link clicks.
  StyledLabelListener* listener_;

  // The ranges that should be linkified, sorted by start position.
  std::priority_queue<LinkRange> link_ranges_;

  // A mapping from Link* control to the range it corresponds to in |text_|.
  std::map<Link*, ui::Range> link_targets_;

  // This variable saves the result of the last GetHeightForWidth call in order
  // to avoid repeated calculation.
  gfx::Size calculated_size_;

  DISALLOW_COPY_AND_ASSIGN(StyledLabel);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_STYLED_LABEL_H_
