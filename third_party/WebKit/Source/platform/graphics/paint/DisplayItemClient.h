// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClient_h
#define DisplayItemClient_h

namespace blink {

class DisplayItemClientInternalVoid;
typedef const DisplayItemClientInternalVoid* DisplayItemClient;

inline DisplayItemClient toDisplayItemClient(const void* object) { return static_cast<DisplayItemClient>(object); }

}

#endif // DisplayItemClient_h
