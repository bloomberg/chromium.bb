/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

function onLoad() {
  var parentDiv = document.getElementById('key-log');
  var chordTracker = new ChordTracker(parentDiv);
  document.body.addEventListener(
      'keydown', chordTracker.addKeyEvent.bind(chordTracker), false);
  document.body.addEventListener(
      'keyup', chordTracker.addKeyEvent.bind(chordTracker), false);
  window.addEventListener(
      'blur', chordTracker.releaseAllKeys.bind(chordTracker), false);
  document.getElementById('clear-log').addEventListener(
      'click', function() { parentDiv.innerText = ''; }, false);
}

window.addEventListener('load', onLoad, false);
