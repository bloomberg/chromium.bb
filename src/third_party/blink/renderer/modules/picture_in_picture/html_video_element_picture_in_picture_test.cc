// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_video_element_picture_in_picture.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_control.h"

namespace blink {

TEST(HTMLVideoElementPictureInPictureTest,
     ToPictureInPictureControlInfoVector_Basic) {
  MediaImage image;
  image.setSrc("https://dummyimage.com/144x144/7d727d/fafafa.gif");
  image.setSizes("144x144");
  image.setType("image/gif");
  HeapVector<MediaImage> images;
  images.push_back(image);

  PictureInPictureControl control;
  control.setId(WTF::String("Test id"));
  control.setLabel(WTF::String("Test label"));
  control.setIcons(images);

  HeapVector<PictureInPictureControl> controls;
  controls.push_back(control);

  std::vector<PictureInPictureControlInfo> converted_controls =
      HTMLVideoElementPictureInPicture::ToPictureInPictureControlInfoVector(
          controls);

  ASSERT_EQ(1U, converted_controls.size());
  EXPECT_EQ(144, converted_controls[0].icons[0].sizes[0].width());
  EXPECT_EQ(144, converted_controls[0].icons[0].sizes[0].height());
}

TEST(HTMLVideoElementPictureInPictureTest,
     ToPictureInPictureControlInfoVector_MultipleImages) {
  MediaImage image;
  image.setSrc("https://dummyimage.com/144x144/7d727d/fafafa.gif");
  image.setSizes("144x144");
  image.setType("image/gif");

  MediaImage image2;
  image2.setSrc("https://dummyimage.com/96x96/7d727d/fafafa.gif");
  image2.setSizes("96x96");
  image2.setType("image/gif");

  HeapVector<MediaImage> images;
  images.push_back(image);
  images.push_back(image2);

  PictureInPictureControl control;
  control.setId(WTF::String("Test id"));
  control.setLabel(WTF::String("Test label"));
  control.setIcons(images);

  HeapVector<PictureInPictureControl> controls;
  controls.push_back(control);

  std::vector<PictureInPictureControlInfo> converted_controls =
      HTMLVideoElementPictureInPicture::ToPictureInPictureControlInfoVector(
          controls);

  ASSERT_EQ(1U, converted_controls.size());
  EXPECT_EQ(144, converted_controls[0].icons[0].sizes[0].width());
  EXPECT_EQ(144, converted_controls[0].icons[0].sizes[0].height());
  EXPECT_EQ(96, converted_controls[0].icons[1].sizes[0].width());
  EXPECT_EQ(96, converted_controls[0].icons[1].sizes[0].height());
}

TEST(HTMLVideoElementPictureInPictureTest,
     ToPictureInPictureControlInfoVector_InvalidSize) {
  MediaImage image;
  image.setSrc("https://dummyimage.com/144x144/7d727d/fafafa.gif");
  image.setSizes("144");
  image.setType("image/gif");
  HeapVector<MediaImage> images;
  images.push_back(image);

  PictureInPictureControl control;
  control.setId(WTF::String("Test id"));
  control.setLabel(WTF::String("Test label"));
  control.setIcons(images);

  HeapVector<PictureInPictureControl> controls;
  controls.push_back(control);

  std::vector<PictureInPictureControlInfo> converted_controls =
      HTMLVideoElementPictureInPicture::ToPictureInPictureControlInfoVector(
          controls);

  ASSERT_EQ(1U, converted_controls.size());

  ASSERT_EQ(1U, converted_controls[0].icons.size());

  EXPECT_EQ(0U, converted_controls[0].icons[0].sizes.size());
}

TEST(HTMLVideoElementPictureInPictureTest,
     ToPictureInPictureControlInfoVector_Empty) {
  HeapVector<PictureInPictureControl> controls;

  std::vector<PictureInPictureControlInfo> converted_controls =
      HTMLVideoElementPictureInPicture::ToPictureInPictureControlInfoVector(
          controls);

  EXPECT_EQ(0U, converted_controls.size());
}

}  // namespace blink
