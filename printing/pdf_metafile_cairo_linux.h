// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_CAIRO_LINUX_H_
#define PRINTING_PDF_METAFILE_CAIRO_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "printing/metafile.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

typedef struct _cairo_surface cairo_surface_t;

namespace printing {

// This class uses Cairo graphics library to generate PDF stream and stores
// rendering results in a string buffer.
class PRINTING_EXPORT PdfMetafileCairo : public Metafile {
 public:
  PdfMetafileCairo();
  virtual ~PdfMetafileCairo();

  // Metafile methods.
  virtual bool Init();

  //  Calling InitFromData() sets the data for this metafile and masks data
  //  induced by previous calls to Init() or InitFromData(), even if drawing
  //  continues on the surface returned by a previous call to Init().
  virtual bool InitFromData(const void* src_buffer, uint32 src_buffer_size);

  virtual SkDevice* StartPageForVectorCanvas(
      const gfx::Size& page_size, const gfx::Rect& content_area,
      const float& scale_factor);

  virtual bool StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         const float& scale_factor);
  virtual bool FinishPage();
  virtual bool FinishDocument();

  virtual uint32 GetDataSize() const;
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  virtual bool SaveTo(const FilePath& file_path) const;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const;
  virtual unsigned int GetPageCount() const;

  virtual cairo_t* context() const;

#if defined(OS_CHROMEOS)
  virtual bool SaveToFD(const base::FileDescriptor& fd) const;
#endif  // if defined(OS_CHROMEOS)

 private:
  // Cleans up all resources.
  void CleanUpAll();

  // Cairo surface and context for entire PDF file.
  cairo_surface_t* surface_;
  cairo_t* context_;

  // Buffer stores PDF contents for entire PDF file.
  std::string cairo_data_;
  // Buffer stores PDF contents. It can only be populated from InitFromData().
  // Any calls to StartPage(), FinishPage(), FinishDocument() do not affect
  // this buffer.
  // Note: Such calls will result in DCHECK errors if Init() has not been called
  // first.
  std::string raw_data_;
  // Points to the appropriate buffer depending on the way the object was
  // initialized (Init() vs InitFromData()).
  std::string* current_data_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafileCairo);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_CAIRO_LINUX_H_
