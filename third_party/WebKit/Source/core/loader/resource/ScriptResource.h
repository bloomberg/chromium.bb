/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#ifndef ScriptResource_h
#define ScriptResource_h

#include "core/CoreExport.h"
#include "core/loader/resource/TextResource.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"

namespace blink {

class Document;
class FetchParameters;
class KURL;
class ResourceFetcher;
class ScriptResource;

class CORE_EXPORT ScriptResourceClient : public ResourceClient {
 public:
  ~ScriptResourceClient() override {}
  static bool IsExpectedType(ResourceClient* client) {
    return client->GetResourceClientType() == kScriptType;
  }
  ResourceClientType GetResourceClientType() const final { return kScriptType; }

  virtual void NotifyAppendData(ScriptResource* resource) {}
};

class CORE_EXPORT ScriptResource final : public TextResource {
 public:
  using ClientType = ScriptResourceClient;
  static ScriptResource* Fetch(FetchParameters&, ResourceFetcher*);

  // Public for testing
  static ScriptResource* CreateForTest(const KURL& url,
                                       const WTF::TextEncoding& encoding) {
    ResourceRequest request(url);
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
    ResourceLoaderOptions options;
    TextResourceDecoderOptions decoder_options(
        TextResourceDecoderOptions::kPlainTextContent, encoding);
    return new ScriptResource(request, options, decoder_options);
  }

  ~ScriptResource() override;

  void DidAddClient(ResourceClient*) override;
  void AppendData(const char*, size_t) override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

  void DestroyDecodedDataForFailedRevalidation() override;

  const String& SourceText();

  static bool MimeTypeAllowedByNosniff(const ResourceResponse&);

  AccessControlStatus CalculateAccessControlStatus() const;

  void CheckResourceIntegrity(Document&);

 private:
  class ScriptResourceFactory : public ResourceFactory {
   public:
    ScriptResourceFactory()
        : ResourceFactory(Resource::kScript,
                          TextResourceDecoderOptions::kPlainTextContent) {}

    Resource* Create(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        const TextResourceDecoderOptions& decoder_options) const override {
      return new ScriptResource(request, options, decoder_options);
    }
  };

  ScriptResource(const ResourceRequest&,
                 const ResourceLoaderOptions&,
                 const TextResourceDecoderOptions&);

  AtomicString source_text_;
};

DEFINE_RESOURCE_TYPE_CASTS(Script);

}  // namespace blink

#endif
