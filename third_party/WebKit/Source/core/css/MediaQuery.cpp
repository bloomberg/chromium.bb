/*
 * CSS Media Query
 *
 * Copyright (C) 2005, 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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

#include "core/css/MediaQuery.h"

#include <memory>
#include "core/css/MediaQueryExp.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/media_type_names.h"
#include "platform/wtf/NonCopyingSort.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

// http://dev.w3.org/csswg/cssom/#serialize-a-media-query
String MediaQuery::Serialize() const {
  StringBuilder result;
  switch (restrictor_) {
    case MediaQuery::kOnly:
      result.Append("only ");
      break;
    case MediaQuery::kNot:
      result.Append("not ");
      break;
    case MediaQuery::kNone:
      break;
  }

  if (expressions_.IsEmpty()) {
    result.Append(media_type_);
    return result.ToString();
  }

  if (media_type_ != MediaTypeNames::all || restrictor_ != kNone) {
    result.Append(media_type_);
    result.Append(" and ");
  }

  result.Append(expressions_.at(0).Serialize());
  for (size_t i = 1; i < expressions_.size(); ++i) {
    result.Append(" and ");
    result.Append(expressions_.at(i).Serialize());
  }
  return result.ToString();
}

static bool ExpressionCompare(const MediaQueryExp& a, const MediaQueryExp& b) {
  return CodePointCompare(a.Serialize(), b.Serialize()) < 0;
}

std::unique_ptr<MediaQuery> MediaQuery::CreateNotAll() {
  return WTF::MakeUnique<MediaQuery>(MediaQuery::kNot, MediaTypeNames::all,
                                     ExpressionHeapVector());
}

std::unique_ptr<MediaQuery> MediaQuery::Create(
    RestrictorType restrictor,
    String media_type,
    ExpressionHeapVector expressions) {
  return WTF::MakeUnique<MediaQuery>(restrictor, std::move(media_type),
                                     std::move(expressions));
}

MediaQuery::MediaQuery(RestrictorType restrictor,
                       String media_type,
                       ExpressionHeapVector expressions)
    : restrictor_(restrictor),
      media_type_(AttemptStaticStringCreation(media_type.LowerASCII())),
      expressions_(std::move(expressions)) {
  NonCopyingSort(expressions_.begin(), expressions_.end(), ExpressionCompare);

  // Remove all duplicated expressions.
  MediaQueryExp key = MediaQueryExp::Invalid();
  for (int i = expressions_.size() - 1; i >= 0; --i) {
    MediaQueryExp exp = expressions_.at(i);
    CHECK(exp.IsValid());
    if (exp == key)
      expressions_.EraseAt(i);
    else
      key = exp;
  }
}

MediaQuery::MediaQuery(const MediaQuery& o)
    : restrictor_(o.restrictor_),
      media_type_(o.media_type_),
      serialization_cache_(o.serialization_cache_) {
  expressions_.ReserveInitialCapacity(o.expressions_.size());
  for (unsigned i = 0; i < o.expressions_.size(); ++i)
    expressions_.push_back(o.expressions_[i]);
}

MediaQuery::~MediaQuery() {}

// http://dev.w3.org/csswg/cssom/#compare-media-queries
bool MediaQuery::operator==(const MediaQuery& other) const {
  return CssText() == other.CssText();
}

// http://dev.w3.org/csswg/cssom/#serialize-a-list-of-media-queries
String MediaQuery::CssText() const {
  if (serialization_cache_.IsNull())
    const_cast<MediaQuery*>(this)->serialization_cache_ = Serialize();

  return serialization_cache_;
}

}  // namespace blink
