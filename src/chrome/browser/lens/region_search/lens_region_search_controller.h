// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/lens/metrics/lens_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget.h"

class Browser;

namespace content {
class WebContents;
enum class Visibility;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

namespace lens {

class LensRegionSearchController : public content::WebContentsObserver {
 public:
  explicit LensRegionSearchController(content::WebContents* web_contents,
                                      Browser* browser);
  ~LensRegionSearchController() override;

  // Creates and runs the drag and capture flow. When run, the user enters into
  // a screenshot capture mode with the ability to draw a rectagular region
  // around the web contents. When finished with selection, the region is
  // converted into a PNG and sent to Lens. If `use_fullscreen_capture` is set
  // to true, the whole screen will automatically be captured.
  void Start(bool use_fullscreen_capture,
             bool is_google_default_search_provider);

  // Closes the UI overlay and user education bubble if currently being shown.
  // The closed reason for this method is defaulted to the close button being
  // clicked.
  void Close();
  void Escape();

  // Closes the UI overlay and user education bubble if shown with the specified
  // closed reason.
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Calculates the percentage that the image area takes up in the screen area.
  // This value is calculated as double and then floored to the nearest integer.
  static int CalculateViewportProportionFromAreas(int screen_height,
                                                  int screen_width,
                                                  int image_width,
                                                  int image_height);
  // Returns an enum representing the aspect ratio of the image as defined in
  // lens_metrics.h.
  static lens::LensRegionSearchAspectRatio GetAspectRatioFromSize(
      int image_height,
      int image_width);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // The function handling the metrics recording and resizing that happens when
  // the capture has been completed.
  void OnCaptureCompleted(const image_editor::ScreenshotCaptureResult& result);

 private:
  void RecordCaptureResult(lens::LensRegionSearchCaptureResult result);

  void RecordRegionSizeRelatedMetrics(gfx::Rect screen_bounds,
                                      gfx::Size region_size);

  gfx::Image ResizeImageIfNecessary(const gfx::Image& image);

  // Variable for tracking the default search provider as to launch the image
  // results in correct search engine. This value is set every time the capture
  // mode is started to have an accurate value for the completed capture.
  bool is_google_default_search_provider_ = false;

  bool in_capture_mode_ = false;

  std::unique_ptr<image_editor::ScreenshotFlow> screenshot_flow_;

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  base::WeakPtr<LensRegionSearchController> weak_this_;

  base::WeakPtrFactory<LensRegionSearchController> weak_factory_{this};
};
}  // namespace lens
#endif  // CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
