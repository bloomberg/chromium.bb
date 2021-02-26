// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

// static
bool Clipboard::IsSupportedClipboardBuffer(ClipboardBuffer buffer) {
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste:
      return true;
    case ClipboardBuffer::kSelection:
#if defined(USE_OZONE) && !defined(OS_CHROMEOS)
      if (features::IsUsingOzonePlatform()) {
        ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
        CHECK(clipboard);
        return clipboard->IsSelectionBufferAvailable();
      }
#endif
#if !defined(OS_WIN) && !defined(OS_APPLE) && !defined(OS_CHROMEOS)
      return true;
#else
      return false;
#endif
    case ClipboardBuffer::kDrag:
      return false;
  }
  NOTREACHED();
}

// static
void Clipboard::SetAllowedThreads(
    const std::vector<base::PlatformThreadId>& allowed_threads) {
  base::AutoLock lock(ClipboardMapLock());

  AllowedThreads().clear();
  std::copy(allowed_threads.begin(), allowed_threads.end(),
            std::back_inserter(AllowedThreads()));
}

// static
void Clipboard::SetClipboardForCurrentThread(
    std::unique_ptr<Clipboard> platform_clipboard) {
  base::AutoLock lock(ClipboardMapLock());
  base::PlatformThreadId id = Clipboard::GetAndValidateThreadID();

  ClipboardMap* clipboard_map = ClipboardMapPtr();
  // This shouldn't happen. The clipboard should not already exist.
  DCHECK(!base::Contains(*clipboard_map, id));
  clipboard_map->insert({id, std::move(platform_clipboard)});
}

// static
Clipboard* Clipboard::GetForCurrentThread() {
  base::AutoLock lock(ClipboardMapLock());
  base::PlatformThreadId id = GetAndValidateThreadID();

  ClipboardMap* clipboard_map = ClipboardMapPtr();
  auto it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    return it->second.get();

  Clipboard* clipboard = Clipboard::Create();
  clipboard_map->insert({id, base::WrapUnique(clipboard)});
  return clipboard;
}

// static
std::unique_ptr<Clipboard> Clipboard::TakeForCurrentThread() {
  base::AutoLock lock(ClipboardMapLock());

  ClipboardMap* clipboard_map = ClipboardMapPtr();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();

  Clipboard* clipboard = nullptr;

  auto it = clipboard_map->find(id);
  if (it != clipboard_map->end()) {
    clipboard = it->second.release();
    clipboard_map->erase(it);
  }

  return base::WrapUnique(clipboard);
}

// static
void Clipboard::OnPreShutdownForCurrentThread() {
  base::AutoLock lock(ClipboardMapLock());
  base::PlatformThreadId id = GetAndValidateThreadID();

  ClipboardMap* clipboard_map = ClipboardMapPtr();
  auto it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    it->second->OnPreShutdown();
}

// static
void Clipboard::DestroyClipboardForCurrentThread() {
  base::AutoLock lock(ClipboardMapLock());

  ClipboardMap* clipboard_map = ClipboardMapPtr();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();
  auto it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    clipboard_map->erase(it);
}

base::Time Clipboard::GetLastModifiedTime() const {
  return base::Time();
}

void Clipboard::ClearLastModifiedTime() {}

Clipboard::Clipboard() = default;
Clipboard::~Clipboard() = default;

void Clipboard::DispatchPortableRepresentation(PortableFormat format,
                                               const ObjectMapParams& params) {
  // Ignore writes with empty parameters.
  for (const auto& param : params) {
    if (param.empty())
      return;
  }

  switch (format) {
    case PortableFormat::kText:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case PortableFormat::kHtml:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), nullptr, 0);
      }
      break;

    case PortableFormat::kSvg:
      WriteSvg(&(params[0].front()), params[0].size());
      break;

    case PortableFormat::kRtf:
      WriteRTF(&(params[0].front()), params[0].size());
      break;

    case PortableFormat::kBookmark:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case PortableFormat::kWebkit:
      WriteWebSmartPaste();
      break;

    case PortableFormat::kBitmap: {
      // Usually, the params are just UTF-8 strings. However, for images,
      // ScopedClipboardWriter actually sizes the buffer to sizeof(SkBitmap*),
      // aliases the contents of the vector to a SkBitmap**, and writes the
      // pointer to the actual SkBitmap in the clipboard object param.
      const char* packed_pointer_buffer = &params[0].front();
      WriteBitmap(**reinterpret_cast<SkBitmap* const*>(packed_pointer_buffer));
      break;
    }

    case PortableFormat::kData:
      WriteData(ClipboardFormatType::Deserialize(
                    std::string(&(params[0].front()), params[0].size())),
                &(params[1].front()), params[1].size());
      break;

    default:
      NOTREACHED();
  }
}

void Clipboard::DispatchPlatformRepresentations(
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  for (const auto& representation : platform_representations) {
    WriteData(ClipboardFormatType::GetType(representation.format),
              reinterpret_cast<const char*>(representation.data.data()),
              representation.data.size());
  }
}

base::PlatformThreadId Clipboard::GetAndValidateThreadID() {
  ClipboardMapLock().AssertAcquired();

  const base::PlatformThreadId id = base::PlatformThread::CurrentId();

  // A Clipboard instance must be allocated for every thread that uses the
  // clipboard. To prevented unbounded memory use, CHECK that the current thread
  // was allowlisted to use the clipboard. This is a CHECK rather than a DCHECK
  // to catch incorrect usage in production (e.g. https://crbug.com/872737).
  CHECK(AllowedThreads().empty() || base::Contains(AllowedThreads(), id));

  return id;
}

// static
std::vector<base::PlatformThreadId>& Clipboard::AllowedThreads() {
  static base::NoDestructor<std::vector<base::PlatformThreadId>>
      allowed_threads;
  return *allowed_threads;
}

// static
Clipboard::ClipboardMap* Clipboard::ClipboardMapPtr() {
  static base::NoDestructor<ClipboardMap> clipboard_map;
  return clipboard_map.get();
}

// static
base::Lock& Clipboard::ClipboardMapLock() {
  static base::NoDestructor<base::Lock> clipboard_map_lock;
  return *clipboard_map_lock;
}

bool Clipboard::IsMarkedByOriginatorAsConfidential() const {
  return false;
}

void Clipboard::MarkAsConfidential() {}

}  // namespace ui
