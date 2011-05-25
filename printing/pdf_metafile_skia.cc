// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_skia.h"

#include "base/eintr_wrapper.h"
#include "base/file_descriptor_posix.h"
#include "base/file_util.h"
#include "skia/ext/vector_platform_device_skia.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/pdf/SkPDFDevice.h"
#include "third_party/skia/include/pdf/SkPDFDocument.h"
#include "third_party/skia/include/pdf/SkPDFPage.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace printing {

struct PdfMetafileSkiaData {
  SkRefPtr<SkPDFDevice> current_page_;
  SkPDFDocument pdf_doc_;
  SkDynamicMemoryWStream pdf_stream_;
};

PdfMetafileSkia::~PdfMetafileSkia() {}

bool PdfMetafileSkia::Init() {
  return true;
}
bool PdfMetafileSkia::InitFromData(const void* src_buffer,
                                   uint32 src_buffer_size) {
  return data_->pdf_stream_.write(src_buffer, src_buffer_size);
}

SkDevice* PdfMetafileSkia::StartPageForVectorCanvas(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor) {
  DCHECK(data_->current_page_.get() == NULL);

  // Adjust for the margins and apply the scale factor.
  SkMatrix transform;
  transform.setTranslate(SkIntToScalar(content_area.x()),
                         SkIntToScalar(content_area.y()));
  transform.preScale(SkFloatToScalar(scale_factor),
                     SkFloatToScalar(scale_factor));

  // TODO(ctguil): Refactor: don't create the PDF device explicitly here.
  SkISize pdf_page_size = SkISize::Make(page_size.width(), page_size.height());
  SkISize pdf_content_size =
      SkISize::Make(content_area.width(), content_area.height());
  SkRefPtr<SkPDFDevice> pdf_device =
      new SkPDFDevice(pdf_page_size, pdf_content_size, transform);
  pdf_device->unref();  // SkRefPtr and new both took a reference.
  skia::VectorPlatformDeviceSkia* device =
      new skia::VectorPlatformDeviceSkia(pdf_device.get());
  data_->current_page_ = device->PdfDevice();
  return device;
}

bool PdfMetafileSkia::StartPage(const gfx::Size& page_size,
                                const gfx::Rect& content_area,
                                const float& scale_factor) {
  NOTREACHED();
  return NULL;
}

bool PdfMetafileSkia::FinishPage() {
  DCHECK(data_->current_page_.get());

  data_->pdf_doc_.appendPage(data_->current_page_);
  data_->current_page_ = NULL;
  return true;
}

bool PdfMetafileSkia::FinishDocument() {
  // Don't do anything if we've already set the data in InitFromData.
  if (data_->pdf_stream_.getOffset())
    return true;

  if (data_->current_page_.get())
    FinishPage();
  return data_->pdf_doc_.emitPDF(&data_->pdf_stream_);
}

uint32 PdfMetafileSkia::GetDataSize() const {
  return data_->pdf_stream_.getOffset();
}

bool PdfMetafileSkia::GetData(void* dst_buffer,
                              uint32 dst_buffer_size) const {
  if (dst_buffer_size < GetDataSize())
    return false;

  memcpy(dst_buffer, data_->pdf_stream_.getStream(), dst_buffer_size);
  return true;
}

bool PdfMetafileSkia::SaveTo(const FilePath& file_path) const {
  DCHECK_GT(data_->pdf_stream_.getOffset(), 0U);
  if (file_util::WriteFile(file_path, data_->pdf_stream_.getStream(),
                           GetDataSize()) != static_cast<int>(GetDataSize())) {
    DLOG(ERROR) << "Failed to save file " << file_path.value().c_str();
    return false;
  }
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

HENHMETAFILE PdfMetafileSkia::emf() const {
  NOTREACHED();
  return NULL;
}
#endif  // if defined(OS_WIN)

#if defined(OS_CHROMEOS)
bool PdfMetafileSkia::SaveToFD(const base::FileDescriptor& fd) const {
  DCHECK_GT(data_->pdf_stream_.getOffset(), 0U);

  if (fd.fd < 0) {
    DLOG(ERROR) << "Invalid file descriptor!";
    return false;
  }

  bool result = true;
  if (file_util::WriteFileDescriptor(fd.fd, data_->pdf_stream_.getStream(),
                                     GetDataSize()) !=
      static_cast<int>(GetDataSize())) {
    DLOG(ERROR) << "Failed to save file with fd " << fd.fd;
    result = false;
  }

  if (fd.auto_close) {
    if (HANDLE_EINTR(close(fd.fd)) < 0) {
      DPLOG(WARNING) << "close";
      result = false;
    }
  }
  return result;
}
#endif

PdfMetafileSkia::PdfMetafileSkia() : data_(new PdfMetafileSkiaData) {}

}  // namespace printing
