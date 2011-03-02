// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_MAC_H_
#define PRINTING_PDF_METAFILE_MAC_H_

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"

namespace gfx {
class Rect;
class Size;
class Point;
}
class FilePath;

namespace printing {

// This class creates a graphics context that renders into a PDF data stream.
class PdfMetafile {
 public:
  // To create PDF data, callers should also call Init() to set up the
  // rendering context.
  // To create a metafile from existing Data, callers should also call
  // Init(const void*, uint32).
  PdfMetafile();

  ~PdfMetafile();

  // Initializes a new metafile, and returns a drawing context for rendering
  // into the PDF. Returns NULL on failure.
  // Note: The returned context *must not be retained* past Close(). If it is,
  // the data returned from GetData will not be valid PDF data.
  CGContextRef Init();

  // Initializes a copy of metafile from PDF data. Returns true on success.
  bool Init(const void* src_buffer, uint32 src_buffer_size);

  // Prepares a new pdf page at specified |content_origin| with the given
  // |page_size| and a |scale_factor| to use for the drawing.
  CGContextRef StartPage(const gfx::Size& page_size,
                         const gfx::Point& content_origin,
                         const float& scale_factor);

  // Closes the current page.
  void FinishPage();

  // Closes the PDF file; no further rendering is allowed.
  void Close();

  // Renders the given page into |rect| in the given context.
  // Pages use a 1-based index. The rendering uses the following arguments
  // to determine scaling and translation factors.
  // |shrink_to_fit| specifies whether the output should be shrunk to fit the
  // supplied |rect| if the page size is larger than |rect| in any dimension.
  // If this is false, parts of the PDF page that lie outside the bounds will be
  // clipped.
  // |stretch_to_fit| specifies whether the output should be stretched to fit
  // the supplied bounds if the page size is smaller than |rect| in all
  // dimensions.
  // |center_horizontally| specifies whether the final image (after any scaling
  // is done) should be centered horizontally within the given |rect|.
  // |center_vertically| specifies whether the final image (after any scaling
  // is done) should be centered vertically within the given |rect|.
  // Note that all scaling preserves the original aspect ratio of the page.
  bool RenderPage(unsigned int page_number, CGContextRef context,
                  const CGRect rect, bool shrink_to_fit, bool stretch_to_fit,
                  bool center_horizontally, bool center_vertically) const;
  unsigned int GetPageCount() const;

  // Returns the bounds of the given page.
  // Pages use a 1-based index.
  gfx::Rect GetPageBounds(unsigned int page_number) const;

  // Returns the size of the underlying PDF data. Only valid after Close() has
  // been called.
  uint32 GetDataSize() const;

  // Copies the first |dst_buffer_size| bytes of the PDF data into |dst_buffer|.
  // Only valid after Close() has been called.
  // Returns true if the copy succeeds.
  bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  // Saves the raw PDF data to the given file. For testing only.
  // Returns true if writing succeeded.
  bool SaveTo(const FilePath& file_path) const;

 private:
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
