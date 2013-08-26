// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_ANALYSIS_CANVAS_H_
#define SKIA_EXT_ANALYSIS_CANVAS_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace skia {

class AnalysisDevice;

// Does not render anything, but gathers statistics about a region
// (specified as a clip rectangle) of an SkPicture as the picture is
// played back through it.
// To use: create a SkBitmap with kNo_Config, create an AnalysisDevice
// using that bitmap, and create an AnalysisCanvas using the device.
// Play a picture into the canvas, and then check result.
class SK_API AnalysisCanvas : public SkCanvas, public SkDrawPictureCallback {
 public:
  AnalysisCanvas(AnalysisDevice*);
  virtual ~AnalysisCanvas();

  // Returns true when a SkColor can be used to represent result.
  bool GetColorIfSolid(SkColor* color) const;
  bool HasText() const;

  // SkDrawPictureCallback override.
  virtual bool abortDrawing() OVERRIDE;

  // SkCanvas overrides.
  virtual bool clipRect(const SkRect& rect,
                        SkRegion::Op op = SkRegion::kIntersect_Op,
                        bool do_anti_alias = false) OVERRIDE;
  virtual bool clipPath(const SkPath& path,
                        SkRegion::Op op = SkRegion::kIntersect_Op,
                        bool do_anti_alias = false) OVERRIDE;
  virtual bool clipRRect(const SkRRect& rrect,
                         SkRegion::Op op = SkRegion::kIntersect_Op,
                         bool do_anti_alias = false) OVERRIDE;

  virtual int saveLayer(const SkRect* bounds,
                        const SkPaint* paint,
                        SkCanvas::SaveFlags flags) OVERRIDE;
  virtual int save(SaveFlags flags = kMatrixClip_SaveFlag) OVERRIDE;

  virtual void restore() OVERRIDE;

 private:
  typedef SkCanvas INHERITED;

  int saved_stack_size_;
  int force_not_solid_stack_level_;
  int force_not_transparent_stack_level_;
};

// TODO(robertphillips): Once Skia's SkBaseDevice API is refactored to
// remove the bitmap-specific entry points, it might make sense for this
// to be derived from SkBaseDevice (rather than SkBitmapDevice)
class SK_API AnalysisDevice : public SkBitmapDevice {
 public:
  AnalysisDevice(const SkBitmap& bitmap);
  virtual ~AnalysisDevice();

  bool GetColorIfSolid(SkColor* color) const;
  bool HasText() const;

  void SetForceNotSolid(bool flag);
  void SetForceNotTransparent(bool flag);

 protected:
  // SkBaseDevice overrides.
  virtual void clear(SkColor color) OVERRIDE;
  virtual void drawPaint(const SkDraw& draw, const SkPaint& paint) OVERRIDE;
  virtual void drawPoints(const SkDraw& draw,
                          SkCanvas::PointMode mode,
                          size_t count,
                          const SkPoint points[],
                          const SkPaint& paint) OVERRIDE;
  virtual void drawRect(const SkDraw& draw,
                        const SkRect& rect,
                        const SkPaint& paint) OVERRIDE;
  virtual void drawOval(const SkDraw& draw,
                        const SkRect& oval,
                        const SkPaint& paint) OVERRIDE;
  virtual void drawPath(const SkDraw& draw,
                        const SkPath& path,
                        const SkPaint& paint,
                        const SkMatrix* pre_path_matrix = NULL,
                        bool path_is_mutable = false) OVERRIDE;
  virtual void drawBitmap(const SkDraw& draw,
                          const SkBitmap& bitmap,
                          const SkMatrix& matrix,
                          const SkPaint& paint) OVERRIDE;
  virtual void drawSprite(const SkDraw& draw,
                          const SkBitmap& bitmap,
                          int x,
                          int y,
                          const SkPaint& paint) OVERRIDE;
  virtual void drawBitmapRect(const SkDraw& draw,
                              const SkBitmap& bitmap,
                              const SkRect* src_or_null,
                              const SkRect& dst,
                              const SkPaint& paint,
                              SkCanvas::DrawBitmapRectFlags flags) OVERRIDE;
  virtual void drawText(const SkDraw& draw,
                        const void* text,
                        size_t len,
                        SkScalar x,
                        SkScalar y,
                        const SkPaint& paint) OVERRIDE;
  virtual void drawPosText(const SkDraw& draw,
                           const void* text,
                           size_t len,
                           const SkScalar pos[],
                           SkScalar const_y,
                           int scalars_per_pos,
                           const SkPaint& paint) OVERRIDE;
  virtual void drawTextOnPath(const SkDraw& draw,
                              const void* text,
                              size_t len,
                              const SkPath& path,
                              const SkMatrix* matrix,
                              const SkPaint& paint) OVERRIDE;
#ifdef SK_BUILD_FOR_ANDROID
  virtual void drawPosTextOnPath(const SkDraw& draw,
                                 const void* text,
                                 size_t len,
                                 const SkPoint pos[],
                                 const SkPaint& paint,
                                 const SkPath& path,
                                 const SkMatrix* matrix) OVERRIDE;
#endif
  virtual void drawVertices(const SkDraw& draw,
                            SkCanvas::VertexMode vertex_mode,
                            int vertex_count,
                            const SkPoint verts[],
                            const SkPoint texs[],
                            const SkColor colors[],
                            SkXfermode* xmode,
                            const uint16_t indices[],
                            int index_count,
                            const SkPaint& paint) OVERRIDE;
  virtual void drawDevice(const SkDraw& draw,
                          SkBaseDevice* device,
                          int x,
                          int y,
                          const SkPaint& paint) OVERRIDE;

 private:
  typedef SkBitmapDevice INHERITED;

  bool is_forced_not_solid_;
  bool is_forced_not_transparent_;
  bool is_solid_color_;
  SkColor color_;
  bool is_transparent_;
  bool has_text_;
};

}  // namespace skia

#endif  // SKIA_EXT_ANALYSIS_CANVAS_H_

