// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_FACTORY_H_
#define PRINTING_NATIVE_METAFILE_FACTORY_H_

#include "base/basictypes.h"
#include "printing/native_metafile.h"

namespace printing {

// Various printing contexts will be supported in the future (cairo, skia, emf).
// So this class returns the appropriate context depending on the platform and
// user preferences.
// (Note: For the moment there is only one option per platform.)
class NativeMetafileFactory {
 public:
  // This method returns a pointer to the appropriate NativeMetafile object
  // according to the platform. The metafile is already initialized by invoking
  // Init() on it. NULL is returned if Init() fails.
  static printing::NativeMetafile* Create();

  // This method returns a pointer to the appropriate NativeMetafile object
  // according to the platform. The metafile is already initialized by invoking
  // InitFromData(src_buffer,_buffer_size) on it. NULL is returned if
  // InitiFromData() fails.
  static printing::NativeMetafile* CreateFromData(const void* src_buffer,
                                                  uint32 src_buffer_size);

 private:
  NativeMetafileFactory();

  // Retrieves a new uninitialized metafile.
  static NativeMetafile* CreateNewMetafile();

  DISALLOW_COPY_AND_ASSIGN(NativeMetafileFactory);
};

}  // namespace printing

#endif  // PRINTING_NATIVE_METAFILE_FACTORY_H_
