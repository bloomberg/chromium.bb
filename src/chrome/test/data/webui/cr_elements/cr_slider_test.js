// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-slider', function() {
  let element;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-slider min="0" max="100"></cr-slider>
    `;

    element = document.body.querySelector('cr-slider');
  });

  // Ensure that immediate-value-changed event bubbles, since users of
  // cr-slider rely on such event.
  test('immediate-value-changed bubbles', function() {
    const whenFired =
        test_util.eventToPromise('immediate-value-changed', element);
    element.value = 50;
    return whenFired;
  });
});
