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
#include "wtf/SHA1.h"

#include "wtf/StringExtras.h"

#include <gtest/gtest.h>

namespace {

class SHA1Test : public testing::Test {
    char m_buffer[41];

protected:
    const char* SHA1HexDigest(const char* input, int repeat)
    {
        SHA1 sha1;
        for (int i = 0; i < repeat; i++)
            sha1.addBytes(input);
        Vector<uint8_t, 20> digest;
        sha1.computeHash(digest);

        char* buffer = &m_buffer[0];
        for (size_t i = 0; i < 20; ++i) {
            snprintf(buffer, 3, "%02X", digest.at(i));
            buffer += 2;
        }
        return &m_buffer[0];
    }
};

TEST_F(SHA1Test, RFC3174)
{
    // Examples taken from sample code in RFC 3174.
    EXPECT_STREQ("A9993E364706816ABA3E25717850C26C9CD0D89D", SHA1HexDigest("abc", 1));
    EXPECT_STREQ("84983E441C3BD26EBAAE4AA1F95129E5E54670F1", SHA1HexDigest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1));
    EXPECT_STREQ("34AA973CD4C4DAA4F61EEB2BDBAD27316534016F", SHA1HexDigest("a", 1000000));
    EXPECT_STREQ("DEA356A2CDDD90C7A7ECEDC5EBB563934F460452", SHA1HexDigest("0123456701234567012345670123456701234567012345670123456701234567", 10));
}

} // namespace
