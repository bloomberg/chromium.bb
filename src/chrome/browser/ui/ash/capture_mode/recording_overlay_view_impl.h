// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_IMPL_H_

#include "ash/public/cpp/capture_mode/recording_overlay_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Profile;

namespace views {
class WebView;
}  // namespace views

// The actual implementation of the view that will be used as the contents view
// of the recording overlay widget. This view hosts a |views::WebView| which
// will show the contents of the annotator embedder URL.
class RecordingOverlayViewImpl : public ash::RecordingOverlayView {
 public:
  METADATA_HEADER(RecordingOverlayViewImpl);

  explicit RecordingOverlayViewImpl(Profile* profile);
  RecordingOverlayViewImpl(const RecordingOverlayViewImpl&) = delete;
  RecordingOverlayViewImpl& operator=(const RecordingOverlayViewImpl&) = delete;
  ~RecordingOverlayViewImpl() override = default;

  views::WebView* GetWebViewForTest() { return web_view_; }

 private:
  views::WebView* web_view_;
};

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_IMPL_H_
