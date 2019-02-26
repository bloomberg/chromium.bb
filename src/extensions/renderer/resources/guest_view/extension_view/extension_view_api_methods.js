// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the public-facing API functions for the
// <extensionview> tag.

var EXTENSION_VIEW_API_METHODS = [
  // Loads the given src into extensionview. Must be called every time the
  // the extensionview should load a new page. This is the only way to set
  // the extension and src attributes. Returns a promise indicating whether
  // or not load was successful.
  'load'
];

// Exports.
exports.$set('EXTENSION_VIEW_API_METHODS', EXTENSION_VIEW_API_METHODS);
