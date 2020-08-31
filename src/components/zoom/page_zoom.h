// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZOOM_PAGE_ZOOM_H_
#define COMPONENTS_ZOOM_PAGE_ZOOM_H_

#include <vector>

#include "base/macros.h"
#include "content/public/common/page_zoom.h"

namespace content {
class WebContents;
}

namespace zoom {

// This class provides a means of zooming pages according to a predetermined
// set of zoom levels/factors. In future, the static methods in this class
// can be made non-static, with PresetZoomX() being virtual, to allow clients
// to create custom sets of zoom levels.
class PageZoom {
 public:
  // Return a sorted vector of zoom factors. The vector will consist of preset
  // values along with a custom value (if the custom value is not already
  // represented.)
  static std::vector<double> PresetZoomFactors(double custom_factor);

  // Return a sorted vector of zoom levels. The vector will consist of preset
  // values along with a custom value (if the custom value is not already
  // represented.)
  static std::vector<double> PresetZoomLevels(double custom_level);

  // Adjusts the zoom level of |web_contents|.
  static void Zoom(content::WebContents* web_contents, content::PageZoom zoom);

 private:
  // We don't expect (currently) to create instances of this class.
  PageZoom() {}
  DISALLOW_COPY_AND_ASSIGN(PageZoom);
};

}  // namespace zoom

#endif  // COMPONENTS_ZOOM_PAGE_ZOOM_H_
