// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_SKIA_H_
#define PRINTING_PDF_METAFILE_SKIA_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "printing/metafile.h"
#include "skia/ext/platform_canvas.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
namespace base {
struct FileDescriptor;
}
#endif

namespace printing {

struct PdfMetafileSkiaData;

// This class uses Skia graphics library to generate a PDF document.
class PRINTING_EXPORT PdfMetafileSkia : public Metafile {
 public:
  PdfMetafileSkia();
  ~PdfMetafileSkia() override;

  // Metafile methods.
  bool Init() override;
  bool InitFromData(const void* src_buffer, uint32_t src_buffer_size) override;

  bool StartPage(const gfx::Size& page_size,
                 const gfx::Rect& content_area,
                 const float& scale_factor) override;
  bool FinishPage() override;
  bool FinishDocument() override;

  uint32_t GetDataSize() const override;
  bool GetData(void* dst_buffer, uint32_t dst_buffer_size) const override;

  gfx::Rect GetPageBounds(unsigned int page_number) const override;
  unsigned int GetPageCount() const override;

  gfx::NativeDrawingContext context() const override;

#if defined(OS_WIN)
  bool Playback(gfx::NativeDrawingContext hdc,
                const RECT* rect) const override;
  bool SafePlayback(gfx::NativeDrawingContext hdc) const override;
#elif defined(OS_MACOSX)
  bool RenderPage(unsigned int page_number,
                  gfx::NativeDrawingContext context,
                  const CGRect rect,
                  const MacRenderPageParams& params) const override;
#endif

  bool SaveTo(base::File* file) const override;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // TODO(vitalybuka): replace with SaveTo().
  bool SaveToFD(const base::FileDescriptor& fd) const;
#endif  // if defined(OS_CHROMEOS) || defined(OS_ANDROID)

  // Return a new metafile containing just the current page in draft mode.
  scoped_ptr<PdfMetafileSkia> GetMetafileForCurrentPage();

  // This method calls StartPage and then returns an appropriate
  // PlatformCanvas implementation bound to the context created by
  // StartPage or NULL on error.  The SkCanvas pointer that
  // is returned is owned by this PdfMetafileSkia object and does not
  // need to be ref()ed or unref()ed.  The canvas will remain valid
  // until FinishPage() or FinishDocument() is called.
  SkCanvas* GetVectorCanvasForNewPage(const gfx::Size& page_size,
                                      const gfx::Rect& content_area,
                                      const float& scale_factor);

 private:
  scoped_ptr<PdfMetafileSkiaData> data_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafileSkia);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_SKIA_H_
