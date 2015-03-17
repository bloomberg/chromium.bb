// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebMessagePortChannelClient.h"

namespace blink {

// The following definitions should not be in WebMessagePortChannelClient.h,
// because when define as inline a function declared with the
// dllimport attribute, the function can be expanded but never
// instantiated. The rule apply to inline functions whose definitions appear
// within a class definition.
// c.f. https://msdn.microsoft.com/en-us/library/xa0d9ste.aspx
WebMessagePortChannelClient::WebMessagePortChannelClient()
{
}

WebMessagePortChannelClient::~WebMessagePortChannelClient()
{
}

} // namespace blink
