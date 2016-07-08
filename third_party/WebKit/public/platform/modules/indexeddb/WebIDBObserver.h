// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class makes the blink::IDBObserver visible to the content layer by holding a reference to it.

#ifndef WebIDBObserver_h
#define WebIDBObserver_h

namespace blink {

class WebIDBObserver {
public:
    virtual ~WebIDBObserver() {}
};

} // namespace blink

#endif // WebIDBObserver_h
