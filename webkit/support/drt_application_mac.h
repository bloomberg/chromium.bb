// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H
#define WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H

#include "base/message_pump_mac.h"

@interface CrDrtApplication : NSApplication<CrAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
- (BOOL)isHandlingSendEvent;
@end

#endif  // WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H
