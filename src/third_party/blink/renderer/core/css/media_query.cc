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

#include "third_party/blink/renderer/core/css/media_query.h"

#include <algorithm>
#include <memory>
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// https://drafts.csswg.org/cssom/#serialize-a-media-query
String MediaQuery::Serialize() const {
  StringBuilder result;
  switch (Restrictor()) {
    case MediaQuery::kOnly:
      result.Append("only ");
      break;
    case MediaQuery::kNot:
      result.Append("not ");
      break;
    case MediaQuery::kNone:
      break;
  }

  const MediaQueryExpNode* exp_node = ExpNode();

  if (!exp_node) {
    result.Append(MediaType());
    return result.ReleaseString();
  }

  if (MediaType() != media_type_names::kAll || Restrictor() != kNone) {
    result.Append(MediaType());
    result.Append(" and ");
  }

  if (exp_node)
    result.Append(exp_node->Serialize());

  return result.ReleaseString();
}

std::unique_ptr<MediaQuery> MediaQuery::CreateNotAll() {
  return std::make_unique<MediaQuery>(MediaQuery::kNot, media_type_names::kAll,
                                      std::unique_ptr<MediaQueryExpNode>());
}

MediaQuery::MediaQuery(RestrictorType restrictor,
                       String media_type,
                       std::unique_ptr<MediaQueryExpNode> exp_node)
    : restrictor_(restrictor),
      media_type_(AttemptStaticStringCreation(media_type.LowerASCII())),
      exp_node_(std::move(exp_node)),
      has_unknown_(exp_node_ ? exp_node_->HasUnknown() : false) {}

MediaQuery::MediaQuery(const MediaQuery& o)
    : restrictor_(o.restrictor_),
      media_type_(o.media_type_),
      exp_node_(o.exp_node_ ? o.exp_node_->Copy() : nullptr),
      serialization_cache_(o.serialization_cache_),
      has_unknown_(o.has_unknown_) {}

MediaQuery::~MediaQuery() = default;

MediaQuery::RestrictorType MediaQuery::Restrictor() const {
  if (has_unknown_)
    return MediaQuery::kNot;
  return restrictor_;
}

const MediaQueryExpNode* MediaQuery::ExpNode() const {
  if (has_unknown_)
    return nullptr;
  return exp_node_.get();
}

const String& MediaQuery::MediaType() const {
  if (has_unknown_) {
    DEFINE_STATIC_LOCAL(const AtomicString, all, ("all"));
    return all;
  }
  return media_type_;
}

// https://drafts.csswg.org/cssom/#compare-media-queries
bool MediaQuery::operator==(const MediaQuery& other) const {
  return CssText() == other.CssText();
}

// https://drafts.csswg.org/cssom/#serialize-a-list-of-media-queries
String MediaQuery::CssText() const {
  if (serialization_cache_.IsNull())
    const_cast<MediaQuery*>(this)->serialization_cache_ = Serialize();

  return serialization_cache_;
}

std::unique_ptr<MediaQuery> MediaQuery::CopyIgnoringUnknownForTest() const {
  auto query = Copy();
  query->has_unknown_ = false;
  query->serialization_cache_ = g_null_atom;
  return query;
}

}  // namespace blink
