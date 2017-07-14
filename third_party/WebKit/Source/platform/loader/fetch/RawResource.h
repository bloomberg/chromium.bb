/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RawResource_h
#define RawResource_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/WebDataConsumerHandle.h"

namespace blink {
class FetchParameters;
class RawResourceClient;
class ResourceFetcher;
class SubstituteData;

class PLATFORM_EXPORT RawResource final : public Resource {
 public:
  using ClientType = RawResourceClient;

  static RawResource* FetchSynchronously(FetchParameters&, ResourceFetcher*);
  static RawResource* Fetch(FetchParameters&, ResourceFetcher*);
  static RawResource* FetchMainResource(FetchParameters&,
                                        ResourceFetcher*,
                                        const SubstituteData&);
  static RawResource* FetchImport(FetchParameters&, ResourceFetcher*);
  static RawResource* FetchMedia(FetchParameters&, ResourceFetcher*);
  static RawResource* FetchTextTrack(FetchParameters&, ResourceFetcher*);
  static RawResource* FetchManifest(FetchParameters&, ResourceFetcher*);

  // Exposed for testing
  static RawResource* CreateForTest(ResourceRequest request, Type type) {
    ResourceLoaderOptions options;
    return new RawResource(request, type, options);
  }
  static RawResource* CreateForTest(const KURL& url, Type type) {
    ResourceRequest request(url);
    return CreateForTest(request, type);
  }
  static RawResource* CreateForTest(const char* url, Type type) {
    return CreateForTest(KURL(kParsedURLString, url), type);
  }

  // FIXME: AssociatedURLLoader shouldn't be a DocumentThreadableLoader and
  // therefore shouldn't use RawResource. However, it is, and it needs to be
  // able to defer loading. This can be fixed by splitting CORS preflighting out
  // of DocumentThreadableLoader.
  void SetDefersLoading(bool);

  // Resource implementation
  bool CanReuse(const FetchParameters&) const override;
  bool WillFollowRedirect(const ResourceRequest&,
                          const ResourceResponse&) override;

 private:
  class RawResourceFactory : public NonTextResourceFactory {
   public:
    explicit RawResourceFactory(Resource::Type type)
        : NonTextResourceFactory(type) {}

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options) const override {
      return new RawResource(request, type_, options);
    }
  };

  RawResource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

  // Resource implementation
  void DidAddClient(ResourceClient*) override;
  void AppendData(const char*, size_t) override;
  bool ShouldIgnoreHTTPStatusCodeErrors() const override {
    return !IsLinkPreload();
  }
  void WillNotFollowRedirect() override;
  void ResponseReceived(const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override;
  void SetSerializedCachedMetadata(const char*, size_t) override;
  void DidSendData(unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent) override;
  void DidDownloadData(int) override;
  void ReportResourceTimingToClients(const ResourceTimingInfo&) override;
};

// TODO(yhirano): Recover #if ENABLE_SECURITY_ASSERT when we stop adding
// RawResources to MemoryCache.
inline bool IsRawResource(const Resource& resource) {
  Resource::Type type = resource.GetType();
  return type == Resource::kMainResource || type == Resource::kRaw ||
         type == Resource::kTextTrack || type == Resource::kMedia ||
         type == Resource::kManifest || type == Resource::kImportResource;
}
inline RawResource* ToRawResource(Resource* resource) {
  SECURITY_DCHECK(!resource || IsRawResource(*resource));
  return static_cast<RawResource*>(resource);
}

class PLATFORM_EXPORT RawResourceClient : public ResourceClient {
 public:
  static bool IsExpectedType(ResourceClient* client) {
    return client->GetResourceClientType() == kRawResourceType;
  }
  ResourceClientType GetResourceClientType() const final {
    return kRawResourceType;
  }

  // The order of the callbacks is as follows:
  // [Case 1] A successful load:
  // 0+  redirectReceived() and/or dataSent()
  // 1   responseReceived()
  // 0-1 setSerializedCachedMetadata()
  // 0+  dataReceived() or dataDownloaded(), but never both
  // 1   notifyFinished() with errorOccurred() = false
  // [Case 2] When redirect is blocked:
  // 0+  redirectReceived() and/or dataSent()
  // 1   redirectBlocked()
  // 1   notifyFinished() with errorOccurred() = true
  // [Case 3] Other failures:
  //     notifyFinished() with errorOccurred() = true is called at any time
  //     (unless notifyFinished() is already called).
  // In all cases:
  //     No callbacks are made after notifyFinished() or
  //     removeClient() is called.
  virtual void DataSent(Resource*,
                        unsigned long long /* bytesSent */,
                        unsigned long long /* totalBytesToBeSent */) {}
  virtual void ResponseReceived(Resource*,
                                const ResourceResponse&,
                                std::unique_ptr<WebDataConsumerHandle>) {}
  virtual void SetSerializedCachedMetadata(Resource*, const char*, size_t) {}
  virtual void DataReceived(Resource*,
                            const char* /* data */,
                            size_t /* length */) {}
  virtual bool RedirectReceived(Resource*,
                                const ResourceRequest&,
                                const ResourceResponse&) {
    return true;
  }
  virtual void RedirectBlocked() {}
  virtual void DataDownloaded(Resource*, int) {}
  virtual void DidReceiveResourceTiming(Resource*, const ResourceTimingInfo&) {}
};

// Checks the sequence of callbacks of RawResourceClient. This can be used only
// when a RawResourceClient is added as a client to at most one RawResource.
class PLATFORM_EXPORT RawResourceClientStateChecker final {
 public:
  RawResourceClientStateChecker();
  ~RawResourceClientStateChecker();

  // Call before addClient()/removeClient() is called.
  void WillAddClient();
  void WillRemoveClient();

  // Call RawResourceClientStateChecker::f() at the beginning of
  // RawResourceClient::f().
  void RedirectReceived();
  void RedirectBlocked();
  void DataSent();
  void ResponseReceived();
  void SetSerializedCachedMetadata();
  void DataReceived();
  void DataDownloaded();
  void NotifyFinished(Resource*);

 private:
  enum State {
    kNotAddedAsClient,
    kStarted,
    kRedirectBlocked,
    kResponseReceived,
    kSetSerializedCachedMetadata,
    kDataReceived,
    kDataDownloaded,
    kNotifyFinished
  };
  State state_;
};

}  // namespace blink

#endif  // RawResource_h
