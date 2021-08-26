// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js', 'test_utility.js']);

/** Test fixture for the focus ring manager. */
SwitchAccessFocusRingManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
      await importModule(
          'FocusRingManager', '/switch_access/focus_ring_manager.js');
      await importModule(
          'BackButtonNode', '/switch_access/nodes/back_button_node.js');
      await importModule('Navigator', '/switch_access/navigator.js');
      await importModule(
          'SAConstants', '/switch_access/switch_access_constants.js');
      await importModule('ActionManager', '/switch_access/action_manager.js');

      await TestUtility.setup();
      runTest();
    })();
  }
};

TEST_F('SwitchAccessFocusRingManagerTest', 'BackButtonFocus', function() {
  this.runWithLoadedDesktop((desktop) => {
    // Focus the back button.
    Navigator.byItem.moveTo_(
        desktop.find({role: chrome.automation.RoleType.TAB}));
    BackButtonNode
        .locationForTesting = {top: 10, left: 10, width: 10, height: 10};

    TestUtility.pressSelectSwitch();
    TestUtility.pressNextSwitch();
    TestUtility.pressNextSwitch();
    assertTrue(
        Navigator.byItem.node_ instanceof BackButtonNode,
        'Third node should be a BackButtonNode');

    const rings = FocusRingManager.instance.rings_;
    const primary = rings.get(SAConstants.Focus.ID.PRIMARY);
    const preview = rings.get(SAConstants.Focus.ID.PREVIEW);
    assertEquals(SAConstants.Focus.ID.PRIMARY, primary.id);
    assertEquals(SAConstants.Focus.ID.PREVIEW, preview.id);
    assertEquals('solid', primary.type);
    assertEquals('dashed', preview.type);

    // Primary focus should be empty, preview focus should contain one element.
    assertEquals(0, primary.rects.length);
    assertEquals(1, preview.rects.length);
  });
});

TEST_F(
    'SwitchAccessFocusRingManagerTest', 'BackButtonForMenuFocus', function() {
      const site = '<input type="text">';
      this.runWithLoadedTree(site, async (rootWebArea) => {
        // Open the menu and focus the back button.
        TestUtility.startFocusInside(rootWebArea);
        // Check the current node directly, as the TestUtility relies on the
        // FocusManager to identify the current focus.
        assertEquals(
            chrome.automation.RoleType.TEXT_FIELD, Navigator.byItem.node_.role);
        TestUtility.pressSelectSwitch();

        let found = false;
        while (!found) {
          TestUtility.pressNextSwitch();
          if (Navigator.byItem.node_ instanceof BackButtonNode) {
            found = true;
          }
        }

        const rings = FocusRingManager.instance.rings_;
        const primary = rings.get(SAConstants.Focus.ID.PRIMARY);
        const preview = rings.get(SAConstants.Focus.ID.PREVIEW);
        // Primary and preview focus should be empty.
        assertEquals(0, primary.rects.length);
        assertEquals(0, preview.rects.length);
      });
    });

TEST_F('SwitchAccessFocusRingManagerTest', 'ButtonFocus', function() {
  const site = '<button>Test</button>';
  this.runWithLoadedTree(site, async (rootWebArea) => {
    const button = rootWebArea.find({role: chrome.automation.RoleType.BUTTON});
    Navigator.byItem.moveTo_(button);

    const rings = FocusRingManager.instance.rings_;
    const primary = rings.get(SAConstants.Focus.ID.PRIMARY);
    const preview = rings.get(SAConstants.Focus.ID.PREVIEW);
    assertEquals(1, primary.rects.length);
    assertEquals(0, preview.rects.length);
    // Primary focus should be on the button.
    const focusLocation = primary.rects[0];
    const buttonLocation = button.location;
    assertTrue(RectUtil.equal(buttonLocation, focusLocation));
  });
});

TEST_F('SwitchAccessFocusRingManagerTest', 'GroupFocus', function() {
  const site = `
    <div role="menu">
      <div role="menuitem">Dog</div>
      <div role="menuitem">Cat</div>
    </div>
  `;
  this.runWithLoadedTree(site, async (rootWebArea) => {
    const menu = rootWebArea.find({role: chrome.automation.RoleType.MENU});
    const menuItem = rootWebArea.find({
      role: chrome.automation.RoleType.MENU_ITEM,
      attributes: {name: 'Dog'}
    });
    assertNotNullNorUndefined(menu);
    assertNotNullNorUndefined(menuItem);
    Navigator.byItem.moveTo_(menu);

    // Verify the number of rings.
    const rings = FocusRingManager.instance.rings_;
    const primary = rings.get(SAConstants.Focus.ID.PRIMARY);
    const preview = rings.get(SAConstants.Focus.ID.PREVIEW);
    assertEquals(1, primary.rects.length);
    assertEquals(1, preview.rects.length);

    // Use ringNodesForTesting_ to verify the underlying nodes.
    const ringNodes = FocusRingManager.instance.ringNodesForTesting_;
    const primaryNode =
        ringNodes.get(SAConstants.Focus.ID.PRIMARY).automationNode;
    const previewNode =
        ringNodes.get(SAConstants.Focus.ID.PREVIEW).automationNode;

    assertEquals(
        menu, primaryNode,
        'primary focus should be around the group (the menu)');
    assertEquals(
        menuItem, previewNode, 'preview focus should be around the menu item');
  });
});
