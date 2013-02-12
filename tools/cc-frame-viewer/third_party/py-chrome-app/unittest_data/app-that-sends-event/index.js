/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
chromeapp.sendEvent(
  'hello-world',
  [[1, 2, 3], true],
  function(result) {
    if (result.length != 2)
      throw new Error('Expected array of size 2');
    if (result[0] != 314)
      throw new Error('Expected result[0] to be 314');
    if (result[1] != 'hi')
      throw new Error('Expected result[1] to be "hi"');
    chromeapp.exit(0)
  },
  function() {
    chromeapp.exit(1)
  });
