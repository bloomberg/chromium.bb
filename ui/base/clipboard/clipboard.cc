// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// Extracts the bitmap size from the passed in params. Since the renderer could
// send us bad data, explicitly copy the parameters to make sure we go through
// gfx::Size's sanity checks.
bool GetBitmapSizeFromParams(const Clipboard::ObjectMapParams& params,
                             gfx::Size* size) {
  DCHECK(params.size() == 2);
  if (params[1].size() != sizeof(gfx::Size))
    return false;
  const gfx::Size* size_from_renderer =
      reinterpret_cast<const gfx::Size*>(&(params[1].front()));
  size->set_width(size_from_renderer->width());
  size->set_height(size_from_renderer->height());
  return true;
}

// A compromised renderer could send us bad size data, so validate it to verify
// that calculating the number of bytes in the bitmap won't overflow a uint32.
//
// |size| - Clipboard bitmap size to validate.
// |bitmap_bytes| - On return contains the number of bytes needed to store
// the bitmap data or -1 if the data is invalid.
// returns: true if the bitmap size is valid, false otherwise.
bool IsBitmapSizeSane(const gfx::Size& size, uint32* bitmap_bytes) {
  DCHECK_GE(size.width(), 0);
  DCHECK_GE(size.height(), 0);

  *bitmap_bytes = -1;
  uint32 total_size = size.width();
  // Use max uint32 instead of max size_t to put a reasonable bound on things.
  if (std::numeric_limits<uint32>::max() / size.width() <=
      static_cast<uint32>(size.height()))
    return false;
  total_size *= size.height();
  if (std::numeric_limits<uint32>::max() / total_size <= 4)
    return false;
  total_size *= 4;
  *bitmap_bytes = total_size;
  return true;
}

// Validates a plain bitmap on the clipboard.
// Returns true if the clipboard data makes sense and it's safe to access the
// bitmap.
bool ValidatePlainBitmap(const gfx::Size& size,
                         const Clipboard::ObjectMapParams& params) {
  uint32 bitmap_bytes = -1;
  if (!IsBitmapSizeSane(size, &bitmap_bytes))
    return false;
  if (bitmap_bytes != params[0].size())
    return false;
  return true;
}

// Valides a shared bitmap on the clipboard.
// Returns true if the clipboard data makes sense and it's safe to access the
// bitmap.
bool ValidateAndMapSharedBitmap(const gfx::Size& size,
                                base::SharedMemory* bitmap_data) {
  using base::SharedMemory;
  uint32 bitmap_bytes = -1;
  if (!IsBitmapSizeSane(size, &bitmap_bytes))
    return false;

  if (!bitmap_data || !SharedMemory::IsHandleValid(bitmap_data->handle()))
    return false;

  if (!bitmap_data->Map(bitmap_bytes)) {
    PLOG(ERROR) << "Failed to map bitmap memory";
    return false;
  }
  return true;
}

// Adopts a blob of bytes (assumed to be ARGB pixels) into a SkBitmap. Since the
// pixel data is not copied, the caller must ensure that |data| is valid as long
// as the SkBitmap is in use. Note that on little endian machines, each pixel is
// actually BGRA in memory.
SkBitmap AdoptBytesIntoSkBitmap(const void* data, const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  // Guaranteed not to overflow since it's been validated by IsBitmapSizeSane().
  DCHECK_EQ(4U * size.width(), bitmap.rowBytes());
  bitmap.setPixels(const_cast<void*>(data));
  return bitmap;
}

// A list of allowed threads. By default, this is empty and no thread checking
// is done (in the unit test case), but a user (like content) can set which
// threads are allowed to call this method.
typedef std::vector<base::PlatformThreadId> AllowedThreadsVector;
static base::LazyInstance<AllowedThreadsVector> g_allowed_threads =
    LAZY_INSTANCE_INITIALIZER;

// Mapping from threads to clipboard objects.
typedef std::map<base::PlatformThreadId, Clipboard*> ClipboardMap;
static base::LazyInstance<ClipboardMap> g_clipboard_map =
    LAZY_INSTANCE_INITIALIZER;

// Mutex that controls access to |g_clipboard_map|.
static base::LazyInstance<base::Lock>::Leaky
    g_clipboard_map_lock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void Clipboard::SetAllowedThreads(
    const std::vector<base::PlatformThreadId>& allowed_threads) {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  g_allowed_threads.Get().clear();
  std::copy(allowed_threads.begin(), allowed_threads.end(),
            std::back_inserter(g_allowed_threads.Get()));
}

// static
Clipboard* Clipboard::GetForCurrentThread() {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  base::PlatformThreadId id = base::PlatformThread::CurrentId();

  AllowedThreadsVector* allowed_threads = g_allowed_threads.Pointer();
  if (!allowed_threads->empty()) {
    bool found = false;
    for (AllowedThreadsVector::const_iterator it = allowed_threads->begin();
         it != allowed_threads->end(); ++it) {
      if (*it == id) {
        found = true;
        break;
      }
    }

    DCHECK(found);
  }

  ClipboardMap* clipboard_map = g_clipboard_map.Pointer();
  ClipboardMap::iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    return it->second;

  Clipboard* clipboard = new ui::Clipboard;
  clipboard_map->insert(std::make_pair(id, clipboard));
  return clipboard;
}

void Clipboard::DestroyClipboardForCurrentThread() {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  ClipboardMap* clipboard_map = g_clipboard_map.Pointer();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();
  ClipboardMap::iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end()) {
    delete it->second;
    clipboard_map->erase(it);
  }
}

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

    case CBF_RTF:
      WriteRTF(&(params[0].front()), params[0].size());
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_BITMAP: {
      gfx::Size size;
      if (!GetBitmapSizeFromParams(params, &size))
        return;

      if (!ValidatePlainBitmap(size, params))
        return;

      const SkBitmap& bitmap = AdoptBytesIntoSkBitmap(
          static_cast<const void*>(&params[0].front()), size);
      WriteBitmap(bitmap);
      break;
    }

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

      gfx::Size size;
      if (!GetBitmapSizeFromParams(params, &size))
        return;

      if (!ValidateAndMapSharedBitmap(size, bitmap_data.get()))
        return;

      const SkBitmap& bitmap = AdoptBytesIntoSkBitmap(
          bitmap_data->memory(), size);
      WriteBitmap(bitmap);
      break;
    }

    case CBF_DATA:
      WriteData(
          FormatType::Deserialize(
              std::string(&(params[0].front()), params[0].size())),
          &(params[1].front()),
          params[1].size());
      break;

    default:
      NOTREACHED();
  }
}

// static
bool Clipboard::ReplaceSharedMemHandle(ObjectMap* objects,
                                       base::SharedMemoryHandle bitmap_handle,
                                       base::ProcessHandle process) {
  using base::SharedMemory;
  bool has_shared_bitmap = false;

  for (ObjectMap::iterator iter = objects->begin(); iter != objects->end();
       ++iter) {
    if (iter->first == CBF_SMBITMAP) {
      // The code currently only accepts sending a single bitmap over this way.
      // Fail if we ever encounter more than one shmem bitmap structure to fill.
      if (has_shared_bitmap)
        return false;

#if defined(OS_WIN)
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true, process);
#else
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true);
#endif

      // There must always be two parameters associated with each shmem bitmap.
      if (iter->second.size() != 2)
        return false;

      // We store the shared memory object pointer so it can be retrieved by the
      // UI thread (see DispatchObject()).
      iter->second[0].clear();
      for (size_t i = 0; i < sizeof(SharedMemory*); ++i)
        iter->second[0].push_back(reinterpret_cast<char*>(&bitmap)[i]);
      has_shared_bitmap = true;
    }
  }
  return true;
}

}  // namespace ui
