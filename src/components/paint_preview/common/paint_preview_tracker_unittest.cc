// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/paint_preview_tracker.h"

#include "base/unguessable_token.h"
#include "components/paint_preview/common/serial_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

struct TestContext {
  const gfx::Rect* rect;
  bool was_called;
};

}  // namespace

TEST(PaintPreviewTrackerTest, TestGetters) {
  const base::UnguessableToken kDocToken = base::UnguessableToken::Create();
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(kDocToken, kEmbeddingToken, true);
  EXPECT_EQ(tracker.Guid(), kDocToken);
  EXPECT_EQ(tracker.EmbeddingToken(), kEmbeddingToken);
  EXPECT_TRUE(tracker.IsMainFrame());
}

TEST(PaintPreviewTrackerTest, TestRemoteFramePlaceholderPicture) {
  const base::UnguessableToken kDocToken = base::UnguessableToken::Create();
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(kDocToken, kEmbeddingToken, true);

  const base::UnguessableToken kEmbeddingTokenChild =
      base::UnguessableToken::Create();
  gfx::Rect rect(50, 40, 30, 20);
  uint32_t content_id =
      tracker.CreateContentForRemoteFrame(rect, kEmbeddingTokenChild);
  PictureSerializationContext* context =
      tracker.GetPictureSerializationContext();
  EXPECT_TRUE(context->count(content_id));
  EXPECT_EQ((*context)[content_id], kEmbeddingTokenChild);

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(100, 100);
  tracker.CustomDataToSkPictureCallback(canvas, content_id);
  sk_sp<SkPicture> pic = recorder.finishRecordingAsPicture();

  // TODO(crbug/1009552): find a good way to check that a filler picture was
  // actually inserted into |pic|. This is difficult without using the
  // underlying private picture record.
}

TEST(PaintPreviewTrackerTest, TestGlyphRunList) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  std::string unichars = "abc";
  auto typeface = SkTypeface::MakeDefault();
  SkFont font(typeface);
  auto blob = SkTextBlob::MakeFromString(unichars.c_str(), font);
  tracker.AddGlyphs(blob.get());
  auto* usage_map = tracker.GetTypefaceUsageMap();
  EXPECT_TRUE(usage_map->count(typeface->uniqueID()));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('a')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('b')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('c')));
}

TEST(PaintPreviewTrackerTest, TestAnnotateLinks) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  const GURL url_1("https://www.chromium.org");
  const gfx::Rect rect_1(10, 20, 30, 40);
  tracker.AnnotateLink(url_1, rect_1);

  const GURL url_2("https://www.w3.org");
  const gfx::Rect rect_2(15, 25, 35, 45);
  tracker.AnnotateLink(url_2, rect_2);

  ASSERT_EQ(tracker.GetLinks().size(), 2U);

  EXPECT_EQ(tracker.GetLinks()[0]->url, url_1);
  EXPECT_EQ(tracker.GetLinks()[0]->rect.width(), rect_1.width());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.height(), rect_1.height());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.x(), rect_1.x());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.y(), rect_1.y());
  EXPECT_EQ(tracker.GetLinks()[1]->url, url_2);
  EXPECT_EQ(tracker.GetLinks()[1]->rect.width(), rect_2.width());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.height(), rect_2.height());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.x(), rect_2.x());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.y(), rect_2.y());
}

TEST(PaintPreviewTrackerTest, TestAnnotateAndMoveLinks) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  const GURL url_1("https://www.chromium.org");
  const gfx::Rect rect_1(10, 20, 30, 40);
  tracker.AnnotateLink(url_1, rect_1);

  const GURL url_2("https://www.w3.org");
  const gfx::Rect rect_2(15, 25, 35, 45);
  tracker.AnnotateLink(url_2, rect_2);

  std::vector<mojom::LinkDataPtr> links;
  tracker.MoveLinks(&links);
  ASSERT_EQ(tracker.GetLinks().size(), 0U);

  ASSERT_EQ(links.size(), 2U);
  EXPECT_EQ(links[0]->url, url_1);
  EXPECT_EQ(links[0]->rect.width(), rect_1.width());
  EXPECT_EQ(links[0]->rect.height(), rect_1.height());
  EXPECT_EQ(links[0]->rect.x(), rect_1.x());
  EXPECT_EQ(links[0]->rect.y(), rect_1.y());
  EXPECT_EQ(links[1]->url, url_2);
  EXPECT_EQ(links[1]->rect.width(), rect_2.width());
  EXPECT_EQ(links[1]->rect.height(), rect_2.height());
  EXPECT_EQ(links[1]->rect.x(), rect_2.x());
  EXPECT_EQ(links[1]->rect.y(), rect_2.y());
}

}  // namespace paint_preview
