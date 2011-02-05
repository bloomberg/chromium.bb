// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// A compromised renderer could send us bad data, so validate it.
// This function only checks that the size parameter makes sense, the caller
// is responsible for further validating the bitmap buffer against
// |bitmap_bytes|.
//
// |params| - Clipboard bitmap contents to validate.
// |bitmap_bytes| - On return contains the number of bytes needed to store
// the bitmap data or -1 if the data is invalid.
// returns: true if the bitmap size is valid, false otherwise.
bool IsBitmapSafe(const Clipboard::ObjectMapParams& params,
                  uint32* bitmap_bytes) {
  *bitmap_bytes = -1;
  if (params[1].size() != sizeof(gfx::Size))
    return false;
  const gfx::Size* size =
      reinterpret_cast<const gfx::Size*>(&(params[1].front()));
  uint32 total_size = size->width();
  // Using INT_MAX not SIZE_T_MAX to put a reasonable bound on things.
  if (INT_MAX / size->width() <= size->height())
    return false;
  total_size *= size->height();
  if (INT_MAX / total_size <= 4)
    return false;
  total_size *= 4;
  *bitmap_bytes = total_size;
  return true;
}

// Validates a plain bitmap on the clipboard.
// Returns true if the clipboard data makes sense and it's safe to access the
// bitmap.
bool ValidatePlainBitmap(const Clipboard::ObjectMapParams& params) {
  uint32 bitmap_bytes = -1;
  if (!IsBitmapSafe(params, &bitmap_bytes))
    return false;
  if (bitmap_bytes != params[0].size())
    return false;
  return true;
}

// Valides a shared bitmap on the clipboard.
// Returns true if the clipboard data makes sense and it's safe to access the
// bitmap.
bool ValidateAndMapSharedBitmap(const Clipboard::ObjectMapParams& params,
                                base::SharedMemory* bitmap_data) {
  using base::SharedMemory;
  uint32 bitmap_bytes = -1;
  if (!IsBitmapSafe(params, &bitmap_bytes))
    return false;

  if (!bitmap_data || !SharedMemory::IsHandleValid(bitmap_data->handle()))
    return false;

  if (!bitmap_data->Map(bitmap_bytes)) {
    PLOG(ERROR) << "Failed to map bitmap memory";
    return false;
  }
  return true;
}

}  // namespace

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  // All types apart from CBF_WEBKIT need at least 1 non-empty param.
  if (type != CBF_WEBKIT && (params.empty() || params[0].empty()))
    return;
  // Some other types need a non-empty 2nd param.
  if ((type == CBF_BOOKMARK || type == CBF_BITMAP ||
       type == CBF_SMBITMAP || type == CBF_DATA) &&
      (params.size() != 2 || params[1].empty()))
    return;
  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      }
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_BITMAP:
      if (!ValidatePlainBitmap(params))
        return;

      WriteBitmap(&(params[0].front()), &(params[1].front()));
      break;

    case CBF_SMBITMAP: {
      using base::SharedMemory;
      using base::SharedMemoryHandle;

      if (params[0].size() != sizeof(SharedMemory*))
        return;

      // It's OK to cast away constness here since we map the handle as
      // read-only.
      const char* raw_bitmap_data_const =
          reinterpret_cast<const char*>(&(params[0].front()));
      char* raw_bitmap_data = const_cast<char*>(raw_bitmap_data_const);
      scoped_ptr<SharedMemory> bitmap_data(
          *reinterpret_cast<SharedMemory**>(raw_bitmap_data));

      if (!ValidateAndMapSharedBitmap(params, bitmap_data.get()))
        return;
      WriteBitmap(static_cast<const char*>(bitmap_data->memory()),
                  &(params[1].front()));
      break;
    }

#if !defined(OS_MACOSX)
    case CBF_DATA:
      WriteData(&(params[0].front()), params[0].size(),
                &(params[1].front()), params[1].size());
      break;
#endif  // !defined(OS_MACOSX)

    default:
      NOTREACHED();
  }
}

// static
void Clipboard::ReplaceSharedMemHandle(ObjectMap* objects,
                                       base::SharedMemoryHandle bitmap_handle,
                                       base::ProcessHandle process) {
  using base::SharedMemory;
  bool has_shared_bitmap = false;

  for (ObjectMap::iterator iter = objects->begin(); iter != objects->end();
       ++iter) {
    if (iter->first == CBF_SMBITMAP) {
      // The code currently only accepts sending a single bitmap over this way.
      // Fail hard if we ever encounter more than one shared bitmap structure to
      // fill.
      CHECK(!has_shared_bitmap);

#if defined(OS_WIN)
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true, process);
#else
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true);
#endif

      // We store the shared memory object pointer so it can be retrieved by the
      // UI thread (see DispatchObject()).
      iter->second[0].clear();
      for (size_t i = 0; i < sizeof(SharedMemory*); ++i)
        iter->second[0].push_back(reinterpret_cast<char*>(&bitmap)[i]);
      has_shared_bitmap = true;
    }
  }
}

}  // namespace ui
