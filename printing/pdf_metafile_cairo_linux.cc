// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cairo_linux.h"

#include <stdio.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "base/eintr_wrapper.h"
#include "base/file_descriptor_posix.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "printing/units.h"
#include "skia/ext/vector_platform_device_cairo_linux.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

// Tests if |surface| is valid.
bool IsSurfaceValid(cairo_surface_t* surface) {
  return cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS;
}

// Tests if |context| is valid.
bool IsContextValid(cairo_t* context) {
  return cairo_status(context) == CAIRO_STATUS_SUCCESS;
}

// Destroys and resets |surface|.
void CleanUpSurface(cairo_surface_t** surface) {
  if (*surface) {
    cairo_surface_destroy(*surface);
    *surface = NULL;
  }
}

// Destroys and resets |context|.
void CleanUpContext(cairo_t** context) {
  if (*context) {
    cairo_destroy(*context);
    *context = NULL;
  }
}

// Callback function for Cairo to write PDF stream.
// |dst_buffer| is actually a pointer of type `std::string*`.
cairo_status_t WriteCairoStream(void* dst_buffer,
                                const unsigned char* src_data,
                                unsigned int src_data_length) {
  DCHECK(dst_buffer);
  DCHECK(src_data);
  DCHECK_GT(src_data_length, 0u);

  std::string* buffer = reinterpret_cast<std::string*>(dst_buffer);
  buffer->append(reinterpret_cast<const char*>(src_data), src_data_length);

  return CAIRO_STATUS_SUCCESS;
}

}  // namespace

namespace printing {

PdfMetafileCairo::PdfMetafileCairo()
    : surface_(NULL),
      context_(NULL),
      current_data_(NULL) {
}

PdfMetafileCairo::~PdfMetafileCairo() {
  // Releases all resources if we forgot to do so.
  CleanUpAll();
}

bool PdfMetafileCairo::Init() {
  // We need to check |current_data_| to ensure Init/InitFromData has not been
  // called before.
  DCHECK(!current_data_);

  current_data_ = &cairo_data_;
  // Creates an 1 by 1 Cairo surface for the entire PDF file.
  // The size for each page will be overwritten later in StartPage().
  surface_ = cairo_pdf_surface_create_for_stream(WriteCairoStream,
                                                 current_data_, 1, 1);

  // Cairo always returns a valid pointer.
  // Hence, we have to check if it points to a "nil" object.
  if (!IsSurfaceValid(surface_)) {
    DLOG(ERROR) << "Cannot create Cairo surface for PdfMetafileCairo!";
    CleanUpSurface(&surface_);
    return false;
  }

  // Creates a context.
  context_ = cairo_create(surface_);
  if (!IsContextValid(context_)) {
    DLOG(ERROR) << "Cannot create Cairo context for PdfMetafileCairo!";
    CleanUpContext(&context_);
    CleanUpSurface(&surface_);
    return false;
  }

  return true;
}

bool PdfMetafileCairo::InitFromData(const void* src_buffer,
                                    uint32 src_buffer_size) {
  if (src_buffer == NULL || src_buffer_size == 0)
    return false;

  raw_data_ = std::string(reinterpret_cast<const char*>(src_buffer),
                      src_buffer_size);
  current_data_ = &raw_data_;
  return true;
}

skia::PlatformDevice* PdfMetafileCairo::StartPageForVectorCanvas(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor) {
  if (!StartPage(page_size, content_area, scale_factor))
    return NULL;

  return skia::VectorPlatformDeviceCairoFactory::CreateDevice(
      context_, page_size.width(), page_size.height(), true);
}

bool PdfMetafileCairo::StartPage(const gfx::Size& page_size,
                                 const gfx::Rect& content_area,
                                 const float& scale_factor) {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));
  // Passing this check implies page_surface_ is NULL, and current_page_ is
  // empty.
  DCHECK_GT(page_size.width(), 0);
  DCHECK_GT(page_size.height(), 0);
  // |scale_factor| is not supported yet.
  DCHECK_EQ(scale_factor, 1);

  // Don't let WebKit draw over the margins.
  cairo_surface_set_device_offset(surface_,
                                  content_area.x(),
                                  content_area.y());

  cairo_pdf_surface_set_size(surface_, page_size.width(), page_size.height());
  return context_ != NULL;
}

bool PdfMetafileCairo::FinishPage() {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));

  // Flushes all rendering for current page.
  cairo_surface_flush(surface_);
  cairo_show_page(context_);
  return true;
}

bool PdfMetafileCairo::FinishDocument() {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));

  cairo_surface_finish(surface_);

  DCHECK(!cairo_data_.empty());  // Make sure we did get something.

  CleanUpContext(&context_);
  CleanUpSurface(&surface_);
  return true;
}

uint32 PdfMetafileCairo::GetDataSize() const {
  // We need to check at least these two members to ensure that either Init()
  // has been called to initialize |data_|, or metafile has been closed.
  DCHECK(!context_);
  DCHECK(!current_data_->empty());

  return current_data_->size();
}

bool PdfMetafileCairo::GetData(void* dst_buffer, uint32 dst_buffer_size) const {
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0u);
  memcpy(dst_buffer, current_data_->data(), dst_buffer_size);

  return true;
}

cairo_t* PdfMetafileCairo::context() const {
  return context_;
}

bool PdfMetafileCairo::SaveTo(const FilePath& file_path) const {
  // We need to check at least these two members to ensure that either Init()
  // has been called to initialize |data_|, or metafile has been closed.
  DCHECK(!context_);
  DCHECK(!current_data_->empty());

  bool success = true;
  if (file_util::WriteFile(file_path, current_data_->data(), GetDataSize())
      != static_cast<int>(GetDataSize())) {
    DLOG(ERROR) << "Failed to save file " << file_path.value().c_str();
    success = false;
  }
  return success;
}

gfx::Rect PdfMetafileCairo::GetPageBounds(unsigned int page_number) const  {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

unsigned int PdfMetafileCairo::GetPageCount() const {
  NOTIMPLEMENTED();
  return 1;
}

#if defined(OS_CHROMEOS)
bool PdfMetafileCairo::SaveToFD(const base::FileDescriptor& fd) const {
  // We need to check at least these two members to ensure that either Init()
  // has been called to initialize |data_|, or metafile has been closed.
  DCHECK(!context_);
  DCHECK(!current_data_->empty());

  if (fd.fd < 0) {
    DLOG(ERROR) << "Invalid file descriptor!";
    return false;
  }

  bool success = true;
  if (file_util::WriteFileDescriptor(fd.fd, current_data_->data(),
                                     GetDataSize()) < 0) {
    DLOG(ERROR) << "Failed to save file with fd " << fd.fd;
    success = false;
  }

  if (fd.auto_close) {
    if (HANDLE_EINTR(close(fd.fd)) < 0) {
      DPLOG(WARNING) << "close";
      success = false;
    }
  }

  return success;
}
#endif  // if defined(OS_CHROMEOS)

void PdfMetafileCairo::CleanUpAll() {
  CleanUpContext(&context_);
  CleanUpSurface(&surface_);
  cairo_data_.clear();
  raw_data_.clear();
  skia::VectorPlatformDeviceCairo::ClearFontCache();
}

}  // namespace printing
