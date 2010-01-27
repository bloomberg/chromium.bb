/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client stub for audio-visual initialization.
 */

/*
 * This stub provides a definition for the function that waits for the browser
 * to set up the audio-visual buffers.  Since it is declared as a weak symbol,
 * it is overridden if libav is used.
 */
void __av_wait() __attribute__((weak));

void __av_wait() {
}
