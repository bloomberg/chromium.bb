// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_PS_METAFILE_CAIRO_H_
#define PRINTING_PDF_PS_METAFILE_CAIRO_H_

#include <string>

#include "base/basictypes.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

namespace base {
struct FileDescriptor;
}

class FilePath;

namespace printing {

// This class uses Cairo graphics library to generate PostScript/PDF stream
// and stores rendering results in a string buffer.
class PdfPsMetafile {
 public:
  PdfPsMetafile();
  ~PdfPsMetafile();

  // Initializes to a fresh new metafile. Returns true on success.
  // Note: Only call in the renderer to allocate rendering resources.
  bool Init();

  // Initializes a copy of metafile from PDF stream data.
  // Returns true on success.
  // |src_buffer| should point to the shared memory which stores PDF
  // contents generated in the renderer.
  // Note: Only call in the browser to initialize |data_|.
  bool Init(const void* src_buffer, uint32 src_buffer_size);

  // Sets raw PDF data for the document. This is used when a cairo drawing
  // surface has already been created. This method will cause all subsequent
  // drawing on the surface to be discarded (in Close()). If Init() has not yet
  // been called this method simply calls the second version of Init.
  bool SetRawData(const void* src_buffer, uint32 src_buffer_size);

  // Prepares a new cairo surface/context for rendering a new page.
  // The unit is in point (=1/72 in).
  // Returns NULL when failed.
  cairo_t* StartPage(double width_in_points,
                     double height_in_points,
                     double margin_top_in_points,
                     double margin_right_in_points,
                     double margin_bottom_in_points,
                     double margin_left_in_points);

  // Destroys the surface and the context used in rendering current page.
  // The results of current page will be appended into buffer |data_|.
  // Returns true on success.
  bool FinishPage();

  // Closes resulting PDF file. No further rendering is allowed.
  void Close();

  // Returns size of PDF contents stored in buffer |data_|.
  // This function should ONLY be called after PDF file is closed.
  uint32 GetDataSize() const;

  // Copies PDF contents stored in buffer |data_| into |dst_buffer|.
  // This function should ONLY be called after PDF file is closed.
  // Returns true only when success.
  bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  // Saves PDF contents stored in buffer |data_| into the file
  // associated with |fd|.
  // This function should ONLY be called after PDF file is closed.
  bool SaveTo(const base::FileDescriptor& fd) const;

  // The hardcoded margins, in points. These values are based on 72 dpi,
  // with 0.25 margins on top, left, and right, and 0.56 on bottom.
  static const double kTopMarginInInch;
  static const double kRightMarginInInch;
  static const double kBottomMarginInInch;
  static const double kLeftMarginInInch;

  // Returns the PdfPsMetafile object that owns the given context. Returns NULL
  // if the context was not created by a PdfPsMetafile object.
  static PdfPsMetafile* FromCairoContext(cairo_t* context);

 private:
  // Cleans up all resources.
  void CleanUpAll();

  // Cairo surface and context for entire PDF file.
  cairo_surface_t* surface_;
  cairo_t* context_;

  // Buffer stores PDF contents for entire PDF file.
  std::string data_;
  // Buffer stores raw PDF contents set by SetRawPageData.
  std::string raw_override_data_;

  DISALLOW_COPY_AND_ASSIGN(PdfPsMetafile);
};

}  // namespace printing

#endif  // PRINTING_PDF_PS_METAFILE_CAIRO_H_
