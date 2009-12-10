// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_COREGRAPHICS_PRIVATE_SYMBOLS_MAC_H_
#define WEBKIT_GLUE_PLUGINS_COREGRAPHICS_PRIVATE_SYMBOLS_MAC_H_

// These are CoreGraphics SPI, verified to exist in both 10.5 and 10.6.

#ifdef __cplusplus
extern "C" {
#endif

// Copies the contents of the window with id |wid| into the given rect in the
// given context
OSStatus CGContextCopyWindowCaptureContentsToRect(
    CGContextRef, CGRect, int cid, int wid, int unknown);

// Returns the connection ID we need for the third argument to
// CGContextCopyWindowCaptureContentsToRect
int _CGSDefaultConnection(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBKIT_GLUE_PLUGINS_COREGRAPHICS_PRIVATE_SYMBOLS_MAC_H_
