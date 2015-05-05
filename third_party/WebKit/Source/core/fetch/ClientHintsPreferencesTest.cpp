// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/fetch/ClientHintsPreferences.h"

#include <gtest/gtest.h>

namespace blink {

class ClientHintsPreferencesTest : public ::testing::Test {
};

TEST_F(ClientHintsPreferencesTest, Basic)
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

    for (auto testCase : cases) {
        ClientHintsPreferences preferences;
        const char* value = testCase.headerValue;
        bool expectationRW = testCase.expectationRW;
        bool expectationDPR = testCase.expectationDPR;

        handleAcceptClientHintsHeader(value, preferences);
        EXPECT_EQ(expectationRW, preferences.shouldSendRW());
        EXPECT_EQ(expectationDPR, preferences.shouldSendDPR());
    }
}

} // namespace blink
