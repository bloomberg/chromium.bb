// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/SourceMap.h"

#include "platform/JSONParser.h"
#include "platform/JSONValues.h"
#include "platform/weborigin/KURL.h"

namespace {

const int kSupportedVersion = 3;
const char kVersionString[] = "version";
const char kFileString[] = "file";
const char kSourceRootString[] = "sourceRoot";
const char kSourcesString[] = "sources";
const char kSourcesContentString[] = "sourcesContent";
const char kNamesString[] = "names";
const char kMappingsString[] = "mappings";
const char kSectionsString[] = "sections";
const char kOffsetString[] = "offset";
const char kLineString[] = "line";
const char kColumnString[] = "column";
const char kMapString[] = "map";
const char kURLString[] = "url";

const int kVLQBaseShift = 5;
const int kVLQBaseMask = (1 << 5) - 1;
const int kVLQContinuationMask = 1 << 5;

bool jsonStringArrayAsVector(PassRefPtr<blink::JSONArray> jsonArray, Vector<String>* vector, bool mandatory)
{
    if (!jsonArray)
        return false;
    vector->resize(jsonArray->length());
    for (unsigned i = 0; i < jsonArray->length(); ++i) {
        String source;
        RefPtr<blink::JSONValue> sourceValue = jsonArray->get(i);
        if (sourceValue && !sourceValue->isNull() && sourceValue->asString(&source))
            (*vector)[i] = source;
        else if (mandatory)
            return false;
    }
    return true;
}

class StringPeekIterator {
public:
    StringPeekIterator(const String& str)
        : m_str(str)
        , m_index(0)
    {
    }

    bool hasNext() { return m_index < m_str.length(); }
    UChar next() { return m_str[m_index++]; }
    UChar peek() { return m_str[m_index]; }

private:
    const String& m_str;
    unsigned m_index;
};

int base64SymbolToNumber(UChar sym)
{
    if (sym >= 'A' && sym <= 'Z')
        return sym - 'A';
    if (sym >= 'a' && sym <= 'z')
        return sym - 'a' + ('Z' - 'A') + 1;
    if (sym >= '0' && sym <= '9')
        return sym - '0' + ('Z' - 'A' + 1) * 2;
    if (sym == '+')
        return 62;
    if (sym == '/')
        return 63;
    return -1;
}

int decodeVLQ(StringPeekIterator& it)
{
    int result = 0;
    int shift = 0;
    int digit = 0;
    do {
        char v = it.next();
        digit = base64SymbolToNumber(v);
        result += (digit & kVLQBaseMask) << shift;
        shift += kVLQBaseShift;
    } while (digit & kVLQContinuationMask);

    int negative = result & 1;
    result >>= 1;
    return negative ? -result : result;
}

bool entryCompare(const OwnPtr<blink::SourceMap::Entry>& a, const OwnPtr<blink::SourceMap::Entry>& b)
{
    if (a->line != b->line)
        return a->line < b->line;
    return a->column < b->column;
}

bool entryCompareWithTarget(const OwnPtr<blink::SourceMap::Entry>& a, const std::pair<int, int>& b)
{
    if (a->line != b.first)
        return a->line < b.first;
    return a->column < b.second;
}

String completeURL(const blink::KURL& base, const String& url)
{
    blink::KURL completedURL(base, url);
    return completedURL.isValid() ? completedURL.string() : url;
}

} // anonymous namespace

namespace blink {

SourceMap::Entry::Entry(int line, int column, const String& sourceURL,
    int sourceLine, int sourceColumn)
    : line(line)
    , column(column)
    , sourceURL(sourceURL)
    , sourceLine(sourceLine)
    , sourceColumn(sourceColumn)
{
}

PassOwnPtr<SourceMap> SourceMap::parse(const String& json, const String& sourceMapUrl, int offsetLine, int offsetColumn)
{
    RefPtr<JSONValue> sourceMapValue = parseJSON(json);
    RefPtr<JSONObject> sourceMapObject;
    if (!sourceMapValue || !sourceMapValue->asObject(&sourceMapObject))
        return nullptr;

    OwnPtr<SourceMap> map = adoptPtr(new SourceMap());

    RefPtr<JSONArray> sectionsArray = sourceMapObject->getArray(kSectionsString);
    if (sectionsArray) {
        for (unsigned i = 0; i < sectionsArray->length(); ++i) {
            RefPtr<JSONValue> sectionValue = sectionsArray->get(i);
            if (!sectionValue)
                return nullptr;

            RefPtr<JSONObject> sectionObject = sectionValue->asObject();
            if (!sectionObject || !map->parseSection(sectionObject, sourceMapUrl, offsetLine, offsetColumn))
                return nullptr;
        }
    } else if (!map->parseMap(sourceMapObject, sourceMapUrl, offsetLine, offsetColumn)) {
        return nullptr;
    }
    std::sort(map->m_mappings.begin(), map->m_mappings.end(), entryCompare);
    return map.release();
}

const SourceMap::Entry* SourceMap::findEntry(int line, int column)
{
    if (!m_mappings.size())
        return nullptr;
    auto it = std::lower_bound(m_mappings.begin(), m_mappings.end(), std::make_pair(line, column), entryCompareWithTarget);
    if (it != m_mappings.end() && (*it)->line == line && (*it)->column == column)
        return it->get();
    if (it == m_mappings.begin())
        return nullptr;
    return (--it)->get();
}

bool SourceMap::parseSection(PassRefPtr<JSONObject> prpSectionObject, const String& sourceMapUrl, int offsetLine, int offsetColumn)
{
    RefPtr<JSONObject> sectionObject = prpSectionObject;
    RefPtr<JSONObject> offsetObject = sectionObject->getObject(kOffsetString);
    if (!offsetObject)
        return false;

    int line = 0;
    int column = 0;
    if (!offsetObject->getNumber(kLineString, &line))
        return false;
    if (!offsetObject->getNumber(kColumnString, &column))
        return false;

    RefPtr<JSONObject> mapObject = sectionObject->getObject(kMapString);
    String url;
    if (!mapObject && !sectionObject->getString(kURLString, &url))
        return false;
    return mapObject ? parseMap(mapObject, sourceMapUrl, line + offsetLine, column + offsetColumn) : true;
}

bool SourceMap::parseMap(PassRefPtr<JSONObject> prpMapObject, const String& sourceMapUrl, int line, int column)
{
    RefPtr<JSONObject> mapObject = prpMapObject;
    if (!mapObject)
        return false;

    int version = 0;
    if (!mapObject->getNumber(kVersionString, &version) || version != kSupportedVersion)
        return false;

    String file;
    mapObject->getString(kFileString, &file);

    String sourceRoot;
    mapObject->getString(kSourceRootString, &sourceRoot);

    if (!sourceRoot.isNull() && sourceRoot.length() > 0 && sourceRoot[sourceRoot.length() - 1] != '/')
        sourceRoot.append('/');

    Vector<String> sources;
    if (!jsonStringArrayAsVector(mapObject->getArray(kSourcesString), &sources, true))
        return false;

    if (sources.size() == 0)
        return false;

    KURL baseURL(KURL(), sourceMapUrl);
    if (!baseURL.isValid())
        baseURL = KURL();

    // For information about resolving sources, please see:
    // https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit#heading=h.75yo6yoyk7x5
    for (size_t i = 0; i < sources.size(); ++i)
        sources[i] = completeURL(baseURL, sourceRoot + sources[i]);

    Vector<String> sourcesContent;
    if (jsonStringArrayAsVector(mapObject->getArray(kSourcesContentString), &sourcesContent, false)) {
        if (sourcesContent.size() != sources.size())
            return false;
    }

    Vector<String> names;
    if (!jsonStringArrayAsVector(mapObject->getArray(kNamesString), &names, true))
        return false;

    String mappings;
    if (!mapObject->getString(kMappingsString, &mappings))
        return false;

    size_t sourceIndex = 0;
    String sourceURL = sources[sourceIndex];
    int sourceLine = 0;
    int sourceColumn = 0;
    int nameIndex = 0;

    StringPeekIterator it(mappings);

    while (it.hasNext()) {
        if (it.peek() == ',') {
            it.next();
        } else {
            while (it.peek() == ';') {
                ++line;
                column = 0;
                it.next();
            }
            if (!it.hasNext())
                break;
        }

        column += decodeVLQ(it);
        if (!it.hasNext() || it.peek() == ',' || it.peek() == ';') {
            m_mappings.append(adoptPtr(new Entry(line, column)));
            continue;
        }

        int sourceIndexDelta = decodeVLQ(it);
        if (sourceIndexDelta) {
            sourceIndex += sourceIndexDelta;
            if (sourceIndex >= sources.size())
                return false;
            sourceURL = sources[sourceIndex];
        }

        if (it.hasNext()) {
            sourceLine += decodeVLQ(it);
            sourceColumn += decodeVLQ(it);
            if (it.hasNext() && it.peek() != ',' && it.peek() != ';')
                nameIndex += decodeVLQ(it);
        }
        m_mappings.append(adoptPtr(new Entry(line, column, sourceURL, sourceLine, sourceColumn)));
    }
    return true;
}

} // namespace blink
