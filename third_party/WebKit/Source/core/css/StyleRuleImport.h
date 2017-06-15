/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef StyleRuleImport_h
#define StyleRuleImport_h

#include "core/css/StyleRule.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSStyleSheetResource;
class MediaQuerySet;
class StyleSheetContents;

class StyleRuleImport : public StyleRuleBase {
  USING_PRE_FINALIZER(StyleRuleImport, Dispose);

 public:
  static StyleRuleImport* Create(const String& href, RefPtr<MediaQuerySet>);

  ~StyleRuleImport();

  StyleSheetContents* ParentStyleSheet() const { return parent_style_sheet_; }
  void SetParentStyleSheet(StyleSheetContents* sheet) {
    DCHECK(sheet);
    parent_style_sheet_ = sheet;
  }
  void ClearParentStyleSheet() { parent_style_sheet_ = nullptr; }

  String Href() const { return str_href_; }
  StyleSheetContents* GetStyleSheet() const { return style_sheet_.Get(); }

  bool IsLoading() const;
  MediaQuerySet* MediaQueries() { return media_queries_.Get(); }

  void RequestStyleSheet();

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  // FIXME: inherit from StyleSheetResourceClient directly to eliminate back
  // pointer, as there are no space savings in this.
  // NOTE: We put the StyleSheetResourceClient in a member instead of inheriting
  // from it to avoid adding a vptr to StyleRuleImport.
  class ImportedStyleSheetClient final
      : public GarbageCollectedFinalized<ImportedStyleSheetClient>,
        public StyleSheetResourceClient {
    USING_GARBAGE_COLLECTED_MIXIN(ImportedStyleSheetClient);

   public:
    ImportedStyleSheetClient(StyleRuleImport* owner_rule)
        : owner_rule_(owner_rule) {}
    ~ImportedStyleSheetClient() override {}
    void SetCSSStyleSheet(const String& href,
                          const KURL& base_url,
                          ReferrerPolicy referrer_policy,
                          const WTF::TextEncoding& charset,
                          const CSSStyleSheetResource* sheet) override {
      owner_rule_->SetCSSStyleSheet(href, base_url, referrer_policy, charset,
                                    sheet);
    }
    String DebugName() const override { return "ImportedStyleSheetClient"; }

    DEFINE_INLINE_TRACE() {
      visitor->Trace(owner_rule_);
      StyleSheetResourceClient::Trace(visitor);
    }

   private:
    Member<StyleRuleImport> owner_rule_;
  };

  void SetCSSStyleSheet(const String& href,
                        const KURL& base_url,
                        ReferrerPolicy,
                        const WTF::TextEncoding&,
                        const CSSStyleSheetResource*);

  StyleRuleImport(const String& href, RefPtr<MediaQuerySet>);

  void Dispose();

  Member<StyleSheetContents> parent_style_sheet_;

  Member<ImportedStyleSheetClient> style_sheet_client_;
  String str_href_;
  RefPtr<MediaQuerySet> media_queries_;
  Member<StyleSheetContents> style_sheet_;
  Member<CSSStyleSheetResource> resource_;
  bool loading_;
};

DEFINE_STYLE_RULE_TYPE_CASTS(Import);

}  // namespace blink

#endif
