// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/**
 * @constructor
 * @extends {SwitchAccessE2ETest}
 */
function SwitchAccessTextNavigationManagerTest() {
  SwitchAccessE2ETest.call(this);
}

SwitchAccessTextNavigationManagerTest.prototype = {
  __proto__: SwitchAccessE2ETest.prototype,

  /** @override */
  setUp() {
    TextNavigationManager.initialize();
    this.textNavigationManager = TextNavigationManager.instance;
    this.navigationManager = NavigationManager.instance;
  }
};


/**
 * Generates a website with a text area, finds the node for the text
 * area, sets up the node to listen for a text navigation action, and then
 * executes the specified text navigation action. Upon detecting the
 * text navigation action, the node will verify that the action correctly
 * changed the index of the text caret.
 * @param {!SwitchAccessE2ETest} testHelper
 * @param {{content: string,
 *          initialIndex: number,
 *          targetIndex: number,
 *          navigationAction: function(),
 *          id: (string || undefined),
 *          cols: (number || undefined),
 *          wrap: (string || undefined)}} textParams
 */
function runTextNavigationTest(testHelper, textParams) {
  // Required parameters.
  const textContent = textParams.content;
  const initialTextIndex = textParams.initialIndex;
  const targetTextIndex = textParams.targetIndex;
  const textNavigationAction = textParams.navigationAction;

  // Default parameters.
  const textId = textParams.id || 'test';
  const textCols = textParams.cols || 20;
  const textWrap = textParams.wrap || 'soft';

  const website = generateWebsiteWithTextArea(
      textId, textContent, initialTextIndex, textCols, textWrap);

  testHelper.runWithLoadedTree(website, function(desktop) {
    const inputNode = findNodeById(desktop, textId);
    assertNotEquals(inputNode, null);

    setUpCursorChangeListener(
        testHelper, inputNode, initialTextIndex, targetTextIndex,
        targetTextIndex);

    textNavigationAction();
  });
}

/**
 * This function:
 * - Generates a website with a text area
 * - Executes setSelectStart finds the node for the text
 * area
 * - Sets up the node to listen for a text navigation action
 * - executes the specified text navigation action. Upon detecting the
 * - Verifies that the action correctly changed the index of the text caret
 * - Sets up a second listener for a text selection action
 * - Calls saveSelectEnd function from the event listener
 * - Verifies that the selection was set correctly
 * textParams should specify parameters
 * for the test as follows:
 *  -content: content of the text area.
 *  -initialIndex: index of the text caret before the navigation action.
 *  -targetStartIndex: start index of the selection after the selection action.
 *  -targetEndIndex: end index of the selection after the navigation action.
 *  -navigationAction: function executing a text navigation action or selection
 * action. -id: id of the text area element (optional). -cols: number of columns
 * in the text area (optional). -wrap: the wrap attribute ("hard" or "soft") of
 * the text area (optional).
 *
 * @param {!SwitchAccessE2ETest} testHelper
 * @param {selectionTextParams} textParams,
 */
function runTextSelectionTest(testHelper, textParams) {
  // Required parameters.
  const textContent = textParams.content;
  const initialTextIndex = textParams.initialIndex;
  const targetTextStartIndex = textParams.targetStartIndex;
  const targetTextEndIndex = textParams.targetEndIndex;
  const textNavigationAction = textParams.navigationAction;

  // Default parameters.
  const selectionIsBackward = textParams.backward || false;
  const textId = textParams.id || 'test';
  const textCols = textParams.cols || 20;
  const textWrap = textParams.wrap || 'soft';

  const website = generateWebsiteWithTextArea(
      textId, textContent, initialTextIndex, textCols, textWrap);

  let navigationTargetIndex = targetTextEndIndex;
  if (selectionIsBackward) {
    navigationTargetIndex = targetTextStartIndex;
  }

  testHelper.runWithLoadedTree(website, function(desktop) {
    const inputNode = findNodeById(desktop, textId, testHelper);
    assertNotEquals(inputNode, null);
    checkNodeIsFocused(inputNode);
    const callback = testHelper.newCallback(function() {
      setUpCursorChangeListener(
          testHelper, inputNode, targetTextEndIndex, targetTextStartIndex,
          targetTextEndIndex);
      testHelper.textNavigationManager.saveSelectEnd();
    });

    testHelper.textNavigationManager.saveSelectStart();

    setUpCursorChangeListener(
        testHelper, inputNode, initialTextIndex, navigationTargetIndex,
        navigationTargetIndex, callback);

    textNavigationAction();
  });
}

/**
 * Returns a website string with a text area with the given properties.
 * @param {number} id The ID of the text area element.
 * @param {string} contents The contents of the text area.
 * @param {number} textIndex The index of the text caret within the text area.
 * @param {number} cols The number of columns in the text area.
 * @param {string} wrap The wrap attribute of the text area ('hard' or 'soft').
 * @return {string}
 */
function generateWebsiteWithTextArea(id, contents, textIndex, cols, wrap) {
  const website = `data:text/html;charset=utf-8,
    <textarea id=${id} cols=${cols} wrap=${wrap}
    autofocus="true">${contents}</textarea>
    <script>
      const input = document.getElementById("${id}");
      input.selectionStart = ${textIndex};
      input.selectionEnd = ${textIndex};
      input.focus();
    </script>`;
  return website;
}

/**
 * Given the desktop node, returns the node with the given
 * id.
 * @param {!chrome.automation.AutomationNode} desktop
 * @param {string} id
 * @return {!chrome.automation.AutomationNode}
 */
function findNodeById(desktop, id) {
  // The loop ensures that the page has loaded before trying to find the node.
  let inputNode;
  while (inputNode == null) {
    inputNode = new AutomationTreeWalker(
                    desktop, constants.Dir.FORWARD,
                    {visit: (node) => node.htmlAttributes.id === id})
                    .next()
                    .node;
  }
  return inputNode;
}

/**
 * Check that the node in the JS file matches the node in the test.
 * The nodes can be assumed to be the same if their roles match as there is only
 * one text input node on the generated webpage.
 * @param {!chrome.automation.AutomationNode} inputNode
 */
function checkNodeIsFocused(inputNode) {
  chrome.automation.getFocus((focusedNode) => {
    assertEquals(focusedNode.role, inputNode.role);
  });
}

/**
 * Sets up the input node (text field) to listen for text
 * navigation and selection actions. When the index of the text selection
 * changes from its initial position, checks that the text
 * indices now matches the target text start and end index. Assumes that the
 * initial and target text start/end indices are distinct (to detect a
 * change from the text navigation action). Also assumes that
 * the text navigation and selection actions directly changes the text caret
 * to the correct index (with no intermediate movements).
 * @param {!SwitchAccessE2ETest} testHelper
 * @param {!chrome.automation.AutomationNode} inputNode
 * @param {number} initialTextIndex
 * @param {number} targetTextStartIndex
 * @param {number} targetTextEndIndex
 * @param {function() || undefined} callback
 */
function setUpCursorChangeListener(
    testHelper, inputNode, initialTextIndex, targetTextStartIndex,
    targetTextEndIndex, callback) {
  // Ensures that the text index has changed before checking the new index.
  const checkActionFinished = function(tab) {
    if (inputNode.textSelStart != initialTextIndex ||
        inputNode.textSelEnd != initialTextIndex) {
      checkTextIndex();
      if (callback) {
        callback();
      }
    }
  };

  // Test will not exit until this check is called.
  const checkTextIndex = testHelper.newCallback(function() {
    assertEquals(inputNode.textSelStart, targetTextStartIndex);
    assertEquals(inputNode.textSelEnd, targetTextEndIndex);
    // If there's a callback then this is the navigation listener for a
    // selection test, thus remove it when fired to make way for the selection
    // listener.
    if (callback) {
      inputNode.removeEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          checkActionFinished);
    }
  });

  inputNode.addEventListener(
      chrome.automation.EventType.TEXT_SELECTION_CHANGED, checkActionFinished);
}

TEST_F('SwitchAccessTextNavigationManagerTest', 'JumpToBeginning', function() {
  runTextNavigationTest(this, {
    content: 'hi there',
    initialIndex: 6,
    targetIndex: 0,
    navigationAction: () => {
      TextNavigationManager.jumpToBeginning();
    }
  });
});

TEST_F('SwitchAccessTextNavigationManagerTest', 'JumpToEnd', function() {
  runTextNavigationTest(this, {
    content: 'hi there',
    initialIndex: 3,
    targetIndex: 8,
    navigationAction: () => {
      TextNavigationManager.jumpToEnd();
    }
  });
});

TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'MoveBackwardOneChar', function() {
      runTextNavigationTest(this, {
        content: 'parrots!',
        initialIndex: 7,
        targetIndex: 6,
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneChar();
        }
      });
    });

TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'MoveBackwardOneWord', function() {
      runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 5,
        targetIndex: 0,
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneWord();
        }
      });
    });

TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'MoveForwardOneChar', function() {
      runTextNavigationTest(this, {
        content: 'hello',
        initialIndex: 0,
        targetIndex: 1,
        navigationAction: () => {
          TextNavigationManager.moveForwardOneChar();
        }
      });
    });

TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'MoveForwardOneWord', function() {
      runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 4,
        targetIndex: 12,
        navigationAction: () => {
          TextNavigationManager.moveForwardOneWord();
        }
      });
    });

TEST_F('SwitchAccessTextNavigationManagerTest', 'MoveUpOneLine', function() {
  runTextNavigationTest(this, {
    content: 'more parrots!',
    initialIndex: 7,
    targetIndex: 2,
    cols: 8,
    wrap: 'hard',
    navigationAction: () => {
      TextNavigationManager.moveUpOneLine();
    }
  });
});

TEST_F('SwitchAccessTextNavigationManagerTest', 'MoveDownOneLine', function() {
  runTextNavigationTest(this, {
    content: 'more parrots!',
    initialIndex: 3,
    targetIndex: 8,
    cols: 8,
    wrap: 'hard',
    navigationAction: () => {
      TextNavigationManager.moveDownOneLine();
    }
  });
});


/**
 * Test the setSelectStart function by checking correct index is stored as the
 * selection start index.
 */
TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectStart',
    function() {
      const website =
          generateWebsiteWithTextArea('test', 'test123', 3, 20, 'hard');

      this.runWithLoadedTree(website, function(desktop) {
        const inputNode = findNodeById(desktop, 'test', this);
        assertNotEquals(inputNode, null);
        checkNodeIsFocused(inputNode);

        this.textNavigationManager.saveSelectStart();
        const startIndex = this.textNavigationManager.selectionStartIndex_;
        assertEquals(startIndex, 3);
      });
    });

/**
 * Test the setSelectEnd function by manually setting the selection start index
 * and node then calling setSelectEnd and checking for the correct selection
 * bounds
 */
TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectEnd', function() {
      const website =
          generateWebsiteWithTextArea('test', 'test 123', 6, 20, 'hard');

      this.runWithLoadedTree(website, function(desktop) {
        const inputNode = findNodeById(desktop, 'test', this);
        assertNotEquals(inputNode, null);
        checkNodeIsFocused(inputNode);


        const startIndex = 3;
        this.textNavigationManager.selectionStartIndex_ = startIndex;
        this.textNavigationManager.selectionStartObject_ = inputNode;
        this.textNavigationManager.saveSelectEnd();
        const endIndex = inputNode.textSelEnd;
        assertEquals(6, endIndex);
      });
    });

/**
 * Test use of setSelectStart and setSelectEnd with the moveForwardOneChar
 * function.
 */
TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectCharacter',
    function() {
      runTextSelectionTest(this, {
        content: 'hello world!',
        initialIndex: 0,
        targetStartIndex: 0,
        targetEndIndex: 1,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveForwardOneChar();
        }
      });
    });

/**
 * Test use of setSelectStart and setSelectEnd with a backward selection using
 * the moveBackwardOneWord function.
 */
TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectWordBackward',
    function() {
      runTextSelectionTest(this, {
        content: 'hello world!',
        initialIndex: 5,
        targetStartIndex: 0,
        targetEndIndex: 5,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneWord();
        },
        backward: true
      });
    });

/**
 * selectionTextParams should specify parameters
 * for the test as follows:
 *  -content: content of the text area.
 *  -initialIndex: index of the text caret before the navigation action.
 *  -targetIndex: index of the text caret after the navigation action.
 *  -navigationAction: function executing a text navigation action.
 *  -id: id of the text area element (optional).
 *  -cols: number of columns in the text area (optional).
 *  -wrap: the wrap attribute ("hard" or "soft") of the text area (optional).
 *@typedef {{content: string,
 *          initialIndex: number,
 *          targetStartIndex: number,
 *          targetEndIndex: number,
 *          textAction: function(),
 *          backward: (boolean || undefined)
 *          id: (string || undefined),
 *          cols: (number || undefined),
 *          wrap: (string || undefined),}}
 */
let selectionTextParams;
