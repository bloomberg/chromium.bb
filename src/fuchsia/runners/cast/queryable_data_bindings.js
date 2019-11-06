// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!cast)
  var cast = new Object;

if (!cast.__platform__)
  cast.__platform__ = new Object;

// Don't clobber the QueryableData API if it was previously injected.
if (!cast.__platform__.queryPlatformValue) {
  cast.__platform__.__queryPlatformValueStore__ = new class {
    mergeValues(values) {
      for (var key in values)
        this.values_[key] = values[key];
    }

    getValue(key) {
      if (!this.values_.hasOwnProperty(key)) {
        console.error('Unknown platformValue: ' + key);
        return null;
      }
      return this.values_[key];
    }

    values_ = {};
  };

  cast.__platform__.queryPlatformValue =
      cast.__platform__.__queryPlatformValueStore__.getValue.bind(
          cast.__platform__.__queryPlatformValueStore__);
}
