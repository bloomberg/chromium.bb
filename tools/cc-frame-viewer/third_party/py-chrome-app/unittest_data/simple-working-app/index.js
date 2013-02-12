/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
chromeapp.addEventListener('launch', function(event) {
  if (event.args.length != 1 || event.args[0] != 'hello') {
    chromeapp.exit(0);
    return;
  }
  chromeapp.exit(42);
});
