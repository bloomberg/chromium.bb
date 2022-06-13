// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"

namespace content {
class WebContents;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context for Views
//
//  The implementation of the Caption Bubble Context for Views.
//
class CaptionBubbleContextViews : public CaptionBubbleContextBrowser {
 public:
  explicit CaptionBubbleContextViews(content::WebContents* web_contents);
  ~CaptionBubbleContextViews() override;
  CaptionBubbleContextViews(const CaptionBubbleContextViews&) = delete;
  CaptionBubbleContextViews& operator=(const CaptionBubbleContextViews&) =
      delete;

  // CaptionBubbleContextBrowser:
  absl::optional<gfx::Rect> GetBounds() const override;
  void Activate() override;
  bool IsActivatable() const override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
