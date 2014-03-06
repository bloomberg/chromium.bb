// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaQuery.h"

#include "core/css/MediaList.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"

#include <gtest/gtest.h>

const unsigned outputCharArrayLen = 256;

namespace WebCore {

typedef struct {
    const char* input;
    const char* output;
} TestCase;

int getCharArray(String str, char* output)
{
    if (str.isNull())
        return 0;

    unsigned i;
    if (str.is8Bit()) {
        for (i = 0; i < str.length(); ++i)
            output[i] = str.characters8()[i];
    } else {
        for (i = 0; i < str.length(); ++i)
            output[i] = str.characters16()[i];
    }
    output[i++] = 0;
    return i;
}

TEST(MediaQueryParserTest, Basic)
{
    // The first string represents the input string.
    // The second string represents the output string, if present.
    // Otherwise, the output string is identical to the first string.
    TestCase testCases[] = {
        {"screen", 0},
        {"screen and (color)", 0},
        {"all and (min-width:500px)", "(min-width: 500px)"},
        {"(min-width:500px)", "(min-width: 500px)"},
        {"screen and (color), projection and (color)", 0},
        {"not screen and (color)", 0},
        {"only screen and (color)", 0},
        {"screen and (color), projection and (color)", 0},
        {"aural and (device-aspect-ratio: 16/9)", 0},
        {"speech and (min-device-width: 800px)", 0},
        {"example", 0},
        {"screen and (max-weight: 3kg) and (color), (monochrome)", "not all, (monochrome)"},
        {"(min-width: -100px)", "not all"},
        {"(example, all,), speech", "not all, speech"},
        {"&test, screen", "not all, screen"},
        {"print and (min-width: 25cm)", 0},
        {"screen and (min-width: 400px) and (max-width: 700px)", "screen and (max-width: 700px) and (min-width: 400px)"},
        {"screen and (device-width: 800px)", 0},
        {"screen and (device-height: 60em)", 0},
        {"screen and (device-aspect-ratio: 16/9)", 0},
        {"(device-aspect-ratio: 16.0/9.0)", "not all"},
        {"(device-aspect-ratio: 16/ 9)", "(device-aspect-ratio: 16/9)"},
        {"(device-aspect-ratio: 16/\r9)", "(device-aspect-ratio: 16/9)"},
        {"all and (color)", "(color)"},
        {"all and (min-color: 1)", "(min-color: 1)"},
        {"all and (min-color: 1.0)", "not all"},
        {"all and (min-color: 2)", "(min-color: 2)"},
        {"all and (color-index)", "(color-index)"},
        {"all and (min-color-index: 1)", "(min-color-index: 1)"},
        {"all and (monochrome)", "(monochrome)"},
        {"all and (min-monochrome: 1)", "(min-monochrome: 1)"},
        {"all and (min-monochrome: 2)", "(min-monochrome: 2)"},
        {"print and (monochrome)", 0},
        {"handheld and (grid) and (max-width: 15em)", 0},
        {"handheld and (grid) and (max-device-height: 7em)", 0},
        {"screen and (max-width: 50%)", "not all"},
        {"screen and (max-WIDTH: 500px)", "screen and (max-width: 500px)"},
        {"screen and (max-width: 24.4em)", 0},
        {"screen and (max-width: 24.4EM)", "screen and (max-width: 24.4em)"},
        {"screen and (max-width: blabla)", "not all"},
        {"screen and (max-width: 1)", "not all"},
        {"screen and (max-width: 0)", 0},
        {"screen and (max-width: 1deg)", "not all"},
        {"handheld and (min-width: 20em), \nscreen and (min-width: 20em)", "handheld and (min-width: 20em), screen and (min-width: 20em)"},
        {"print and (min-resolution: 300dpi)", 0},
        {"print and (min-resolution: 118dpcm)", 0},
        {"(resolution: 0.83333333333333333333dppx)", "(resolution: 0.8333333333333334dppx)"},
        {"(resolution: 2.4dppx)", 0},
        {"all and(color)", "not all"},
        {"all and (", "not all"},
        {"test;,all", "not all, all"},
        // {"(color:20example)", "not all"}, // BisonCSSParser fails to parse that MQ, producing an infinitesimal float.
        {"not braille", 0},
        {",screen", "not all, screen"},
        {",all", "not all, all"},
        {",,all,,", "not all, not all, all, not all, not all"},
        {",,all,, ", "not all, not all, all, not all, not all"},
        {",screen,,&invalid,,", "not all, screen, not all, not all, not all, not all"},
        {",screen,,(invalid,),,", "not all, screen, not all, not all, not all, not all"},
        {",(all,),,", "not all, not all, not all, not all"},
        {",", "not all, not all"},
        // {"  ", ""}, // BisonCSSParser fails to parse that MQ, producing "not all, not all".
        {"(color", "(color)"},
        {"(min-color: 2", "(min-color: 2)"},
        {"(orientation: portrait)", 0},
        {"tv and (scan: progressive)", 0},
        {"(pointer: coarse)", 0},
        {"(min-orientation:portrait)", "not all"},
        {"all and (orientation:portrait)", "(orientation: portrait)"},
        {"all and (orientation:landscape)", "(orientation: landscape)"},
        {"NOT braille, tv AND (max-width: 200px) and (min-WIDTH: 100px) and (orientation: landscape), (color)",
            "not braille, tv and (max-width: 200px) and (min-width: 100px) and (orientation: landscape), (color)"},
        {0, 0} // Do not remove the terminator line.
    };

    for (unsigned i = 0; testCases[i].input; ++i) {
        RefPtr<MediaQuerySet> querySet = MediaQuerySet::create(testCases[i].input);
        StringBuilder output;
        char outputCharArray[outputCharArrayLen];
        size_t j = 0;
        while (j < querySet->queryVector().size()) {
            String queryText = querySet->queryVector()[j]->cssText();
            output.append(queryText);
            ++j;
            if (j >= querySet->queryVector().size())
                break;
            output.append(", ");
        }
        ASSERT(output.length() < outputCharArrayLen);
        getCharArray(output.toString(), outputCharArray);
        if (testCases[i].output)
            ASSERT_STREQ(testCases[i].output, outputCharArray);
        else
            ASSERT_STREQ(testCases[i].input, outputCharArray);
    }
}

} // namespace
