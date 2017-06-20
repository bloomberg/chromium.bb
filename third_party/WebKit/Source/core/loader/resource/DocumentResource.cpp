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

#include "core/loader/resource/DocumentResource.h"

#include "core/dom/XMLDocument.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

DocumentResource* DocumentResource::FetchSVGDocument(FetchParameters& params,
                                                     ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextImage);
  return ToDocumentResource(
      fetcher->RequestResource(params, SVGDocumentResourceFactory()));
}

DocumentResource::DocumentResource(const ResourceRequest& request,
                                   Type type,
                                   const ResourceLoaderOptions& options)
    : TextResource(request,
                   type,
                   options,
                   TextResourceDecoder::kXMLContent,
                   String()) {
  // FIXME: We'll support more types to support HTMLImports.
  DCHECK_EQ(type, kSVGDocument);
}

DocumentResource::~DocumentResource() {}

DEFINE_TRACE(DocumentResource) {
  visitor->Trace(document_);
  Resource::Trace(visitor);
}

void DocumentResource::CheckNotify() {
  if (Data() && MimeTypeAllowed()) {
    // We don't need to create a new frame because the new document belongs to
    // the parent UseElement.
    document_ = CreateDocument(GetResponse().Url());
    document_->SetContent(DecodedText());
  }
  Resource::CheckNotify();
}

bool DocumentResource::MimeTypeAllowed() const {
  DCHECK_EQ(GetType(), kSVGDocument);
  AtomicString mime_type = GetResponse().MimeType();
  if (GetResponse().IsHTTP())
    mime_type = HttpContentType();
  return mime_type == "image/svg+xml" || mime_type == "text/xml" ||
         mime_type == "application/xml" || mime_type == "application/xhtml+xml";
}

Document* DocumentResource::CreateDocument(const KURL& url) {
  switch (GetType()) {
    case kSVGDocument:
      return XMLDocument::CreateSVG(DocumentInit(url));
    default:
      // FIXME: We'll add more types to support HTMLImports.
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
