// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_SKIA_H_
#define PRINTING_PDF_METAFILE_SKIA_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "printing/common/pdf_metafile_utils.h"
#include "printing/metafile.h"
#include "skia/ext/platform_canvas.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace printing {

struct PdfMetafileSkiaData;

// This class uses Skia graphics library to generate a PDF document.
class PRINTING_EXPORT PdfMetafileSkia : public Metafile {
 public:
  explicit PdfMetafileSkia(SkiaDocumentType type);
  ~PdfMetafileSkia() override;

  // Metafile methods.
  bool Init() override;
  bool InitFromData(const void* src_buffer, size_t src_buffer_size) override;

  void StartPage(const gfx::Size& page_size,
                 const gfx::Rect& content_area,
                 const float& scale_factor) override;
  bool FinishPage() override;
  bool FinishDocument() override;

  uint32_t GetDataSize() const override;
  bool GetData(void* dst_buffer, uint32_t dst_buffer_size) const override;

  gfx::Rect GetPageBounds(unsigned int page_number) const override;
  unsigned int GetPageCount() const override;

  skia::NativeDrawingContext context() const override;

#if defined(OS_WIN)
  bool Playback(skia::NativeDrawingContext hdc,
                const RECT* rect) const override;
  bool SafePlayback(skia::NativeDrawingContext hdc) const override;
#elif defined(OS_MACOSX)
  bool RenderPage(unsigned int page_number,
                  skia::NativeDrawingContext context,
                  const CGRect rect,
                  const MacRenderPageParams& params) const override;
#endif

  bool SaveTo(base::File* file) const override;

  // Return a new metafile containing just the current page in draft mode.
  std::unique_ptr<PdfMetafileSkia> GetMetafileForCurrentPage(
      SkiaDocumentType type);

  // This method calls StartPage and then returns an appropriate
  // PlatformCanvas implementation bound to the context created by
  // StartPage or NULL on error.  The PaintCanvas pointer that
  // is returned is owned by this PdfMetafileSkia object and does not
  // need to be ref()ed or unref()ed.  The canvas will remain valid
  // until FinishPage() or FinishDocument() is called.
  cc::PaintCanvas* GetVectorCanvasForNewPage(const gfx::Size& page_size,
                                             const gfx::Rect& content_area,
                                             const float& scale_factor);

 private:
  std::unique_ptr<PdfMetafileSkiaData> data_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafileSkia);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_SKIA_H_
