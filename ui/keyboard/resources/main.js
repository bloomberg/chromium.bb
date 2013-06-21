// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onResize = function() {
  var x = window.innerWidth;
  var y = window.innerHeight;
  var height =  (x > ASPECT_RATIO * y) ? y : Math.floor(x / ASPECT_RATIO);
  keyboard.style.height = height + 'px';
  keyboard.style.width = Math.floor(ASPECT_RATIO * height) + 'px';
  keyboard.style.fontSize = (height / FONT_SIZE_RATIO / ROW_LENGTH) + 'px';
};

addEventListener('WebComponentsReady', function() {
  keyboard.appendChild(
      keysets.content.body.firstElementChild.createInstance());
});

addEventListener('resize', onResize);

addEventListener('load', onResize);
