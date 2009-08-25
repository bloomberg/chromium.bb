// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_PS_METAFILE_LINUX_H_
#define PRINTING_PDF_PS_METAFILE_LINUX_H_

#include <cairo.h>

#include <string>

#include "base/basictypes.h"

class FilePath;

namespace printing {

// This class uses Cairo graphics library to generate PostScript/PDF stream
// and stores rendering results in a string buffer.
class PdfPsMetafile {
 public:
  enum FileFormat {
    PDF,
    PS,
  };

  // The constructor we should use in the renderer process.
  explicit PdfPsMetafile(const FileFormat& format);

  // The constructor we should use in the browser process.
  // |src_buffer| should point to the shared memory which stores PDF/PS
  // contents generated in the renderer.
  PdfPsMetafile(const FileFormat& format,
                const void* src_buffer,
                size_t src_buffer_size);

  ~PdfPsMetafile();

  FileFormat GetFileFormat() { return format_; }

  // Prepares a new cairo surface/context for rendering a new page.
  bool StartPage(double width, double height);  // The unit is pt (=1/72 in).

  // Returns the Cairo context for rendering current page.
  cairo_t* GetPageContext() const { return page_context_; }

  // Destroys the surface and the context used in rendering current page.
  // The results of current page will be appended into buffer |all_pages_|.
  // TODO(myhuang): I plan to also do page setup here (margins, the header
  // and the footer). At this moment, only pre-defined margins for US letter
  // paper are hard-coded here.
  // |shrink| decides the scaling factor to fit raw printing results into
  // printable area.
  void FinishPage(float shrink);

  // Closes resulting PDF/PS file. No further rendering is allowed.
  void Close();

  // Returns size of PDF/PS contents stored in buffer |all_pages_|.
  // This function should ONLY be called after PDF/PS file is closed.
  unsigned int GetDataSize() const;

  // Copies PDF/PS contents stored in buffer |all_pages_| into |dst_buffer|.
  // This function should ONLY be called after PDF/PS file is closed.
  void GetData(void* dst_buffer, size_t dst_buffer_size) const;

  // Saves PDF/PS contents stored in buffer |all_pages_| into |filename| on
  // the disk.
  // This function should ONLY be called after PDF/PS file is closed.
  bool SaveTo(const FilePath& filename) const;

 private:
  // Callback function for Cairo to write PDF/PS stream.
  // |dst_buffer| is actually a pointer of type `std::string*`.
  static cairo_status_t WriteCairoStream(void* dst_buffer,
                                         const unsigned char* src_data,
                                         unsigned int src_data_length);

  // Convenient function to test if |surface| is valid.
  bool IsSurfaceValid(cairo_surface_t* surface) const;

  // Convenient function to test if |context| is valid.
  bool IsContextValid(cairo_t* context) const;

  FileFormat format_;

  // Cairo surface and context for entire PDF/PS file.
  cairo_surface_t* surface_;
  cairo_t* context_;

  // Cairo surface and context for current page only.
  cairo_surface_t* page_surface_;
  cairo_t* page_context_;

  // Buffer stores PDF/PS contents for entire PDF/PS file.
  std::string all_pages_;

  // Buffer stores PDF/PS contents for current page only.
  std::string current_page_;

  DISALLOW_COPY_AND_ASSIGN(PdfPsMetafile);
};

}  // namespace printing

#endif  // PRINTING_PDF_PS_METAFILE_LINUX_H_

