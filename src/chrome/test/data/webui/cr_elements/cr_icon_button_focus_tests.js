// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-icon-button-focus-tests', function() {
  let button;

  setup(async () => {
    PolymerTest.clearBody();
    button = document.createElement('cr-icon-button');
    document.body.appendChild(button);
    await PolymerTest.flushTasks();
  });

  test('focus shows ripple', () => {
    button.focus();
    assertTrue(button.getRipple().holdDown);
    button.blur();
    assertFalse(button.getRipple().holdDown);
  });

  test('when disabled, focus does not show ripple', () => {
    button.disabled = true;
    button.focus();
    assertFalse(button.getRipple().holdDown);
    button.blur();
    button.disabled = false;
    button.focus();
    assertTrue(button.getRipple().holdDown);
    // Settings |disabled| to true does remove an existing ripple.
    button.disabled = true;
    assertFalse(button.getRipple().holdDown);
  });

  test('when noink, focus does not show ripple', () => {
    button.noink = true;
    button.focus();
    assertFalse(button.getRipple().holdDown);
    button.blur();
    button.noink = false;
    button.focus();
    assertTrue(button.getRipple().holdDown);
    // Setting |noink| to true does not remove an existing ripple.
    button.noink = true;
    assertTrue(button.getRipple().holdDown);
  });
});
