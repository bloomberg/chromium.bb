// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the item scan manager. */
SwitchAccessItemScanManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
      await importModule(
          'BackButtonNode', '/switch_access/nodes/back_button_node.js');
      await importModule(
          ['BasicNode', 'BasicRootNode'], '/switch_access/nodes/basic_node.js');
      await importModule(
          ['KeyboardNode', 'KeyboardRootNode'],
          '/switch_access/nodes/keyboard_node.js');
      await importModule(
          'ItemScanManager', '/switch_access/item_scan_manager.js');
      await importModule('SACache', '/switch_access/cache.js');
      await importModule(
          'SwitchAccessMenuAction',
          '/switch_access/switch_access_constants.js');
      await importModule(
          'SwitchAccessPredicate', '/switch_access/switch_access_predicate.js');
      await importModule('Navigator', '/switch_access/navigator.js');
      BackButtonNode
          .locationForTesting = {top: 10, left: 10, width: 20, height: 20};

      runTest();
    })();
  }

  moveToPageContents(pageContents) {
    const cache = new SACache();
    if (!SwitchAccessPredicate.isGroup(pageContents, null, cache)) {
      pageContents =
          new AutomationTreeWalker(pageContents, constants.Dir.FORWARD, {
            visit: (node) => SwitchAccessPredicate.isGroup(node, null, cache)
          })
              .next()
              .node;
    }
    assertNotNullNorUndefined(
        pageContents, 'Could not find group corresponding to page contents');
    Navigator.byItem.moveTo_(pageContents);
    Navigator.byItem.enterGroup();
  }
};

function currentNode() {
  return Navigator.byItem.node_;
}

TEST_F('SwitchAccessItemScanManagerTest', 'MoveTo', function() {
  const website = `<div id="outerGroup">
                     <div id="group">
                       <input type="text">
                       <input type="range">
                     </div>
                     <button></button>
                   </div>`;
  this.runWithLoadedTree(website, (rootWebArea) => {
    const desktop = rootWebArea.parent.root;
    const textFields =
        desktop.findAll({role: chrome.automation.RoleType.TEXT_FIELD});
    assertEquals(2, textFields.length, 'Should be exactly 2 text fields.');
    const omnibar = textFields[0];
    const textInput = textFields[1];
    const sliders = desktop.findAll({role: chrome.automation.RoleType.SLIDER});
    assertEquals(1, sliders.length, 'Should be exactly 1 slider.');
    const slider = sliders[0];
    const group = this.findNodeById('group');
    const outerGroup = this.findNodeById('outerGroup');

    Navigator.byItem.moveTo_(omnibar);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, Navigator.byItem.node_.role,
        'Did not successfully move to the omnibar');
    assertFalse(
        Navigator.byItem.group_.isEquivalentTo(group),
        'Omnibar is in the group in page contents somehow');
    assertFalse(
        Navigator.byItem.group_.isEquivalentTo(outerGroup),
        'Omnibar is in the outer group in page contents somehow');
    const grandGroup = Navigator.byItem.history_.peek().group;
    assertFalse(
        grandGroup.isEquivalentTo(group),
        'Group stack contains the group from page contents');
    assertFalse(
        grandGroup.isEquivalentTo(outerGroup),
        'Group stack contains the outer group from page contents');

    Navigator.byItem.moveTo_(textInput);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, Navigator.byItem.node_.role,
        'Did not successfully move to the text input');
    assertTrue(
        Navigator.byItem.group_.isEquivalentTo(group),
        'Group node was not successfully populated');
    assertTrue(
        Navigator.byItem.history_.peek().group.isEquivalentTo(outerGroup),
        'History was not built properly');

    Navigator.byItem.moveTo_(slider);
    assertEquals(
        chrome.automation.RoleType.SLIDER, Navigator.byItem.node_.role,
        'Did not successfully move to the slider');

    Navigator.byItem.moveTo_(group);
    assertTrue(Navigator.byItem.node_.isGroup(), 'Current node is not a group');
    assertTrue(
        Navigator.byItem.node_.isEquivalentTo(group),
        'Did not find the right group');
  });
});

TEST_F('SwitchAccessItemScanManagerTest', 'JumpTo', function() {
  const website = `<div id="group1">
                     <input type="text">
                     <button></button>
                   </div>
                   <div id="group2">
                     <button></button>
                     <button></button>
                   </div>`;
  this.runWithLoadedTree(website, (rootWebArea) => {
    const desktop = rootWebArea.parent.root;
    const textInput =
        desktop.findAll({role: chrome.automation.RoleType.TEXT_FIELD})[1];
    assertNotNullNorUndefined(textInput, 'Text field is undefined');
    const group1 = this.findNodeById('group1');
    const group2 = this.findNodeById('group2');

    Navigator.byItem.moveTo_(textInput);
    const textInputNode = Navigator.byItem.node_;
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, textInputNode.role,
        'Did not successfully move to the text input');
    assertTrue(
        Navigator.byItem.group_.isEquivalentTo(group1),
        'Group is initialized in an unexpected manner');

    Navigator.byItem.jumpTo_(BasicRootNode.buildTree(group2));
    assertFalse(
        Navigator.byItem.group_.isEquivalentTo(group1), 'Jump did nothing');
    assertTrue(
        Navigator.byItem.group_.isEquivalentTo(group2),
        'Jumped to the wrong group');

    Navigator.byItem.exitGroup_();
    assertTrue(
        Navigator.byItem.group_.isEquivalentTo(group1),
        'Did not jump back to the right group.');
  });
});

TEST_F('SwitchAccessItemScanManagerTest', 'SelectButton', function() {
  const website = `<button id="test" aria-pressed=false>First Button</button>
      <button>Second Button</button>
      <script>
        let state = false;
        let button = document.getElementById("test");
        button.onclick = () => {
          state = !state;
          button.setAttribute("aria-pressed", state);
        };
      </script>`;

  this.runWithLoadedTree(website, function(pageContents) {
    this.moveToPageContents(pageContents);

    const node = currentNode().automationNode;
    assertNotNullNorUndefined(node, 'Node is invalid');
    assertEquals(node.name, 'First Button', 'Did not find the right node');

    node.addEventListener(
        chrome.automation.EventType.CHECKED_STATE_CHANGED,
        this.newCallback((event) => {
          assertEquals(
              node.name, event.target.name,
              'Checked state changed on unexpected node');
        }));

    Navigator.byItem.node_.performAction('select');
  });
});

TEST_F('SwitchAccessItemScanManagerTest', 'EnterGroup', function() {
  const website = `<div id="group">
                     <button></button>
                     <button></button>
                   </div>
                   <input type="range">`;
  this.runWithLoadedTree(website, (rootWebArea) => {
    const targetGroup = this.findNodeById('group');
    Navigator.byItem.moveTo_(targetGroup);

    const originalGroup = Navigator.byItem.group_;
    assertEquals(
        Navigator.byItem.node_.automationNode.htmlAttributes.id, 'group',
        'Did not move to group properly');

    Navigator.byItem.enterGroup();
    assertEquals(
        chrome.automation.RoleType.BUTTON, Navigator.byItem.node_.role,
        'Current node is not a button');
    assertTrue(
        Navigator.byItem.group_.isEquivalentTo(targetGroup),
        'Target group was not entered');

    Navigator.byItem.exitGroup_();
    assertTrue(
        originalGroup.equals(Navigator.byItem.group_),
        'Did not move back to the original group');
  });
});

TEST_F('SwitchAccessItemScanManagerTest', 'MoveForward', function() {
  const website = `<div>
                     <button id="button1"></button>
                     <button id="button2"></button>
                     <button id="button3"></button>
                   </div>`;
  this.runWithLoadedTree(website, (rootWebArea) => {
    Navigator.byItem.moveTo_(this.findNodeById('button1'));
    const button1 = Navigator.byItem.node_;
    assertFalse(
        button1 instanceof BackButtonNode,
        'button1 should not be a BackButtonNode');
    assertEquals(
        'button1', button1.automationNode.htmlAttributes.id,
        'Current node is not button1');

    Navigator.byItem.moveForward();
    assertFalse(
        button1.equals(Navigator.byItem.node_),
        'Still on button1 after moveForward()');
    const button2 = Navigator.byItem.node_;
    assertFalse(
        button2 instanceof BackButtonNode,
        'button2 should not be a BackButtonNode');
    assertEquals(
        'button2', button2.automationNode.htmlAttributes.id,
        'Current node is not button2');

    Navigator.byItem.moveForward();
    assertFalse(
        button1.equals(Navigator.byItem.node_),
        'Unexpected navigation to button1');
    assertFalse(
        button2.equals(Navigator.byItem.node_),
        'Still on button2 after moveForward()');
    const button3 = Navigator.byItem.node_;
    assertFalse(
        button3 instanceof BackButtonNode,
        'button3 should not be a BackButtonNode');
    assertEquals(
        'button3', button3.automationNode.htmlAttributes.id,
        'Current node is not button3');

    Navigator.byItem.moveForward();
    assertTrue(
        Navigator.byItem.node_ instanceof BackButtonNode,
        'BackButtonNode should come after button3');

    Navigator.byItem.moveForward();
    assertTrue(
        button1.equals(Navigator.byItem.node_),
        'button1 should come after the BackButtonNode');
  });
});

TEST_F('SwitchAccessItemScanManagerTest', 'MoveBackward', function() {
  const website = `<div>
                     <button id="button1"></button>
                     <button id="button2"></button>
                     <button id="button3"></button>
                   </div>`;
  this.runWithLoadedTree(website, (rootWebArea) => {
    Navigator.byItem.moveTo_(this.findNodeById('button1'));
    const button1 = Navigator.byItem.node_;
    assertFalse(
        button1 instanceof BackButtonNode,
        'button1 should not be a BackButtonNode');
    assertEquals(
        'button1', button1.automationNode.htmlAttributes.id,
        'Current node is not button1');

    Navigator.byItem.moveBackward();
    assertTrue(
        Navigator.byItem.node_ instanceof BackButtonNode,
        'BackButtonNode should come before button1');

    Navigator.byItem.moveBackward();
    assertFalse(
        button1.equals(Navigator.byItem.node_),
        'Unexpected navigation to button1');
    const button3 = Navigator.byItem.node_;
    assertFalse(
        button3 instanceof BackButtonNode,
        'button3 should not be a BackButtonNode');
    assertEquals(
        'button3', button3.automationNode.htmlAttributes.id,
        'Current node is not button3');

    Navigator.byItem.moveBackward();
    assertFalse(
        button3.equals(Navigator.byItem.node_),
        'Still on button3 after moveBackward()');
    assertFalse(button1.equals(Navigator.byItem.node_), 'Skipped button2');
    const button2 = Navigator.byItem.node_;
    assertFalse(
        button2 instanceof BackButtonNode,
        'button2 should not be a BackButtonNode');
    assertEquals(
        'button2', button2.automationNode.htmlAttributes.id,
        'Current node is not button2');

    Navigator.byItem.moveBackward();
    assertTrue(
        button1.equals(Navigator.byItem.node_),
        'button1 should come before button2');
  });
});

TEST_F(
    'SwitchAccessItemScanManagerTest', 'NodeUndefinedBeforeTreeChangeRemoved',
    function() {
      const website = `<div>
                     <button id="button1"></button>
                   </div>`;
      this.runWithLoadedTree(website, (rootWebArea) => {
        Navigator.byItem.moveTo_(this.findNodeById('button1'));
        const button1 = Navigator.byItem.node_;
        assertFalse(
            button1 instanceof BackButtonNode,
            'button1 should not be a BackButtonNode');
        assertEquals(
            'button1', button1.automationNode.htmlAttributes.id,
            'Current node is not button1');

        // Simulate the underlying node's deletion. Note that this is different
        // than an orphaned node (which can have a valid AutomationNode
        // instance, but no backing C++ object, so attributes returned like role
        // are undefined).
        Navigator.byItem.node_.baseNode_ = undefined;

        // Tree change removed gets sent by C++ after the tree has already
        // applied changes (so this comes after the above clearing).
        Navigator.byItem.onTreeChange_(
            {type: chrome.automation.TreeChangeType.NODE_REMOVED});
      });
    });

TEST_F(
    'SwitchAccessItemScanManagerTest', 'ScanAndTypeVirtualKeyboard',
    function() {
      const website = `<input type="text" id="input"></input>`;
      this.runWithLoadedTree(website, async (rootWebArea) => {
        // SA initially focuses this node; wait for it first.
        await this.untilFocusIs(
            {className: 'BrowserNonClientFrameViewChromeOS'});

        // Move to the text field.
        Navigator.byItem.moveTo_(this.findNodeById('input'));
        const input = Navigator.byItem.node_;
        assertEquals(
            'input', input.automationNode.htmlAttributes.id,
            'Current node is not input');
        input.performAction(SwitchAccessMenuAction.KEYBOARD);

        const keyboard = await this.untilFocusIs(
            {role: chrome.automation.RoleType.KEYBOARD});
        keyboard.performAction('select');

        const key = await this.untilFocusIs({instance: KeyboardNode});

        key.performAction('select');

        if (input.automationNode.value !== 'q') {
          // Wait for the potential value change.
          await new Promise(resolve => {
            input.automationNode.addEventListener(
                chrome.automation.EventType.VALUE_CHANGED, (event) => {
                  if (event.target.value === 'q') {
                    resolve();
                  }
                });
          });
        }
      });
    });

TEST_F('SwitchAccessItemScanManagerTest', 'DismissVirtualKeyboard', function() {
  const website = `<input type="text" id="input"></input><button>ok</button>`;
  this.runWithLoadedTree(website, async (rootWebArea) => {
    // SA initially focuses this node; wait for it first.
    await this.untilFocusIs({className: 'BrowserNonClientFrameViewChromeOS'});

    // Move to the text field.
    Navigator.byItem.moveTo_(this.findNodeById('input'));
    const input = Navigator.byItem.node_;
    assertEquals(
        'input', input.automationNode.htmlAttributes.id,
        'Current node is not input');
    input.performAction(SwitchAccessMenuAction.KEYBOARD);

    const keyboard =
        await this.untilFocusIs({role: chrome.automation.RoleType.KEYBOARD});
    keyboard.performAction('select');

    // Grab the key.
    const key = await this.untilFocusIs({instance: KeyboardNode});

    // Simulate a page focusing the ok button.
    const okButton = rootWebArea.find({attributes: {name: 'ok'}});
    okButton.focus();

    // Wait for the keyboard to become invisible and the ok button to be focused
    // by automation.
    await new Promise(resolve => {
      okButton.addEventListener(chrome.automation.EventType.FOCUS, resolve);
    });
    await new Promise(resolve => {
      keyboard.automationNode.addEventListener(
          chrome.automation.EventType.STATE_CHANGED, (event) => {
            if (event.target.role === chrome.automation.RoleType.KEYBOARD &&
                event.target.state.invisible) {
              resolve();
            }
          });
    });

    // We should end up back on the focused button in SA.
    const button =
        await this.untilFocusIs({role: chrome.automation.RoleType.BUTTON});
    assertEquals('ok', button.automationNode.name);
  });
});

TEST_F(
    'SwitchAccessItemScanManagerTest', 'ChildrenChangedDoesNotRefresh',
    function() {
      const website = `
    <div id="slider" role="slider">
      <div role="group"><div></div></div>
    </div>
    <button>done</button>
  `;
      this.runWithLoadedTree(website, async (rootWebArea) => {
        // SA initially focuses this node; wait for it first.
        await this.untilFocusIs(
            {className: 'BrowserNonClientFrameViewChromeOS'});

        // Move to the slider.
        Navigator.byItem.moveTo_(this.findNodeById('slider'));
        const slider = Navigator.byItem.node_;
        assertEquals(
            'slider', slider.automationNode.htmlAttributes.id,
            'Current node is not slider');

        // Trigger a children changed on the group.
        const automationGroup =
            rootWebArea.find({role: chrome.automation.RoleType.GROUP});
        assertTrue(!!automationGroup);
        const group = Navigator.byItem.group_;
        assertTrue(!!group);
        const handler = group.childrenChangedHandler_;
        assertTrue(!!handler);

        // Fake a children changed event.
        handler.eventStack_ = [{
          type: chrome.automation.EventType.CHILDREN_CHANGED,
          target: automationGroup
        }];
        handler.handleEvent_();

        // This subtree is not interesting, so it should not have triggered a
        // complete refresh of the SA tree.
        assertEquals(slider, Navigator.byItem.node_);
      });
    });

TEST_F('SwitchAccessItemScanManagerTest', 'InitialFocus', function() {
  const website = `<input></input><button autofocus></button>`;
  this.runWithLoadedTree(website, async (rootWebArea) => {
    // The button should have initial focus. This ensures we move past the focus
    // event below.
    const button =
        await this.untilFocusIs({role: chrome.automation.RoleType.BUTTON});

    // Build a new ItemScanManager to see what it sets as the initial node.
    const desktop = rootWebArea.parent.root;
    assertEquals(
        chrome.automation.RoleType.DESKTOP, desktop.role,
        `Unexpected desktop ${desktop.toString()}`);
    const manager = new ItemScanManager(desktop);
    assertEquals(
        button.automationNode, manager.node_.automationNode,
        `Unexpected focus ${manager.node_.debugString()}`);
  });
});


TEST_F('SwitchAccessItemScanManagerTest', 'SyncFocusToNewWindow', function() {
  const website1 = `<button autofocus>one</button>`;
  const website2 = `<button autofocus>two</button>`;
  this.runWithLoadedTree(website1, async (rootWebArea) => {
    // Wait for the first button to get SA focused.
    const button1 = await this.untilFocusIs(
        {role: chrome.automation.RoleType.BUTTON, name: 'one'});

    // Launch a new browser window and load up the second site.
    EventGenerator.sendKeyPress(KeyCode.N, {ctrl: true});
    this.runWithLoadedTree(website2, async (rootWebArea) => {
      // Wait for the second button to get SA focused.
      const button2 = await this.untilFocusIs(
          {role: chrome.automation.RoleType.BUTTON, name: 'two'});

      // Do a search for the title bar nodes for each of the browser windows. We
      // do this by walking up to the widget for each window from the buttons.
      let widget1 = button1.automationNode;
      while (widget1.role !== chrome.automation.RoleType.WINDOW ||
             widget1.className !== 'Widget') {
        widget1 = widget1.parent;
      }
      assertTrue(!!widget1);

      let widget2 = button2.automationNode;
      while (widget2.role !== chrome.automation.RoleType.WINDOW ||
             widget2.className !== 'Widget') {
        widget2 = widget2.parent;
      }
      assertTrue(!!widget2);

      const titleBar1 =
          widget1.find({role: chrome.automation.RoleType.TITLE_BAR});
      assertTrue(!!titleBar1);
      const titleBar2 =
          widget2.find({role: chrome.automation.RoleType.TITLE_BAR});
      assertTrue(!!titleBar2);

      // The focus is currently on widget2 (since button2 has focus). Start with
      // focusing widget1 which should occur as a result of moving SA to title
      // bar 1.
      Navigator.byItem.moveTo_(titleBar1);

      // Simulate entering this group to trigger the focus.
      Navigator.byItem.enterGroup();

      // Note that this and the second instance below is system OS focus, but
      // doesn't impact SA focus to preserve current behavior and prevent
      // flickering.
      let currentFocus = await new Promise(r => {
        widget1.addEventListener(
            chrome.automation.EventType.FOCUS, e => r(e.target));
      });
      assertEquals(currentFocus, button1.automationNode);

      // Now, switch to widget2 by moving to title bar 2.
      Navigator.byItem.moveTo_(titleBar2);

      // Simulate entering this group to trigger the focus.
      Navigator.byItem.enterGroup();

      currentFocus = await new Promise(r => {
        widget2.addEventListener(
            chrome.automation.EventType.FOCUS, e => r(e.target));
      });
      assertEquals(currentFocus, button2.automationNode);
    });
  });
});

// TODO(crbug.com/1219067): Unflake.
TEST_F(
    'SwitchAccessItemScanManagerTest', 'DISABLED_LockScreenBlocksUserSession',
    function() {
      const website = `<button autofocus>kitties!</button>`;
      this.runWithLoadedTree(website, async (rootWebArea) => {
        let button =
            await this.untilFocusIs({role: chrome.automation.RoleType.BUTTON});
        assertEquals('kitties!', button.automationNode.name);

        // Lock the screen.
        EventGenerator.sendKeyPress(KeyCode.L, {search: true});

        // Wait for focus to move to the password field.
        await this.untilFocusIs({
          role: chrome.automation.RoleType.TEXT_FIELD,
          name: 'Password for stub-user@example.com'
        });

        // The button is no longer in the tree because the screen is locked.
        const predicate = (node) => node.name === 'kitties!' &&
            node.role === chrome.automation.RoleType.BUTTON;
        assertNotNullNorUndefined(
            this.desktop_, 'this.desktop_ is null or undefined.');
        const treeWalker = new AutomationTreeWalker(
            this.desktop_, constants.Dir.FORWARD, {visit: predicate});
        const node = treeWalker.next().node;
        assertEquals(null, node);

        // Log in again and confirm that the button is back and gets focus
        // again.
        EventGenerator.sendKeyPress(KeyCode.T);
        EventGenerator.sendKeyPress(KeyCode.E);
        EventGenerator.sendKeyPress(KeyCode.S);
        EventGenerator.sendKeyPress(KeyCode.T);
        EventGenerator.sendKeyPress(KeyCode.ZERO);
        EventGenerator.sendKeyPress(KeyCode.ZERO);
        EventGenerator.sendKeyPress(KeyCode.ZERO);
        EventGenerator.sendKeyPress(KeyCode.ZERO);
        EventGenerator.sendKeyPress(KeyCode.RETURN);

        button =
            await this.untilFocusIs({role: chrome.automation.RoleType.BUTTON});
        assertEquals('kitties!', button.automationNode.name);
      });
    });
