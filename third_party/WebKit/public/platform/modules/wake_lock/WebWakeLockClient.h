// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebWakeLockClient_h
#define WebWakeLockClient_h

namespace blink {

class WebWakeLockClient {
public:
    virtual ~WebWakeLockClient() = default;

    virtual void requestKeepScreenAwake(bool) = 0;
};

} // namespace blink

#endif
