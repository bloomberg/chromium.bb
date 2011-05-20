// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = {};

function setItem(key, value) {
  window.localStorage.setItem(key, value);
}

function getItem(key, defaultValue) {
  var result = window.localStorage.getItem(key);
  return (result != null) ? result : defaultValue;
}

function removeItem(key) {
  window.localStorage.removeItem(key);
}

function clearAll() {
  window.localStorage.clear();
}
