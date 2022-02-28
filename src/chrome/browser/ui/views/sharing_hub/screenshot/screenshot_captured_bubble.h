// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageView;
class LabelButton;
class MdTextButton;
class View;
}  // namespace views

class Profile;
struct NavigateParams;

namespace sharing_hub {

// Dialog that displays a captured screenshot, and provides the option
// to edit, share, or download.
class ScreenshotCapturedBubble : public LocationBarBubbleDelegateView {
 public:
  METADATA_HEADER(ScreenshotCapturedBubble);
  ScreenshotCapturedBubble(
      views::View* anchor_view,
      content::WebContents* web_contents,
      const gfx::Image& image,
      Profile* profile,
      base::OnceCallback<void(NavigateParams*)> edit_callback);
  ScreenshotCapturedBubble(const ScreenshotCapturedBubble&) = delete;
  ScreenshotCapturedBubble& operator=(const ScreenshotCapturedBubble&) = delete;
  ~ScreenshotCapturedBubble() override;

  void Show();

 private:
  // LocationBarBubbleDelegateView:
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  static const std::u16string GetFilenameForURL(const GURL& url);

  // views::BubbleDialogDelegateView:
  void Init() override;

  void DownloadButtonPressed();

  void EditButtonPressed();

  gfx::Size GetImageSize();

  const gfx::Image& image_;

  base::WeakPtr<content::WebContents> web_contents_;

  raw_ptr<Profile> profile_;

  base::OnceCallback<void(NavigateParams*)> edit_callback_;

  // Pointers to view widgets; weak.
  raw_ptr<views::ImageView> image_view_ = nullptr;
  views::MdTextButton* download_button_ = nullptr;
  views::LabelButton* edit_button_ = nullptr;
};

BEGIN_VIEW_BUILDER(, ScreenshotCapturedBubble, LocationBarBubbleDelegateView)
END_VIEW_BUILDER

}  // namespace sharing_hub

DEFINE_VIEW_BUILDER(, sharing_hub::ScreenshotCapturedBubble)

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
