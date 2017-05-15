// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaDeviceChangeObserver_h
#define WebMediaDeviceChangeObserver_h

#include "public/platform/WebPrivatePtr.h"

namespace blink {

class MediaDevices;
class WebSecurityOrigin;

class WebMediaDeviceChangeObserver {
 public:
  BLINK_EXPORT WebMediaDeviceChangeObserver();
  BLINK_EXPORT explicit WebMediaDeviceChangeObserver(bool);  // for testing only
  BLINK_EXPORT WebMediaDeviceChangeObserver(
      const WebMediaDeviceChangeObserver&);
  BLINK_EXPORT WebMediaDeviceChangeObserver& operator=(
      const WebMediaDeviceChangeObserver&);
  BLINK_EXPORT ~WebMediaDeviceChangeObserver();

  // Notify that the set of media devices has changed.
  BLINK_EXPORT void DidChangeMediaDevices();
  BLINK_EXPORT bool IsNull() const;
  BLINK_EXPORT WebSecurityOrigin GetSecurityOrigin() const;

#if INSIDE_BLINK
  BLINK_EXPORT explicit WebMediaDeviceChangeObserver(MediaDevices*);
#endif
 private:
  void Assign(const WebMediaDeviceChangeObserver&);
  void Reset();
  WebPrivatePtr<MediaDevices> private_;
};

}  // namespace blink

#endif
