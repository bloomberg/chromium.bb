// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_LINUX_H_
#define PRINTING_NATIVE_METAFILE_LINUX_H_

#include "base/basictypes.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

namespace base {
struct FileDescriptor;
}

class FilePath;

namespace printing {

// This class is used to generate a PDF stream and to  store rendering results
// in a string buffer.
class NativeMetafile {
 public:
  virtual ~NativeMetafile() {}

  // Initializes to a fresh new metafile. Returns true on success.
  // Note: It should only be called from the renderer process to allocate
  // rendering resources.
  virtual bool Init() = 0;

  // Initializes a copy of metafile from PDF stream data.
  // Returns true on success.
  // |src_buffer| should point to the shared memory which stores PDF
  // contents generated in the renderer.
  // Note: It should only be called from the browser process to initialize
  // |data_|.
  virtual bool Init(const void* src_buffer, uint32 src_buffer_size) = 0;

  // Sets raw PDF data for the document. This is used when a cairo drawing
  // surface has already been created. This method will cause all subsequent
  // drawing on the surface to be discarded (in Close()). If Init() has not yet
  // been called this method simply calls the second version of Init.
  virtual bool SetRawData(const void* src_buffer, uint32 src_buffer_size) = 0;

  // Prepares a new cairo surface/context for rendering a new page.
  // The unit is in point (=1/72 in).
  // Returns NULL when failed.
  virtual cairo_t* StartPage(double width_in_points,
                             double height_in_points,
                             double margin_top_in_points,
                             double margin_right_in_points,
                             double margin_bottom_in_points,
                             double margin_left_in_points) = 0;

  // Destroys the surface and the context used in rendering current page.
  // The results of current page will be appended into buffer |data_|.
  // Returns true on success.
  virtual bool FinishPage() = 0;

  // Closes resulting PDF file. No further rendering is allowed.
  virtual void Close() = 0;

  // Returns size of PDF contents stored in buffer |data_|.
  // This function should ONLY be called after PDF file is closed.
  virtual uint32 GetDataSize() const = 0;

  // Copies PDF contents stored in buffer |data_| into |dst_buffer|.
  // This function should ONLY be called after PDF file is closed.
  // Returns true only when success.
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const = 0;

  // Saves PDF contents stored in buffer |data_| into the file
  // associated with |fd|.
  // This function should ONLY be called after PDF file is closed.
  virtual bool SaveTo(const base::FileDescriptor& fd) const = 0;
};

}  // namespace printing

#endif  // PRINTING_NATIVE_METAFILE_LINUX_H_
