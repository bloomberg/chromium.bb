// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintRecordBuilder_h
#define PaintRecordBuilder_h

#include <memory>

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Noncopyable.h"

class SkMetaData;

namespace blink {

class GraphicsContext;
class PaintController;

// TODO(enne): rename this class to not be named SkPicture
// When slimming paint ships we can remove this PaintRecord abstraction and
// rely on PaintController here.
class PLATFORM_EXPORT PaintRecordBuilder final : public DisplayItemClient {
  WTF_MAKE_NONCOPYABLE(PaintRecordBuilder);

 public:
  // Constructs a new builder with the given bounds for the resulting recorded
  // picture. If |metadata| is specified, that metadata is propagated to the
  // builder's internal canvas. If |containingContext| is specified, the device
  // scale factor, printing, and disabled state are propagated to the builder's
  // internal context.
  // If a PaintController is passed, it is used as the PaintController for
  // painting the picture (and hence we can use its cache). Otherwise, a new
  // PaintController is used for the duration of the picture building, which
  // therefore has no caching.
  PaintRecordBuilder(const FloatRect& bounds,
                     SkMetaData* = nullptr,
                     GraphicsContext* containingContext = nullptr,
                     PaintController* = nullptr);
  ~PaintRecordBuilder();

  GraphicsContext& context() { return *m_context; }

  // Returns a picture capturing all drawing performed on the builder's context
  // since construction.
  sk_sp<PaintRecord> endRecording();

  // DisplayItemClient methods
  String debugName() const final { return "PaintRecordBuilder"; }
  LayoutRect visualRect() const final { return LayoutRect(); }

 private:
  PaintController* m_paintController;
  std::unique_ptr<PaintController> m_paintControllerPtr;
  std::unique_ptr<GraphicsContext> m_context;
  FloatRect m_bounds;
};

}  // namespace blink

#endif  // PaintRecordBuilder_h
