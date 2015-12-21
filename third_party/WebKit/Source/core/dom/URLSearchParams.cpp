// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/URLSearchParams.h"

#include "platform/network/FormDataEncoder.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

namespace {

class URLSearchParamsIterationSource final : public PairIterable<String, String>::IterationSource {
public:
    URLSearchParamsIterationSource(Vector<std::pair<String, String>> params) : m_params(params), m_current(0) { }

    bool next(ScriptState*, String& key, String& value, ExceptionState&) override
    {
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

} // namespace

URLSearchParams* URLSearchParams::create(const URLSearchParamsInit& init)
{
    if (init.isUSVString())
        return new URLSearchParams(init.getAsUSVString());
    if (init.isURLSearchParams())
        return new URLSearchParams(init.getAsURLSearchParams());

    ASSERT(init.isNull());
    return new URLSearchParams(String());
}

URLSearchParams::URLSearchParams(const String& queryString)
{
    if (!queryString.isEmpty())
        setInput(queryString);
}

URLSearchParams::URLSearchParams(URLSearchParams* searchParams)
{
    ASSERT(searchParams);
    m_params = searchParams->m_params;
}

URLSearchParams::~URLSearchParams()
{
}

static String decodeString(String input)
{
    return decodeURLEscapeSequences(input.replace('+', ' '));
}

void URLSearchParams::setInput(const String& queryString)
{
    ASSERT(m_params.isEmpty());
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
            String name = decodeString(queryString.substring(nameStart, endOfName - nameStart));
            String value;
            if (endOfName != nameValueEnd)
                value = decodeString(queryString.substring(endOfName + 1, nameValueEnd - endOfName - 1));
            if (value.isNull())
                value = "";
            m_params.append(std::make_pair(name, value));
        }
        start = nameValueEnd + 1;
    }
}
static String encodeString(const String& input)
{
    return encodeWithURLEscapeSequences(input).replace("%20", "+");
}

String URLSearchParams::toString() const
{
    StringBuilder result;
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (i)
            result.append('&');
        result.append(encodeString(m_params[i].first));
        result.append('=');
        result.append(encodeString(m_params[i].second));
    }
    return result.toString();
}

void URLSearchParams::append(const String& name, const String& value)
{
    m_params.append(std::make_pair(name, value));
}

void URLSearchParams::deleteAllWithName(const String& name)
{
    for (size_t i = 0; i < m_params.size();) {
        if (m_params[i].first == name)
            m_params.remove(i);
        else
            i++;
    }
}

String URLSearchParams::get(const String& name) const
{
    for (const auto& param : m_params) {
        if (param.first == name)
            return param.second;
    }
    return String();
}

Vector<String> URLSearchParams::getAll(const String& name) const
{
    Vector<String> result;
    for (const auto& param : m_params) {
        if (param.first == name)
            result.append(param.second);
    }
    return result;
}

bool URLSearchParams::has(const String& name) const
{
    for (const auto& param : m_params) {
        if (param.first == name)
            return true;
    }
    return false;
}

void URLSearchParams::set(const String& name, const String& value)
{
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
}

PassRefPtr<EncodedFormData> URLSearchParams::encodeFormData() const
{
    Vector<char> encodedData;
    for (const auto& param : m_params)
        FormDataEncoder::addKeyValuePairAsFormData(encodedData, param.first.utf8(), param.second.utf8(), EncodedFormData::FormURLEncoded);
    return EncodedFormData::create(encodedData.data(), encodedData.size());
}

DEFINE_TRACE(URLSearchParams)
{
}

PairIterable<String, String>::IterationSource* URLSearchParams::startIteration(ScriptState*, ExceptionState&)
{
    return new URLSearchParamsIterationSource(m_params);
}

} // namespace blink
