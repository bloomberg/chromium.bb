/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

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

#ifndef DocumentResource_h
#define DocumentResource_h

#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/resource/TextResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceClient.h"
#include <memory>

namespace blink {

class Document;
class FetchParameters;
class ResourceFetcher;

class CORE_EXPORT DocumentResource final : public TextResource {
 public:
  using ClientType = ResourceClient;

  static DocumentResource* FetchSVGDocument(FetchParameters&, ResourceFetcher*);
  ~DocumentResource() override;
  DECLARE_VIRTUAL_TRACE();

  Document* GetDocument() const { return document_.Get(); }

  void CheckNotify() override;

 private:
  class SVGDocumentResourceFactory : public ResourceFactory {
   public:
    SVGDocumentResourceFactory() : ResourceFactory(Resource::kSVGDocument) {}

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options,
                     const String& charset) const override {
      return new DocumentResource(request, Resource::kSVGDocument, options);
    }
  };
  DocumentResource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

  bool MimeTypeAllowed() const;
  Document* CreateDocument(const KURL&);

  Member<Document> document_;
};

DEFINE_TYPE_CASTS(DocumentResource,
                  Resource,
                  resource,
                  resource->GetType() == Resource::kSVGDocument,
                  resource.GetType() == Resource::kSVGDocument);

class CORE_EXPORT DocumentResourceClient : public ResourceClient {
 public:
  ~DocumentResourceClient() override {}
  static bool IsExpectedType(ResourceClient* client) {
    return client->GetResourceClientType() == kDocumentType;
  }
  ResourceClientType GetResourceClientType() const final {
    return kDocumentType;
  }
};

}  // namespace blink

#endif  // DocumentResource_h
