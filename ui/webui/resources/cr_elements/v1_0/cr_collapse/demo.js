// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var button = document.querySelector('button');
var collapse = document.querySelector('#collapse');
button.addEventListener('click', function() {
  collapse.toggle();
});
