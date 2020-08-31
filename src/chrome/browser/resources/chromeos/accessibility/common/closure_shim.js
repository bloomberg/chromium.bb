// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Provides a shim to allow select-to-speak to use closure files.
 */

var goog = {};

goog.provide = function(n) {
  window[n] = {};
};

goog.require = function() {};

goog.scope = function(c) {
  c();
};
