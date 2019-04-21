// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_QR_SCANNER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_QR_SCANNER_COMMANDS_H_

// QRScannerCommands contains commands related to scanning QR codes.
@protocol QRScannerCommands<NSObject>

// Shows the QR scanner UI.
- (void)showQRScanner;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_QR_SCANNER_COMMANDS_H_
