/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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

#include "core/loader/resource/XSLStyleSheetResource.h"

#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"

namespace blink {

static void ApplyXSLRequestProperties(FetchParameters& params) {
  params.SetRequestContext(WebURLRequest::kRequestContextXSLT);
  // TODO(japhet): Accept: headers can be set manually on XHRs from script, in
  // the browser process, and... here. The browser process can't tell the
  // difference between an XSL stylesheet and a CSS stylesheet, so it assumes
  // stylesheets are all CSS unless they already have an Accept: header set.
  // Should we teach the browser process the difference?
  DEFINE_STATIC_LOCAL(const AtomicString, accept_xslt,
                      ("text/xml, application/xml, application/xhtml+xml, "
                       "text/xsl, application/rss+xml, application/atom+xml"));
  params.MutableResourceRequest().SetHTTPAccept(accept_xslt);
}

XSLStyleSheetResource* XSLStyleSheetResource::FetchSynchronously(
    FetchParameters& params,
    ResourceFetcher* fetcher) {
  ApplyXSLRequestProperties(params);
  params.MakeSynchronous();
  XSLStyleSheetResource* resource = ToXSLStyleSheetResource(
      fetcher->RequestResource(params, XSLStyleSheetResourceFactory()));
  if (resource && resource->Data())
    resource->sheet_ = resource->DecodedText();
  return resource;
}

XSLStyleSheetResource* XSLStyleSheetResource::Fetch(FetchParameters& params,
                                                    ResourceFetcher* fetcher) {
  DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
  ApplyXSLRequestProperties(params);
  return ToXSLStyleSheetResource(
      fetcher->RequestResource(params, XSLStyleSheetResourceFactory()));
}

XSLStyleSheetResource::XSLStyleSheetResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const String& charset)
    : StyleSheetResource(resource_request,
                         kXSLStyleSheet,
                         options,
                         TextResourceDecoderOptions::kXMLContent,
                         charset) {}

void XSLStyleSheetResource::DidAddClient(ResourceClient* c) {
  DCHECK(StyleSheetResourceClient::IsExpectedType(c));
  Resource::DidAddClient(c);
  if (!IsLoading()) {
    static_cast<StyleSheetResourceClient*>(c)->SetXSLStyleSheet(
        GetResourceRequest().Url(), GetResponse().Url(), sheet_);
  }
}

void XSLStyleSheetResource::CheckNotify() {
  TriggerNotificationForFinishObservers();

  if (Data())
    sheet_ = DecodedText();

  ResourceClientWalker<StyleSheetResourceClient> w(Clients());
  while (StyleSheetResourceClient* c = w.Next()) {
    MarkClientFinished(c);
    c->SetXSLStyleSheet(GetResourceRequest().Url(), GetResponse().Url(),
                        sheet_);
  }
}

}  // namespace blink
