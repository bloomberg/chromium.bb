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
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_MACOSX)
#include "printing/pdf_metafile_cg_mac.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {
// This struct represents all the data we need to draw and redraw this
// page into a SkDocument.
struct Page {
  Page(const SkSize& page_size, const SkRect& content_area)
      : page_size_(page_size),
        content_area_(content_area),
        content_(/*NULL*/) {}
  SkSize page_size_;
  SkRect content_area_;
  skia::RefPtr<SkPicture> content_;
};
}  // namespace

static bool WriteAssetToBuffer(const SkStreamAsset* asset,
                               void* buffer,
                               size_t size) {
  // Calling duplicate() keeps original asset state unchanged.
  scoped_ptr<SkStreamAsset> assetCopy(asset->duplicate());
  size_t length = assetCopy->getLength();
  if (length > size)
    return false;
  return (length == assetCopy->read(buffer, length));
}

namespace printing {

struct PdfMetafileSkiaData {
  SkPictureRecorder recorder_;  // Current recording

  std::vector<Page> pages_;
  scoped_ptr<SkStreamAsset> pdf_data_;

#if defined(OS_MACOSX)
  PdfMetafileCg pdf_cg_;
#endif
};

PdfMetafileSkia::~PdfMetafileSkia() {}

bool PdfMetafileSkia::Init() {
  return true;
}

// TODO(halcanary): Create a Metafile class that only stores data.
// Metafile::InitFromData is orthogonal to what the rest of
// PdfMetafileSkia does.
bool PdfMetafileSkia::InitFromData(const void* src_buffer,
                                   uint32 src_buffer_size) {
  data_->pdf_data_.reset(new SkMemoryStream(src_buffer, src_buffer_size, true));
  return true;
}

bool PdfMetafileSkia::StartPage(const gfx::Size& page_size,
                                const gfx::Rect& content_area,
                                const float& scale_factor) {
  if (data_->recorder_.getRecordingCanvas())
    this->FinishPage();
  DCHECK(!data_->recorder_.getRecordingCanvas());
  SkSize sk_page_size = gfx::SizeFToSkSize(gfx::SizeF(page_size));
  data_->pages_.push_back(Page(sk_page_size, gfx::RectToSkRect(content_area)));

  SkCanvas* recordingCanvas = data_->recorder_.beginRecording(
      sk_page_size.width(), sk_page_size.height(), NULL, 0);
  // recordingCanvas is owned by the data_->recorder_.  No ref() necessary.
  if (!recordingCanvas)
    return false;
  recordingCanvas->scale(scale_factor, scale_factor);
  return true;
}

skia::PlatformCanvas* PdfMetafileSkia::GetVectorCanvasForNewPage(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const float& scale_factor) {
  if (!StartPage(page_size, content_area, scale_factor))
    return nullptr;
  return data_->recorder_.getRecordingCanvas();
}

bool PdfMetafileSkia::FinishPage() {
  if (!data_->recorder_.getRecordingCanvas())
    return false;
  DCHECK(!(data_->pages_.back().content_));
  data_->pages_.back().content_ =
      skia::AdoptRef(data_->recorder_.endRecording());
  return true;
}

bool PdfMetafileSkia::FinishDocument() {
  // If we've already set the data in InitFromData, leave it be.
  if (data_->pdf_data_)
    return false;

  if (data_->recorder_.getRecordingCanvas())
    this->FinishPage();

  SkDynamicMemoryWStream pdf_stream;
  skia::RefPtr<SkDocument> pdf_doc =
      skia::AdoptRef(SkDocument::CreatePDF(&pdf_stream));
  for (const auto& page : data_->pages_) {
    SkCanvas* canvas = pdf_doc->beginPage(
        page.page_size_.width(), page.page_size_.height(), &page.content_area_);
    canvas->drawPicture(page.content_.get());
    pdf_doc->endPage();
  }
  if (!pdf_doc->close())
    return false;

  data_->pdf_data_.reset(pdf_stream.detachAsStream());
  return true;
}

uint32 PdfMetafileSkia::GetDataSize() const {
  if (!data_->pdf_data_)
    return 0;
  return base::checked_cast<uint32>(data_->pdf_data_->getLength());
}

bool PdfMetafileSkia::GetData(void* dst_buffer,
                              uint32 dst_buffer_size) const {
  if (!data_->pdf_data_)
    return false;
  return WriteAssetToBuffer(data_->pdf_data_.get(), dst_buffer,
                            base::checked_cast<size_t>(dst_buffer_size));
}

gfx::Rect PdfMetafileSkia::GetPageBounds(unsigned int page_number) const {
  if (page_number < data_->pages_.size()) {
    return gfx::Rect(gfx::ToFlooredSize(
        gfx::SkSizeToSizeF(data_->pages_[page_number].page_size_)));
  }
  return gfx::Rect();
}

unsigned int PdfMetafileSkia::GetPageCount() const {
  return base::checked_cast<unsigned int>(data_->pages_.size());
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
  DCHECK_GT(GetDataSize(), 0U);
  if (data_->pdf_cg_.GetDataSize() == 0) {
    if (GetDataSize() == 0)
      return false;
    size_t length = data_->pdf_data_->getLength();
    std::vector<uint8_t> buffer(length);
    (void)WriteAssetToBuffer(data_->pdf_data_.get(), &buffer[0], length);
    data_->pdf_cg_.InitFromData(&buffer[0], base::checked_cast<uint32>(length));
  }
  return data_->pdf_cg_.RenderPage(page_number, context, rect, params);
}
#endif

bool PdfMetafileSkia::SaveTo(base::File* file) const {
  if (GetDataSize() == 0U)
    return false;

  // Calling duplicate() keeps original asset state unchanged.
  scoped_ptr<SkStreamAsset> asset(data_->pdf_data_->duplicate());

  const size_t maximum_buffer_size = 1024 * 1024;
  std::vector<char> buffer(std::min(maximum_buffer_size, asset->getLength()));
  do {
    size_t read_size = asset->read(&buffer[0], buffer.size());
    if (read_size == 0)
      break;
    DCHECK_GE(buffer.size(), read_size);
    if (!file->WriteAtCurrentPos(&buffer[0],
                                 base::checked_cast<int>(read_size))) {
      return false;
    }
  } while (!asset->isAtEnd());

  return true;
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
bool PdfMetafileSkia::SaveToFD(const base::FileDescriptor& fd) const {
  DCHECK_GT(GetDataSize(), 0U);

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
  // If we only ever need the metafile for the last page, should we
  // only keep a handle on one SkPicture?
  scoped_ptr<PdfMetafileSkia> metafile(new PdfMetafileSkia);

  if (data_->pages_.size() == 0)
    return metafile.Pass();

  if (data_->recorder_.getRecordingCanvas())  // page outstanding
    return metafile.Pass();

  const Page& page = data_->pages_.back();

  metafile->data_->pages_.push_back(page);

  if (!metafile->FinishDocument())  // Generate PDF.
    metafile.reset();

  return metafile.Pass();
}

}  // namespace printing
