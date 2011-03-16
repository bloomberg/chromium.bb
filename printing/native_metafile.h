// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_H_
#define PRINTING_NATIVE_METAFILE_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vector>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/scoped_cftyperef.h"
#endif

class FilePath;

namespace gfx {
class Rect;
class Size;
class Point;
}

#if defined(OS_CHROMEOS)
namespace base {
class  FileDescriptor;
}
#endif

namespace printing {

// This class creates a graphics context that renders into a data stream
// (usually PDF or EMF).
class NativeMetafile {
 public:
  virtual ~NativeMetafile() {}

  // Initializes a fresh new metafile for rendering. Returns false on failure.
  // Note: It should only be called from within the renderer process to allocate
  // rendering resources.
  virtual bool Init() = 0;

  // Initializes the metafile with the data in |src_buffer|. Returns true
  // on success.
  // Note: It should only be called from within the browser process.
  virtual bool Init(const void* src_buffer, uint32 src_buffer_size) = 0;

#if defined(OS_WIN)
  // Inserts a custom GDICOMMENT records indicating StartPage/EndPage calls
  // (since StartPage and EndPage do not work in a metafile DC). Only valid
  // when hdc_ is non-NULL.
  virtual bool StartPage() = 0;
#elif defined(OS_MACOSX)
  // Prepares a new pdf page at specified |content_origin| with the given
  // |page_size| and a |scale_factor| to use for the drawing.
  virtual gfx::NativeDrawingContext StartPage(const gfx::Size& page_size,
                                              const gfx::Point& content_origin,
                                              const float& scale_factor) = 0;
#elif defined(OS_POSIX)
  // Prepares a new cairo surface/context for rendering a new page.
  // The unit is in point (=1/72 in).
  // Returns NULL when failed.
  virtual gfx::NativeDrawingContext StartPage(const gfx::Size& page_size,
                                              double margin_top_in_points,
                                              double margin_left_in_points) = 0;
#endif


  // Closes the current page and destroys the context used in rendering that
  // page. The results of current page will be appended into the underlying
  // data stream. Returns true on success.
  virtual bool FinishPage() = 0;

  // Closes the metafile. No further rendering is allowed (the current page
  // is implicitly closed).
  virtual bool Close() = 0;

  // Returns the size of the underlying data stream. Only valid after Close()
  // has been called.
  virtual uint32 GetDataSize() const = 0;

  // Copies the first |dst_buffer_size| bytes of the underlying data stream into
  // |dst_buffer|. This function should ONLY be called after Close() is invoked.
  // Returns true if the copy succeeds.
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const = 0;

  // Saves the underlying data to the given file. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool SaveTo(const FilePath& file_path) const = 0;

  // Returns the bounds of the given page. Pages use a 1-based index.
  virtual gfx::Rect GetPageBounds(unsigned int page_number) const = 0;
  virtual unsigned int GetPageCount() const = 0;

  // Get the context for rendering to the PDF.
  virtual gfx::NativeDrawingContext context() const = 0;

#if defined(OS_WIN)
  // Generates a virtual HDC that will record every GDI commands and compile it
  // in a EMF data stream.
  // hdc is used to setup the default DPI and color settings. hdc is optional.
  // rect specifies the dimensions (in .01-millimeter units) of the EMF. rect is
  // optional.
  virtual bool CreateDc(gfx::NativeDrawingContext sibling,
                        const RECT* rect) = 0;

  // Similar to the above method but the metafile is backed by a file.
  virtual bool CreateFileBackedDc(gfx::NativeDrawingContext sibling,
                                  const RECT* rect,
                                  const FilePath& path) = 0;

  // TODO(maruel): CreateFromFile(). If ever used. Maybe users would like to
  // have the ability to save web pages to an EMF file? Afterward, it is easy to
  // convert to PDF or PS.
  // Load a metafile fromdisk.
  virtual bool CreateFromFile(const FilePath& metafile_path) = 0;

  // Closes the HDC created by CreateDc() and generates the compiled EMF
  // data.
  virtual void CloseEmf() = 0;

  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. |rect| is in
  // "logical units" and is optional. If |rect| is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  virtual bool Playback(gfx::NativeDrawingContext hdc,
                        const RECT* rect) const = 0;

  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  virtual bool SafePlayback(gfx::NativeDrawingContext hdc) const = 0;

  // Retrieves the underlying data stream. It is a helper function.
  virtual bool GetData(std::vector<uint8>* buffer) const = 0;

  virtual HENHMETAFILE emf() const = 0;
#elif defined(OS_MACOSX)
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
  virtual bool RenderPage(unsigned int page_number,
                          gfx::NativeDrawingContext context,
                          const CGRect rect,
                          bool shrink_to_fit,
                          bool stretch_to_fit,
                          bool center_horizontally,
                          bool center_vertically) const = 0;
#elif defined(OS_POSIX)
  // Sets raw PDF data for the document. This is used when a cairo drawing
  // surface has already been created. This method will cause all subsequent
  // drawing on the surface to be discarded (in Close()). If Init() has not yet
  // been called this method simply calls the second version of Init.
  virtual bool SetRawData(const void* src_buffer, uint32 src_buffer_size) = 0;
#if defined(OS_CHROMEOS)
  // Saves the underlying data to the file associated with fd. This function
  // should ONLY be called after the metafile is closed.
  // Returns true if writing succeeded.
  virtual bool SaveToFD(const base::FileDescriptor& fd) const = 0;
#endif  // if defined(OS_CHROMEOS)

#endif
};

}  // namespace printing

#endif  // PRINTING_NATIVE_METAFILE_H_
