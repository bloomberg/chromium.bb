/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef MediaList_h
#define MediaList_h

#include "core/CoreExport.h"
#include "core/css/MediaQuery.h"
#include "core/dom/ExceptionCode.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class ExceptionState;
class MediaList;
class MediaQuery;

class CORE_EXPORT MediaQuerySet : public RefCounted<MediaQuerySet> {
 public:
  static RefPtr<MediaQuerySet> Create() {
    return WTF::AdoptRef(new MediaQuerySet());
  }
  static RefPtr<MediaQuerySet> Create(const String& media_string);

  bool Set(const String&);
  bool Add(const String&);
  bool Remove(const String&);

  void AddMediaQuery(std::unique_ptr<MediaQuery>);

  const Vector<std::unique_ptr<MediaQuery>>& QueryVector() const {
    return queries_;
  }

  String MediaText() const;

  RefPtr<MediaQuerySet> Copy() const {
    return WTF::AdoptRef(new MediaQuerySet(*this));
  }

 private:
  MediaQuerySet();
  MediaQuerySet(const MediaQuerySet&);

  Vector<std::unique_ptr<MediaQuery>> queries_;
};

class MediaList final : public GarbageCollectedFinalized<MediaList>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaList* Create(RefPtr<MediaQuerySet> media_queries,
                           CSSStyleSheet* parent_sheet) {
    return new MediaList(std::move(media_queries), parent_sheet);
  }

  static MediaList* Create(RefPtr<MediaQuerySet> media_queries,
                           CSSRule* parent_rule) {
    return new MediaList(std::move(media_queries), parent_rule);
  }

  unsigned length() const { return media_queries_->QueryVector().size(); }
  String item(unsigned index) const;
  void deleteMedium(const String& old_medium, ExceptionState&);
  void appendMedium(const String& new_medium, ExceptionState&);

  String mediaText() const { return media_queries_->MediaText(); }
  void setMediaText(const String&);

  // Not part of CSSOM.
  CSSRule* ParentRule() const { return parent_rule_; }
  CSSStyleSheet* ParentStyleSheet() const { return parent_style_sheet_; }

  const MediaQuerySet* Queries() const { return media_queries_.get(); }

  void Reattach(RefPtr<MediaQuerySet>);

  void Trace(blink::Visitor*);

 private:
  MediaList(RefPtr<MediaQuerySet>, CSSStyleSheet* parent_sheet);
  MediaList(RefPtr<MediaQuerySet>, CSSRule* parent_rule);

  RefPtr<MediaQuerySet> media_queries_;
  Member<CSSStyleSheet> parent_style_sheet_;
  Member<CSSRule> parent_rule_;
};

}  // namespace blink

#endif
