// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  if (cr.ui.focusWithoutInk) {
    return;
  }

  let hideInk = false;

  assert(!cr.isIOS, 'pointerdown doesn\'t work on iOS');

  document.addEventListener('pointerdown', function() {
    hideInk = true;
  }, true);

  document.addEventListener('keydown', function() {
    hideInk = false;
  }, true);

  /**
   * Attempts to track whether focus outlines should be shown, and if they
   * shouldn't, removes the "ink" (ripple) from a control while focusing it.
   * This is helpful when a user is clicking/touching, because it's not super
   * helpful to show focus ripples in that case. This is Polymer-specific.
   * @param {!Element} toFocus
   */
  const focusWithoutInk = function(toFocus) {
    // |toFocus| does not have a 'noink' property, so it's unclear whether the
    // element has "ink" and/or whether it can be suppressed. Just focus().
    if (!('noink' in toFocus) || !hideInk) {
      toFocus.focus();
      return;
    }

    // Make sure the element is in the document we're listening to events on.
    assert(document == toFocus.ownerDocument);
    const {noink} = toFocus;
    toFocus.noink = true;
    toFocus.focus();
    toFocus.noink = noink;
  };

  return {focusWithoutInk: focusWithoutInk};
});
