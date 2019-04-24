// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"

#if defined(OS_ANDROID)
#include "third_party/blink/public/platform/modules/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#elif defined(OS_LINUX)
#include "third_party/blink/renderer/platform/fonts/linux/font_unique_name_lookup_linux.h"
#elif defined(OS_WIN)
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"
#endif

namespace blink {

FontUniqueNameLookup::FontUniqueNameLookup() = default;

// static
std::unique_ptr<FontUniqueNameLookup>
FontUniqueNameLookup::GetPlatformUniqueNameLookup() {
#if defined(OS_ANDROID)
  return std::make_unique<FontUniqueNameLookupAndroid>();
#elif defined(OS_LINUX)
  return std::make_unique<FontUniqueNameLookupLinux>();
#elif defined(OS_WIN)
  return std::make_unique<FontUniqueNameLookupWin>();
#else
  NOTREACHED();
  return nullptr;
#endif
}

#if defined(OS_WIN) || defined(OS_ANDROID)
template <class ServicePtrType>
bool FontUniqueNameLookup::EnsureMatchingServiceConnected() {
  if (font_table_matcher_)
    return true;

  ServicePtrType service;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service));

  base::ReadOnlySharedMemoryRegion region_ptr;
  if (!service->GetUniqueNameLookupTable(&region_ptr)) {
    // Tests like StyleEngineTest do not set up a full browser where Blink can
    // connect to a browser side service for font lookups. Placing a DCHECK here
    // is too strict for such a case.
    LOG(ERROR) << "Unable to connect to browser side service for src: local() "
                  "font lookup.";
    return false;
  }

  font_table_matcher_ = std::make_unique<FontTableMatcher>(region_ptr.Map());
  return true;
}
#endif

#if defined(OS_ANDROID)
template bool FontUniqueNameLookup::EnsureMatchingServiceConnected<
    mojom::blink::FontUniqueNameLookupPtr>();
#elif defined(OS_WIN)
template bool FontUniqueNameLookup::EnsureMatchingServiceConnected<
    mojom::blink::DWriteFontProxyPtr>();
#endif

}  // namespace blink
