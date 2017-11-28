// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DraggedIsolatedFileSystem_h
#define DraggedIsolatedFileSystem_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class DataObject;

class CORE_EXPORT DraggedIsolatedFileSystem {
 public:
  DraggedIsolatedFileSystem() = default;
  virtual ~DraggedIsolatedFileSystem() = default;

  using FileSystemIdPreparationCallback = void (*)(DataObject*);
  static void Init(FileSystemIdPreparationCallback);

  static void PrepareForDataObject(DataObject*);

 private:
  static FileSystemIdPreparationCallback prepare_callback_;

  DISALLOW_COPY_AND_ASSIGN(DraggedIsolatedFileSystem);
};

}  // namespace blink

#endif  // DraggedIsolatedFileSystem_h
