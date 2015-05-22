// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PagePopupClient.h"

#include <gtest/gtest.h>
#include <string>

namespace blink {

TEST(PagePopupClientTest, AddJavaScriptString)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create();
    PagePopupClient::addJavaScriptString("abc\r\n'\"</script>", buffer.get());
    EXPECT_EQ("\"abc\\r\\n'\\\"\\x3C/script>\"", std::string(buffer->data(), buffer->size()));
}

} // namespace blink

