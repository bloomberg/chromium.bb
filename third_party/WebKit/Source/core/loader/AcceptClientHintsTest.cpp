// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/AcceptClientHints.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"

#include <gtest/gtest.h>

namespace blink {

class AcceptClientHintsTest : public ::testing::Test {
};

TEST_F(AcceptClientHintsTest, Basic)
{
    struct TestCase {
        const char* headerValue;
        bool expectationRW;
        bool expectationDPR;
    } cases[] = {
        {"rw, dpr", true, true},
        {"Rw, dPr", true, true},
        {"RW, DPR", true, true},
        {"dprw", false, false},
        {"DPRW", false, false},
    };

    for (size_t i = 0; i < arraysize(cases); ++i) {
        ClientHintsPreferences preferences;
        const char* value = cases[i].headerValue;
        bool expectationRW = cases[i].expectationRW;
        bool expectationDPR = cases[i].expectationDPR;

        handleAcceptClientHintsHeader(value, preferences);
        EXPECT_EQ(expectationRW, preferences.shouldSendRW());
        EXPECT_EQ(expectationDPR, preferences.shouldSendDPR());
    }
}

} // namespace blink
