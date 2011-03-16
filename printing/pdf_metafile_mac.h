// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_MAC_H_
#define PRINTING_PDF_METAFILE_MAC_H_

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "printing/native_metafile.h"

class FilePath;

namespace gfx {
class Rect;
class Size;
class Point;
}

namespace printing {

// This class creates a graphics context that renders into a PDF data stream.
class PdfMetafile : public NativeMetafile {
 public:

  virtual ~PdfMetafile();

  // NativeMetafile methods.
  virtual bool Init();
  virtual bool Init(const void* src_buffer, uint32 src_buffer_size);

  virtual CGContextRef StartPage(const gfx::Size& page_size,
                                 const gfx::Point& content_origin,
                                 const float& scale_factor);
  virtual bool FinishPage();
  virtual bool Close();

  virtual uint32 GetDataSize() const;
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  // For testing purposes only.
  virtual bool SaveTo(const FilePath& file_path) const;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const;
  virtual unsigned int GetPageCount() const;

  // Note: The returned context *must not be retained* past Close(). If it is,
  // the data returned from GetData will not be valid PDF data.
  virtual CGContextRef context() const;

  virtual bool RenderPage(unsigned int page_number,
                          CGContextRef context,
                          const CGRect rect,
                          bool shrink_to_fit,
                          bool stretch_to_fit,
                          bool center_horizontally,
                          bool center_vertically) const;

 protected:
  PdfMetafile();

 private:
  friend class NativeMetafileFactory;
  FRIEND_TEST_ALL_PREFIXES(PdfMetafileTest, Pdf);

  // Returns a CGPDFDocumentRef version of pdf_data_.
  CGPDFDocumentRef GetPDFDocument() const;

  // Context for rendering to the pdf.
  base::mac::ScopedCFTypeRef<CGContextRef> context_;

  // PDF backing store.
  base::mac::ScopedCFTypeRef<CFMutableDataRef> pdf_data_;

  // Lazily-created CGPDFDocument representation of pdf_data_.
  mutable base::mac::ScopedCFTypeRef<CGPDFDocumentRef> pdf_doc_;

  // Whether or not a page is currently open.
  bool page_is_open_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafile);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_MAC_H_
