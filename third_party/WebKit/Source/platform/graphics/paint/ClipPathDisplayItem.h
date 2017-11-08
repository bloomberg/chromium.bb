// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathDisplayItem_h
#define ClipPathDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

class PLATFORM_EXPORT BeginClipPathDisplayItem final
    : public PairedBeginDisplayItem {
 public:
  BeginClipPathDisplayItem(const DisplayItemClient& client,
                           const Path& clip_path)
      : PairedBeginDisplayItem(client, kBeginClipPath, sizeof(*this)),
        clip_path_(clip_path.GetSkPath()) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif
  bool Equals(const DisplayItem& other) const final {
    return DisplayItem::Equals(other) &&
           clip_path_ ==
               static_cast<const BeginClipPathDisplayItem&>(other).clip_path_;
  }

  const SkPath clip_path_;
};

class PLATFORM_EXPORT EndClipPathDisplayItem final
    : public PairedEndDisplayItem {
 public:
  EndClipPathDisplayItem(const DisplayItemClient& client)
      : PairedEndDisplayItem(client, kEndClipPath, sizeof(*this)) {}

  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;

 private:
#if DCHECK_IS_ON()
  bool IsEndAndPairedWith(DisplayItem::Type other_type) const final {
    return other_type == kBeginClipPath;
  }
#endif
};

}  // namespace blink

#endif  // ClipPathDisplayItem_h
