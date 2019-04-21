// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

/**
 * This interface is used to communicate the results of an image decoding
 * request.
 */
interface IDecoderServiceCallback {
 /**
  * Called when decoding is done.
  * @param payload The results of the image decoding request, including the
  *                decoded bitmap.
  */
  oneway void onDecodeImageDone(in Bundle payload);
}
