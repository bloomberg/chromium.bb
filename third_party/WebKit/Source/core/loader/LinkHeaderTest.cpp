// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/LinkHeader.h"

#include <base/macros.h>
#include <gtest/gtest.h>

namespace blink {

TEST(LinkHeaderTest, Basic)
{
    struct TestCase {
        const char* headerValue;
        const char* url;
        const char* rel;
        bool valid;
    } cases[] = {
        {0, "", "", false},
        {"", "", "", false},
        {"</images/cat.jpg>; rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>;rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>   ;rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>   ;   rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"< /images/cat.jpg>   ;   rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg >   ;   rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg wutwut>   ;   rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg wutwut  \t >   ;   rel=prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>; rel=prefetch   ", "/images/cat.jpg", "prefetch", true},
        {"  </images/cat.jpg>; rel=prefetch   ", "/images/cat.jpg", "prefetch", true},
        {"\t  </images/cat.jpg>; rel=prefetch   ", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>\t\t ; \trel=prefetch \t  ", "/images/cat.jpg", "prefetch", true},
        {"\f</images/cat.jpg>\t\t ; \trel=prefetch \t  ", "", "", false},
        {"</images/cat.jpg>; rel= prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>; rel =prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>; rel pel=prefetch", "/images/cat.jpg", "", false},
        {"< /images/cat.jpg>", "/images/cat.jpg", "", true},
        {"</images/cat.jpg>; rel =", "/images/cat.jpg", "", false},
        {"</images/cat.jpg>; wut=sup; rel =prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>; wut=sup ; rel =prefetch", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;", "/images/cat.jpg", "prefetch", true},
        {"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;", "/images/cat.jpg", "", false},
        {"<   /images/cat.jpg", "", "", false},
        {"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch", "http://wut.com/", "dns-prefetch", true},
        {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch", "http://wut.com/%20%20%3dsdfsdf?sd", "dns-prefetch", true},
        {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch", "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", true},
        {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch", "http://wut.com/dfsdf?sdf=ghj&wer=rty", "", false},
    };

    for (size_t i = 0; i < arraysize(cases); ++i) {
        const char* headerValue = cases[i].headerValue;
        const char* url = cases[i].url;
        const char* rel = cases[i].rel;
        bool valid = cases[i].valid;

        LinkHeader header(headerValue);
        ASSERT_EQ(valid, header.valid());
        ASSERT_STREQ(url, header.url().ascii().data());
        ASSERT_STREQ(rel, header.rel().ascii().data());
    }
}

} // namespace blink
