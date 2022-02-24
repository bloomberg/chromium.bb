// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestURLSelectionOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('mockResult')) {
      return data['mockResult'];
    }

    return -1;
  }
}

registerURLSelectionOperation("test-url-selection-operation", TestURLSelectionOperation);
