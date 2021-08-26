// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

// static
bool Clipboard::IsSupportedClipboardBuffer(ClipboardBuffer buffer) {
  // Use lambda instead of local helper function in order to access private
  // member IsSelectionBufferAvailable().
  static auto IsSupportedSelectionClipboard = []() -> bool {
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
  };

  switch (buffer) {
    case ClipboardBuffer::kCopyPaste:
      return true;
    case ClipboardBuffer::kSelection:
      // Cache the result to make this function cheap.
      static bool selection_result = IsSupportedSelectionClipboard();
      return selection_result;
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

std::map<std::string, std::string> Clipboard::ExtractCustomPlatformNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  // Read the JSON metadata payload.
  std::map<std::string, std::string> custom_format_names;
  if (IsFormatAvailable(ui::ClipboardFormatType::WebCustomFormatMap(), buffer,
                        data_dst)) {
    std::string custom_format_json;
    // Read the custom format map.
    ReadData(ui::ClipboardFormatType::WebCustomFormatMap(), data_dst,
             &custom_format_json);
    if (!custom_format_json.empty()) {
      absl::optional<base::Value> json_val =
          base::JSONReader::Read(custom_format_json);
      if (json_val.has_value()) {
        for (const auto it : json_val->DictItems()) {
          std::string custom_format_name;
          if (it.second.GetAsString(&custom_format_name)) {
            custom_format_names.emplace(it.first, custom_format_name);
          }
        }
      }
    }
  }
  return custom_format_names;
}

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
        WriteHTML(&(params[0].front()), params[0].size(), &(params[1].front()),
                  params[1].size());
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

    case PortableFormat::kFilenames: {
      std::string uri_list(&(params[0].front()), params[0].size());
      WriteFilenames(ui::URIListToFileInfos(uri_list));
      break;
    }

    case PortableFormat::kData:
      WriteData(ClipboardFormatType::Deserialize(
                    std::string(&(params[0].front()), params[0].size())),
                &(params[1].front()), params[1].size());
      break;

    case PortableFormat::kWebCustomFormatMap:
      WriteData(ClipboardFormatType::WebCustomFormatMap(),
                &(params[0].front()), params[0].size());
      break;

    default:
      NOTREACHED();
  }
}

void Clipboard::DispatchPlatformRepresentations(
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  for (const auto& representation : platform_representations) {
    WriteData(ClipboardFormatType::CustomPlatformType(representation.format),
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

void Clipboard::ReadAvailableTypes(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   ReadAvailableTypesCallback callback) const {
  std::vector<std::u16string> types;
  ReadAvailableTypes(buffer, data_dst, &types);
  std::move(callback).Run(std::move(types));
}

void Clipboard::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    ReadAvailablePlatformSpecificFormatNamesCallback callback) const {
  std::move(callback).Run(
      ReadAvailablePlatformSpecificFormatNames(buffer, data_dst));
}

void Clipboard::ReadText(ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst,
                         ReadTextCallback callback) const {
  std::u16string result;
  ReadText(buffer, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadAsciiText(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              ReadAsciiTextCallback callback) const {
  std::string result;
  ReadAsciiText(buffer, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadHTML(ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst,
                         ReadHtmlCallback callback) const {
  std::u16string markup;
  std::string src_url;
  uint32_t fragment_start;
  uint32_t fragment_end;
  ReadHTML(buffer, data_dst, &markup, &src_url, &fragment_start, &fragment_end);
  std::move(callback).Run(std::move(markup), GURL(src_url), fragment_start,
                          fragment_end);
}

void Clipboard::ReadSvg(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        ReadSvgCallback callback) const {
  std::u16string result;
  ReadSvg(buffer, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadRTF(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        ReadRTFCallback callback) const {
  std::string result;
  ReadRTF(buffer, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadCustomData(ClipboardBuffer buffer,
                               const std::u16string& type,
                               const DataTransferEndpoint* data_dst,
                               ReadCustomDataCallback callback) const {
  std::u16string result;
  ReadCustomData(buffer, type, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadFilenames(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              ReadFilenamesCallback callback) const {
  std::vector<ui::FileInfo> result;
  ReadFilenames(buffer, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

void Clipboard::ReadBookmark(const DataTransferEndpoint* data_dst,
                             ReadBookmarkCallback callback) const {
  std::u16string title;
  std::string url;
  ReadBookmark(data_dst, &title, &url);
  std::move(callback).Run(std::move(title), GURL(url));
}

void Clipboard::ReadData(const ClipboardFormatType& format,
                         const DataTransferEndpoint* data_dst,
                         ReadDataCallback callback) const {
  std::string result;
  ReadData(format, data_dst, &result);
  std::move(callback).Run(std::move(result));
}

}  // namespace ui
