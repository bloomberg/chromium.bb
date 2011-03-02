// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_WIN_H_
#define PRINTING_NATIVE_METAFILE_WIN_H_

#include <windows.h>
#include <vector>

#include "base/basictypes.h"

class FilePath;

namespace gfx {
class Rect;
}

namespace printing {

// Simple wrapper class that manages a data stream and its virtual HDC.
class NativeMetafile {
 public:
  virtual ~NativeMetafile() {}

  // Initializes the data stream with the data in |src_buffer|. Returns
  // true on success.
  virtual bool Init(const void* src_buffer, uint32 src_buffer_size) = 0;

  // Generates a virtual HDC that will record every GDI commands and compile it
  // in a EMF data stream.
  // hdc is used to setup the default DPI and color settings. hdc is optional.
  // rect specifies the dimensions (in .01-millimeter units) of the EMF. rect is
  // optional.
  virtual bool CreateDc(HDC sibling, const RECT* rect) = 0;

  // Similar to the above method but the metafile is backed by a file.
  virtual bool CreateFileBackedDc(HDC sibling,
                                  const RECT* rect,
                                  const FilePath& path) = 0;

  // Load a data stream from file.
  virtual bool CreateFromFile(const FilePath& metafile_path) = 0;

  // TODO(maruel): CreateFromFile(). If ever used. Maybe users would like to
  // have the ability to save web pages to an EMF file? Afterward, it is easy to
  // convert to PDF.

  // Closes the HDC created by CreateDc() and generates the compiled EMF
  // data.
  virtual bool CloseDc() = 0;

  // Closes the EMF data handle when it is not needed anymore.
  virtual void CloseEmf() = 0;

  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. |rect| is in
  // "logical units" and is optional. If |rect| is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  virtual bool Playback(HDC hdc, const RECT* rect) const = 0;

  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  virtual bool SafePlayback(HDC hdc) const = 0;

  // Retrieves the bounds of the painted area by this EMF buffer. This value
  // should be passed to Playback to keep the exact same size.
  virtual gfx::Rect GetBounds() const = 0;

  // Retrieves the data stream size.
  virtual uint32 GetDataSize() const = 0;

  // Retrieves the data stream.
  virtual bool GetData(void* buffer, uint32 size) const = 0;

  // Retrieves the data stream. It is an helper function.
  virtual bool GetData(std::vector<uint8>* buffer) const = 0;

  virtual HENHMETAFILE emf() const = 0;

  virtual HDC hdc() const = 0;

  // Inserts a custom GDICOMMENT records indicating StartPage/EndPage calls
  // (since StartPage and EndPage do not work in a metafile DC). Only valid
  // when hdc_ is non-NULL.
  virtual bool StartPage() = 0;
  virtual bool EndPage() = 0;

  // Saves the EMF data to a file as-is. It is recommended to use the .emf file
  // extension but it is not enforced. This function synchronously writes to the
  // file. For testing only.
  virtual bool SaveTo(const std::wstring& filename) const = 0;
};

}  // namespace printing

#endif  // PRINTING_NATIVE_METAFILE_WIN_H_
