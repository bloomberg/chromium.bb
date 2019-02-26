// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_LEGACY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class CommandDispatcher;

// QRScannerLegacyCoordinator presents the public interface for the QR scanner
// feature.
@interface QRScannerLegacyCoordinator : ChromeCoordinator

// Models.
@property(nonatomic, readwrite, weak) CommandDispatcher* dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_LEGACY_COORDINATOR_H_
