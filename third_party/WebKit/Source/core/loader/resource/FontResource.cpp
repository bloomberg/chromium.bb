/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/resource/FontResource.h"

#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

// Durations of font-display periods.
// https://tabatkins.github.io/specs/css-font-display/#font-display-desc
// TODO(toyoshim): Revisit short limit value once cache-aware font display is
// launched. crbug.com/570205
static const double kFontLoadWaitShortLimitSec = 0.1;
static const double kFontLoadWaitLongLimitSec = 3.0;

enum FontPackageFormat {
  kPackageFormatUnknown,
  kPackageFormatSFNT,
  kPackageFormatWOFF,
  kPackageFormatWOFF2,
  kPackageFormatSVG,
  kPackageFormatEnumMax
};

static FontPackageFormat PackageFormatOf(SharedBuffer* buffer) {
  static constexpr size_t kMaxHeaderSize = 4;
  char data[kMaxHeaderSize];
  if (!buffer->GetBytes(data, kMaxHeaderSize))
    return kPackageFormatUnknown;

  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F')
    return kPackageFormatWOFF;
  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2')
    return kPackageFormatWOFF2;
  return kPackageFormatSFNT;
}

static void RecordPackageFormatHistogram(FontPackageFormat format) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, package_format_histogram,
      ("WebFont.PackageFormat", kPackageFormatEnumMax));
  package_format_histogram.Count(format);
}

FontResource* FontResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextFont);
  return ToFontResource(
      fetcher->RequestResource(params, FontResourceFactory()));
}

FontResource::FontResource(const ResourceRequest& resource_request,
                           const ResourceLoaderOptions& options)
    : Resource(resource_request, kFont, options),
      load_limit_state_(kLoadNotStarted),
      cors_failed_(false),
      font_load_short_limit_timer_(this,
                                   &FontResource::FontLoadShortLimitCallback),
      font_load_long_limit_timer_(this,
                                  &FontResource::FontLoadLongLimitCallback) {}

FontResource::~FontResource() {}

void FontResource::DidAddClient(ResourceClient* c) {
  DCHECK(FontResourceClient::IsExpectedType(c));
  Resource::DidAddClient(c);

  // Block client callbacks if currently loading from cache.
  if (IsLoading() && Loader()->IsCacheAwareLoadingActivated())
    return;

  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  if (load_limit_state_ == kShortLimitExceeded ||
      load_limit_state_ == kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadShortLimitExceeded(this);
  if (load_limit_state_ == kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadLongLimitExceeded(this);
}

void FontResource::SetRevalidatingRequest(const ResourceRequest& request) {
  // Reload will use the same object, and needs to reset |m_loadLimitState|
  // before any didAddClient() is called again.
  DCHECK(IsLoaded());
  DCHECK(!font_load_short_limit_timer_.IsActive());
  DCHECK(!font_load_long_limit_timer_.IsActive());
  load_limit_state_ = kLoadNotStarted;
  Resource::SetRevalidatingRequest(request);
}

void FontResource::StartLoadLimitTimers() {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, kLoadNotStarted);
  load_limit_state_ = kUnderLimit;
  font_load_short_limit_timer_.StartOneShot(kFontLoadWaitShortLimitSec,
                                            BLINK_FROM_HERE);
  font_load_long_limit_timer_.StartOneShot(kFontLoadWaitLongLimitSec,
                                           BLINK_FROM_HERE);
}

RefPtr<FontCustomPlatformData> FontResource::GetCustomFontData() {
  if (!font_data_ && !ErrorOccurred() && !IsLoading()) {
    if (Data())
      font_data_ = FontCustomPlatformData::Create(Data(), ots_parsing_message_);

    if (font_data_) {
      RecordPackageFormatHistogram(PackageFormatOf(Data()));
    } else {
      SetStatus(ResourceStatus::kDecodeError);
      RecordPackageFormatHistogram(kPackageFormatUnknown);
    }
  }
  return font_data_;
}

void FontResource::WillReloadAfterDiskCacheMiss() {
  DCHECK(IsLoading());
  DCHECK(Loader()->IsCacheAwareLoadingActivated());
  if (load_limit_state_ == kShortLimitExceeded ||
      load_limit_state_ == kLongLimitExceeded) {
    NotifyClientsShortLimitExceeded();
  }
  if (load_limit_state_ == kLongLimitExceeded)
    NotifyClientsLongLimitExceeded();

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, load_limit_histogram,
      ("WebFont.LoadLimitOnDiskCacheMiss", kLoadLimitStateEnumMax));
  load_limit_histogram.Count(load_limit_state_);
}

void FontResource::FontLoadShortLimitCallback(TimerBase*) {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, kUnderLimit);
  load_limit_state_ = kShortLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (Loader()->IsCacheAwareLoadingActivated())
    return;
  NotifyClientsShortLimitExceeded();
}

void FontResource::FontLoadLongLimitCallback(TimerBase*) {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, kShortLimitExceeded);
  load_limit_state_ = kLongLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (Loader()->IsCacheAwareLoadingActivated())
    return;
  NotifyClientsLongLimitExceeded();
}

void FontResource::NotifyClientsShortLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next())
    client->FontLoadShortLimitExceeded(this);
}

void FontResource::NotifyClientsLongLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next())
    client->FontLoadLongLimitExceeded(this);
}

void FontResource::AllClientsAndObserversRemoved() {
  font_data_.Clear();
  Resource::AllClientsAndObserversRemoved();
}

void FontResource::NotifyFinished() {
  font_load_short_limit_timer_.Stop();
  font_load_long_limit_timer_.Stop();

  Resource::NotifyFinished();
}

bool FontResource::IsLowPriorityLoadingAllowedForRemoteFont() const {
  DCHECK(!Url().ProtocolIsData());
  DCHECK(!IsLoaded());
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next()) {
    if (!client->IsLowPriorityLoadingAllowedForRemoteFont()) {
      return false;
    }
  }
  return true;
}

void FontResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level,
                                WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level, memory_dump);
  if (!font_data_)
    return;
  const String name = GetMemoryDumpName() + "/decoded_webfont";
  WebMemoryAllocatorDump* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", font_data_->DataSize());
  memory_dump->AddSuballocation(dump->Guid(), "malloc");
}

}  // namespace blink
