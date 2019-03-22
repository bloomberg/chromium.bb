// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_ORIGIN_TEXT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_ORIGIN_TEXT_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class Label;
}

// A URL's origin text with a fade in/out animation.
class HostedAppOriginText : public views::View {
 public:
  explicit HostedAppOriginText(Browser* browser);
  ~HostedAppOriginText() override;

  void SetTextColor(SkColor color);

  // Fades the text in and out.
  void StartFadeAnimation();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Owned by the views hierarchy.
  views::Label* label_ = nullptr;

  void AnimationComplete();

  base::WeakPtrFactory<HostedAppOriginText> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostedAppOriginText);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_ORIGIN_TEXT_H_
