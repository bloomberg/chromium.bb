// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/**
 * @constructor
 * @extends {SwitchAccessE2ETest}
 */
function SwitchAccessDesktopNodeTest() {
  SwitchAccessE2ETest.call(this);
}

SwitchAccessDesktopNodeTest.prototype = {
  __proto__: SwitchAccessE2ETest.prototype,
};

TEST_F('SwitchAccessDesktopNodeTest', 'Build', function() {
  this.runWithLoadedTree('', (desktop) => {
    const desktopNode = DesktopNode.build(desktop);

    const children = desktopNode.children;
    for (let i = 0; i < children.length; i++) {
      const child = children[i];
      // The desktop tree should not include a back button.
      assertFalse(child instanceof BackButtonNode);

      // Check that the children form a loop.
      const next = children[(i + 1) % children.length];
      assertEquals(
          next, child.next, 'next not properly initialized on child ' + i);
      // We add children.length to ensure the value is greater than zero.
      const previous = children[(i - 1 + children.length) % children.length];
      assertEquals(
          previous, child.previous,
          'previous not properly initialized on child ' + i);
    }
  });
});
