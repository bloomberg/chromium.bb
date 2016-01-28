// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/SourceMap.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

bool isEqualEntry(const blink::SourceMap::Entry* a, const OwnPtr<blink::SourceMap::Entry>& b)
{
    return a->line == b->line && a->column == b->column
        && a->sourceURL == b->sourceURL && a->sourceLine == b->sourceLine
        && a->sourceColumn == b->sourceColumn;
}

TEST(SourceMapTest, ParseValid)
{
    const char* sources[] = {
        "{\"version\": 3, \"sources\": [\"1.js\"], \"sourcesContent\": [null], \"names\": [], \"mappings\":\";\"}"
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(const char*); ++i) {
        OwnPtr<SourceMap> sourceMap = SourceMap::parse(sources[i], String());
        EXPECT_FALSE(!sourceMap) << sources[i];
    }
    OwnPtr<SourceMap> sourceMap = SourceMap::parse(
        "{\"version\":3,\"sources\":[\"fib.js\"],\"names\":[],\"mappings\":\";;eAEU,SAAS;;AAFnB,OAAO,CAAC,GAAG,"
        "CAAC,SAAS,CAAC,CAAC;;AAEvB,SAAU,SAAS;MACb,GAAG,EACH,GAAG,EAED,OAAO,EAGP,KAAK;;;;AANP,WAAG,GAAG,CAAC;"
        "AACP,WAAG,GAAG,CAAC;;;aACJ,IAAI;;;;;AACL,eAAO,GAAG,GAAG;;AACjB,WAAG,GAAG,GAAG,CAAC;AACV,WAAG,GAAG,GAAG,"
        "GAAG,OAAO,CAAC;;eACF,OAAO;;;AAArB,aAAK;;AACT,YAAI,KAAK,EAAC;AACN,aAAG,GAAG,CAAC,CAAC;AACR,aAAG,GAAG,CAAC,"
        "CAAC;SACX;;;;;;;;;CAEJ;;AAED,IAAI,QAAQ,GAAG,SAAS,EAAE,CAAC;AAC3B,IAAI,KAAK,GAAG,IAAI,CAAC,KAAK,CAAC,IAAI,"
        "CAAC,MAAM,EAAE,GAAG,EAAE,CAAC,GAAG,CAAC,CAAC;AAC/C,KAAK,IAAI,CAAC,GAAG,CAAC,EAAE,CAAC,GAAG,KAAK,EAAE,EAAE,"
        "CAAC;AAC5B,SAAO,CAAC,GAAG,CAAC,QAAQ,CAAC,IAAI,EAAE,CAAC,KAAK,CAAC,CAAC;CAAA\",\"file\":\"fib-compiled.js\","
        "\"sourcesContent\":[null]}", String());
    EXPECT_FALSE(!sourceMap);
}

TEST(SourceMapTest, ParseInvalid)
{
    const char* sources[] = {
        "",
        "{{}",
        "{\"version\": 42, \"sources\": [\"1.js\"], \"sourcesContent\": [null], \"names\": [], \"mappings\":\";\"}",
        "{\"version\": 3, \"sources\": 42, \"sourcesContent\": [null], \"names\": [], \"mappings\":\";\"}",
        "{\"version\": 3, \"sources\": [null], \"sourcesContent\": [null], \"names\": [], \"mappings\":\";\"}",
        "{\"version\": 3, \"sources\": [], \"sourcesContent\": [null], \"names\": [], \"mappings\":\";\"}"
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(const char*); ++i) {
        OwnPtr<SourceMap> sourceMap = SourceMap::parse(String::fromUTF8(sources[i]), String());
        EXPECT_TRUE(!sourceMap) << sources[i];
    }
}

TEST(SourceMapTest, FindEntry)
{
    OwnPtr<SourceMap> sourceMap = SourceMap::parse(
        "{\"version\":3,\"sources\":[\"fib.js\"],\"names\":[],\"mappings\":\";;eAEU,SAAS;;AAFnB,OAAO,CAAC,GAAG,"
        "CAAC,SAAS,CAAC,CAAC;;AAEvB,SAAU,SAAS;MACb,GAAG,EACH,GAAG,EAED,OAAO,EAGP,KAAK;;;;AANP,WAAG,GAAG,CAAC;AACP,"
        "WAAG,GAAG,CAAC;;;aACJ,IAAI;;;;;AACL,eAAO,GAAG,GAAG;;AACjB,WAAG,GAAG,GAAG,CAAC;AACV,WAAG,GAAG,GAAG,GAAG,"
        "OAAO,CAAC;;eACF,OAAO;;;AAArB,aAAK;;AACT,YAAI,KAAK,EAAC;AACN,aAAG,GAAG,CAAC,CAAC;AACR,aAAG,GAAG,CAAC,CAAC;"
        "SACX;;;;;;;;;CAEJ;;AAED,IAAI,QAAQ,GAAG,SAAS,EAAE,CAAC;AAC3B,IAAI,KAAK,GAAG,IAAI,CAAC,KAAK,CAAC,IAAI,CAAC,"
        "MAAM,EAAE,GAAG,EAAE,CAAC,GAAG,CAAC,CAAC;AAC/C,KAAK,IAAI,CAAC,GAAG,CAAC,EAAE,CAAC,GAAG,KAAK,EAAE,EAAE,CAAC;"
        "AAC5B,SAAO,CAAC,GAAG,CAAC,QAAQ,CAAC,IAAI,EAAE,CAAC,KAAK,CAAC,CAAC;CAAA\",\"file\":\"fib-compiled.js\","
        "\"sourcesContent\":[null],\"sourceRoot\":\"/boo\"}", String("http://localhost/source.map"));

    ASSERT_TRUE(!!sourceMap);

    OwnPtr<SourceMap::Entry> expectedEntries[] = {
        adoptPtr(new SourceMap::Entry(2, 15, "http://localhost/boo/fib.js", 2, 10)), adoptPtr(new SourceMap::Entry(2, 24, "http://localhost/boo/fib.js", 2, 19)),
        adoptPtr(new SourceMap::Entry(4, 0, "http://localhost/boo/fib.js", 0, 0)), adoptPtr(new SourceMap::Entry(4, 7, "http://localhost/boo/fib.js", 0, 7)),
        adoptPtr(new SourceMap::Entry(4, 8, "http://localhost/boo/fib.js", 0, 8)), adoptPtr(new SourceMap::Entry(4, 11, "http://localhost/boo/fib.js", 0, 11)),
        adoptPtr(new SourceMap::Entry(4, 12, "http://localhost/boo/fib.js", 0, 12)), adoptPtr(new SourceMap::Entry(4, 21, "http://localhost/boo/fib.js", 0, 21)),
        adoptPtr(new SourceMap::Entry(4, 22, "http://localhost/boo/fib.js", 0, 22)), adoptPtr(new SourceMap::Entry(4, 23, "http://localhost/boo/fib.js", 0, 23)),
        adoptPtr(new SourceMap::Entry(6, 0, "http://localhost/boo/fib.js", 2, 0)), adoptPtr(new SourceMap::Entry(6, 9, "http://localhost/boo/fib.js", 2, 10)),
        adoptPtr(new SourceMap::Entry(6, 18, "http://localhost/boo/fib.js", 2, 19)), adoptPtr(new SourceMap::Entry(7, 6, "http://localhost/boo/fib.js", 3, 6)),
        adoptPtr(new SourceMap::Entry(7, 9, "http://localhost/boo/fib.js", 3, 9)), adoptPtr(new SourceMap::Entry(7, 11, "http://localhost/boo/fib.js", 4, 6)),
        adoptPtr(new SourceMap::Entry(7, 14, "http://localhost/boo/fib.js", 4, 9)), adoptPtr(new SourceMap::Entry(7, 16, "http://localhost/boo/fib.js", 6, 8)),
        adoptPtr(new SourceMap::Entry(7, 23, "http://localhost/boo/fib.js", 6, 15)), adoptPtr(new SourceMap::Entry(7, 25, "http://localhost/boo/fib.js", 9, 8)),
        adoptPtr(new SourceMap::Entry(7, 30, "http://localhost/boo/fib.js", 9, 13)), adoptPtr(new SourceMap::Entry(11, 0, "http://localhost/boo/fib.js", 3, 6)),
        adoptPtr(new SourceMap::Entry(11, 11, "http://localhost/boo/fib.js", 3, 9)), adoptPtr(new SourceMap::Entry(11, 14, "http://localhost/boo/fib.js", 3, 12)),
        adoptPtr(new SourceMap::Entry(11, 15, "http://localhost/boo/fib.js", 3, 13)), adoptPtr(new SourceMap::Entry(12, 0, "http://localhost/boo/fib.js", 4, 6)),
        adoptPtr(new SourceMap::Entry(12, 11, "http://localhost/boo/fib.js", 4, 9)), adoptPtr(new SourceMap::Entry(12, 14, "http://localhost/boo/fib.js", 4, 12)),
        adoptPtr(new SourceMap::Entry(12, 15, "http://localhost/boo/fib.js", 4, 13)), adoptPtr(new SourceMap::Entry(15, 13, "http://localhost/boo/fib.js", 5, 9)),
        adoptPtr(new SourceMap::Entry(15, 17, "http://localhost/boo/fib.js", 5, 13)), adoptPtr(new SourceMap::Entry(20, 0, "http://localhost/boo/fib.js", 6, 8)),
        adoptPtr(new SourceMap::Entry(20, 15, "http://localhost/boo/fib.js", 6, 15)), adoptPtr(new SourceMap::Entry(20, 18, "http://localhost/boo/fib.js", 6, 18)),
        adoptPtr(new SourceMap::Entry(20, 21, "http://localhost/boo/fib.js", 6, 21)), adoptPtr(new SourceMap::Entry(22, 0, "http://localhost/boo/fib.js", 7, 4)),
        adoptPtr(new SourceMap::Entry(22, 11, "http://localhost/boo/fib.js", 7, 7)), adoptPtr(new SourceMap::Entry(22, 14, "http://localhost/boo/fib.js", 7, 10)),
        adoptPtr(new SourceMap::Entry(22, 17, "http://localhost/boo/fib.js", 7, 13)), adoptPtr(new SourceMap::Entry(23, 0, "http://localhost/boo/fib.js", 8, 4)),
        adoptPtr(new SourceMap::Entry(23, 11, "http://localhost/boo/fib.js", 8, 7)), adoptPtr(new SourceMap::Entry(23, 14, "http://localhost/boo/fib.js", 8, 10)),
        adoptPtr(new SourceMap::Entry(23, 17, "http://localhost/boo/fib.js", 8, 13)), adoptPtr(new SourceMap::Entry(23, 20, "http://localhost/boo/fib.js", 8, 16)),
        adoptPtr(new SourceMap::Entry(23, 27, "http://localhost/boo/fib.js", 8, 23)), adoptPtr(new SourceMap::Entry(23, 28, "http://localhost/boo/fib.js", 8, 24)),
        adoptPtr(new SourceMap::Entry(25, 15, "http://localhost/boo/fib.js", 9, 22)), adoptPtr(new SourceMap::Entry(25, 22, "http://localhost/boo/fib.js", 9, 29)),
        adoptPtr(new SourceMap::Entry(28, 0, "http://localhost/boo/fib.js", 9, 8)), adoptPtr(new SourceMap::Entry(28, 13, "http://localhost/boo/fib.js", 9, 13)),
        adoptPtr(new SourceMap::Entry(30, 0, "http://localhost/boo/fib.js", 10, 4)), adoptPtr(new SourceMap::Entry(30, 12, "http://localhost/boo/fib.js", 10, 8)),
        adoptPtr(new SourceMap::Entry(30, 17, "http://localhost/boo/fib.js", 10, 13)), adoptPtr(new SourceMap::Entry(30, 19, "http://localhost/boo/fib.js", 10, 14)),
        adoptPtr(new SourceMap::Entry(31, 0, "http://localhost/boo/fib.js", 11, 8)), adoptPtr(new SourceMap::Entry(31, 13, "http://localhost/boo/fib.js", 11, 11)),
        adoptPtr(new SourceMap::Entry(31, 16, "http://localhost/boo/fib.js", 11, 14)), adoptPtr(new SourceMap::Entry(31, 17, "http://localhost/boo/fib.js", 11, 15)),
        adoptPtr(new SourceMap::Entry(32, 0, "http://localhost/boo/fib.js", 12, 8)), adoptPtr(new SourceMap::Entry(32, 13, "http://localhost/boo/fib.js", 12, 11)),
        adoptPtr(new SourceMap::Entry(32, 16, "http://localhost/boo/fib.js", 12, 14)), adoptPtr(new SourceMap::Entry(32, 17, "http://localhost/boo/fib.js", 12, 15)),
        adoptPtr(new SourceMap::Entry(32, 18, "http://localhost/boo/fib.js", 12, 16)), adoptPtr(new SourceMap::Entry(33, 9, "http://localhost/boo/fib.js", 13, 5)),
        adoptPtr(new SourceMap::Entry(42, 1, "http://localhost/boo/fib.js", 15, 1)), adoptPtr(new SourceMap::Entry(44, 0, "http://localhost/boo/fib.js", 17, 0)),
        adoptPtr(new SourceMap::Entry(44, 4, "http://localhost/boo/fib.js", 17, 4)), adoptPtr(new SourceMap::Entry(44, 12, "http://localhost/boo/fib.js", 17, 12)),
        adoptPtr(new SourceMap::Entry(44, 15, "http://localhost/boo/fib.js", 17, 15)), adoptPtr(new SourceMap::Entry(44, 24, "http://localhost/boo/fib.js", 17, 24)),
        adoptPtr(new SourceMap::Entry(44, 26, "http://localhost/boo/fib.js", 17, 26)), adoptPtr(new SourceMap::Entry(45, 0, "http://localhost/boo/fib.js", 18, 0)),
        adoptPtr(new SourceMap::Entry(45, 4, "http://localhost/boo/fib.js", 18, 4)), adoptPtr(new SourceMap::Entry(45, 9, "http://localhost/boo/fib.js", 18, 9)),
        adoptPtr(new SourceMap::Entry(45, 12, "http://localhost/boo/fib.js", 18, 12)), adoptPtr(new SourceMap::Entry(45, 16, "http://localhost/boo/fib.js", 18, 16)),
        adoptPtr(new SourceMap::Entry(45, 17, "http://localhost/boo/fib.js", 18, 17)), adoptPtr(new SourceMap::Entry(45, 22, "http://localhost/boo/fib.js", 18, 22)),
        adoptPtr(new SourceMap::Entry(45, 23, "http://localhost/boo/fib.js", 18, 23)), adoptPtr(new SourceMap::Entry(45, 27, "http://localhost/boo/fib.js", 18, 27)),
        adoptPtr(new SourceMap::Entry(45, 28, "http://localhost/boo/fib.js", 18, 28)), adoptPtr(new SourceMap::Entry(45, 34, "http://localhost/boo/fib.js", 18, 34)),
        adoptPtr(new SourceMap::Entry(45, 36, "http://localhost/boo/fib.js", 18, 36)), adoptPtr(new SourceMap::Entry(45, 39, "http://localhost/boo/fib.js", 18, 39)),
        adoptPtr(new SourceMap::Entry(45, 41, "http://localhost/boo/fib.js", 18, 41)), adoptPtr(new SourceMap::Entry(45, 42, "http://localhost/boo/fib.js", 18, 42)),
        adoptPtr(new SourceMap::Entry(45, 45, "http://localhost/boo/fib.js", 18, 45)), adoptPtr(new SourceMap::Entry(45, 46, "http://localhost/boo/fib.js", 18, 46)),
        adoptPtr(new SourceMap::Entry(46, 0, "http://localhost/boo/fib.js", 19, 0)), adoptPtr(new SourceMap::Entry(46, 5, "http://localhost/boo/fib.js", 19, 5)),
        adoptPtr(new SourceMap::Entry(46, 9, "http://localhost/boo/fib.js", 19, 9)), adoptPtr(new SourceMap::Entry(46, 10, "http://localhost/boo/fib.js", 19, 10)),
        adoptPtr(new SourceMap::Entry(46, 13, "http://localhost/boo/fib.js", 19, 13)), adoptPtr(new SourceMap::Entry(46, 14, "http://localhost/boo/fib.js", 19, 14)),
        adoptPtr(new SourceMap::Entry(46, 16, "http://localhost/boo/fib.js", 19, 16)), adoptPtr(new SourceMap::Entry(46, 17, "http://localhost/boo/fib.js", 19, 17)),
        adoptPtr(new SourceMap::Entry(46, 20, "http://localhost/boo/fib.js", 19, 20)), adoptPtr(new SourceMap::Entry(46, 25, "http://localhost/boo/fib.js", 19, 25)),
        adoptPtr(new SourceMap::Entry(46, 27, "http://localhost/boo/fib.js", 19, 27)), adoptPtr(new SourceMap::Entry(46, 29, "http://localhost/boo/fib.js", 19, 29)),
        adoptPtr(new SourceMap::Entry(46, 30, "http://localhost/boo/fib.js", 19, 30)), adoptPtr(new SourceMap::Entry(47, 0, "http://localhost/boo/fib.js", 20, 2)),
        adoptPtr(new SourceMap::Entry(47, 9, "http://localhost/boo/fib.js", 20, 9)), adoptPtr(new SourceMap::Entry(47, 10, "http://localhost/boo/fib.js", 20, 10)),
        adoptPtr(new SourceMap::Entry(47, 13, "http://localhost/boo/fib.js", 20, 13)), adoptPtr(new SourceMap::Entry(47, 14, "http://localhost/boo/fib.js", 20, 14)),
        adoptPtr(new SourceMap::Entry(47, 22, "http://localhost/boo/fib.js", 20, 22)), adoptPtr(new SourceMap::Entry(47, 23, "http://localhost/boo/fib.js", 20, 23)),
        adoptPtr(new SourceMap::Entry(47, 27, "http://localhost/boo/fib.js", 20, 27)), adoptPtr(new SourceMap::Entry(47, 29, "http://localhost/boo/fib.js", 20, 29)),
        adoptPtr(new SourceMap::Entry(47, 30, "http://localhost/boo/fib.js", 20, 30)), adoptPtr(new SourceMap::Entry(47, 35, "http://localhost/boo/fib.js", 20, 35)),
        adoptPtr(new SourceMap::Entry(47, 36, "http://localhost/boo/fib.js", 20, 36)), adoptPtr(new SourceMap::Entry(47, 37, "http://localhost/boo/fib.js", 20, 37)),
        adoptPtr(new SourceMap::Entry(48, 1, "http://localhost/boo/fib.js", 20, 37))
    };
    const int expectedEntriesCount = sizeof(expectedEntries) / sizeof(OwnPtr<SourceMap::Entry>);

    int lineLengths[] = { 13, 1, 55, 1, 23, 1, 22, 31, 64, 54, 13, 16, 16, 1, 13, 20, 29, 16, 9, 1, 22, 1, 18,
        28, 26, 23, 1, 13, 30, 1, 20, 18, 18, 9, 26, 14, 1, 14, 17, 31, 5, 23, 1, 1, 27, 47, 33, 37, 1, 1 };
    const int linesCount = sizeof(lineLengths) / sizeof(int);

    size_t currentEntry = 0;
    for (int line = 0; line < linesCount; ++line) {
        for (int column = 0; column < lineLengths[line]; ++column) {
            const SourceMap::Entry* entry = sourceMap->findEntry(line, column);
            if (line > expectedEntries[0]->line || (line == expectedEntries[0]->line && column >= expectedEntries[0]->column)) {
                ASSERT_FALSE(!entry);
                EXPECT_TRUE(isEqualEntry(entry, expectedEntries[currentEntry]) || (currentEntry + 1 < expectedEntriesCount && isEqualEntry(entry, expectedEntries[++currentEntry])));
            } else {
                EXPECT_TRUE(!entry);
            }
        }
    }
}

TEST(SourceMapTest, ParseIndexed)
{
    const char* source = "{ \"version\" : 3, \"file\": \"app.js\", \"sections\": ["
        "   { \"offset\": {\"line\" : 0, \"column\" : 0}, \"url\": \"url_for_part1.map\" },"
        "   { \"offset\": {\"line\" : 100, \"column\" : 10}, \"map\": {"
        "       \"version\" : 3, \"file\": \"section.js\", \"sources\": [\"foo.js\", \"bar.js\"],"
        "       \"names\" : [\"src\", \"maps\", \"are\", \"fun\"], \"mappings\": \"AAAA,E;;ABCDE;\""
        "   }}]}";
    OwnPtr<SourceMap> sourceMap = SourceMap::parse(String::fromUTF8(source), String());
    ASSERT_FALSE(!sourceMap) << source;
}

TEST(SourceMapTest, FindEntryWithIndexed)
{
    const char* source = "{ \"version\" : 3, \"file\": \"app.js\", \"sections\": ["
        "   { \"offset\": {\"line\" : 0, \"column\" : 0}, \"url\": \"url_for_part1.map\" },"
        "   { \"offset\": {\"line\" : 100, \"column\" : 10}, \"map\": {"
        "       \"version\" : 3, \"file\": \"section.js\", \"sources\": [\"foo.js\", \"bar.js\"],"
        "       \"names\" : [\"src\", \"maps\", \"are\", \"fun\"], \"mappings\": \";;eAEU,SAAS\""
        "   }}]}";
    OwnPtr<SourceMap> sourceMap = SourceMap::parse(String::fromUTF8(source), String());
    ASSERT_FALSE(!sourceMap) << source;

    const SourceMap::Entry* entry = sourceMap->findEntry(102, 24);
    ASSERT_FALSE(!entry);
    EXPECT_TRUE(isEqualEntry(entry, adoptPtr(new SourceMap::Entry(102, 24, "foo.js", 2, 19))));

    entry = sourceMap->findEntry(102, 15);
    ASSERT_FALSE(!entry);
    EXPECT_TRUE(isEqualEntry(entry, adoptPtr(new SourceMap::Entry(102, 15, "foo.js", 2, 10))));

    ASSERT_TRUE(!sourceMap->findEntry(0, 0));
}

} // namespace blink
