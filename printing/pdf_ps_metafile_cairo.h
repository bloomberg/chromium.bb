// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_PS_METAFILE_CAIRO_H_
#define PRINTING_PDF_PS_METAFILE_CAIRO_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "printing/native_metafile.h"

typedef struct _cairo_surface cairo_surface_t;

namespace printing {

// This class uses Cairo graphics library to generate PDF stream and stores
// rendering results in a string buffer.
class PdfPsMetafile : public NativeMetafile {
 public:
  virtual ~PdfPsMetafile();

  // NativeMetafile methods.
  virtual bool Init();
  virtual bool Init(const void* src_buffer, uint32 src_buffer_size);

  virtual cairo_t* StartPage(const gfx::Size& page_size,
                             double margin_top_in_points,
                             double margin_left_in_points);
  virtual bool FinishPage();
  virtual bool Close();

  virtual uint32 GetDataSize() const;
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  virtual bool SaveTo(const FilePath& file_path) const;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const;
  virtual unsigned int GetPageCount() const;

  virtual cairo_t* context() const;

  virtual bool SetRawData(const void* src_buffer, uint32 src_buffer_size);

#if defined(OS_CHROMEOS)
  virtual bool SaveToFD(const base::FileDescriptor& fd) const;
#endif  // if defined(OS_CHROMEOS)

  // Returns the PdfPsMetafile object that owns the given context. Returns NULL
  // if the context was not created by a PdfPsMetafile object.
  static PdfPsMetafile* FromCairoContext(cairo_t* context);

 protected:
  PdfPsMetafile();

 private:
  friend class NativeMetafileFactory;
  FRIEND_TEST_ALL_PREFIXES(PdfPsTest, Pdf);

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
