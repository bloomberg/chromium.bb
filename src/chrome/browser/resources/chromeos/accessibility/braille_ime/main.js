// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The Braille IME object.  Attached to the window object for ease of
 * debugging.
 * @type {BrailleIme}
 */
window.ime = new BrailleIme();
window.ime.init();
