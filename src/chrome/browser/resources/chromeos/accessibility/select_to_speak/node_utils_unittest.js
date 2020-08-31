// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for node_utils.js.
 */
SelectToSpeakNodeUtilsUnitTest = class extends testing.Test {};

/** @override */
SelectToSpeakNodeUtilsUnitTest.prototype.extraLibraries = [
  'test_support.js',
  'paragraph_utils.js',
  'node_utils.js',
  'word_utils.js',
  'rect_utils.js',
];


TEST_F('SelectToSpeakNodeUtilsUnitTest', 'GetNodeVisibilityState', function() {
  const nodeWithoutRoot1 = {root: null};
  const nodeWithoutRoot2 = {root: null, state: {invisible: true}};
  assertEquals(
      NodeUtils.getNodeState(nodeWithoutRoot1),
      NodeUtils.NodeState.NODE_STATE_INVALID);
  assertEquals(
      NodeUtils.getNodeState(nodeWithoutRoot2),
      NodeUtils.NodeState.NODE_STATE_INVALID);

  const invisibleNode1 = {
    root: {},
    parent: {role: ''},
    state: {invisible: true}
  };
  // Currently nodes aren't actually marked 'invisible', so we need to navigate
  // up their tree.
  const invisibleNode2 = {
    root: {},
    parent: {role: 'window', state: {invisible: true}},
    state: {}
  };
  const invisibleNode3 = {root: {}, parent: invisibleNode2, state: {}};
  const invisibleNode4 = {root: {}, parent: invisibleNode3, state: {}};
  assertEquals(
      NodeUtils.getNodeState(invisibleNode1),
      NodeUtils.NodeState.NODE_STATE_INVISIBLE);
  assertEquals(
      NodeUtils.getNodeState(invisibleNode2),
      NodeUtils.NodeState.NODE_STATE_INVISIBLE);
  assertEquals(
      NodeUtils.getNodeState(invisibleNode3),
      NodeUtils.NodeState.NODE_STATE_INVISIBLE);

  const normalNode1 = {
    root: {},
    parent: {role: 'window', state: {}},
    state: {}
  };
  const normalNode2 = {root: {}, parent: {normalNode1}, state: {}};
  assertEquals(
      NodeUtils.getNodeState(normalNode1),
      NodeUtils.NodeState.NODE_STATE_NORMAL);
  assertEquals(
      NodeUtils.getNodeState(normalNode2),
      NodeUtils.NodeState.NODE_STATE_NORMAL);
});

TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'GetNodeVisibilityStateWithRootWebArea',
    function() {
      // Currently nodes aren't actually marked 'invisible', so we need to
      // navigate up their tree.
      const window = {root: {}, role: 'window', state: {invisible: true}};
      const rootNode =
          {root: {}, parent: window, state: {}, role: 'rootWebArea'};
      const container = {root: rootNode, parent: rootNode, state: {}};
      const node = {root: rootNode, parent: container, state: {}};
      assertEquals(
          NodeUtils.getNodeState(window),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(container),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(node),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);

      // Make a fake iframe in this invisible window by adding another
      // RootWebArea. The iframe has no root but is parented to the container
      // above.
      const iframeRoot = {parent: container, state: {}, role: 'rootWebArea'};
      const iframeContainer = {root: iframeRoot, parent: iframeRoot, state: {}};
      const iframeNode = {root: iframeRoot, parent: iframeContainer, state: {}};
      assertEquals(
          NodeUtils.getNodeState(iframeContainer),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);
      assertEquals(
          NodeUtils.getNodeState(iframeNode),
          NodeUtils.NodeState.NODE_STATE_INVISIBLE);

      // Make the window visible and try again.
      window.state = {};
      assertEquals(
          NodeUtils.getNodeState(window),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(container),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(node), NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(iframeContainer),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
      assertEquals(
          NodeUtils.getNodeState(iframeNode),
          NodeUtils.NodeState.NODE_STATE_NORMAL);
    });

TEST_F('SelectToSpeakNodeUtilsUnitTest', 'findAllMatching', function() {
  const rect = {left: 0, top: 0, width: 100, height: 100};
  const rootNode = {
    root: {},
    state: {},
    role: 'rootWebArea',
    state: {},
    location: {left: 0, top: 0, width: 600, height: 600}
  };
  const container1 = {
    root: rootNode,
    parent: rootNode,
    role: 'staticText',
    name: 'one two',
    state: {},
    location: {left: 0, top: 0, width: 200, height: 200}
  };
  const container2 = {
    root: rootNode,
    parent: rootNode,
    state: {},
    role: 'genericContainer',
    location: {left: 0, top: 0, width: 200, height: 200}
  };
  const node1 = {
    root: rootNode,
    parent: container1,
    name: 'one',
    role: 'inlineTextBox',
    state: {},
    location: {left: 50, top: 0, width: 50, height: 50}
  };
  const node2 = {
    root: rootNode,
    parent: container1,
    name: 'two',
    role: 'inlineTextBox',
    state: {},
    location: {left: 0, top: 50, width: 50, height: 50}
  };
  const node3 = {
    root: rootNode,
    parent: container1,
    value: 'text',
    role: 'textField',
    state: {},
    location: {left: 0, top: 0, width: 25, height: 25}
  };

  // Set up relationships between nodes.
  rootNode.children = [container1, container2];
  rootNode.firstChild = container1;
  container1.nextSibling = container2;
  container1.children = [node1, node2, node3];
  container1.firstChild = node1;
  node1.nextSibling = node2;
  node2.nextSibling = node3;

  // Should get both children of the first container, without getting
  // the first container itself or the empty container.
  let result = [];
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(3, result.length);
  assertEquals(node1, result[0]);
  assertEquals(node2, result[1]);
  assertEquals(node3, result[2]);

  // If a node doesn't have a name, it should not be included.
  result = [];
  node2.name = undefined;
  node3.value = undefined;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node1, result[0]);

  // Try a rect that only overlaps one of the children.
  result = [];
  node2.name = 'two';
  rect.height = 25;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node1, result[0]);

  // Now just overlap a different child.
  result = [];
  rect.top = 50;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node2, result[0]);

  // Offscreen should cause a node to be skipped.
  result = [];
  node2.state = {offscreen: true};
  assertFalse(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(0, result.length);

  // No location should cause a node to be skipped.
  result = [];
  node2.state = {};
  node2.location = undefined;
  assertFalse(NodeUtils.findAllMatching(rootNode, rect, result));

  // A non staticText container without a name should still have
  // children found if they are valid.
  result = [];
  const node4 = {
    root: rootNode,
    parent: container2,
    name: 'four',
    state: {},
    location: {left: 0, top: 50, width: 50, height: 50}
  };
  container2.firstChild = node4;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node4, result[0]);

  // A non staticText container with a valid name should not be
  // read if its children are read. Children take precidence.
  result = [];
  container2.name = 'container2';
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(node4, result[0]);

  // A non staticText container with a valid name which has only
  // children without names should be read instead of its children.
  result = [];
  node4.name = undefined;
  assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
  assertEquals(1, result.length);
  assertEquals(container2, result[0]);
});

TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'findAllMatchingWithInputs', function() {
      const rect = {left: 0, top: 0, width: 100, height: 100};
      const rootNode = {
        root: {},
        state: {},
        role: 'rootWebArea',
        location: {left: 0, top: 0, width: 600, height: 600}
      };
      const checkbox = {
        root: rootNode,
        parent: rootNode,
        role: 'checkBox',
        state: {},
        location: {left: 0, top: 0, width: 200, height: 200},
        checked: 'true'
      };
      rootNode.children = [checkbox];
      rootNode.firstChild = checkbox;

      const result = [];
      assertTrue(NodeUtils.findAllMatching(rootNode, rect, result));
      assertEquals(1, result.length);
      assertEquals(checkbox, result[0]);
    });

TEST_F(
    'SelectToSpeakNodeUtilsUnitTest', 'getDeepEquivalentForSelectionNoChildren',
    function() {
      const node = {name: 'Hello, world', children: []};
      let result = NodeUtils.getDeepEquivalentForSelection(node, 0);
      assertEquals(node, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(node, 6);
      assertEquals(node, result.node);
      assertEquals(6, result.offset);
    });

TEST_F(
    'SelectToSpeakNodeUtilsUnitTest',
    'getDeepEquivalentForSelectionSimpleChildren', function() {
      const child1 =
          {name: 'Hello,', children: [], role: 'inlineTextBox', state: {}};
      const child2 =
          {name: ' world', children: [], role: 'inlineTextBox', state: {}};
      const root = {
        name: 'Hello, world',
        children: [child1, child2],
        role: 'staticText',
        state: {}
      };
      child1.parent = root;
      child2.parent = root;
      let result = NodeUtils.getDeepEquivalentForSelection(root, 0, true);
      assertEquals(child1, result.node);
      assertEquals(0, result.offset);

      // Get the last index of the first child
      result = NodeUtils.getDeepEquivalentForSelection(root, 5, false);
      assertEquals(child1, result.node);
      assertEquals(5, result.offset);

      // Get the first index of the second child
      result = NodeUtils.getDeepEquivalentForSelection(root, 6, true);
      assertEquals(child2, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(root, 9, true);
      assertEquals(child2, result.node);
      assertEquals(3, result.offset);
    });

TEST_F(
    'SelectToSpeakNodeUtilsUnitTest',
    'getDeepEquivalentForSelectionComplexChildren', function() {
      const child1 =
          {name: 'Hello', children: [], role: 'inlineTextBox', state: {}};
      // Empty name
      const child2 =
          {name: undefined, children: [], role: 'inlineTextBox', state: {}};
      const child3 =
          {name: ',', children: [], role: 'inlineTextBox', state: {}};
      const child4 = {
        name: 'Hello,',
        children: [child1, child2, child3],
        role: 'staticText',
        state: {},
        firstChild: child1,
        lastChild: child3
      };
      child1.parent = child4;
      child2.parent = child4;
      child3.parent = child4;

      const child5 =
          {name: ' ', children: [], role: 'inlineTextBox', state: {}};
      const child6 =
          {name: 'world', children: [], role: 'inlineTextBox', state: {}};
      const child7 = {
        name: ' world',
        children: [child5, child6],
        role: 'staticText',
        state: {},
        firstChild: child5,
        lastChild: child6
      };
      child5.parent = child7;
      child6.parent = child7;

      const root = {
        name: undefined,
        children: [child4, child7],
        role: 'genericContainer',
        state: {},
        firstChild: child4,
        lastChild: child7
      };
      child4.parent = root;
      child7.parent = root;

      let result = NodeUtils.getDeepEquivalentForSelection(root, 0, true);
      assertEquals(child1, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(root, 1, true);
      assertEquals(child5, result.node);
      assertEquals(0, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(root, 2, false);
      assertEquals(child6, result.node);
      assertEquals(5, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(child4, 2, true);
      assertEquals(child1, result.node);
      assertEquals(2, result.offset);

      result = NodeUtils.getDeepEquivalentForSelection(child4, 5, true);
      assertEquals(child3, result.node);
      assertEquals(0, result.offset);
    });
