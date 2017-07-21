// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptSourceCode.h"

namespace blink {

ScriptSourceCode::ScriptSourceCode()
    : start_position_(TextPosition::MinimumPosition()) {}

ScriptSourceCode::ScriptSourceCode(const String& source,
                                   const KURL& url,
                                   const TextPosition& start_position)
    : source_(source), url_(url), start_position_(start_position) {
  TreatNullSourceAsEmpty();
  if (!url_.IsEmpty())
    url_.RemoveFragmentIdentifier();
}

ScriptSourceCode::ScriptSourceCode(ScriptResource* resource)
    : ScriptSourceCode(nullptr, resource) {}

ScriptSourceCode::ScriptSourceCode(ScriptStreamer* streamer,
                                   ScriptResource* resource)
    : source_(resource->SourceText()),
      resource_(resource),
      streamer_(streamer),
      start_position_(TextPosition::MinimumPosition()) {
  TreatNullSourceAsEmpty();
}

ScriptSourceCode::~ScriptSourceCode() {}

DEFINE_TRACE(ScriptSourceCode) {
  visitor->Trace(resource_);
  visitor->Trace(streamer_);
}

const KURL& ScriptSourceCode::Url() const {
  if (url_.IsEmpty() && resource_) {
    url_ = resource_->GetResponse().Url();
    if (!url_.IsEmpty())
      url_.RemoveFragmentIdentifier();
  }
  return url_;
}

String ScriptSourceCode::SourceMapUrl() const {
  if (!resource_)
    return String();
  const ResourceResponse& response = resource_->GetResponse();
  String source_map_url = response.HttpHeaderField(HTTPNames::SourceMap);
  if (source_map_url.IsEmpty()) {
    // Try to get deprecated header.
    source_map_url = response.HttpHeaderField(HTTPNames::X_SourceMap);
  }
  return source_map_url;
}

void ScriptSourceCode::TreatNullSourceAsEmpty() {
  // ScriptSourceCode allows for the representation of the null/not-there-really
  // ScriptSourceCode value.  Encoded by way of a m_source.isNull() being true,
  // with the nullary constructor to be used to construct such a value.
  //
  // Should the other constructors be passed a null string, that is interpreted
  // as representing the empty script. Consequently, we need to disambiguate
  // between such null string occurrences.  Do that by converting the latter
  // case's null strings into empty ones.
  if (source_.IsNull())
    source_ = "";
}

}  // namespace blink
