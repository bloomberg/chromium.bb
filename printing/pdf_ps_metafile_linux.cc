// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_ps_metafile_linux.h"

#include <stdio.h>

#include <cairo-pdf.h>
#include <cairo-ps.h>

#include "base/file_util.h"
#include "base/logging.h"

namespace printing {

PdfPsMetafile::PdfPsMetafile(const FileFormat& format)
    : format_(format),
      surface_(NULL), context_(NULL),
      page_surface_(NULL), page_context_(NULL) {

  // Create an 1 by 1 Cairo surface for entire PDF/PS file.
  // The size for each page will be overwritten later in StartPage().
  switch (format_) {
    case PDF: {
      surface_ = cairo_pdf_surface_create_for_stream(WriteCairoStream,
                                                     &all_pages_, 1, 1);
    }
    break;

    case PS: {
      surface_ = cairo_ps_surface_create_for_stream(WriteCairoStream,
                                                    &all_pages_, 1, 1);
    }
    break;

    default:
      NOTREACHED();
      return;
  }

  // Cairo always returns a valid pointer.
  // Hence, we have to check if it points to a "nil" object.
  if (!IsSurfaceValid(surface_)) {
    DLOG(ERROR) << "Cannot create Cairo surface for PdfPsMetafile!";
    cairo_surface_destroy(surface_);
    return;
  }

  // Create a context.
  context_ = cairo_create(surface_);
  if (!IsContextValid(context_)) {
    DLOG(ERROR) << "Cannot create Cairo context for PdfPsMetafile!";
    cairo_destroy(context_);
    cairo_surface_destroy(surface_);
    return;
  }

  // Since |context_| will keep a reference of |surface_|, we can decreace
  // surface's reference count by one safely.
  cairo_surface_destroy(surface_);
}

PdfPsMetafile::PdfPsMetafile(const FileFormat& format,
                             const void* src_buffer,
                             size_t src_buffer_size)
    : format_(format),
      surface_(NULL), context_(NULL),
      page_surface_(NULL), page_context_(NULL) {

  all_pages_ = std::string(reinterpret_cast<const char*>(src_buffer),
                           src_buffer_size);
}

PdfPsMetafile::~PdfPsMetafile() {
  // Releases resources if we forgot to do so.
  Close();
}

bool PdfPsMetafile::StartPage(double width_in_points, double height_in_points) {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));
  DCHECK(!page_surface_ && !page_context_);
  DCHECK_GT(width_in_points, 0.);
  DCHECK_GT(height_in_points, 0.);

  // Create a target surface for the new page.
  // Cairo 1.6.0 does NOT allow the first argument be NULL,
  // but some newer versions do support NULL pointer.
  switch (format_) {
    case PDF: {
      page_surface_ = cairo_pdf_surface_create_for_stream(WriteCairoStream,
                                                          &current_page_,
                                                          width_in_points,
                                                          height_in_points);
    }
    break;

    case PS: {
      page_surface_ = cairo_ps_surface_create_for_stream(WriteCairoStream,
                                                         &current_page_,
                                                         width_in_points,
                                                         height_in_points);
    }
    break;

    default:
      NOTREACHED();
      return false;
  }

  // Cairo always returns a valid pointer.
  // Hence, we have to check if it points to a "nil" object.
  if (!IsSurfaceValid(page_surface_)) {
    DLOG(ERROR) << "Cannot create Cairo surface for PdfPsMetafile!";
    cairo_surface_destroy(page_surface_);
    return false;
  }

  // Create a context.
  page_context_ = cairo_create(page_surface_);
  if (!IsContextValid(page_context_)) {
    DLOG(ERROR) << "Cannot create Cairo context for PdfPsMetafile!";
    cairo_destroy(page_context_);
    cairo_surface_destroy(page_surface_);
    return false;
  }

  // Since |page_context_| will keep a reference of |page_surface_|, we can
  // decreace surface's reference count by one safely.
  cairo_surface_destroy(page_surface_);

  return true;
}

void PdfPsMetafile::FinishPage(float shrink) {
  DCHECK(IsSurfaceValid(surface_));
  DCHECK(IsContextValid(context_));
  DCHECK(IsSurfaceValid(page_surface_));
  DCHECK(IsContextValid(page_context_));

  // Flush all rendering for current page.
  cairo_surface_flush(page_surface_);

  // TODO(myhuang): Use real page settings.
  // We hard-coded page settings here for testing purpose.
  // The paper size is US Letter (8.5 in. by 11 in.).
  // The default margins are:
  //   Left = 0.25 in.
  //   Right = 0.25 in.
  //   Top = 0.25 in.
  //   Bottom = 0.56 in.
  const double kDPI = 72.0;  // Dots (points) per inch.
  const double kWidthInInch = 8.5;
  const double kHeightInInch = 11.0;
  const double kWidthInPoint = kWidthInInch * kDPI;
  const double kHeightInPoint = kHeightInInch * kDPI;
  switch (format_) {
    case PDF: {
      cairo_pdf_surface_set_size(surface_, kWidthInPoint, kHeightInPoint);
    }
    break;

    case PS: {
      cairo_ps_surface_set_size(surface_, kWidthInPoint, kHeightInPoint);
    }
    break;

    default:
      NOTREACHED();
      return;
  }

  // Save context's states.
  cairo_save(context_);
  // Copy current page onto the surface of final result.
  // Margins are done by coordinates transformation.
  // Please NOTE that we have to call cairo_scale() before we call
  // cairo_set_source_surface().
  const double scale_factor = 1. / shrink;
  cairo_scale(context_, scale_factor, scale_factor);
  const double kLeftMarginInInch = 0.25;
  const double kTopMarginInInch = 0.25;
  const double kLeftMarginInPoint = kLeftMarginInInch * kDPI;
  const double kTopMarginInPoint = kTopMarginInInch * kDPI;
  const double kScaledLeftMarginInPoint = kLeftMarginInPoint * shrink;
  const double kScaledTopMarginInPoint = kTopMarginInPoint * shrink;
  cairo_set_source_surface(context_,
                           page_surface_,
                           kScaledLeftMarginInPoint,
                           kScaledTopMarginInPoint);
  // In Cairo 1.6.0, if we use the following API, either the renderer will
  // crash, or we will get an empty page. This might be a bug in Cairo.
  // cairo_set_operator(context_, CAIRO_OPERATOR_SOURCE);
  const double kRightMarginInInch = 0.25;
  const double kBottomMarginInInch = 0.56;
  const double kPrintableWidthInInch =
      kWidthInInch - kLeftMarginInInch - kRightMarginInInch;
  const double kPrintableHeightInInch =
      kHeightInInch - kTopMarginInInch - kBottomMarginInInch;
  const double kScaledPrintableWidthInPoint =
      kPrintableWidthInInch * kDPI * shrink;
  const double kScaledPrintableHeightInPoint =
      kPrintableHeightInInch * kDPI * shrink;
  cairo_rectangle(context_,
                  kScaledLeftMarginInPoint,
                  kScaledTopMarginInPoint,
                  kScaledPrintableWidthInPoint,
                  kScaledPrintableHeightInPoint);
  cairo_fill(context_);

  // Finishing the duplication of current page.
  cairo_show_page(context_);
  cairo_surface_flush(surface_);

  // Destroy resoreces for current page.
  cairo_destroy(page_context_);
  page_context_ = NULL;
  page_surface_ = NULL;
  current_page_.clear();

  // Restore context's states.
  cairo_restore(context_);
}

void PdfPsMetafile::Close() {
  if (surface_ != NULL && IsSurfaceValid(surface_)) {
    cairo_surface_finish(surface_);
    surface_ = NULL;
  }
  if (context_ != NULL && IsContextValid(context_)) {
    cairo_destroy(context_);
    context_ = NULL;
  }
}

unsigned int PdfPsMetafile::GetDataSize() const {
  DCHECK(!surface_ && !context_);

  return all_pages_.size();
}

void PdfPsMetafile::GetData(void* dst_buffer, size_t dst_buffer_size) const {
  DCHECK(!surface_ && !context_);
  DCHECK(dst_buffer);

  size_t data_size = GetDataSize();
  if (data_size < dst_buffer_size)
    dst_buffer_size = data_size;
  memcpy(dst_buffer, all_pages_.data(), dst_buffer_size);
}

bool PdfPsMetafile::SaveTo(const FilePath& filename) const {
  DCHECK(!surface_ && !context_);

  const unsigned int data_size = GetDataSize();
  const unsigned int bytes_written =
      file_util::WriteFile(filename, all_pages_.data(), data_size);
  if (bytes_written != data_size) {
    DLOG(ERROR) << "Failed to save file: " << filename.value();
    return false;
  }

  return true;
}

cairo_status_t PdfPsMetafile::WriteCairoStream(void* dst_buffer,
                                               const unsigned char* src_data,
                                               unsigned int src_data_length) {
  DCHECK(dst_buffer);
  DCHECK(src_data);

  std::string* buffer = reinterpret_cast<std::string* >(dst_buffer);
  buffer->append(reinterpret_cast<const char*>(src_data), src_data_length);

  return CAIRO_STATUS_SUCCESS;
}

bool PdfPsMetafile::IsSurfaceValid(cairo_surface_t* surface) const {
  return cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS;
}

bool PdfPsMetafile::IsContextValid(cairo_t* context) const {
  return cairo_status(context) == CAIRO_STATUS_SUCCESS;
}

}  // namespace printing

