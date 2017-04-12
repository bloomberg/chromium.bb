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

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
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

class CORE_EXPORT MediaQuerySet : public GarbageCollected<MediaQuerySet> {
 public:
  static MediaQuerySet* Create() { return new MediaQuerySet(); }
  static MediaQuerySet* Create(const String& media_string);

  bool Set(const String&);
  bool Add(const String&);
  bool Remove(const String&);

  void AddMediaQuery(MediaQuery*);

  const HeapVector<Member<MediaQuery>>& QueryVector() const { return queries_; }

  String MediaText() const;

  MediaQuerySet* Copy() const { return new MediaQuerySet(*this); }

  DECLARE_TRACE();

 private:
  MediaQuerySet();
  MediaQuerySet(const MediaQuerySet&);

  HeapVector<Member<MediaQuery>> queries_;
};

class MediaList final : public GarbageCollected<MediaList>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaList* Create(MediaQuerySet* media_queries,
                           CSSStyleSheet* parent_sheet) {
    return new MediaList(media_queries, parent_sheet);
  }

  static MediaList* Create(MediaQuerySet* media_queries, CSSRule* parent_rule) {
    return new MediaList(media_queries, parent_rule);
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

  const MediaQuerySet* Queries() const { return media_queries_.Get(); }

  void Reattach(MediaQuerySet*);

  DECLARE_TRACE();

 private:
  MediaList(MediaQuerySet*, CSSStyleSheet* parent_sheet);
  MediaList(MediaQuerySet*, CSSRule* parent_rule);

  Member<MediaQuerySet> media_queries_;
  // Cleared in ~CSSStyleSheet destructor when oilpan is not enabled.
  Member<CSSStyleSheet> parent_style_sheet_;
  // Cleared in the ~CSSMediaRule and ~CSSImportRule destructors when oilpan is
  // not enabled.
  Member<CSSRule> parent_rule_;
};

}  // namespace blink

#endif
