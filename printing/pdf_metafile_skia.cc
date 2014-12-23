// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_skia.h"

#include "base/containers/hash_tables.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "skia/ext/refptr.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/pdf/SkPDFDevice.h"
#include "third_party/skia/include/pdf/SkPDFDocument.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(OS_MACOSX)
#include "printing/pdf_metafile_cg_mac.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace printing {

struct PdfMetafileSkiaData {
  skia::RefPtr<SkPDFDevice> current_page_;
  skia::RefPtr<SkCanvas> current_page_canvas_;
  SkPDFDocument pdf_doc_;
  SkDynamicMemoryWStream pdf_stream_;
#if defined(OS_MACOSX)
  PdfMetafileCg pdf_cg_;
#endif
};

PdfMetafileSkia::~PdfMetafileSkia() {}

bool PdfMetafileSkia::Init() {
  return true;
}
bool PdfMetafileSkia::InitFromData(const void* src_buffer,
                                   uint32 src_buffer_size) {
  return data_->pdf_stream_.write(src_buffer, src_buffer_size);
}

bool PdfMetafileSkia::StartPage(const gfx::Size& page_size,
                                const gfx::Rect& content_area,
                                const float& scale_factor) {
  DCHECK(!data_->current_page_canvas_);

  // Adjust for the margins and apply the scale factor.
  SkMatrix transform;
  transform.setTranslate(SkIntToScalar(content_area.x()),
                         SkIntToScalar(content_area.y()));
  transform.preScale(SkFloatToScalar(scale_factor),
                     SkFloatToScalar(scale_factor));

  SkISize pdf_page_size = SkISize::Make(page_size.width(), page_size.height());
  SkISize pdf_content_size =
      SkISize::Make(content_area.width(), content_area.height());

  data_->current_page_ = skia::AdoptRef(
      new SkPDFDevice(pdf_page_size, pdf_content_size, transform));
  data_->current_page_canvas_ =
      skia::AdoptRef(new SkCanvas(data_->current_page_.get()));
  return true;
}

skia::VectorCanvas* PdfMetafileSkia::GetVectorCanvasForNewPage(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const float& scale_factor) {
  if (!StartPage(page_size, content_area, scale_factor))
    return nullptr;
  return data_->current_page_canvas_.get();
}

bool PdfMetafileSkia::FinishPage() {
  DCHECK(data_->current_page_canvas_);
  DCHECK(data_->current_page_);

  data_->current_page_canvas_.clear();  // Unref SkCanvas.
  data_->pdf_doc_.appendPage(data_->current_page_.get());
  return true;
}

bool PdfMetafileSkia::FinishDocument() {
  // Don't do anything if we've already set the data in InitFromData.
  if (data_->pdf_stream_.getOffset())
    return true;

  if (data_->current_page_canvas_)
    FinishPage();

  data_->current_page_.clear();

  int font_counts[SkAdvancedTypefaceMetrics::kOther_Font + 2];
  data_->pdf_doc_.getCountOfFontTypes(font_counts);
  for (int type = 0;
       type <= SkAdvancedTypefaceMetrics::kOther_Font + 1;
       type++) {
    for (int count = 0; count < font_counts[type]; count++) {
      UMA_HISTOGRAM_ENUMERATION(
          "PrintPreview.FontType", type,
          SkAdvancedTypefaceMetrics::kOther_Font + 2);
    }
  }

  return data_->pdf_doc_.emitPDF(&data_->pdf_stream_);
}

uint32 PdfMetafileSkia::GetDataSize() const {
  return base::checked_cast<uint32>(data_->pdf_stream_.getOffset());
}

bool PdfMetafileSkia::GetData(void* dst_buffer,
                              uint32 dst_buffer_size) const {
  if (dst_buffer_size < GetDataSize())
    return false;

  SkAutoDataUnref data(data_->pdf_stream_.copyToData());
  memcpy(dst_buffer, data->bytes(), dst_buffer_size);
  return true;
}

gfx::Rect PdfMetafileSkia::GetPageBounds(unsigned int page_number) const {
  // TODO(vandebo) add a method to get the page size for a given page to
  // SkPDFDocument.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

unsigned int PdfMetafileSkia::GetPageCount() const {
  // TODO(vandebo) add a method to get the number of pages to SkPDFDocument.
  NOTIMPLEMENTED();
  return 0;
}

gfx::NativeDrawingContext PdfMetafileSkia::context() const {
  NOTREACHED();
  return NULL;
}

#if defined(OS_WIN)
bool PdfMetafileSkia::Playback(gfx::NativeDrawingContext hdc,
                               const RECT* rect) const {
  NOTREACHED();
  return false;
}

bool PdfMetafileSkia::SafePlayback(gfx::NativeDrawingContext hdc) const {
  NOTREACHED();
  return false;
}

#elif defined(OS_MACOSX)
/* TODO(caryclark): The set up of PluginInstance::PrintPDFOutput may result in
   rasterized output.  Even if that flow uses PdfMetafileCg::RenderPage,
   the drawing of the PDF into the canvas may result in a rasterized output.
   PDFMetafileSkia::RenderPage should be not implemented as shown and instead
   should do something like the following CL in PluginInstance::PrintPDFOutput:
http://codereview.chromium.org/7200040/diff/1/webkit/plugins/ppapi/ppapi_plugin_instance.cc
*/
bool PdfMetafileSkia::RenderPage(unsigned int page_number,
                                 CGContextRef context,
                                 const CGRect rect,
                                 const MacRenderPageParams& params) const {
  DCHECK_GT(data_->pdf_stream_.getOffset(), 0U);
  if (data_->pdf_cg_.GetDataSize() == 0) {
    SkAutoDataUnref data(data_->pdf_stream_.copyToData());
    data_->pdf_cg_.InitFromData(data->bytes(), data->size());
  }
  return data_->pdf_cg_.RenderPage(page_number, context, rect, params);
}
#endif

bool PdfMetafileSkia::SaveTo(base::File* file) const {
  if (GetDataSize() == 0U)
    return false;
  SkAutoDataUnref data(data_->pdf_stream_.copyToData());
  // TODO(halcanary): rewrite this function without extra data copy
  // using SkStreamAsset.
  const char* ptr = reinterpret_cast<const char*>(data->data());
  int size = base::checked_cast<int>(data->size());
  return file->WriteAtCurrentPos(ptr, size) == size;
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
bool PdfMetafileSkia::SaveToFD(const base::FileDescriptor& fd) const {
  DCHECK_GT(data_->pdf_stream_.getOffset(), 0U);

  if (fd.fd < 0) {
    DLOG(ERROR) << "Invalid file descriptor!";
    return false;
  }
  base::File file(fd.fd);
  bool result = SaveTo(&file);
  DLOG_IF(ERROR, !result) << "Failed to save file with fd " << fd.fd;

  if (!fd.auto_close)
    file.TakePlatformFile();
  return result;
}
#endif

PdfMetafileSkia::PdfMetafileSkia() : data_(new PdfMetafileSkiaData) {
}

scoped_ptr<PdfMetafileSkia> PdfMetafileSkia::GetMetafileForCurrentPage() {
  scoped_ptr<PdfMetafileSkia> metafile;
  SkPDFDocument pdf_doc(SkPDFDocument::kDraftMode_Flags);
  if (!pdf_doc.appendPage(data_->current_page_.get()))
    return metafile.Pass();

  SkDynamicMemoryWStream pdf_stream;
  if (!pdf_doc.emitPDF(&pdf_stream))
    return metafile.Pass();

  SkAutoDataUnref data_copy(pdf_stream.copyToData());
  if (data_copy->size() == 0)
    return scoped_ptr<PdfMetafileSkia>();

  metafile.reset(new PdfMetafileSkia);
  if (!metafile->InitFromData(data_copy->bytes(),
                              base::checked_cast<uint32>(data_copy->size()))) {
    metafile.reset();
  }
  return metafile.Pass();
}

}  // namespace printing
