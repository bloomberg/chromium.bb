// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/URLSearchParams.h"

#include <utility>
#include "core/dom/DOMURL.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

namespace {

class URLSearchParamsIterationSource final
    : public PairIterable<String, String>::IterationSource {
 public:
  URLSearchParamsIterationSource(Vector<std::pair<String, String>> params)
      : m_params(params), m_current(0) {}

  bool next(ScriptState*,
            String& key,
            String& value,
            ExceptionState&) override {
    if (m_current >= m_params.size())
      return false;

    key = m_params[m_current].first;
    value = m_params[m_current].second;
    m_current++;
    return true;
  }

 private:
  Vector<std::pair<String, String>> m_params;
  size_t m_current;
};

}  // namespace

URLSearchParams* URLSearchParams::create(const URLSearchParamsInit& init,
                                         ExceptionState& exceptionState) {
  if (init.isUSVString()) {
    const String& queryString = init.getAsUSVString();
    if (queryString.startsWith('?'))
      return new URLSearchParams(queryString.substring(1));
    return new URLSearchParams(queryString);
  }
  // TODO(sof): copy constructor no longer in the spec,
  // consider removing.
  if (init.isURLSearchParams())
    return new URLSearchParams(init.getAsURLSearchParams());

  if (init.isUSVStringSequenceSequence()) {
    return URLSearchParams::create(init.getAsUSVStringSequenceSequence(),
                                   exceptionState);
  }

  DCHECK(init.isNull());
  return new URLSearchParams(String());
}

URLSearchParams* URLSearchParams::create(const Vector<Vector<String>>& init,
                                         ExceptionState& exceptionState) {
  URLSearchParams* instance = new URLSearchParams(String());
  if (!init.size())
    return instance;
  for (unsigned i = 0; i < init.size(); ++i) {
    const Vector<String>& pair = init[i];
    if (pair.size() != 2) {
      exceptionState.throwTypeError(ExceptionMessages::failedToConstruct(
          "URLSearchParams",
          "Sequence initializer must only contain pair elements"));
      return nullptr;
    }
    instance->appendWithoutUpdate(pair[0], pair[1]);
  }
  instance->runUpdateSteps();
  return instance;
}

URLSearchParams::URLSearchParams(const String& queryString, DOMURL* urlObject)
    : m_urlObject(urlObject) {
  if (!queryString.isEmpty())
    setInput(queryString);
}

URLSearchParams::URLSearchParams(URLSearchParams* searchParams) {
  DCHECK(searchParams);
  m_params = searchParams->m_params;
}

URLSearchParams::~URLSearchParams() {}

DEFINE_TRACE(URLSearchParams) {
  visitor->trace(m_urlObject);
}

#if DCHECK_IS_ON()
DOMURL* URLSearchParams::urlObject() const {
  return m_urlObject;
}
#endif

void URLSearchParams::runUpdateSteps() {
  if (!m_urlObject)
    return;

  if (m_urlObject->isInUpdate())
    return;

  m_urlObject->setSearchInternal(toString());
}

static String decodeString(String input) {
  return decodeURLEscapeSequences(input.replace('+', ' '));
}

void URLSearchParams::setInput(const String& queryString) {
  m_params.clear();

  size_t start = 0;
  size_t queryStringLength = queryString.length();
  while (start < queryStringLength) {
    size_t nameStart = start;
    size_t nameValueEnd = queryString.find('&', start);
    if (nameValueEnd == kNotFound)
      nameValueEnd = queryStringLength;
    if (nameValueEnd > start) {
      size_t endOfName = queryString.find('=', start);
      if (endOfName == kNotFound || endOfName > nameValueEnd)
        endOfName = nameValueEnd;
      String name =
          decodeString(queryString.substring(nameStart, endOfName - nameStart));
      String value;
      if (endOfName != nameValueEnd)
        value = decodeString(
            queryString.substring(endOfName + 1, nameValueEnd - endOfName - 1));
      if (value.isNull())
        value = "";
      appendWithoutUpdate(name, value);
    }
    start = nameValueEnd + 1;
  }
  runUpdateSteps();
}

String URLSearchParams::toString() const {
  Vector<char> encodedData;
  encodeAsFormData(encodedData);
  return String(encodedData.data(), encodedData.size());
}

void URLSearchParams::appendWithoutUpdate(const String& name,
                                          const String& value) {
  m_params.push_back(std::make_pair(name, value));
}

void URLSearchParams::append(const String& name, const String& value) {
  appendWithoutUpdate(name, value);
  runUpdateSteps();
}

void URLSearchParams::deleteAllWithName(const String& name) {
  for (size_t i = 0; i < m_params.size();) {
    if (m_params[i].first == name)
      m_params.remove(i);
    else
      i++;
  }
  runUpdateSteps();
}

String URLSearchParams::get(const String& name) const {
  for (const auto& param : m_params) {
    if (param.first == name)
      return param.second;
  }
  return String();
}

Vector<String> URLSearchParams::getAll(const String& name) const {
  Vector<String> result;
  for (const auto& param : m_params) {
    if (param.first == name)
      result.push_back(param.second);
  }
  return result;
}

bool URLSearchParams::has(const String& name) const {
  for (const auto& param : m_params) {
    if (param.first == name)
      return true;
  }
  return false;
}

void URLSearchParams::set(const String& name, const String& value) {
  bool foundMatch = false;
  for (size_t i = 0; i < m_params.size();) {
    // If there are any name-value whose name is 'name', set
    // the value of the first such name-value pair to 'value'
    // and remove the others.
    if (m_params[i].first == name) {
      if (!foundMatch) {
        m_params[i++].second = value;
        foundMatch = true;
      } else {
        m_params.remove(i);
      }
    } else {
      i++;
    }
  }
  // Otherwise, append a new name-value pair to the list.
  if (!foundMatch)
    append(name, value);
  else
    runUpdateSteps();
}

void URLSearchParams::encodeAsFormData(Vector<char>& encodedData) const {
  for (const auto& param : m_params)
    FormDataEncoder::addKeyValuePairAsFormData(
        encodedData, param.first.utf8(), param.second.utf8(),
        EncodedFormData::FormURLEncoded, FormDataEncoder::DoNotNormalizeCRLF);
}

PassRefPtr<EncodedFormData> URLSearchParams::toEncodedFormData() const {
  Vector<char> encodedData;
  encodeAsFormData(encodedData);
  return EncodedFormData::create(encodedData.data(), encodedData.size());
}

PairIterable<String, String>::IterationSource* URLSearchParams::startIteration(
    ScriptState*,
    ExceptionState&) {
  return new URLSearchParamsIterationSource(m_params);
}

}  // namespace blink
