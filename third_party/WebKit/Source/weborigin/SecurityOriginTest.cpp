/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/testing/WTFTestHelpers.h"

#include <gtest/gtest.h>

using WebCore::SecurityOrigin;

namespace {

const int MaxAllowedPort = 65535;

TEST(SecurityOriginTest, InvalidPortsCreateUniqueOrigins)
{
    int ports[] = { -100, -1, MaxAllowedPort + 1, 1000000 };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(ports); ++i) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "example.com", ports[i]);
        EXPECT_TRUE(origin->isUnique()) << "Port " << ports[i] << " should have generated a unique origin.";
    }
}

TEST(SecurityOriginTest, ValidPortsCreateNonUniqueOrigins)
{
    int ports[] = { 0, 80, 443, 5000, MaxAllowedPort };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(ports); ++i) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "example.com", ports[i]);
        EXPECT_FALSE(origin->isUnique()) << "Port " << ports[i] << " should not have generated a unique origin.";
    }
}

} // namespace

