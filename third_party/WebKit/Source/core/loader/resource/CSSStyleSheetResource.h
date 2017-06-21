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

#ifndef CSSStyleSheetResource_h
#define CSSStyleSheetResource_h

#include "core/CoreExport.h"
#include "core/loader/resource/StyleSheetResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

class CSSParserContext;
class FetchParameters;
class KURL;
class ResourceClient;
class ResourceFetcher;
class StyleSheetContents;

class CORE_EXPORT CSSStyleSheetResource final : public StyleSheetResource {
 public:
  enum class MIMETypeCheck { kStrict, kLax };

  static CSSStyleSheetResource* Fetch(FetchParameters&, ResourceFetcher*);
  static CSSStyleSheetResource* CreateForTest(const KURL&,
                                              const WTF::TextEncoding&);

  ~CSSStyleSheetResource() override;
  DECLARE_VIRTUAL_TRACE();

  const String SheetText(MIMETypeCheck = MIMETypeCheck::kStrict) const;

  void DidAddClient(ResourceClient*) override;

  StyleSheetContents* RestoreParsedStyleSheet(const CSSParserContext*);
  void SaveParsedStyleSheet(StyleSheetContents*);

  void AppendData(const char* data, size_t length) override;

 private:
  class CSSStyleSheetResourceFactory : public ResourceFactory {
   public:
    CSSStyleSheetResourceFactory()
        : ResourceFactory(Resource::kCSSStyleSheet,
                          TextResourceDecoderOptions::kCSSContent) {}

    Resource* Create(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        const TextResourceDecoderOptions& decoder_options) const override {
      return new CSSStyleSheetResource(request, options, decoder_options);
    }
  };
  CSSStyleSheetResource(const ResourceRequest&,
                        const ResourceLoaderOptions&,
                        const TextResourceDecoderOptions&);

  bool CanUseSheet(MIMETypeCheck) const;
  void CheckNotify() override;

  void SetParsedStyleSheetCache(StyleSheetContents*);
  void SetDecodedSheetText(const String&);

  void DestroyDecodedDataIfPossible() override;
  void DestroyDecodedDataForFailedRevalidation() override;
  void UpdateDecodedSize();

  // Decoded sheet text cache is available iff loading this CSS resource is
  // successfully complete.
  String decoded_sheet_text_;

  Member<StyleSheetContents> parsed_style_sheet_cache_;

  bool did_notify_first_data_;
};

DEFINE_RESOURCE_TYPE_CASTS(CSSStyleSheet);

}  // namespace blink

#endif
