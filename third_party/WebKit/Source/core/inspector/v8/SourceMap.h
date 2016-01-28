// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceMap_h
#define SourceMap_h

#include "core/CoreExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class JSONObject;

class CORE_EXPORT SourceMap {
    WTF_MAKE_NONCOPYABLE(SourceMap);
public:
    class CORE_EXPORT Entry {
        WTF_MAKE_NONCOPYABLE(Entry);
    public:
        int line;
        int column;
        String sourceURL;
        int sourceLine;
        int sourceColumn;

        Entry(int line, int column, const String& sourceURL = String(), int sourceLine = 0, int sourceColumn = 0);
        Entry()
            : line(0)
            , column(0)
            , sourceLine(0)
            , sourceColumn(0)
        {
        }
    };

    static PassOwnPtr<SourceMap> parse(const String& json, const String& sourceMapUrl, int offsetLine = 0, int offsetColumn = 0);

    const Entry* findEntry(int line, int column);

private:
    SourceMap() { }

    bool parseSection(PassRefPtr<JSONObject> sectionObject, const String& sourceMapUrl, int offsetLine, int offsetColumn);
    bool parseMap(PassRefPtr<JSONObject> mapObject, const String& sourceMapUrl, int offsetLine, int offsetColumn);

    Vector<OwnPtr<Entry>> m_mappings;
};

} // namespace blink

#endif // SourceMap_h
