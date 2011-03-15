// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_ps_metafile_cairo.h"

#include <stdio.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "base/eintr_wrapper.h"
#include "base/file_descriptor_posix.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "printing/units.h"
#include "skia/ext/vector_platform_device_linux.h"

namespace {

const cairo_user_data_key_t kPdfMetafileKey = {0};

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

void DestroyContextData(void* data) {
  // Nothing to be done here.
}

}  // namespace

namespace printing {

PdfPsMetafile::PdfPsMetafile()
    : surface_(NULL),
      context_(NULL) {
}

PdfPsMetafile::~PdfPsMetafile() {
  // Releases all resources if we forgot to do so.
  CleanUpAll();
}

bool PdfPsMetafile::Init() {
  // We need to check at least these two members to ensure Init() has not been
  // called before.
  DCHECK(!context_);
  DCHECK(data_.empty());

  // Creates an 1 by 1 Cairo surface for the entire PDF file.
  // The size for each page will be overwritten later in StartPage().
  surface_ = cairo_pdf_surface_create_for_stream(WriteCairoStream,
                                                 &data_, 1, 1);

  // Cairo always returns a valid pointer.
  // Hence, we have to check if it points to a "nil" object.
  if (!IsSurfaceValid(surface_)) {
    DLOG(ERROR) << "Cannot create Cairo surface for PdfPsMetafile!";
    CleanUpSurface(&surface_);
    return false;
  }

  // Creates a context.
  context_ = cairo_create(surface_);
  if (!IsContextValid(context_)) {
    DLOG(ERROR) << "Cannot create Cairo context for PdfPsMetafile!";
    CleanUpContext(&context_);
    CleanUpSurface(&surface_);
    return false;
  }

  cairo_set_user_data(context_, &kPdfMetafileKey, this, DestroyContextData);

  return true;
}

bool PdfPsMetafile::Init(const void* src_buffer, uint32 src_buffer_size) {
  // We need to check at least these two members to ensure Init() has not been
  // called before
  DCHECK(!context_);
  DCHECK(data_.empty());

  if (src_buffer == NULL || src_buffer_size == 0)
    return false;

  data_ = std::string(reinterpret_cast<const char*>(src_buffer),
                      src_buffer_size);

  return true;
}

bool PdfPsMetafile::SetRawData(const void* src_buffer,
                               uint32 src_buffer_size) {
  if (!context_) {
    // If Init has not already been called, just call Init()
    return Init(src_buffer, src_buffer_size);
  }
  // If a context has already been created, remember this data in
  // raw_override_data_
  if (src_buffer == NULL || src_buffer_size == 0)
    return false;

  raw_override_data_ = std::string(reinterpret_cast<const char*>(src_buffer),
                                   src_buffer_size);

  return true;
}

cairo_t* PdfPsMetafile::StartPage(double width_in_points,
                                  double height_in_points,
                                  double margin_top_in_points,
                                  double margin_right_in_points,
                                  double margin_bottom_in_points,
                                  double margin_left_in_points) {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));
  // Passing this check implies page_surface_ is NULL, and current_page_ is
  // empty.
  DCHECK_GT(width_in_points, 0.);
  DCHECK_GT(height_in_points, 0.);

  // We build in extra room for the margins. The Cairo PDF backend will scale
  // the output to fit a page.
  double width =
      width_in_points + margin_left_in_points + margin_right_in_points;
  double height =
      height_in_points + margin_top_in_points + margin_bottom_in_points;

  // Don't let WebKit draw over the margins.
  cairo_surface_set_device_offset(surface_,
                                  margin_left_in_points,
                                  margin_top_in_points);

  cairo_pdf_surface_set_size(surface_, width, height);
  return context_;
}

bool PdfPsMetafile::FinishPage() {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));

  // Flushes all rendering for current page.
  cairo_surface_flush(surface_);
  cairo_show_page(context_);
  return true;
}

void PdfPsMetafile::Close() {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));

  cairo_surface_finish(surface_);

  // If we have raw PDF data set use that instead of what was drawn.
  if (!raw_override_data_.empty()) {
    data_ = raw_override_data_;
    raw_override_data_.clear();
  }
  DCHECK(!data_.empty());  // Make sure we did get something.

  CleanUpContext(&context_);
  CleanUpSurface(&surface_);
}

uint32 PdfPsMetafile::GetDataSize() const {
  // We need to check at least these two members to ensure that either Init()
  // has been called to initialize |data_|, or metafile has been closed.
  DCHECK(!context_);
  DCHECK(!data_.empty());

  return data_.size();
}

bool PdfPsMetafile::GetData(void* dst_buffer, uint32 dst_buffer_size) const {
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0u);
  memcpy(dst_buffer, data_.data(), dst_buffer_size);

  return true;
}

bool PdfPsMetafile::SaveTo(const base::FileDescriptor& fd) const {
  // We need to check at least these two members to ensure that either Init()
  // has been called to initialize |data_|, or metafile has been closed.
  DCHECK(!context_);
  DCHECK(!data_.empty());

  if (fd.fd < 0) {
    DLOG(ERROR) << "Invalid file descriptor!";
    return false;
  }

  bool success = true;
  if (file_util::WriteFileDescriptor(fd.fd, data_.data(),
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

PdfPsMetafile* PdfPsMetafile::FromCairoContext(cairo_t* context) {
  return reinterpret_cast<PdfPsMetafile*>(
      cairo_get_user_data(context, &kPdfMetafileKey));
}

void PdfPsMetafile::CleanUpAll() {
  CleanUpContext(&context_);
  CleanUpSurface(&surface_);
  data_.clear();
  skia::VectorPlatformDevice::ClearFontCache();
}

}  // namespace printing
