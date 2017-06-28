/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef FontResource_h
#define FontResource_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class FetchParameters;
class ResourceFetcher;
class FontCustomPlatformData;
class FontResourceClient;

class CORE_EXPORT FontResource final : public Resource {
 public:
  using ClientType = FontResourceClient;

  static FontResource* Fetch(FetchParameters&, ResourceFetcher*);
  ~FontResource() override;

  void DidAddClient(ResourceClient*) override;

  void SetRevalidatingRequest(const ResourceRequest&) override;

  void AllClientsAndObserversRemoved() override;
  void StartLoadLimitTimers();

  String OtsParsingMessage() const { return ots_parsing_message_; }

  PassRefPtr<FontCustomPlatformData> GetCustomFontData();

  // Returns true if the loading priority of the remote font resource can be
  // lowered. The loading priority of the font can be lowered only if the
  // font is not needed for painting the text.
  bool IsLowPriorityLoadingAllowedForRemoteFont() const;

  void WillReloadAfterDiskCacheMiss() override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

 private:
  class FontResourceFactory : public NonTextResourceFactory {
   public:
    FontResourceFactory() : NonTextResourceFactory(Resource::kFont) {}

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options) const override {
      return new FontResource(request, options);
    }
  };
  FontResource(const ResourceRequest&, const ResourceLoaderOptions&);

  void CheckNotify() override;
  void FontLoadShortLimitCallback(TimerBase*);
  void FontLoadLongLimitCallback(TimerBase*);
  void NotifyClientsShortLimitExceeded();
  void NotifyClientsLongLimitExceeded();

  // This is used in UMA histograms, should not change order.
  enum LoadLimitState {
    kLoadNotStarted,
    kUnderLimit,
    kShortLimitExceeded,
    kLongLimitExceeded,
    kLoadLimitStateEnumMax
  };

  RefPtr<FontCustomPlatformData> font_data_;
  String ots_parsing_message_;
  LoadLimitState load_limit_state_;
  bool cors_failed_;
  Timer<FontResource> font_load_short_limit_timer_;
  Timer<FontResource> font_load_long_limit_timer_;

  friend class MemoryCache;
  FRIEND_TEST_ALL_PREFIXES(FontResourceTest, CacheAwareFontLoading);
};

DEFINE_RESOURCE_TYPE_CASTS(Font);

class FontResourceClient : public ResourceClient {
 public:
  ~FontResourceClient() override {}
  static bool IsExpectedType(ResourceClient* client) {
    return client->GetResourceClientType() == kFontType;
  }
  ResourceClientType GetResourceClientType() const final { return kFontType; }

  // If cache-aware loading is activated, both callbacks will be blocked until
  // disk cache miss. Calls to addClient() and removeClient() in both callbacks
  // are prohibited to prevent race issues regarding current loading state.
  virtual void FontLoadShortLimitExceeded(FontResource*) {}
  virtual void FontLoadLongLimitExceeded(FontResource*) {}

  // Returns true if loading priority of remote font resources can be lowered.
  virtual bool IsLowPriorityLoadingAllowedForRemoteFont() const {
    // Only the RemoteFontFaceSources clients can prevent lowering of loading
    // priority of the remote fonts.  Set the default to true to prevent
    // other clients from incorrectly returning false.
    return true;
  }
};

}  // namespace blink

#endif
