// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H
#define WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H

#include "base/message_pump_mac.h"
#include "base/mac/scoped_sending_event.h"

@interface CrDrtApplication : NSApplication<CrAppProtocol,
                                            CrAppControlProtocol> {
 @private
  BOOL handlingSendEvent_;
}
// CrAppProtocol
- (BOOL)isHandlingSendEvent;

// CrAppControlProtocol
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;
@end

#endif  // WEBKIT_SUPPORT_DRT_APPLICATION_MAC_H
