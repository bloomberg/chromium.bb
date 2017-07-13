// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/url/URLSearchParams.h"

#include <algorithm>
#include <utility>
#include "core/url/DOMURL.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

namespace {

class URLSearchParamsIterationSource final
    : public PairIterable<String, String>::IterationSource {
 public:
  URLSearchParamsIterationSource(Vector<std::pair<String, String>> params)
      : params_(params), current_(0) {}

  bool Next(ScriptState*,
            String& key,
            String& value,
            ExceptionState&) override {
    if (current_ >= params_.size())
      return false;

    key = params_[current_].first;
    value = params_[current_].second;
    current_++;
    return true;
  }

 private:
  Vector<std::pair<String, String>> params_;
  size_t current_;
};

bool CompareParams(const std::pair<String, String>& a,
                   const std::pair<String, String>& b) {
  return WTF::CodePointCompareLessThan(a.first, b.first);
}

}  // namespace

URLSearchParams* URLSearchParams::Create(const URLSearchParamsInit& init,
                                         ExceptionState& exception_state) {
  if (init.isUSVString()) {
    const String& query_string = init.getAsUSVString();
    if (query_string.StartsWith('?'))
      return new URLSearchParams(query_string.Substring(1));
    return new URLSearchParams(query_string);
  }
  if (init.isUSVStringUSVStringRecord()) {
    return URLSearchParams::Create(init.getAsUSVStringUSVStringRecord(),
                                   exception_state);
  }
  if (init.isUSVStringSequenceSequence()) {
    return URLSearchParams::Create(init.getAsUSVStringSequenceSequence(),
                                   exception_state);
  }

  DCHECK(init.isNull());
  return new URLSearchParams(String());
}

URLSearchParams* URLSearchParams::Create(const Vector<Vector<String>>& init,
                                         ExceptionState& exception_state) {
  URLSearchParams* instance = new URLSearchParams(String());
  if (!init.size())
    return instance;
  for (unsigned i = 0; i < init.size(); ++i) {
    const Vector<String>& pair = init[i];
    if (pair.size() != 2) {
      exception_state.ThrowTypeError(ExceptionMessages::FailedToConstruct(
          "URLSearchParams",
          "Sequence initializer must only contain pair elements"));
      return nullptr;
    }
    instance->AppendWithoutUpdate(pair[0], pair[1]);
  }
  instance->RunUpdateSteps();
  return instance;
}

URLSearchParams::URLSearchParams(const String& query_string, DOMURL* url_object)
    : url_object_(url_object) {
  if (!query_string.IsEmpty())
    SetInput(query_string);
}

URLSearchParams* URLSearchParams::Create(
    const Vector<std::pair<String, String>>& init,
    ExceptionState& exception_state) {
  URLSearchParams* instance = new URLSearchParams(String());
  if (init.IsEmpty())
    return instance;
  for (const auto& item : init)
    instance->AppendWithoutUpdate(item.first, item.second);
  instance->RunUpdateSteps();
  return instance;
}

URLSearchParams::~URLSearchParams() {}

DEFINE_TRACE(URLSearchParams) {
  visitor->Trace(url_object_);
}

#if DCHECK_IS_ON()
DOMURL* URLSearchParams::UrlObject() const {
  return url_object_;
}
#endif

void URLSearchParams::RunUpdateSteps() {
  if (!url_object_)
    return;

  if (url_object_->IsInUpdate())
    return;

  url_object_->SetSearchInternal(toString());
}

static String DecodeString(String input) {
  return DecodeURLEscapeSequences(input.Replace('+', ' '));
}

void URLSearchParams::SetInput(const String& query_string) {
  params_.clear();

  size_t start = 0;
  size_t query_string_length = query_string.length();
  while (start < query_string_length) {
    size_t name_start = start;
    size_t name_value_end = query_string.find('&', start);
    if (name_value_end == kNotFound)
      name_value_end = query_string_length;
    if (name_value_end > start) {
      size_t end_of_name = query_string.find('=', start);
      if (end_of_name == kNotFound || end_of_name > name_value_end)
        end_of_name = name_value_end;
      String name = DecodeString(
          query_string.Substring(name_start, end_of_name - name_start));
      String value;
      if (end_of_name != name_value_end)
        value = DecodeString(query_string.Substring(
            end_of_name + 1, name_value_end - end_of_name - 1));
      if (value.IsNull())
        value = "";
      AppendWithoutUpdate(name, value);
    }
    start = name_value_end + 1;
  }
  RunUpdateSteps();
}

String URLSearchParams::toString() const {
  Vector<char> encoded_data;
  EncodeAsFormData(encoded_data);
  return String(encoded_data.data(), encoded_data.size());
}

void URLSearchParams::AppendWithoutUpdate(const String& name,
                                          const String& value) {
  params_.push_back(std::make_pair(name, value));
}

void URLSearchParams::append(const String& name, const String& value) {
  AppendWithoutUpdate(name, value);
  RunUpdateSteps();
}

void URLSearchParams::deleteAllWithName(const String& name) {
  for (size_t i = 0; i < params_.size();) {
    if (params_[i].first == name)
      params_.erase(i);
    else
      i++;
  }
  RunUpdateSteps();
}

String URLSearchParams::get(const String& name) const {
  for (const auto& param : params_) {
    if (param.first == name)
      return param.second;
  }
  return String();
}

Vector<String> URLSearchParams::getAll(const String& name) const {
  Vector<String> result;
  for (const auto& param : params_) {
    if (param.first == name)
      result.push_back(param.second);
  }
  return result;
}

bool URLSearchParams::has(const String& name) const {
  for (const auto& param : params_) {
    if (param.first == name)
      return true;
  }
  return false;
}

void URLSearchParams::set(const String& name, const String& value) {
  bool found_match = false;
  for (size_t i = 0; i < params_.size();) {
    // If there are any name-value whose name is 'name', set
    // the value of the first such name-value pair to 'value'
    // and remove the others.
    if (params_[i].first == name) {
      if (!found_match) {
        params_[i++].second = value;
        found_match = true;
      } else {
        params_.erase(i);
      }
    } else {
      i++;
    }
  }
  // Otherwise, append a new name-value pair to the list.
  if (!found_match)
    append(name, value);
  else
    RunUpdateSteps();
}

void URLSearchParams::sort() {
  std::stable_sort(params_.begin(), params_.end(), CompareParams);
  RunUpdateSteps();
}

void URLSearchParams::EncodeAsFormData(Vector<char>& encoded_data) const {
  for (const auto& param : params_)
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, param.first.Utf8(), param.second.Utf8(),
        EncodedFormData::kFormURLEncoded, FormDataEncoder::kDoNotNormalizeCRLF);
}

PassRefPtr<EncodedFormData> URLSearchParams::ToEncodedFormData() const {
  Vector<char> encoded_data;
  EncodeAsFormData(encoded_data);
  return EncodedFormData::Create(encoded_data.data(), encoded_data.size());
}

PairIterable<String, String>::IterationSource* URLSearchParams::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return new URLSearchParamsIterationSource(params_);
}

}  // namespace blink
