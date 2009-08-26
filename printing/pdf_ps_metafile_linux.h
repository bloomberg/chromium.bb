// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_PS_METAFILE_LINUX_H_
#define PRINTING_PDF_PS_METAFILE_LINUX_H_

#include <string>

#include "base/basictypes.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

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

  // In the renderer process, callers should also call Init(void) to see if the
  // metafile can obtain all necessary rendering resources.
  // In the browser process, callers should also call Init(const void*, size_t)
  // to initialize the buffer |all_pages_| to use SaveTo().
  explicit PdfPsMetafile(const FileFormat& format);

  ~PdfPsMetafile();

  // Initializes to a fresh new metafile. Returns true on success.
  // Note: Only call in the renderer to allocate rendering resources.
  bool Init();

  // Initializes a copy of metafile from PDF/PS stream data.
  // Returns true on success.
  // |src_buffer| should point to the shared memory which stores PDF/PS
  // contents generated in the renderer.
  // Note: Only call in the browser to initialize |all_pages_|.
  bool Init(const void* src_buffer, size_t src_buffer_size);

  FileFormat GetFileFormat() const { return format_; }

  // Prepares a new cairo surface/context for rendering a new page.
  // The unit is in point (=1/72 in).
  // Returns NULL when failed.
  cairo_t* StartPage(double width, double height);

  // Destroys the surface and the context used in rendering current page.
  // The results of current page will be appended into buffer |all_pages_|.
  // Returns true on success
  // TODO(myhuang): I plan to also do page setup here (margins, the header
  // and the footer). At this moment, only pre-defined margins for US letter
  // paper are hard-coded here.
  // |shrink| decides the scaling factor to fit raw printing results into
  // printable area.
  bool FinishPage(float shrink);

  // Closes resulting PDF/PS file. No further rendering is allowed.
  void Close();

  // Returns size of PDF/PS contents stored in buffer |all_pages_|.
  // This function should ONLY be called after PDF/PS file is closed.
  unsigned int GetDataSize() const;

  // Copies PDF/PS contents stored in buffer |all_pages_| into |dst_buffer|.
  // This function should ONLY be called after PDF/PS file is closed.
  // Returns true only when success.
  bool GetData(void* dst_buffer, size_t dst_buffer_size) const;

  // Saves PDF/PS contents stored in buffer |all_pages_| into |filename| on
  // the disk.
  // This function should ONLY be called after PDF/PS file is closed.
  bool SaveTo(const FilePath& filename) const;

 private:
  // Cleans up all resources.
  void CleanUp();

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
