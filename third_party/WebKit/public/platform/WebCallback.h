// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCallback_h
#define WebCallback_h

namespace blink {

class WebCallback {
public:
    virtual ~WebCallback() { };
    virtual void run() { };
};

} // namespace blink

#endif // WebCallback_h
