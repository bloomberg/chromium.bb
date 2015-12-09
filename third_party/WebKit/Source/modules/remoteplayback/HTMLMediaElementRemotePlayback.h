// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementRemotePlayback_h
#define HTMLMediaElementRemotePlayback_h

namespace blink {

class HTMLMediaElement;
class QualifiedName;

// Class used to implement the Remote Playback API. It will be a supplement to
// HTMLMediaElement later.
class HTMLMediaElementRemotePlayback final {
public:
    static bool fastHasAttribute(const QualifiedName&, const HTMLMediaElement&);
    static void setBooleanAttribute(const QualifiedName&, HTMLMediaElement&, bool);
};

} // namespace blink

#endif // HTMLMediaElementRemotePlayback_h
