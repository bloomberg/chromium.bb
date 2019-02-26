// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif defined(OS_LINUX)
#include "base/synchronization/lock.h"
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "content/child/child_thread_impl.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/linux/out_of_process_font.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#endif

namespace blink {
class WebSandboxSupport;
struct OutOfProcessFont;
struct WebFontRenderStyle;
}  // namespace blink

namespace content {

#if defined(OS_LINUX)

class UtilityBlinkPlatformWithSandboxSupportImpl::SandboxSupport
    : public blink::WebSandboxSupport {
 public:
  explicit SandboxSupport(sk_sp<font_service::FontLoader> font_loader)
      : font_loader_(std::move(font_loader)) {}
  ~SandboxSupport() override {}

  void GetFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      blink::OutOfProcessFont* fallbackFont) override;
  void MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      blink::OutOfProcessFont* uniquely_matched_font) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int size,
                                      bool is_bold,
                                      bool is_italic,
                                      float device_scale_factor,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  base::Lock unicode_font_families_mutex_;
  // Maps unicode chars to their fallback fonts.
  std::map<int32_t, blink::OutOfProcessFont> unicode_font_families_;
  sk_sp<font_service::FontLoader> font_loader_;
};

#endif  // defined(OS_LINUX)

UtilityBlinkPlatformWithSandboxSupportImpl::
    UtilityBlinkPlatformWithSandboxSupportImpl(
        service_manager::Connector* connector) {
#if defined(OS_LINUX)
  font_loader_ = sk_make_sp<font_service::FontLoader>(connector);
  SkFontConfigInterface::SetGlobal(font_loader_);
  sandbox_support_ = std::make_unique<SandboxSupport>(font_loader_);
#elif defined(OS_MACOSX)
  sandbox_support_ = std::make_unique<WebSandboxSupportMac>(connector);
#endif
}

UtilityBlinkPlatformWithSandboxSupportImpl::
    ~UtilityBlinkPlatformWithSandboxSupportImpl() {}

blink::WebSandboxSupport*
UtilityBlinkPlatformWithSandboxSupportImpl::GetSandboxSupport() {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

#if defined(OS_LINUX)

void UtilityBlinkPlatformWithSandboxSupportImpl::SandboxSupport::
    GetFallbackFontForCharacter(blink::WebUChar32 character,
                                const char* preferred_locale,
                                blink::OutOfProcessFont* fallback_font) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const std::map<int32_t, blink::OutOfProcessFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallback_font->name = iter->second.name;
    fallback_font->filename = iter->second.filename;
    fallback_font->fontconfig_interface_id =
        iter->second.fontconfig_interface_id;
    fallback_font->ttc_index = iter->second.ttc_index;
    fallback_font->is_bold = iter->second.is_bold;
    fallback_font->is_italic = iter->second.is_italic;
    return;
  }
  content::GetFallbackFontForCharacter(font_loader_, character,
                                       preferred_locale, fallback_font);
  unicode_font_families_.emplace(character, *fallback_font);
}

void UtilityBlinkPlatformWithSandboxSupportImpl::SandboxSupport::
    GetWebFontRenderStyleForStrike(const char* family,
                                   int size,
                                   bool is_bold,
                                   bool is_italic,
                                   float device_scale_factor,
                                   blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(font_loader_, family, size, is_bold, is_italic,
                          device_scale_factor, out);
}

void UtilityBlinkPlatformWithSandboxSupportImpl::SandboxSupport::
    MatchFontByPostscriptNameOrFullFontName(
        const char* font_unique_name,
        blink::OutOfProcessFont* uniquely_matched_font) {
  content::MatchFontByPostscriptNameOrFullFontName(
      font_loader_, font_unique_name, uniquely_matched_font);
}

#endif

}  // namespace content
