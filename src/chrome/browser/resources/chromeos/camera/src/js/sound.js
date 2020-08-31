// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Plays a sound.
 * @param {string} selector Selector of the sound.
 * @return {!Promise} Promise for waiting finishing playing or canceling wait.
 */
export function play(selector) {
  // Use a timeout to wait for sound finishing playing instead of end-event
  // as it might not be played at all (crbug.com/135780).
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  let cancel;
  const p = new Promise((resolve, reject) => {
    const element = document.querySelector(selector);
    const timeout = setTimeout(resolve, Number(element.dataset.timeout || 0));
    cancel = () => {
      clearTimeout(timeout);
      reject(new Error('cancel'));
    };
    element.currentTime = 0;
    element.play();
  });
  p.cancel = cancel;
  return p;
}
