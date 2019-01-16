// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ImageElementTimingTest : public RenderingTest {
 protected:
  // Sets an image resource for the LayoutImage with the given |id| and return
  // the LayoutImage.
  LayoutImage* SetImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    auto* layout_image = ToLayoutImage(GetLayoutObjectByElementId(id));
    layout_image->ImageResource()->SetImageResource(content);
    return layout_image;
  }

  const ImageElementTiming& GetImageElementTiming() {
    return ImageElementTiming::From(*GetDocument().domWindow());
  }

 private:
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_resource =
        ImageResourceContent::CreateLoaded(
            StaticBitmapImage::Create(image).get());
    return original_image_resource;
  }
};

TEST_F(ImageElementTimingTest, ImageInsideSVG) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <svg mask="url(#mask)">
      <mask id="mask">
        <foreignObject width="100" height="100">
          <img id="target" style='width: 100px; height: 100px;'/>
        </foreignObject>
      </mask>
      <rect width="100" height="100" fill="green"/>
    </svg>
  )HTML");
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  // Enable compositing and also update document lifecycle.
  EnableCompositing();

  const ImageElementTiming& timing = GetImageElementTiming();
  // |layout_image| should have had its paint notified to ImageElementTiming.
  EXPECT_TRUE(timing.images_notified_.find(layout_image) !=
              timing.images_notified_.end());
}

}  // namespace blink
