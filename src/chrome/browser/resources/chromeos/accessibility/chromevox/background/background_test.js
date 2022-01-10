// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * Test fixture for Background.
 */
ChromeVoxBackgroundTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    super.setUp();
    window.doGesture = this.doGesture;
    window.simulateHitTestResult = this.simulateHitTestResult;
    window.press = this.press;
    window.Mod = constants.ModifierFlag;
    window.ActionType = chrome.automation.ActionType;

    this.forceContextualLastOutput();
  }

  simulateHitTestResult(node) {
    return () => {
      GestureCommandHandler.pointerHandler_.handleHitTestResult(node);
    };
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }

  get linksAndHeadingsDoc() {
    return `
      <p>start</p>
      <a href='#a'>alpha</a>
      <a href='#b'>beta</a>
      <p>
        <h1>charlie</h1>
        <a href='foo'>delta</a>
      </p>
      <a href='#bar'>echo</a>
      <h2>foxtraut</h2>
      <p>end<span>of test</span></p>
    `;
  }

  get buttonDoc() {
    return `
      <p>start</p>
      <button>hello button one</button>
      cats
      <button>hello button two</button>
      <p>end</p>
    `;
  }

  get formsDoc() {
    return `
      <select id="fruitSelect">
        <option>apple</option>
        <option>grape</option>
        <option> banana</option>
      </select>
    `;
  }

  get iframesDoc() {
    return `
      <p>start</p>
      <button>Before</button>
      <iframe srcdoc="<button>Inside</button><h1>Inside</h1>"></iframe>
      <button>After</button>
    `;
  }

  get disappearingObjectDoc() {
    return `
      <p>start</p>
      <div role="group">
        <p>Before1</p>
        <p>Before2</p>
        <p>Before3</p>
      </div>
      <div role="group">
        <p id="disappearing">Disappearing</p>
      </div>
      <div role="group">
        <p>After1</p>
        <p>After2</p>
        <p>After3</p>
      </div>
      <div id="live" aria-live="assertive"></div>
      <div id="delete" role="button">Delete</div>
      <script>
        document.getElementById('delete').addEventListener('click', function() {
          let d = document.getElementById('disappearing');
          d.parentElement.removeChild(d);
          document.getElementById('live').innerText = 'Deleted';
          document.body.offsetTop
        });
      </script>
    `;
  }

  get detailsDoc() {
    return `
      <p aria-details="details">start</p>
      <div role="group">
        <p>Before</p>
        <p id="details">Details</p>
        <p>After</p>
      </div>
    `;
  }

  get comboBoxDoc() {
    return `
      <div id="combo-box-label">Choose an item</div>
      <div aria-labelledby="combo-box-label" role="combobox">
        <input type="text" aria-controls="combo-box-list-box">
        <ul role="listbox" id="combo-box-list-box" hidden>
          <li role="option" tabindex="-1">Item 1</li>
          <li role="option" tabindex="-1">Item 2</li>
        </ul>
      </div>
    `;
  }

  get listBoxDoc() {
    return `
      <p>Start</p>
      <div role="listbox" aria-expanded="false" aria-label="Select an item">
        <div aria-selected="true" tabindex="0" role="option">
          <span>Listbox item one</span>
        </div>
        <div aria-selected="false" tabindex="-1" role="option">
          <span>Listbox item two</span>
        </div>
        <div aria-selected="false" role="option">
          <span>Listbox item three</span>
        </div>
      </div>
      <button>Click</button>
    `;
  }

  get nestedListDoc() {
    return `
      <div>
        <ul>
          <li>Lemons</li>
          <li>Oranges</li>
          <li>Berries
            <ul>
              <li>Strawberries</li>
              <li>Raspberries</li>
            </ul>
          </li>
          <li>Bananas</li>
        </ul>
      </div>
    `;
  }

  /**
   * Fires an onCustomSpokenFeedbackToggled event with enabled state of
   * |enabled|.
   * @param {boolean} enabled TalkBack-enabled state.
   */
  dispatchOnCustomSpokenFeedbackToggledEvent(enabled) {
    chrome.accessibilityPrivate.onCustomSpokenFeedbackToggled.dispatch(enabled);
  }
};

/**
 * Specific test fixture for tests that need a test server running.
 */
ChromeVoxBackgroundTestWithTestServer = class extends ChromeVoxBackgroundTest {
  get testServer() {
    return true;
  }
};



/** Tests that ChromeVox classic is in this context. */
SYNC_TEST_F('ChromeVoxBackgroundTest', 'ClassicNamespaces', function() {
  assertEquals('function', typeof (ChromeVoxBackground));
});

/** Tests that ChromeVox's background object is not available globally. */
SYNC_TEST_F('ChromeVoxBackgroundTest', 'NextNamespaces', function() {
  assertEquals(undefined, window.Background);
});

/** Tests consistency of navigating forward and backward. */
TEST_F('ChromeVoxBackgroundTest', 'ForwardBackwardNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksAndHeadingsDoc, function() {
    mockFeedback.expectSpeech('start').expectBraille('start');

    mockFeedback.call(doCmd('nextLink'))
        .expectSpeech('alpha', 'Link')
        .expectBraille('alpha lnk');
    mockFeedback.call(doCmd('nextLink'))
        .expectSpeech('beta', 'Link')
        .expectBraille('beta lnk');
    mockFeedback.call(doCmd('nextLink'))
        .expectSpeech('delta', 'Link')
        .expectBraille('delta lnk');
    mockFeedback.call(doCmd('previousLink'))
        .expectSpeech('beta', 'Link')
        .expectBraille('beta lnk');
    mockFeedback.call(doCmd('nextHeading'))
        .expectSpeech('charlie', 'Heading 1')
        .expectBraille('charlie h1');
    mockFeedback.call(doCmd('nextHeading'))
        .expectSpeech('foxtraut', 'Heading 2')
        .expectBraille('foxtraut h2');
    mockFeedback.call(doCmd('previousHeading'))
        .expectSpeech('charlie', 'Heading 1')
        .expectBraille('charlie h1');

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('delta', 'Link')
        .expectBraille('delta lnk');
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('echo', 'Link')
        .expectBraille('echo lnk');
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('foxtraut', 'Heading 2')
        .expectBraille('foxtraut h2');
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('end')
        .expectBraille('end');
    mockFeedback.call(doCmd('previousObject'))
        .expectSpeech('foxtraut', 'Heading 2')
        .expectBraille('foxtraut h2');
    mockFeedback.call(doCmd('nextLine')).expectSpeech('foxtraut');
    mockFeedback.call(doCmd('nextLine'))
        .expectSpeech('end', 'of test')
        .expectBraille('endof test');

    mockFeedback.call(doCmd('jumpToTop'))
        .expectSpeech('start')
        .expectBraille('start');
    mockFeedback.call(doCmd('jumpToBottom'))
        .expectSpeech('of test')
        .expectBraille('of test');

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'CaretNavigation', function() {
  // TODO(plundblad): Add braille expectations when crbug.com/523285 is fixed.
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksAndHeadingsDoc, function() {
    mockFeedback.expectSpeech('start');
    mockFeedback.call(doCmd('nextCharacter')).expectSpeech('t');
    mockFeedback.call(doCmd('nextCharacter')).expectSpeech('a');
    mockFeedback.call(doCmd('nextWord')).expectSpeech('alpha', 'Link');
    mockFeedback.call(doCmd('nextWord')).expectSpeech('beta', 'Link');
    mockFeedback.call(doCmd('previousWord')).expectSpeech('alpha', 'Link');
    mockFeedback.call(doCmd('nextWord')).expectSpeech('beta', 'Link');
    mockFeedback.call(doCmd('nextWord')).expectSpeech('charlie', 'Heading 1');
    mockFeedback.call(doCmd('nextLine')).expectSpeech('delta', 'Link');
    mockFeedback.call(doCmd('nextLine')).expectSpeech('echo', 'Link');
    mockFeedback.call(doCmd('nextLine')).expectSpeech('foxtraut', 'Heading 2');
    mockFeedback.call(doCmd('nextLine')).expectSpeech('end', 'of test');
    mockFeedback.call(doCmd('nextCharacter')).expectSpeech('n');
    mockFeedback.call(doCmd('previousCharacter')).expectSpeech('e');
    mockFeedback.call(doCmd('previousCharacter'))
        .expectSpeech('t', 'Heading 2');
    mockFeedback.call(doCmd('previousWord')).expectSpeech('foxtraut');
    mockFeedback.call(doCmd('previousWord')).expectSpeech('echo', 'Link');
    mockFeedback.call(doCmd('previousCharacter')).expectSpeech('a', 'Link');
    mockFeedback.call(doCmd('previousCharacter')).expectSpeech('t');
    mockFeedback.call(doCmd('nextWord')).expectSpeech('echo', 'Link');
    mockFeedback.replay();
  });
});

/** Tests that individual buttons are stops for move-by-word functionality. */
TEST_F(
    'ChromeVoxBackgroundTest', 'CaretNavigationMoveThroughButtonByWord',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.buttonDoc, function() {
        mockFeedback.expectSpeech('start');
        mockFeedback.call(doCmd('nextObject'))
            .expectSpeech('hello button one', 'Button');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('start');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('hello');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('button');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('one');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('cats');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('hello');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('button');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('two');
        mockFeedback.call(doCmd('nextWord')).expectSpeech('end');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('two');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('button');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('hello');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('cats');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('one');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('button');
        mockFeedback.call(doCmd('previousWord')).expectSpeech('hello');
        mockFeedback.replay();
      });
    });

TEST_F('ChromeVoxBackgroundTest', 'SelectSingleBasic', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.formsDoc, function() {
    mockFeedback.expectSpeech('apple', 'has pop up', 'Collapsed')
        .expectBraille('apple btn +popup +3 +')
        .call(press(KeyCode.DOWN))
        .expectSpeech('grape', /2 of 3/)
        .expectBraille('grape 2/3')
        .call(press(KeyCode.DOWN))
        .expectSpeech('banana', /3 of 3/)
        .expectBraille('banana 3/3');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ContinuousRead', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksAndHeadingsDoc, function() {
    mockFeedback.expectSpeech('start')
        .call(doCmd('readFromHere'))
        .expectSpeech(
            'start', 'alpha', 'Link', 'beta', 'Link', 'charlie', 'Heading 1');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'InitialFocus', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<a href="a">a</a>', function(rootNode) {
    mockFeedback.expectSpeech('a').expectSpeech('Link');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AriaLabel', function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<a aria-label="foo" href="a">a</a>';
  this.runWithLoadedTree(site, function(rootNode) {
    rootNode.find({role: RoleType.LINK}).focus();
    mockFeedback.expectSpeech('foo')
        .expectSpeech('Link')
        .expectSpeech('Press Search+Space to activate')
        .expectBraille('foo lnk');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ShowContextMenu', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>before</p><a href="a">a</a>', function(rootNode) {
    const go = rootNode.find({role: RoleType.LINK});
    mockFeedback.call(go.focus.bind(go))
        .expectSpeech('a', 'Link')
        .call(doCmd('contextMenu'))
        .expectSpeech(/menu opened/)
        .call(press(KeyCode.ESCAPE))
        .expectSpeech('a', 'Link');
    mockFeedback.replay();
  }.bind(this));
});

TEST_F('ChromeVoxBackgroundTest', 'BrailleRouting', function() {
  const mockFeedback = this.createMockFeedback();
  const route = function(position) {
    assertTrue(ChromeVoxState.instance.onBrailleKeyEvent(
        {command: BrailleKeyCommand.ROUTING, displayPosition: position},
        mockFeedback.lastMatchedBraille));
  };
  const site = `
    <p>start</p>
    <button type="button" id="btn1">Click me</button>
    <p>Some text</p>
    <button type="button" id="btn2">Focus me</button>
    <p>Some more text</p>
    <input type="text" id ="text" value="Edit me">
    <script>
      document.getElementById('btn1').addEventListener('click', function() {
        document.getElementById('btn2').focus();
      }, false);
    </script>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    const button1 =
        rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Click me'}});
    const textField = rootNode.find({role: RoleType.TEXT_FIELD});
    mockFeedback.expectBraille('start')
        .call(button1.focus.bind(button1))
        .expectBraille(/^Click me btn/)
        .call(route.bind(null, 5))
        .expectBraille(/Focus me btn/)
        .call(textField.focus.bind(textField))
        .expectBraille('Edit me ed', {startIndex: 0})
        .call(route.bind(null, 3))
        .expectBraille('Edit me ed', {startIndex: 3})
        .call(function() {
          assertEquals(3, textField.textSelStart);
        });
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'FocusInputElement', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
      <input id="name" value="Lancelot">
      <input id="quest" value="Grail">
      <input id="color" value="Blue">
    `;
  this.runWithLoadedTree(site, function(rootNode) {
    const name = rootNode.find({attributes: {value: 'Lancelot'}});
    const quest = rootNode.find({attributes: {value: 'Grail'}});
    const color = rootNode.find({attributes: {value: 'Blue'}});

    mockFeedback.call(quest.focus.bind(quest))
        .expectSpeech('Grail', 'Edit text')
        .call(color.focus.bind(color))
        .expectSpeech('Blue', 'Edit text')
        .call(name.focus.bind(name))
        .expectNextSpeechUtteranceIsNot('Blue')
        .expectSpeech('Lancelot', 'Edit text');
    mockFeedback.replay();
  }.bind(this));
});

TEST_F('ChromeVoxBackgroundTest', 'UseEditableState', function() {
  const site = `
      <input type="text"></input>
      <p tabindex=0>hi</p>
    `;
  this.runWithLoadedTree(site, async function(rootNode) {
    const nonEditable = rootNode.find({role: RoleType.PARAGRAPH});
    const editable = rootNode.find({role: RoleType.TEXT_FIELD});

    nonEditable.focus();
    await new Promise(resolve => {
      this.listenOnce(nonEditable, 'focus', resolve);
    });
    assertTrue(!DesktopAutomationHandler.instance.textEditHandler);

    editable.focus();
    await new Promise(resolve => {
      this.listenOnce(editable, 'focus', resolve);
    });
    assertNotNullNorUndefined(
        DesktopAutomationHandler.instance.textEditHandler);
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EarconsForControls', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
      <p>Initial focus will be on something that's not a control.</p>
      <a href="#">MyLink</a>
      <button>MyButton</button>
      <input type=checkbox>
      <input type=checkbox checked>
      <input>
      <select multiple><option>1</option></select>
      <select><option>2</option></select>
      <input type=range value=5>
    `;
  this.runWithLoadedTree(site, function(rootNode) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('MyLink')
        .expectEarcon(Earcon.LINK)
        .call(doCmd('nextObject'))
        .expectSpeech('MyButton')
        .expectEarcon(Earcon.BUTTON)
        .call(doCmd('nextObject'))
        .expectSpeech('Check box')
        .expectEarcon(Earcon.CHECK_OFF)
        .call(doCmd('nextObject'))
        .expectSpeech('Check box')
        .expectEarcon(Earcon.CHECK_ON)
        .call(doCmd('nextObject'))
        .expectSpeech('Edit text')
        .expectEarcon(Earcon.EDITABLE_TEXT)

        // Editable text Search re-mappings are in effect.
        .call(doCmd('toggleStickyMode'))
        .expectSpeech('Sticky mode enabled')
        .call(doCmd('nextObject'))
        .expectSpeech('List box')
        .expectEarcon(Earcon.LISTBOX)
        .call(doCmd('nextObject'))
        .expectSpeech('Button', 'has pop up')
        .expectEarcon(Earcon.POP_UP_BUTTON)
        .call(doCmd('nextObject'))
        .expectSpeech(/Slider/)
        .expectEarcon(Earcon.SLIDER);

    mockFeedback.replay();
  }.bind(this));
});

TEST_F('ChromeVoxBackgroundTest', 'GlobsToRegExp', function() {
  this.newCallback(async () => {
    const module = await import('./background.js');
    const Background = module.Background;
    assertEquals('/^()$/', Background.globsToRegExp_([]).toString());
    assertEquals(
        '/^(http:\\/\\/host\\/path\\+here)$/',
        Background.globsToRegExp_(['http://host/path+here']).toString());
    assertEquals(
        '/^(url1.*|u.l2|.*url3)$/',
        Background.globsToRegExp_(['url1*', 'u?l2', '*url3']).toString());
  })();
});

TEST_F('ChromeVoxBackgroundTest', 'ShouldNotFocusIframe', function() {
  const site = `
    <iframe tabindex=0 src="data:text/html,<p>Inside</p>"></iframe>
    <button>outside</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    const iframe = root.find({role: RoleType.IFRAME});
    const button = root.find({role: RoleType.BUTTON});

    assertEquals('iframe', iframe.role);
    assertEquals('button', button.role);

    let didFocus = false;
    iframe.addEventListener('focus', function() {
      didFocus = true;
    });
    const b = ChromeVoxState.instance;
    b.currentRange_ = cursors.Range.fromNode(button);
    doCmd('previousElement');
    assertFalse(didFocus);
  }.bind(this));
});

TEST_F('ChromeVoxBackgroundTest', 'ShouldFocusLink', function() {
  const site = `
    <div><a href="#">mylink</a></div>
    <button>after</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    const link = root.find({role: RoleType.LINK});
    const button = root.find({role: RoleType.BUTTON});

    assertEquals('link', link.role);
    assertEquals('button', button.role);

    const didFocus = false;
    link.addEventListener('focus', this.newCallback(function() {
      // Success
    }));
    const b = ChromeVoxState.instance;
    b.currentRange_ = cursors.Range.fromNode(button);
    doCmd('previousElement');
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NoisySlider', function() {
  const mockFeedback = this.createMockFeedback();
  // Slider aria-valuetext must change otherwise blink suppresses event.
  const site = `
    <button id="go">go</button>
    <div id="slider" tabindex=0 role="slider"></div>
    <script>
      function update() {
        let s = document.getElementById('slider');
        s.setAttribute('aria-valuetext', '');
        s.setAttribute('aria-valuetext', 'noisy');
        setTimeout(update, 500);
      }
      update();
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const go = root.find({role: RoleType.BUTTON});
    const slider = root.find({role: RoleType.SLIDER});
    const focusButton = go.focus.bind(go);
    const focusSlider = slider.focus.bind(slider);
    mockFeedback.call(focusButton)
        .expectNextSpeechUtteranceIsNot('noisy')
        .call(focusSlider)
        .expectSpeech('noisy')
        .expectSpeech('noisy')
        .replay();
  }.bind(this));
});

TEST_F('ChromeVoxBackgroundTest', 'Checkbox', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="go" role="checkbox">go</div>
    <script>
      let go = document.getElementById('go');
      let isChecked = true;
      go.addEventListener('click', function(e) {
        if (isChecked) {
          go.setAttribute('aria-checked', true);
        } else {
          go.removeAttribute('aria-checked');
        }
        isChecked = !isChecked;
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const cbx = root.find({role: RoleType.CHECK_BOX});
    const click = cbx.doDefault.bind(cbx);
    const focus = cbx.focus.bind(cbx);
    mockFeedback.call(focus)
        .expectSpeech('go')
        .expectSpeech('Check box')
        .expectSpeech('Not checked')
        .call(click)
        .expectSpeech('go')
        .expectSpeech('Check box')
        .expectSpeech('Checked')
        .call(click)
        .expectSpeech('go')
        .expectSpeech('Check box')
        .expectSpeech('Not checked')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'MixedCheckbox', function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<div id="go" role="checkbox" aria-checked="mixed">go</div>';
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('go', 'Check box', 'Partially checked').replay();
  });
});

/** Tests navigating into and out of iframes using nextButton */
TEST_F(
    'ChromeVoxBackgroundTest', 'ForwardNavigationThroughIframeButtons',
    function() {
      const mockFeedback = this.createMockFeedback();

      let running = false;
      const runTestIfIframeIsLoaded = function(rootNode) {
        if (running) {
          return;
        }

        // Return if the iframe hasn't loaded yet.
        const iframe = rootNode.find({role: RoleType.IFRAME});
        const childDoc = iframe.firstChild;
        if (!childDoc || childDoc.children.length === 0) {
          return;
        }

        running = true;
        const beforeButton =
            rootNode.find({role: RoleType.BUTTON, name: 'Before'});
        beforeButton.focus();
        mockFeedback.expectSpeech('Before', 'Button');
        mockFeedback.call(doCmd('nextButton')).expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('nextButton')).expectSpeech('After', 'Button');
        mockFeedback.call(doCmd('previousButton'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('previousButton'))
            .expectSpeech('Before', 'Button');
        mockFeedback.replay();
      }.bind(this);

      this.runWithLoadedTree(this.iframesDoc, function(rootNode) {
        chrome.automation.getDesktop(function(desktopNode) {
          runTestIfIframeIsLoaded(rootNode);

          desktopNode.addEventListener('loadComplete', function(evt) {
            runTestIfIframeIsLoaded(rootNode);
          }, true);
        });
      });
    });

/** Tests navigating into and out of iframes using nextObject */
TEST_F(
    'ChromeVoxBackgroundTest', 'ForwardObjectNavigationThroughIframes',
    function() {
      const mockFeedback = this.createMockFeedback();

      let running = false;
      const runTestIfIframeIsLoaded = function(rootNode) {
        if (running) {
          return;
        }

        // Return if the iframe hasn't loaded yet.
        const iframe = rootNode.find({role: 'iframe'});
        const childDoc = iframe.firstChild;
        if (!childDoc || childDoc.children.length === 0) {
          return;
        }

        running = true;
        const suppressFocusActionOutput = function() {
          DesktopAutomationHandler.announceActions = false;
        };
        const beforeButton =
            rootNode.find({role: RoleType.BUTTON, name: 'Before'});
        mockFeedback.call(beforeButton.focus.bind(beforeButton))
            .expectSpeech('Before', 'Button')
            .call(suppressFocusActionOutput)
            .call(doCmd('nextObject'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('nextObject'))
            .expectSpeech('Inside', 'Heading 1');
        mockFeedback.call(doCmd('nextObject')).expectSpeech('After', 'Button');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Inside', 'Heading 1');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Before', 'Button');
        mockFeedback.replay();
      }.bind(this);

      this.runWithLoadedTree(this.iframesDoc, function(rootNode) {
        chrome.automation.getDesktop(function(desktopNode) {
          runTestIfIframeIsLoaded(rootNode);

          desktopNode.addEventListener('loadComplete', function(evt) {
            runTestIfIframeIsLoaded(rootNode);
          }, true);
        });
      });
    });

TEST_F('ChromeVoxBackgroundTest', 'SelectOptionSelected', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <select>
      <option>apple
      <option>banana
      <option>grapefruit
    </select>
  `;
  this.runWithLoadedTree(site, function(root) {
    const select = root.find({role: RoleType.POP_UP_BUTTON});
    const clickSelect = select.doDefault.bind(select);
    const selectLastOption = () => {
      const options = select.findAll({role: RoleType.LIST_BOX_OPTION});
      options[options.length - 1].doDefault();
    };

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Button', 'Press Search+Space to activate')
        .call(clickSelect)
        .expectSpeech('apple')
        .expectSpeech('Button')
        .expectSpeech('Expanded')
        .call(selectLastOption)
        .expectNextSpeechUtteranceIsNot('apple')
        .expectSpeech('grapefruit')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ToggleButton', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div aria-pressed="mixed" role="button">boldface</div>
    <div aria-pressed="true" role="button">ok</div>
    <div aria-pressed="false" role="button">cancel</div>
    <div aria-pressed role="button">close</div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const b = ChromeVoxState.instance;
    const move = doCmd('nextObject');
    mockFeedback.call(move)
        .expectSpeech('boldface')
        .expectSpeech('Toggle Button')
        .expectSpeech('Partially pressed')

        .call(move)
        .expectSpeech('ok')
        .expectSpeech('Toggle Button')
        .expectSpeech('Pressed')

        .call(move)
        .expectSpeech('cancel')
        .expectSpeech('Toggle Button')
        .expectSpeech('Not pressed')

        .call(move)
        .expectSpeech('close')
        .expectSpeech('Button')

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EditText', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <input type="text"></input>
    <input role="combobox" type="text"></input>
  `;
  this.runWithLoadedTree(site, function(root) {
    const nextEditText = doCmd('nextEditText');
    const previousEditText = doCmd('previousEditText');
    mockFeedback.call(nextEditText)
        .expectSpeech('Combo box')
        .call(previousEditText)
        .expectSpeech('Edit text')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ComboBox', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.comboBoxDoc, function() {
    mockFeedback.expectSpeech('Edit text', 'Choose an item', 'Combo box')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'BackwardForwardSync', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div aria-label="Group" role="group" tabindex=0>
      <input type="text"></input>
    </div>
    <ul>
      <li tabindex=0>
        <button>ok</button>
      </li>
    </ul>
  `;
  this.runWithLoadedTree(site, function(root) {
    const listItem = root.find({role: RoleType.LIST_ITEM});

    mockFeedback.call(listItem.focus.bind(listItem))
        .expectSpeech('ok', 'List item')
        .call(this.doCmd('nextObject'))
        .expectSpeech('\u2022 ')  // bullet
        .call(this.doCmd('nextObject'))
        .expectSpeech('Button')
        .call(this.doCmd('previousObject'))
        .expectSpeech('\u2022 ')  // bullet
        .call(this.doCmd('previousObject'))
        .expectSpeech('List item')
        .call(this.doCmd('previousObject'))
        .expectSpeech('Edit text')
        .call(this.doCmd('previousObject'))
        .expectSpeech('Group')
        .replay();
  });
});

/** Tests that navigation works when the current object disappears. */
TEST_F('ChromeVoxBackgroundTest', 'DisappearingObject', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.disappearingObjectDoc, function(rootNode) {
    const deleteButton =
        rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Delete'}});
    const pressDelete = deleteButton.doDefault.bind(deleteButton);
    mockFeedback.expectSpeech('start').expectBraille('start');

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Before1')
        .call(doCmd('nextObject'))
        .expectSpeech('Before2')
        .call(doCmd('nextObject'))
        .expectSpeech('Before3')
        .call(doCmd('nextObject'))
        .expectSpeech('Disappearing')
        .call(pressDelete)
        .expectSpeech('Deleted')
        .call(doCmd('nextObject'))
        .expectSpeech('After1')
        .call(doCmd('nextObject'))
        .expectSpeech('After2')
        .call(doCmd('previousObject'))
        .expectSpeech('After1')
        .call(doCmd('dumpTree'));

    /*
        // This is broken by cl/1260523 making tree updating (more)
       asynchronous.
        // TODO(aboxhall/dtseng): Add a function to wait for next tree update?
        mockFeedback
            .call(doCmd('previousObject'))
            .expectSpeech('Before3');
    */

    mockFeedback.replay();
  });
});

/** Tests that focus jumps to details properly when indicated. */
TEST_F('ChromeVoxBackgroundTest', 'JumpToDetails', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.detailsDoc, function(rootNode) {
    mockFeedback.call(doCmd('jumpToDetails')).expectSpeech('Details');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ButtonNameValueDescription', function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<input type="submit" aria-label="foo" value="foo"></input>';
  this.runWithLoadedTree(site, function(root) {
    const btn = root.find({role: RoleType.BUTTON});
    mockFeedback.call(btn.focus.bind(btn))
        .expectSpeech('foo')
        .expectSpeech('Button')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NameFromHeadingLink', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before</p>
    <h1><a href="google.com">go</a><p>here</p></h1>
  `;
  this.runWithLoadedTree(site, function(root) {
    const link = root.find({role: RoleType.LINK});
    mockFeedback.call(link.focus.bind(link))
        .expectSpeech('go')
        .expectSpeech('Link')
        .expectSpeech('Heading 1')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'OptionChildIndexCount', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="listbox">
      <p>Fruits</p>
      <div role="option">apple</div>
      <div role="option">banana</div>
    </div>
  `;

  this.runWithLoadedTree(site, function(root) {
    // Select first child of the list box, similar to what happens if navigated
    // by Tab.
    const firstChild = root.find({role: RoleType.PARAGRAPH});
    mockFeedback
        .call(
            () => ChromeVoxState.instance.setCurrentRange(
                cursors.Range.fromNode(firstChild)))
        .call(doCmd('nextObject'))
        .expectSpeech('List box')
        .expectSpeech('Fruits')
        .call(doCmd('nextObject'))
        .expectSpeech('apple')
        .expectSpeech(' 1 of 2 ')
        .call(doCmd('nextObject'))
        .expectSpeech('banana')
        .expectSpeech(' 2 of 2 ')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ListMarkerIsIgnored', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<ul><li>apple</ul>', function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectNextSpeechUtteranceIsNot('listMarker')
        .expectSpeech('\u2022 apple')  // bullet apple
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SymetricComplexHeading', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h4><p>NW</p><p>NE</p></h4>
    <h4><p>SW</p><p>SE</p></h4>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextHeading'))
        .expectNextSpeechUtteranceIsNot('NE')
        .expectSpeech('NW')
        .call(doCmd('previousHeading'))
        .expectNextSpeechUtteranceIsNot('NE')
        .expectSpeech('NW')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ContentEditableJumpSyncsRange', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div contenteditable>
      <h1>Top News</h1>
      <h1>Most Popular</h1>
      <h1>Sports</h1>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const assertRangeHasText = function(text) {
      return function() {
        assertEquals(
            text, ChromeVoxState.instance.getCurrentRange().start.node.name);
      };
    };

    mockFeedback.call(doCmd('nextEditText'))
        .expectSpeech('Top News Most Popular Sports')
        .call(doCmd('nextHeading'))
        .expectSpeech('Top News')
        .call(assertRangeHasText('Top News'))
        .call(doCmd('nextHeading'))
        .expectSpeech('Most Popular')
        .call(assertRangeHasText('Most Popular'))
        .call(doCmd('nextHeading'))
        .expectSpeech('Sports')
        .call(assertRangeHasText('Sports'))
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'Selection', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>simple</p>
    <p>doc</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    // Fakes a toggleSelection command.
    root.addEventListener('textSelectionChanged', function() {
      if (root.focusOffset === 3) {
        CommandHandler.onCommand('toggleSelection');
      }
    }, true);

    mockFeedback.call(doCmd('toggleSelection'))
        .expectSpeech('simple', 'selected')
        .call(doCmd('nextCharacter'))
        .expectSpeech('i', 'selected')
        .call(doCmd('previousCharacter'))
        .expectSpeech('i', 'unselected')
        .call(doCmd('nextCharacter'))
        .call(doCmd('nextCharacter'))
        .expectSpeech('End selection', 'sim')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'BasicTableCommands', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
  <table border=1>
    <tr><td>name</td><td>title</td><td>address</td><td>phone</td></tr>
    <tr><td>Dan</td><td>Mr</td><td>666 Elm Street</td><td>212 222 5555</td></tr>
  </table>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextRow'))
        .expectSpeech('Dan', 'row 2 column 1')
        .call(doCmd('previousRow'))
        .expectSpeech('name', 'row 1 column 1')
        .call(doCmd('previousRow'))
        .expectSpeech('No cell above')
        .call(doCmd('nextCol'))
        .expectSpeech('title', 'row 1 column 2')
        .call(doCmd('nextRow'))
        .expectSpeech('Mr', 'row 2 column 2')
        .call(doCmd('previousRow'))
        .expectSpeech('title', 'row 1 column 2')
        .call(doCmd('nextCol'))
        .expectSpeech('address', 'row 1 column 3')
        .call(doCmd('nextCol'))
        .expectSpeech('phone', 'row 1 column 4')
        .call(doCmd('nextCol'))
        .expectSpeech('No cell right')
        .call(doCmd('previousRow'))
        .expectSpeech('No cell above')
        .call(doCmd('nextRow'))
        .expectSpeech('212 222 5555', 'row 2 column 4')
        .call(doCmd('nextRow'))
        .expectSpeech('No cell below')
        .call(doCmd('nextCol'))
        .expectSpeech('No cell right')
        .call(doCmd('previousCol'))
        .expectSpeech('666 Elm Street', 'row 2 column 3')
        .call(doCmd('previousCol'))
        .expectSpeech('Mr', 'row 2 column 2')

        .call(doCmd('goToRowLastCell'))
        .expectSpeech('212 222 5555', 'row 2 column 4')
        .call(doCmd('goToRowLastCell'))
        .expectSpeech('212 222 5555')
        .call(doCmd('goToRowFirstCell'))
        .expectSpeech('Dan', 'row 2 column 1')
        .call(doCmd('goToRowFirstCell'))
        .expectSpeech('Dan')

        .call(doCmd('goToColFirstCell'))
        .expectSpeech('name', 'row 1 column 1')
        .call(doCmd('goToColFirstCell'))
        .expectSpeech('name')
        .call(doCmd('goToColLastCell'))
        .expectSpeech('Dan', 'row 2 column 1')
        .call(doCmd('goToColLastCell'))
        .expectSpeech('Dan')

        .call(doCmd('goToLastCell'))
        .expectSpeech('212 222 5555', 'row 2 column 4')
        .call(doCmd('goToLastCell'))
        .expectSpeech('212 222 5555')
        .call(doCmd('goToFirstCell'))
        .expectSpeech('name', 'row 1 column 1')
        .call(doCmd('goToFirstCell'))
        .expectSpeech('name')

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'MissingTableCells', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
  <table border=1>
    <tr><td>a</td><td>b</td><td>c</td></tr>
    <tr><td>d</td><td>e</td></tr>
    <tr><td>f</td></tr>
  </table>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('goToRowLastCell'))
        .expectSpeech('c', 'row 1 column 3')
        .call(doCmd('goToRowLastCell'))
        .expectSpeech('c')
        .call(doCmd('goToRowFirstCell'))
        .expectSpeech('a', 'row 1 column 1')
        .call(doCmd('goToRowFirstCell'))
        .expectSpeech('a')

        .call(doCmd('nextCol'))
        .expectSpeech('b', 'row 1 column 2')

        .call(doCmd('goToColLastCell'))
        .expectSpeech('e', 'row 2 column 2')
        .call(doCmd('goToColLastCell'))
        .expectSpeech('e')
        .call(doCmd('goToColFirstCell'))
        .expectSpeech('b', 'row 1 column 2')
        .call(doCmd('goToColFirstCell'))
        .expectSpeech('b')

        .call(doCmd('goToFirstCell'))
        .expectSpeech('a', 'row 1 column 1')
        .call(doCmd('goToFirstCell'))
        .expectSpeech('a')
        .call(doCmd('goToLastCell'))
        .expectSpeech('f', 'row 3 column 1')
        .call(doCmd('goToLastCell'))
        .expectSpeech('f')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'DisabledState', function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<button aria-disabled="true">ok</button>';
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('ok', 'Disabled', 'Button').replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'HeadingLevels', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h1>1</h1><h2>2</h2><h3>3</h3><h4>4</h4><h5>5</h5><h6>6</h6>
  `;
  this.runWithLoadedTree(site, function(root) {
    const makeLevelAssertions = function(level) {
      mockFeedback.call(doCmd('nextHeading' + level))
          .expectSpeech('Heading ' + level)
          .call(doCmd('nextHeading' + level))
          .expectEarcon('wrap')
          .call(doCmd('previousHeading' + level))
          .expectEarcon('wrap');
    };
    for (let i = 1; i <= 6; i++) {
      makeLevelAssertions(i);
    }
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EditableNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable>this is a test</div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('this is a test')
        .call(doCmd('nextObject'))
        .expectSpeech('this is a test')
        .call(doCmd('nextWord'))
        .expectSpeech('is')
        .call(doCmd('nextWord'))
        .expectSpeech('a')
        .call(doCmd('nextWord'))
        .expectSpeech('test')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigationMovesFocus', function() {
  const site = `
    <p>start</p>
    <input type="text"></input>
  `;
  this.runWithLoadedTree(site, function(root) {
    this.listenOnce(
        root.find({role: RoleType.TEXT_FIELD}), 'focus', function(e) {
          const focus = ChromeVoxState.instance.currentRange.start.node;
          assertEquals(RoleType.TEXT_FIELD, focus.role);
          assertTrue(focus.state.focused);
        });
    doCmd('nextEditText')();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'BrailleCaretNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>This is a<em>test</em> of inline braille<br>with a second line</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    const text = 'This is a';
    mockFeedback.call(doCmd('nextCharacter'))
        .expectBraille(text, {startIndex: 1, endIndex: 2})  // h
        .call(doCmd('nextCharacter'))
        .expectBraille(text, {startIndex: 2, endIndex: 3})  // i
        .call(doCmd('nextWord'))
        .expectBraille(text, {startIndex: 5, endIndex: 7})  // is
        .call(doCmd('previousWord'))
        .expectBraille(text, {startIndex: 0, endIndex: 4})  // This
        .call(doCmd('nextLine'))
        // Ensure nothing is selected when the range covers the entire line.
        .expectBraille('with a second line', {startIndex: -1, endIndex: -1})
        .replay();
  });
});

// This tests ChromeVox's special support for following an in-page link
// if you force-click on it. Compare with InPageLinks, below.
TEST_F('ChromeVoxBackgroundTest', 'ForceClickInPageLinks', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="#there">hi</a>
    <button id="there">there</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('hi', 'Internal link')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('there', 'Button')
        .replay();
  });
});

// This tests ChromeVox's handling of the scrolledToAnchor event, which is
// fired when the users follows an in-page link or the document otherwise
// gets navigated to an in-page link target by the url fragment changing,
// not necessarily due to directly clicking on the link via ChromeVox.
//
// Note: this test needs the test server running because the browser
// does not follow same-page links on data urls (because it modifies the
// url fragment, and any change to the url is disallowed for a data url).
TEST_F('ChromeVoxBackgroundTestWithTestServer', 'InPageLinks', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(undefined, function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Jump', 'Internal link')
        .call(press(KeyCode.RETURN))
        .expectSpeech('Found It')
        .call(doCmd('nextHeading'))
        .expectSpeech('Continue Here', 'Heading 2')
        .replay();
  }.bind(this), {
    url: `${testRunnerParams.testServerBaseUrl}accessibility/in_page_links.html`
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ListItem', function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <ul><li>apple<li>grape<li>banana</ul>
    <ol><li>pork<li>beef<li>chicken</ol>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextLine'))
        .expectSpeech('\u2022 apple', 'List item')
        .expectBraille('\u2022 apple lstitm lst +3')
        .call(doCmd('nextLine'))
        .expectSpeech('\u2022 grape', 'List item')
        .expectBraille('\u2022 grape lstitm')
        .call(doCmd('nextLine'))
        .expectSpeech('\u2022 banana', 'List item')
        .expectBraille('\u2022 banana lstitm lst end')

        // Object nav should be the same.
        .call(doCmd('nextObject'))
        .expectSpeech('1. pork', 'List item')
        .expectBraille('1. pork lstitm lst +3')
        .call(doCmd('nextObject'))
        .expectSpeech('2. beef', 'List item')
        .expectBraille('2. beef lstitm')

        // Mixing with line nav.
        .call(doCmd('nextLine'))
        .expectSpeech('3. chicken', 'List item')
        .expectBraille('3. chicken lstitm lst end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'BusyHeading', function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <h2><a href="#">Lots</a><a href="#">going</a><a href="#">here</a></h2>
  `;
  this.runWithLoadedTree(site, function(root) {
    // In the past, this would have inserted the 'heading 2' after the first
    // link's output. Make sure it goes to the end.
    mockFeedback.call(doCmd('nextLine'))
        .expectSpeech(
            'Lots', 'Link', 'going', 'Link', 'here', 'Link', 'Heading 2')
        .expectBraille('Lots lnk going lnk here lnk h2')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NodeVsSubnode', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<a href="#">test</a>', function(root) {
    const link = root.find({role: RoleType.LINK});
    function outputLinkRange(start, end) {
      return function() {
        new Output()
            .withSpeech(new cursors.Range(
                new cursors.Cursor(link, start), new cursors.Cursor(link, end)))
            .go();
      };
    }

    mockFeedback.call(outputLinkRange(0, 0))
        .expectSpeech('test', 'Internal link')
        .call(outputLinkRange(0, 1))
        .expectSpeech('t')
        .call(outputLinkRange(1, 1))
        .expectSpeech('test', 'Internal link')
        .call(outputLinkRange(1, 2))
        .expectSpeech('e')
        .call(outputLinkRange(1, 3))
        .expectNextSpeechUtteranceIsNot('Internal link')
        .expectSpeech('es')
        .call(outputLinkRange(0, 4))
        .expectSpeech('test', 'Internal link')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NativeFind', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="#">grape</a>
    <a href="#">pineapple</a>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(press(KeyCode.F, {ctrl: true}))
        .expectSpeech('Find', 'Edit text')
        .call(press(KeyCode.G))
        .expectSpeech('grape', 'Link')
        .call(press(KeyCode.BACK))
        .call(press(KeyCode.L))
        .expectSpeech('pineapple', 'Link')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EditableKeyCommand', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <input type="text"></input>
    <textarea>test</textarea>
    <div role="textbox" contenteditable>test</div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const assertCurNode = function(node) {
      return function() {
        assertEquals(node, ChromeVoxState.instance.currentRange.start.node);
      };
    };

    const textField = root.firstChild;
    const textArea = textField.nextSibling;
    const contentEditable = textArea.nextSibling;

    mockFeedback.call(assertCurNode(textField))
        .call(doCmd('nextObject'))
        .call(assertCurNode(textArea))
        .call(doCmd('nextObject'))
        .call(assertCurNode(contentEditable))
        .call(doCmd('previousObject'))
        .expectSpeech('Text area')
        .call(assertCurNode(textArea))
        .call(doCmd('previousObject'))
        .call(assertCurNode(textField))

        .replay();
  });
});

// TODO(crbug.com/935678): Test times out flakily in MSAN builds.
TEST_F_WITH_PREAMBLE(
    `
#if defined(MEMORY_SANITIZER)
#define MAYBE_TextSelectionAndLiveRegion DISABLED_TextSelectionAndLiveRegion
#else
#define MAYBE_TextSelectionAndLiveRegion TextSelectionAndLiveRegion
#endif
`,
    'ChromeVoxBackgroundTest', 'MAYBE_TextSelectionAndLiveRegion', function() {
      DesktopAutomationHandler.announceActions = true;
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
    <p>start</p>
    <div><input value="test" type="text"></input></div>
    <div id="live" aria-live="assertive"></div>
    <script>
      const input = document.querySelector('input');
      const [div, live] = document.querySelectorAll('div');
      let clicks = 0;
      div.addEventListener('click', function() {
        clicks++;
        if (clicks == 1) {
          live.textContent = 'go';
        } else if (clicks == 2) {
          input.selectionStart = 1;
          live.textContent = 'queued';
        } else {
          input.selectionStart = 2;
          live.textContent = 'interrupted';
        }
      });
    </script>
  `,
          function(root) {
            const textField = root.find({role: RoleType.TEXT_FIELD});
            const div = textField.parent;
            mockFeedback.call(textField.focus.bind(textField))
                .expectSpeech('Edit text')
                .call(div.doDefault.bind(div))
                .expectSpeechWithQueueMode('go', QueueMode.CATEGORY_FLUSH)

                .call(div.doDefault.bind(div))
                .expectSpeechWithQueueMode('queued', QueueMode.QUEUE)
                .expectSpeechWithQueueMode('e', QueueMode.CATEGORY_FLUSH)

                .call(div.doDefault.bind(div))
                .expectSpeechWithQueueMode('interrupted', QueueMode.QUEUE)
                .expectSpeechWithQueueMode('s', QueueMode.CATEGORY_FLUSH)

                .replay();
          });
    });

TEST_F('ChromeVoxBackgroundTest', 'TableColumnHeaders', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="grid">
      <div role="rowgroup">
        <div role="row">
          <div role="columnheader">city</div>
          <div role="columnheader">state</div>
          <div role="columnheader">zip</div>
        </div>
      </div>
      <div role="rowgroup">
        <div role="row">
          <div role="gridcell">Mountain View</div>
          <div role="gridcell">CA</div>
          <div role="gridcell">94043</div>
        </div>
        <div role="row">
          <div role="gridcell">San Jose</div>
          <div role="gridcell">CA</div>
          <div role="gridcell">95128</div>
        </div>
      </div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextRow'))
        .expectSpeech('Mountain View', 'row 2 column 1')
        .call(doCmd('nextRow'))
        .expectNextSpeechUtteranceIsNot('city')
        .expectSpeech('San Jose', 'row 3 column 1')
        .call(doCmd('nextCol'))
        .expectSpeech('CA', 'row 3 column 2', 'state')
        .call(doCmd('previousRow'))
        .expectSpeech('CA', 'row 2 column 2')
        .call(doCmd('previousRow'))
        .expectSpeech('state', 'row 1 column 2')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ActiveDescendantUpdates', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div aria-label="container" tabindex=0 role="group" id="active"
        aria-activedescendant="1">
      <div id="1" role="treeitem" aria-selected="false"></div>
      <div id="2" role="treeitem" aria-selected="true"></div>
    <script>
      let alt = false;
      let active = document.getElementById('active');
      let one = document.getElementById('1');
      let two = document.getElementById('2');
      active.addEventListener('click', function() {
        let descendant = alt ? one : two;
        active.setAttribute('aria-activedescendant', descendant.id);
        alt = !alt;
      });
      </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const group = root.firstChild;
    mockFeedback.call(group.focus.bind(group))
        .call(group.doDefault.bind(group))
        .expectSpeech('Tree item', ' 2 of 2 ')
        .call(group.doDefault.bind(group))
        .expectSpeech('Tree item', 'Not selected', ' 1 of 2 ')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigationEscapesEdit', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before content editable</p>
    <div role="textbox" contenteditable>this<br>is<br>a<br>test</div>
    <p>after content editable, before text area</p>
    <textarea style="word-spacing: 1000px">this is a test</textarea>
    <p>after text area</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    const assertBeginning = function(expected) {
      const textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
      assertNotNullNorUndefined(textEditHandler);
      assertEquals(expected, textEditHandler.isSelectionOnFirstLine());
    };
    const assertEnd = function(expected) {
      const textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
      assertNotNullNorUndefined(textEditHandler);
      assertEquals(expected, textEditHandler.isSelectionOnLastLine());
    };
    const [contentEditable, textArea] =
        root.findAll({role: RoleType.TEXT_FIELD});

    this.listenOnce(contentEditable, EventType.FOCUS, function() {
      mockFeedback.call(assertBeginning.bind(this, true))
          .call(assertEnd.bind(this, false))

          .call(press(KeyCode.DOWN))
          .expectSpeech('is')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, false))

          .call(press(KeyCode.DOWN))
          .expectSpeech('a')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, false))

          .call(press(KeyCode.DOWN))
          .expectSpeech('test')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, true))

          .call(textArea.focus.bind(textArea))
          .expectSpeech('Text area')
          .call(assertBeginning.bind(this, true))
          .call(assertEnd.bind(this, false))

          .call(press(40 /* ArrowDown */))
          .expectSpeech('is')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, false))

          .call(press(40 /* ArrowDown */))
          .expectSpeech('a')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, false))

          .call(press(40 /* ArrowDown */))
          .expectSpeech('test')
          .call(assertBeginning.bind(this, false))
          .call(assertEnd.bind(this, true))

          .replay();

      // TODO: soft line breaks currently won't work in <textarea>.
    }.bind(this));
    contentEditable.focus();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SelectDoesNotSyncNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <select>
      <option>apple</option>
      <option>grape</option>
    </select>
  `;
  this.runWithLoadedTree(site, function(root) {
    const select = root.find({role: RoleType.POP_UP_BUTTON});
    mockFeedback.expectSpeech('Button', 'has pop up', 'Collapsed')
        .call(select.doDefault.bind(select))
        .expectSpeech('Expanded')
        .call(
            () => assertEquals(
                select, ChromeVoxState.instance.currentRange.start.node))
        .call(press(KeyCode.DOWN))
        .expectSpeech('grape', 'List item', ' 2 of 2 ')
        .call(
            () => assertEquals(
                select, ChromeVoxState.instance.currentRange.start.node))
        .call(press(KeyCode.UP))
        .expectSpeech('apple', 'List item', ' 1 of 2 ')
        .call(
            () => assertEquals(
                select, ChromeVoxState.instance.currentRange.start.node))
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigationIgnoresLabels', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before</p>
    <p id="label">label</p>
    <a href="#next" id="lebal">lebal</a>
    <h2 id="headingLabel">headingLabel</h2>
    <p>after</p>
    <button aria-labelledby="label headingLabel"></button>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('before')
        .call(doCmd('nextObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('nextObject'))
        .expectSpeech('headingLabel', 'Heading 2')
        .call(doCmd('nextObject'))
        .expectSpeech('after')
        .call(doCmd('previousObject'))
        .expectSpeech('headingLabel', 'Heading 2')
        .call(doCmd('previousObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('previousObject'))
        .expectSpeech('before')
        .call(doCmd('nextObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('nextObject'))
        .expectSpeech('headingLabel', 'Heading 2')
        .call(doCmd('nextObject'))
        .expectSpeech('after')
        .call(doCmd('nextObject'))
        .expectSpeech('label headingLabel', 'Button')
        .call(doCmd('nextObject'))
        .expectEarcon(Earcon.WRAP)
        .call(doCmd('nextObject'))
        .expectSpeech('before')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigationIgnoresDescriptions', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before</p>
    <p id="desc">label</p>
    <a href="#next" id="csed">lebal</a>
    <p>after</p>
    <button aria-describedby="desc"></button>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('before')
        .call(doCmd('nextObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('nextObject'))
        .expectSpeech('after')
        .call(doCmd('previousObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('previousObject'))
        .expectSpeech('before')
        .call(doCmd('nextObject'))
        .expectSpeech('lebal', 'Link')
        .call(doCmd('nextObject'))
        .expectSpeech('after')
        .call(doCmd('nextObject'))
        .expectSpeech('label', 'lebal', 'Button')
        .call(doCmd('nextObject'))
        .expectEarcon(Earcon.WRAP)
        .call(doCmd('nextObject'))
        .expectSpeech('before')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'MathContentViaInnerHtml', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="math">
      <semantics>
        <mrow class="MJX-TeXAtom-ORD">
          <mstyle displaystyle="true" scriptlevel="0">
            <mi>a</mi>
            <mo stretchy="false">(</mo>
            <mi>y</mi>
            <mo>+</mo>
            <mi>m</mi>
            <msup>
              <mo stretchy="false">)</mo>
              <mrow class="MJX-TeXAtom-ORD">
                <mn>2</mn>
              </mrow>
            </msup>
            <mo>+</mo>
            <mi>b</mi>
            <mo stretchy="false">(</mo>
            <mi>y</mi>
            <mo>+</mo>
            <mi>m</mi>
            <mo stretchy="false">)</mo>
            <mo>+</mo>
            <mi>c</mi>
            <mo>=</mo>
            <mn>0.</mn>
          </mstyle>
        </mrow>
        <annotation encoding="application/x-tex">{\displaystyle a(y+m)^{2}+b(y+m)+c=0.}</annotation>
      </semantics>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('a ( y + m ) squared + b ( y + m ) + c = 0 .')
        .expectSpeech('Press up, down, left, or right to explore math')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'GestureGranularity', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>This is a test</p>
    <h2>hello</h2>
    <a href="#">greetings</a>
    <h2>here</h2>
    <button>and</button>
    <a href="#">there</a>
    <button>world</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Word')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('is')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('a')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('is')

        .call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Character')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('s')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('i')

        .call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Form field control')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('and', 'Button')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('world', 'Button')

        .call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Link')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('greetings', 'Internal link')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('there', 'Internal link')

        .call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Heading')
        .call(doGesture(Gesture.SWIPE_DOWN1))
        .expectSpeech('hello', 'Heading 2')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('here', 'Heading 2')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('hello', 'Heading 2')

        .call(doGesture(Gesture.SWIPE_LEFT3))
        .expectSpeech('Line')
        .call(doGesture(Gesture.SWIPE_UP1))
        .expectSpeech('This is a test')

        .call(doGesture(Gesture.SWIPE_RIGHT3))
        .expectSpeech('Heading')
        .call(doGesture(Gesture.SWIPE_RIGHT3))
        .expectSpeech('Internal link')
        .call(doGesture(Gesture.SWIPE_RIGHT3))
        .expectSpeech('Form field control')
        .call(doGesture(Gesture.SWIPE_RIGHT3))
        .expectSpeech('Character')

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'LinesFilterWhitespace', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="list">
      <div role="listitem">
        <span>Munich</span>
        <span>London</span>
      </div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('start')
        .clearPendingOutput()
        .call(doCmd('nextLine'))
        .expectSpeech('Munich')
        .expectNextSpeechUtteranceIsNot(' ')
        .expectSpeech('London')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'TabSwitchAndRefreshRecovery', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>tab1</p>', function(root1) {
    this.runWithLoadedTree('<p>tab2</p>', function(root2) {
      mockFeedback.expectSpeech('tab2')
          .clearPendingOutput()
          .call(press(KeyCode.TAB, {shift: true, ctrl: true}))
          .expectSpeech('tab1')
          .clearPendingOutput()
          .call(press(KeyCode.TAB, {ctrl: true}))
          .expectSpeech('tab2')
          .clearPendingOutput()
          .call(press(KeyCode.R, {ctrl: true}))

          // ChromeVox stays on the same node due to tree path recovery.
          .call(() => {
            assertEquals(
                'tab2', ChromeVoxState.instance.currentRange.start.node.name);
          })
          .replay();
    });
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ListName', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="_md-chips-wrapper-76" tabindex="-1" class="md-chips md-readonly"
        aria-setsize="4" aria-label="Favorite Sports" role="list"
        aria-describedby="chipsNote">
      <div role="listitem">Baseball</div>
      <div role="listitem">Hockey</div>
      <div role="listitem">Lacrosse</div>
      <div role="listitem">Football</div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('Favorite Sports').replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'LayoutTable', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <table><tr><td>start</td></tr></table><p>end</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('start')
        .call(doCmd('nextObject'))
        .expectNextSpeechUtteranceIsNot('row 1 column 1')
        .expectNextSpeechUtteranceIsNot('Table , 1 by 1')
        .expectSpeech('end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ReinsertedNodeRecovery', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div>
      <button id="start">start</button>
      <button id="hot">hot</button>
    </div>
    <button id="end">end</button>
    <script>
      let div =       document.body.firstElementChild;
      let start =       document.getElementById('start');
      document.getElementById('hot').addEventListener('focus', (evt) => {
        let hot = evt.target;
        hot.remove();
        div.insertAfter(hot, start);
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('start')
        .clearPendingOutput()
        .call(doCmd('nextObject'))
        .call(doCmd('nextObject'))
        .call(doCmd('nextObject'))
        .expectSpeech('end', 'Button')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PointerTargetsLeafNode', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role=button><p>Washington</p></div>
    <div role=button><p>Adams</p></div>
    <div role=button><p>Jefferson</p></div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const button =
        root.find({role: RoleType.BUTTON, attributes: {name: 'Jefferson'}});
    const buttonP = button.firstChild;
    assertNotNullNorUndefined(buttonP);
    const buttonText = buttonP.firstChild;
    assertNotNullNorUndefined(buttonText);
    mockFeedback.call(simulateHitTestResult(buttonText))
        .expectSpeech('Jefferson')
        .expectSpeech('Button')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AriaSliderWithValueNow', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="slider" role="slider" tabindex="0" aria-valuemin="0"
             aria-valuenow="50" aria-valuemax="100"></div>
    <script>
      let slider = document.getElementById('slider');
      slider.addEventListener('click', () => {
        slider.setAttribute('aria-valuenow',
            parseInt(slider.getAttribute('aria-valuenow'), 10) + 1);
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const slider = root.find({role: RoleType.SLIDER});
    assertNotNullNorUndefined(slider);
    mockFeedback.call(slider.doDefault.bind(slider))
        .expectSpeech('51')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AriaSliderWithValueText', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="slider" role="slider" tabindex="0" aria-valuemin="0"
             aria-valuenow="50" aria-valuemax="100" aria-valuetext="tiny"></div>
    <script>
      let slider = document.getElementById('slider');
      slider.addEventListener('click', () => {
        slider.setAttribute('aria-valuenow',
            parseInt(slider.getAttribute('aria-valuenow'), 10) + 1);
        slider.setAttribute('aria-valuetext', 'large');
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const slider = root.find({role: RoleType.SLIDER});
    assertNotNullNorUndefined(slider);
    mockFeedback.clearPendingOutput()
        .call(slider.doDefault.bind(slider))
        .expectNextSpeechUtteranceIsNot('51')
        .expectSpeech('large')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SelectValidityOutput', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <label for="in1">Name:</label>
    <input id="in1" required>
    <script>
      const in1 = document.querySelector('input');
      in1.addEventListener('focus', () => {
        setTimeout(() => {
          in1.setCustomValidity('Please enter name');
          in1.reportValidity();
        }, 500);
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('start')
        .call(doCmd('nextObject'))
        .expectSpeech('Name:')
        .expectSpeech('Edit text')
        .expectSpeech('Required')
        .expectNextSpeechUtteranceIsNot('Alert')
        .expectSpeech('Please enter name')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EventFromAction', function() {
  const site = '<button>ok</button><button>cancel</button>';
  this.runWithLoadedTree(site, function(root) {
    const button = root.findAll({role: RoleType.BUTTON})[1];
    button.addEventListener(EventType.FOCUS, this.newCallback(function(evt) {
      assertEquals(RoleType.BUTTON, evt.target.role);
      assertEquals('action', evt.eventFrom);
      assertEquals('cancel', evt.target.name);
      assertEquals('focus', evt.eventFromAction);
    }));

    button.focus();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EventFromUser', function() {
  const site = '<button>ok</button><button>cancel</button>';
  this.runWithLoadedTree(site, function(root) {
    const button = root.findAll({role: RoleType.BUTTON})[1];
    button.addEventListener(EventType.FOCUS, this.newCallback(function(evt) {
      assertEquals(RoleType.BUTTON, evt.target.role);
      assertEquals('user', evt.eventFrom);
      assertEquals('cancel', evt.target.name);
    }));

    press(KeyCode.TAB)();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ReadPhoneticPronunciationTest', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
   <button>This is a button</button>
   <input type="text"></input>
  `;
  this.runWithLoadedTree(site, function(root) {
    root.find({role: RoleType.BUTTON}).focus();
    mockFeedback.call(doCmd('readPhoneticPronunciation'))
        .expectSpeech('T: tango, h: hotel, i: india, s: sierra')
        .call(doCmd('nextWord'))
        .call(doCmd('readPhoneticPronunciation'))
        .expectSpeech('i: india, s: sierra')
        .call(doCmd('nextWord'))
        .call(doCmd('nextWord'))
        .call(doCmd('readPhoneticPronunciation'))
        .expectSpeech(
            'b: bravo, u: uniform, t: tango, t: tango, o: oscar, ' +
            'n: november')
        .call(doCmd('nextEditText'))
        .call(doCmd('readPhoneticPronunciation'))
        .expectSpeech('No available text for this item');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SimilarItemNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h3><a href="#a">inner</a></h3>
    <p>some text</p>
    <button>some other text</button>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;
  this.runWithLoadedTree(site, function(root) {
    assertEquals(
        RoleType.LINK, ChromeVoxState.instance.currentRange.start.node.role);
    assertEquals('inner', ChromeVoxState.instance.currentRange.start.node.name);
    mockFeedback.call(doCmd('nextSimilarItem'))
        .expectSpeech('outer1', 'Link')
        .call(doCmd('nextSimilarItem'))
        .expectSpeech('inner', 'Link')
        .call(doCmd('nextSimilarItem'))
        .call(doCmd('previousSimilarItem'))
        .expectSpeech('inner', 'Link')
        .call(doCmd('nextHeading'))
        .expectSpeech('outer2', 'Heading 3')
        .call(doCmd('previousSimilarItem'))
        .expectSpeech('inner', 'Heading 3');

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'InvalidItemNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h3><a href="#a">inner</a></h3>
    <p aria-invalid="spelling">some txet</p>
    <button>button A</button>
    <p aria-invalid="true">some other reason</p>
    <p>no error text 1</P>
    <p aria-invalid=false>no error text 2</P>
    <p aria-invalid="grammar">this are a text</p>
    <p aria-invalid="unknown">error is this</p>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;

  this.runWithLoadedTree(site, function(root) {
    assertEquals(
        RoleType.LINK, ChromeVoxState.instance.currentRange.start.node.role);
    assertEquals('inner', ChromeVoxState.instance.currentRange.start.node.name);
    mockFeedback.call(doCmd('nextInvalidItem'))
        .expectSpeech('some txet', 'misspelled')
        .call(doCmd('nextInvalidItem'))
        .expectSpeech('some other reason')
        .call(doCmd('nextInvalidItem'))
        .expectSpeech('this are a text', 'grammar error')
        .call(doCmd('nextInvalidItem'))
        .expectSpeech('error is this')
        // Ensure wrap.
        .call(doCmd('nextInvalidItem'))
        .expectSpeech('some txet')
        // Wrap backward.
        .call(doCmd('previousInvalidItem'))
        .expectSpeech('error is this')
        .call(doCmd('previousInvalidItem'))
        .expectSpeech('this are a text', 'grammar error');

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'InvalidItemNavigationNoItem', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h3><a href="#a">inner</a></h3>
    <p>some text</p>
    <button>some other text</button>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;
  this.runWithLoadedTree(site, function(root) {
    assertEquals(
        RoleType.LINK, ChromeVoxState.instance.currentRange.start.node.role);
    assertEquals('inner', ChromeVoxState.instance.currentRange.start.node.name);
    mockFeedback.call(doCmd('nextInvalidItem'))
        .expectSpeech('No invalid item')
        .call(doCmd('previousInvalidItem'))
        .expectSpeech('No invalid item');

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'TableWithAriaRowCol', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="table">
      <div role="row" aria-rowindex=3>
        <div role="cell">test</div>
      </div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('fullyDescribe'))
        .expectSpeech('test', 'row 3 column 1', 'Table , 1 by 1')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NonModalDialogHeadingJump', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h2>Heading outside dialog</h2>
    <div role="dialog">
      <h2>Heading inside dialog</h2>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextHeading'))
        .expectSpeech('Heading inside dialog')
        .call(doCmd('previousHeading'))
        .expectSpeech('Heading outside dialog')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'LevelEndsForNestedLists', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div>
      <ul>
        <li>Berries
          <ul>
            <li>Strawberries</li>
            <li>Blueberries</li>
            <li>Raspberries</li>
          </ul>
        </li>
        <li>Citruses
          <ul>
              <li>Oranges
                <ul>
                  <li>Grapefruits</li>
                  <li>Mandarins</li>
                </ul>
              </li>
          </ul>
        </li>
        <li>Bananas</li>
      </ul>
    </div>
  `;

  this.runWithLoadedTree(site, function(root) {
    const blueberries = root.find({attributes: {name: 'Blueberries'}});
    const grapefruits = root.find({attributes: {name: 'Grapefruits'}});

    mockFeedback
        .call(() => {
          ChromeVoxState.instance.setCurrentRange(
              cursors.Range.fromNode(blueberries));
        })
        .call(doCmd('nextObject'))
        .expectSpeech(
            '◦ Raspberries', 'List item', 'List end', 'nested level 2')
        .call(() => {
          ChromeVoxState.instance.setCurrentRange(
              cursors.Range.fromNode(grapefruits));
        })
        .call(doCmd('nextObject'))
        .expectSpeech('■ Mandarins', 'List item', 'List end', 'nested level 3')
        .call(doCmd('nextObject'))
        // Nested level is not mentioned for level 1.
        .expectSpeech('• Bananas', 'List item', 'List end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NestedListNavigationSimple', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.nestedListDoc, function(root) {
    mockFeedback.expectSpeech('• Lemons', 'List item', 'List', 'with 4 items')
        .call(doCmd('nextObject'))
        .expectSpeech('• Oranges', 'List item')
        .call(doCmd('nextObject'))
        .expectSpeech('• ', 'Berries', 'List item')
        .expectBraille('• Berries lstitm')
        .call(doCmd('nextObject'))
        .expectSpeech('◦ Strawberries', 'List item', 'List', 'with 2 items')
        .call(doCmd('nextObject'))
        .expectSpeech('◦ Raspberries', 'List item', 'List end')
        .call(doCmd('nextObject'))
        .expectSpeech('• Bananas', 'List item', 'List end')
        .expectBraille('• Bananas lstitm lst end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NestedListNavigationMixed', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.nestedListDoc, function(root) {
    mockFeedback.expectSpeech('• Lemons', 'List item', 'List', 'with 4 items')
        .call(doCmd('nextObject'))
        .expectSpeech('• Oranges', 'List item')
        .call(doCmd('nextLine'))
        .expectSpeech('• ', 'Berries', 'List item')
        .call(doCmd('nextLine'))
        .expectSpeech('◦ Strawberries', 'List item', 'List', 'with 2 items')
        .call(doCmd('previousLine'))
        .expectSpeech('• ', 'Berries')
        .call(doCmd('nextWord'))
        .expectSpeech('◦ Strawberries')
        .call(doCmd('nextWord'))
        .expectSpeech('◦ Raspberries')
        .call(doCmd('previousObject'))
        .call(doCmd('previousObject'))
        .expectSpeech('• ', 'Berries')
        .call(doCmd('previousCharacter'))
        .call(doCmd('previousCharacter'))
        .call(doCmd('previousCharacter'))
        .expectSpeech('g')  // For Oranges
        .call(doCmd('nextGroup'))
        .expectSpeech('◦ Strawberries', '◦ Raspberries')
        .clearPendingOutput()
        .call(doCmd('previousGroup'))
        .expectSpeech('• Oranges')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigationByList', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start here</p>
    <ul>
      Drinks
      <li>Coffee</li>
      <li>Tea</li>
    </ul>
    <p>A random paragraph</p>
    <ul></ul>
    <ol>
      Lunch
      <li>Burgers</li>
      <li>Fries</li>
      <li>Soda</li>
        <ul>
          Nested list
          <li>Element</li>
        </ul>
    </ol>
    <p>Another random paragraph</p>
    <dl>
      Colors
      <dt>Red</dt>
      <dd>Description for red</dd>
      <dt>Blue</dt>
      <dd>Description for blue</dd>
      <dt>Green</dt>
      <dd>Description for green</dd>
    </dl>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('jumpToTop'))
        .call(doCmd('nextList'))
        .expectSpeech('Drinks', 'List', 'with 2 items')
        .call(doCmd('nextList'))
        .expectSpeech('List', 'with 0 items')
        .call(doCmd('nextList'))
        .expectSpeech('Lunch', 'List', 'with 3 items')
        .call(doCmd('nextList'))
        .expectSpeech('Nested list', 'List', 'with 1 item')
        .call(doCmd('nextList'))
        .expectSpeech('Colors', 'Description list', 'with 3 items')
        .call(doCmd('nextList'))
        // Ensure we wrap correctly.
        .expectSpeech('Drinks', 'List', 'with 2 items')
        .call(doCmd('nextObject'))
        .call(doCmd('nextObject'))
        .expectSpeech('\u2022 Coffee')
        // Ensure we wrap correctly and go to previous list, not top of
        // current list.
        .call(doCmd('previousList'))
        .expectSpeech('Colors')
        .call(doCmd('previousObject'))
        .expectSpeech('Another random paragraph')
        // Ensure we dive into the nested list.
        .call(doCmd('previousList'))
        .expectSpeech('Nested list', 'List', 'with 1 item')
        .call(doCmd('previousList'))
        .expectSpeech('Lunch')
        .call(doCmd('nextObject'))
        .call(doCmd('nextObject'))
        .expectSpeech('1. Burgers')
        // Ensure we go to the previous list, not the top of the current
        // list.
        .call(doCmd('previousList'))
        .expectSpeech('List', 'with 0 items')
        .call(doCmd('previousObject'))
        .expectSpeech('A random paragraph')
        .call(doCmd('previousList'))
        .expectSpeech('Drinks', 'List', 'with 2 items')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NoListTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<button>Click me</button>', function(root) {
    mockFeedback.call(doCmd('nextList'))
        .expectSpeech('No next list')
        .call(doCmd('previousList'))
        .expectSpeech('No previous list');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigateToLastHeading', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h1>First</h1>
    <h1>Second</h1>
    <h1>Third</h1>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('jumpToTop'))
        .expectSpeech('First', 'Heading 1')
        .call(doCmd('previousHeading'))
        .expectSpeech('Third', 'Heading 1')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ReadLinkURLTest', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="https://www.google.com/">A popular link</a>
    <button>Not a link</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextLink'))
        .expectSpeech(
            'A popular link', 'Link', 'Press Search+Space to activate')
        .call(doCmd('readLinkURL'))
        .expectSpeech('Link URL: https://www.google.com/')
        .call(doCmd('nextObject'))
        .expectSpeech('Not a link', 'Button', 'Press Search+Space to activate')
        .call(doCmd('readLinkURL'))
        .expectSpeech('No URL found')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NoRepeatTitle', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="button" aria-label="title" title="title"></div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('title')
        .expectSpeech('Button')
        .expectNextSpeechUtteranceIsNot('title')
        .expectSpeech('Press Search+Space to activate')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PhoneticsAndCommands', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>some sample text</p>
    <button>ok</button>
    <p>A</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    const noPhonetics = {phoneticCharacters: undefined};
    const phonetics = {phoneticCharacters: true};
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeechWithProperties(noPhonetics, 'ok')
        .call(doCmd('previousObject'))
        .expectSpeechWithProperties(noPhonetics, 'some sample text')
        .call(doCmd('nextWord'))
        .expectSpeechWithProperties(noPhonetics, 'sample')
        .call(doCmd('previousWord'))
        .expectSpeechWithProperties(noPhonetics, 'some')
        .call(doCmd('nextCharacter'))
        .expectSpeechWithProperties(phonetics, 'o')
        .call(doCmd('nextCharacter'))
        .expectSpeechWithProperties(phonetics, 'm')
        .call(doCmd('previousCharacter'))
        .expectSpeechWithProperties(phonetics, 'o')
        .call(doCmd('jumpToBottom'))
        .expectSpeechWithProperties(noPhonetics, 'A');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ToggleScreen', function() {
  const mockFeedback = this.createMockFeedback();
  // Pretend we've already accepted the confirmation dialog once.
  localStorage['acceptToggleScreen'] = 'true';
  this.runWithLoadedTree('<div>Unimportant web content</div>', function() {
    mockFeedback.call(doCmd('toggleScreen'))
        .expectSpeech('Screen off')
        .call(doCmd('toggleScreen'))
        .expectSpeech('Screen on')
        .call(doCmd('toggleScreen'))
        .expectSpeech('Screen off')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NoFocusTalkBackDisabled', function() {
  // Fire onCustomSpokenFeedbackEnabled event to communicate that Talkback is
  // off for the current app.
  this.dispatchOnCustomSpokenFeedbackToggledEvent(false);
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>Test document</p>', function() {
    ChromeVoxState.instance.setCurrentRange(null);
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech(
            'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
            'launcher.')
        .call(doCmd('previousObject'))
        .expectSpeech(
            'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
            'launcher.');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NoFocusTalkBackEnabled', function() {
  // Fire onCustomSpokenFeedbackEnabled event to communicate that Talkback is
  // on for the current app. We don't want to announce the no-focus hint message
  // when TalkBack is on because we expect ChromeVox to have no focus in that
  // case. If we announce the hint message, TalkBack and ChromeVox will
  // try to speak at the same time.
  this.dispatchOnCustomSpokenFeedbackToggledEvent(true);
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>Start here</p>', function() {
    ChromeVoxState.instance.setCurrentRange(null);
    mockFeedback.call(doCmd('nextObject'))
        .call(
            () => assertFalse(mockFeedback.utteranceInQueue(
                'No current ChromeVox focus. ' +
                'Press Alt+Shift+L to go to the launcher.')))
        .call(doCmd('previousObject'))
        .call(
            () => assertFalse(mockFeedback.utteranceInQueue(
                'No current ChromeVox focus. ' +
                'Press Alt+Shift+L to go to the launcher.')));
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NavigateOutOfMultiline', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <p>before</p>
    <div contenteditable>
      Testing testing<br>one two three
    </div>
    <p>after</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    const contentEditable =
        root.find({attributes: {nonAtomicTextFieldRoot: true}});
    mockFeedback.call(contentEditable.focus.bind(contentEditable))
        .expectSpeech(/Testing testing\s+one two three/)
        .call(doCmd('nextLine'))
        .expectSpeech('one two three')
        .call(doCmd('nextLine'))
        .expectSpeech('after')

        // In reverse (explicitly focus, instead of moving to previous
        // line, because all subsequent commands require the content
        // editable to be focused first):
        .clearPendingOutput()
        .call(contentEditable.focus.bind(contentEditable))
        .expectSpeech(/Testing testing\s+one two three/)
        .call(doCmd('nextLine'))
        .expectSpeech('one two three')
        .call(doCmd('previousLine'))
        .expectSpeech('Testing testing')
        .call(doCmd('previousLine'))
        .expectSpeech('before')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ReadWindowTitle', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <button id="click"></button>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', _ => document.title = 'bar');
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const clickButtonThenReadCurrentTitle = () => {
      const desktop = root.parent.root;
      desktop.addEventListener(EventType.TREE_CHANGED, (evt) => {
        if (evt.target.role === RoleType.WINDOW &&
            /bar/.test(evt.target.name)) {
          doCmd('readCurrentTitle')();
        }
      });
      const button = root.find({role: RoleType.BUTTON});
      button.doDefault();
    };

    mockFeedback.clearPendingOutput()
        .call(clickButtonThenReadCurrentTitle)

        // This test may run against official builds, so match against
        // utterances starting with 'bar'. This should exclude any other
        // utterances that contain 'bar' e.g. data:...bar.. or the data url.
        .expectSpeech(/^bar*/)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'OutputEmptyQueueMode', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>unused</p>', function(root) {
    const output = new Output();
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);
    output.append_(
        output.speechBuffer_, new Spannable(''),
        {annotation: [new OutputAction()]});
    output.withString('test');
    mockFeedback.clearPendingOutput()
        .call(output.go.bind(output))
        .expectSpeechWithQueueMode('', QueueMode.CATEGORY_FLUSH)
        .expectSpeechWithQueueMode('test', QueueMode.CATEGORY_FLUSH)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SetAccessibilityFocus', function() {
  this.runWithLoadedTree('<p>Text.</p><button>Button</button>', function(root) {
    const node = root.find({role: RoleType.BUTTON});

    node.addEventListener(EventType.FOCUS, this.newCallback(function() {
      chrome.automation.getAccessibilityFocus((focusedNode) => {
        assertEquals(node, focusedNode);
      });
    }));

    node.focus();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'MenuItemRadio', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <ul role="menu" tabindex="0" autofocus>
      <li role="menuitemradio" aria-checked="true">Small</li>
      <li role="menuitemradio" aria-checked="false">Medium</li>
      <li role="menuitemradio" aria-checked="false">Large</li>
    </ul>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('Menu', 'with 3 items')
        .call(doCmd('nextObject'))
        .expectSpeech('Small, menu item radio button selected', ' 1 of 3 ')
        .call(doCmd('nextObject'))
        .expectSpeech('Medium, menu item radio button unselected', ' 2 of 3 ')
        .replay();
  });
});

TEST_F(
    'ChromeVoxBackgroundTest', 'ButtonNavigationIgnoresRadioButtons',
    function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
        <button>Action 1</button>
        <fieldset>
          <p><label> <input type=radio>Radio 1</label></p>
          <p><label> <input type=radio>Radio 2</label></p>
        </fieldset>
        <button>Action 2</button>
      `;

      this.runWithLoadedTree(site, function(root) {
        mockFeedback.call(doCmd('nextButton'))
            .expectSpeech('Action 1', 'Button')
            .call(doCmd('nextButton'))
            .expectSpeech('Action 2', 'Button');

        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxBackgroundTest', 'FocusableNamedDivIsNotContainer', function() {
      const site = `
        <div aria-label="hello world" tabindex="0">hello world</div>
      `;
      this.runWithLoadedTree(site, function(root) {
        const genericContainer = root.find({role: RoleType.GENERIC_CONTAINER});
        assertTrue(AutomationPredicate.object(genericContainer));
        assertFalse(AutomationPredicate.container(genericContainer));
      });
    });

TEST_F('ChromeVoxBackgroundTest', 'HitTestOnExoSurface', function() {
  const site = `
    <button></button>
    <input type="text"</input>
  `;
  this.runWithLoadedTree(site, function(root) {
    const fakeWindow = root.find({role: RoleType.BUTTON});
    const realTextField = root.find({role: RoleType.TEXT_FIELD});

    // Fake the role and className to imitate a ExoSurface.
    Object.defineProperty(fakeWindow, 'role', {get: () => RoleType.WINDOW});
    Object.defineProperty(
        fakeWindow, 'className', {get: () => 'ExoSurface-40'});

    // Mock and expect a call for the fake window.
    chrome.accessibilityPrivate.sendSyntheticMouseEvent =
        this.newCallback(evt => {
          assertEquals(fakeWindow.location.left, evt.x);
          assertEquals(fakeWindow.location.top, evt.y);
        });

    // Fake a mouse explore event on the real text field. This should not
    // trigger the above mouse path.
    GestureCommandHandler.pointerHandler_.onMouseMove(
        realTextField.location.left, realTextField.location.top);

    // Fake a touch explore gesture event on the fake window which should
    // trigger a mouse move.
    GestureCommandHandler.onAccessibilityGesture_(
        Gesture.TOUCH_EXPLORE, fakeWindow.location.left,
        fakeWindow.location.top);
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PointerSkipsContainers', function() {
  PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = -1;
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="grouparia-label="test" " tabindex=0>
      <div role=button><p></p></div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    ChromeVoxState.addObserver(new class {
      onCurrentRangeChanged(range) {
        if (!range) {
          ChromeVox.tts.speak('range cleared!');
        }
      }
    }());

    const button = root.find({role: RoleType.BUTTON});
    assertNotNullNorUndefined(button);
    const group = button.parent;
    assertNotNullNorUndefined(group);
    mockFeedback.call(simulateHitTestResult(button))
        .expectSpeech('Button')
        .call(() => {
          // Override the role to simulate panes which are only found in
          // views.
          Object.defineProperty(group, 'role', {
            get() {
              return chrome.automation.RoleType.PANE;
            }
          });
        })
        .call(simulateHitTestResult(group))
        .expectSpeech('range cleared!')
        .expectEarcon(Earcon.NO_POINTER_ANCHOR)
        .call(simulateHitTestResult(button))
        .expectSpeech('Button')
        .call(simulateHitTestResult(group))
        .expectSpeech('range cleared!')
        .expectEarcon(Earcon.NO_POINTER_ANCHOR)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'FocusOnUnknown', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="group" tabindex=0>
      <p>hello<p>
    </div>
    <div role="group" tabindex=0></div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const [group1, group2] = root.findAll({role: RoleType.GROUP});
    assertNotNullNorUndefined(group1);
    assertNotNullNorUndefined(group2);
    Object.defineProperty(group1, 'role', {
      get() {
        return chrome.automation.RoleType.UNKNOWN;
      }
    });
    Object.defineProperty(group2, 'role', {
      get() {
        return chrome.automation.RoleType.UNKNOWN;
      }
    });

    const evt2 = new CustomAutomationEvent(EventType.FOCUS, group2);
    const currentRange = ChromeVoxState.instance.currentRange;
    DesktopAutomationHandler.instance.onFocus(evt2);
    assertEquals(currentRange, ChromeVoxState.instance.currentRange);

    const evt1 = new CustomAutomationEvent(EventType.FOCUS, group1);
    mockFeedback
        .call(DesktopAutomationHandler.instance.onFocus.bind(
            DesktopAutomationHandler.instance, evt1))
        .expectSpeech('hello')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'TimeDateCommand', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p></p>', function(root) {
    mockFeedback.call(doCmd('speakTimeAndDate'))
        .expectSpeech(/(AM|PM)*(2)/)
        .expectBraille(/(AM|PM)*(2)/)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SwipeToScrollByPage', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doGesture(Gesture.SWIPE_UP3))
        .expectSpeech(/Page 2 of/)
        .call(doGesture(Gesture.SWIPE_UP3))
        .expectSpeech(/Page 3 of/)
        .call(doGesture(Gesture.SWIPE_DOWN3))
        .expectSpeech(/Page 2 of/)
        .call(doGesture(Gesture.SWIPE_DOWN3))
        .expectSpeech(/Page 1 of/)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PointerOnOffOnRepeatsNode', function() {
  PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = -1;
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<button>hi</button>', function(root) {
    ChromeVoxState.addObserver(new class {
      onCurrentRangeChanged(range) {
        if (!range) {
          ChromeVox.tts.speak('range cleared!');
        }
      }
    }());

    const button = root.find({role: RoleType.BUTTON});
    assertNotNullNorUndefined(button);
    mockFeedback.call(simulateHitTestResult(button))
        .expectSpeech('hi', 'Button')

        // Touch slightly off of the button.
        .call(GestureCommandHandler.onAccessibilityGesture_.bind(
            null, Gesture.TOUCH_EXPLORE, button.location.left,
            button.location.top + 60))
        .expectSpeech('range cleared!')
        .expectEarcon(Earcon.NO_POINTER_ANCHOR)
        .clearPendingOutput()
        .call(simulateHitTestResult(button))
        .expectSpeech('hi', 'Button')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PopupButtonCollapsed', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <select id="button">
      <option value="Apple">Apple</option>
      <option value="Banana">Banana</option>
    </select>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('jumpToTop'))
        .expectSpeech(
            'Apple', 'Button', 'has pop up', 'Collapsed',
            'Press Search+Space to activate')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'PopupButtonExpanded', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <button id="button" aria-haspopup="true" aria-expanded="true"
        aria-controls="menu">
      Click me
    </button>
    <ul id="menu"
      role="menu"
      aria-labelledby="button">
      <li role="menuitem">Item 1</li>
      <li role="menuitem">Item 2</li>
      <li role="menuitem">Item 3</li>
    </ul>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback
        .call(doCmd('jumpToTop'))
        // SetSize is only reported if popup button is expanded.
        .expectSpeech(
            'Click me', 'Button', 'has pop up', 'with 3 items', 'Expanded',
            'Press Search+Space to activate')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SortDirection', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <table border="1">
      <th aria-sort="ascending"><button id="sort">Date</button></th>
      <tr><td>1/2/20</td></tr>
      <tr><td>2/2/20</td></tr>
    </table>
    <script>
      let ascending = true;
      const sortButton = document.getElementById('sort');
      sortButton.addEventListener('click', () => {
        ascending = !ascending;
        sortButton.parentElement.setAttribute(
            'aria-sort', ascending ? 'ascending' : 'descending');
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const sortButton = root.find({role: RoleType.BUTTON});
    mockFeedback.expectSpeech('Button', 'Ascending sort')
        .call(sortButton.doDefault.bind(sortButton))
        .expectSpeech('Descending sort')
        .call(sortButton.doDefault.bind(sortButton))
        .expectSpeech('Ascending sort')
        .call(sortButton.doDefault.bind(sortButton))
        .expectSpeech('Descending sort')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'InlineLineNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <p><strong>This</strong><b>is</b>a <em>test</em></p>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextLine'))
        .expectSpeech('This', 'is', 'a ', 'test')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AudioVideo', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <button></button>
    <button></button>
  `;
  this.runWithLoadedTree(site, function(root) {
    const [audio, video] = root.findAll({role: RoleType.BUTTON});

    assertNotNullNorUndefined(audio);
    assertNotNullNorUndefined(video);

    assertEquals(undefined, audio.name);
    assertEquals(undefined, video.name);
    assertEquals(undefined, audio.firstChild);
    assertEquals(undefined, video.firstChild);

    // Fake the roles.
    Object.defineProperty(audio, 'role', {
      get() {
        return chrome.automation.RoleType.AUDIO;
      }
    });

    Object.defineProperty(video, 'role', {
      get() {
        return chrome.automation.RoleType.VIDEO;
      }
    });

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Video')
        .call(doCmd('previousObject'))
        .expectSpeech('Audio')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AlertNoAnnouncement', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<button></button>', function(root) {
    ChromeVoxState.addObserver(new class {
      onCurrentRangeChanged(range) {
        assertNotReached('Range was changed unexpectedly.');
      }
    }());
    const button = root.find({role: RoleType.BUTTON});
    const alertEvt = new CustomAutomationEvent(EventType.ALERT, button);
    mockFeedback
        .call(DesktopAutomationHandler.instance.onAlert.bind(
            DesktopAutomationHandler.instance, alertEvt))
        .call(() => assertFalse(mockFeedback.utteranceInQueue('Alert')))
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AlertAnnouncement', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<button>hello world</button>', function(root) {
    ChromeVoxState.addObserver(new class {
      onCurrentRangeChanged(range) {
        assertNotReached('Range was changed unexpectedly.');
      }
    }());

    const button = root.find({role: RoleType.BUTTON});
    const alertEvt = new CustomAutomationEvent(EventType.ALERT, button);
    mockFeedback
        .call(DesktopAutomationHandler.instance.onAlert.bind(
            DesktopAutomationHandler.instance, alertEvt))
        .expectNextSpeechUtteranceIsNot('Alert')
        .expectSpeech('hello world')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SwipeLeftRight4ByContainers', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(`<p>test</p>`, function(root) {
    mockFeedback.call(doGesture(Gesture.SWIPE_RIGHT4))
        .expectSpeech('Launcher', 'Button', 'Shelf', 'Tool bar', ', window')
        .call(doGesture(Gesture.SWIPE_RIGHT4))
        .expectSpeech('Shelf', 'Tool bar')
        .call(doGesture(Gesture.SWIPE_RIGHT4))
        .expectSpeech(/Status tray*/)
        .call(doGesture(Gesture.SWIPE_RIGHT4))
        .expectSpeech(/Address and search bar*/)

        .call(doGesture(Gesture.SWIPE_LEFT4))
        .expectSpeech(/Status tray*/)
        .call(doGesture(Gesture.SWIPE_LEFT4))
        .expectSpeech('Shelf', 'Tool bar')

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SwipeLeftRight2', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p id="live" aria-live="polite"</p>
    <script>
    document.body.addEventListener('keydown', (evt) => {
      document.getElementById('live').textContent = evt.key;
    });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doGesture(Gesture.SWIPE_RIGHT2)).expectSpeech('Enter');
    mockFeedback.call(doGesture(Gesture.SWIPE_LEFT2))
        .expectSpeech('Escape')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'DialogAutoSummaryTextContent', function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="dialog" aria-label="Setup">
      <h1>Welcome</h1>
      <p>This is some introductory text<p>
      <button>Exit</button>
      <button>Let's go</button>
    </div>
    <p>end</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Setup', 'Dialog')
        .expectSpeech(`Welcome This is some introductory text Exit Let's go`)
        .expectSpeech('Welcome', 'Heading 1')
        .call(doCmd('nextObject'))
        .expectSpeech('This is some introductory text')
        .call(doCmd('nextObject'))
        .expectSpeech('Exit', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech(`Let's go`, 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('end')

        .call(doCmd('previousObject'))
        .expectSpeech(`Let's go`, 'Button')
        .expectSpeech('Setup', 'Dialog')
        .expectSpeech(`Welcome This is some introductory text Exit Let's go`)

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ImageAnnotations', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <img alt="bar" src="data:image/png;base64,iVBORw0KGgoAAAANS">
    <img src="data:image/png;base64,iVBORw0KGgoAAAANS">
  `;
  this.runWithLoadedTree(site, function(root) {
    const [namedImg, unnamedImg] = root.findAll({role: RoleType.IMAGE});

    assertNotNullNorUndefined(namedImg);
    assertNotNullNorUndefined(unnamedImg);

    assertEquals('bar', namedImg.name);
    assertEquals(undefined, unnamedImg.name);

    // Fake the image annotation.
    Object.defineProperty(namedImg, 'imageAnnotation', {
      get() {
        return 'foo';
      }
    });
    Object.defineProperty(unnamedImg, 'imageAnnotation', {
      get() {
        return 'foo';
      }
    });

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('start')
        .expectNextSpeechUtteranceIsNot('foo')
        .expectSpeech('bar', 'Image')
        .call(doCmd('nextObject'))
        .expectNextSpeechUtteranceIsNot('bar')
        .expectSpeech('foo', 'Image')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'VolumeChanges', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<p>test</p>', function() {
    const bounds = ChromeVoxState.instance.getFocusBounds();
    mockFeedback.call(press(KeyCode.VOLUME_UP))
        .expectSpeech('Volume', 'Slider', /\d+%/)
        .call(() => {
          // The bounds should not have changed.
          assertEquals(
              JSON.stringify(bounds),
              JSON.stringify(ChromeVoxState.instance.getFocusBounds()));
        })
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'WrapContentEditableAtEndOfDoc', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<p>start</p>
      <div role="textbox" contenteditable aria-multiline="false"></div>`;
  this.runWithLoadedTree(site, function() {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Edit text')
        .call(doCmd('nextObject'))
        .expectEarcon(Earcon.WRAP)
        .expectSpeech('Web Content')
        .call(doCmd('nextObject'))
        .expectSpeech('start')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ReadFromHereBlankNodes', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<a tabindex=0></a><p>start</p><a tabindex=0></a><p>end</p>`;
  this.runWithLoadedTree(site, function(root) {
    assertEquals(
        RoleType.STATIC_TEXT,
        ChromeVoxState.instance.currentRange.start.node.role);

    // "start" is uttered twice, once for the initial focus as the page loads,
    // and once during the 'read from here' command.
    mockFeedback.expectSpeech('start')
        .call(doCmd('readFromHere'))
        .expectSpeech('start', 'end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ContainerButtons', function() {
  const mockFeedback = this.createMockFeedback();

  // This pattern can be found in ARC++/YouTube.
  const site = `
    <p>videos</p>
    <div aria-label="Cat Video" role="button">
      <div role="group">4 minutes, Cat Video</div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const group = root.find({role: RoleType.GROUP});

    Object.defineProperty(group, 'clickable', {
      get() {
        return true;
      }
    });

    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('Cat Video', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('4 minutes, Cat Video')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'FocusOnWebAreaIgnoresEvents', function() {
  const site = `
    <div role="application" tabindex=0 aria-label="container">
      <select>
        <option>apple</option>
        <option>grape</option>
        <option>pear</option>
      </select>
    </div>
    <p>go</p>
    <script>
      let counter = 0;
      document.body.getElementsByTagName('p')[0].addEventListener('click',
          e => {
            document.body.getElementsByTagName('select')[0].selectedIndex =
                ++counter % 3;
          });
    </script>
  `;
  this.runWithLoadedTree(site, async function(root) {
    const application = root.find({role: RoleType.APPLICATION});
    const popUpButton = root.find({role: RoleType.POP_UP_BUTTON});
    const p = root.find({role: RoleType.PARAGRAPH});

    // Move focus to the select, which honors value changes through
    // FocusAutomationHandler.
    popUpButton.focus();
    await TestUtils.waitForSpeech('apple');

    // Clicking the paragraph programmatically changes the select value.
    p.doDefault();
    await TestUtils.waitForSpeech('grape');
    assertEquals(
        RoleType.POP_UP_BUTTON,
        ChromeVoxState.instance.currentRange.start.node.role);

    // Now, move focus to the application which is a parent of the select.
    application.focus();
    await TestUtils.waitForSpeech('container');

    // Hook into the speak call, to see what comes next.
    let nextSpeech;
    ChromeVox.tts.speak = textString => {
      nextSpeech = textString;
    };

    // Trigger another value update for the select.
    p.doDefault();

    // This comes when the <select>'s value changes.
    await TestUtils.waitForEvent(application, EventType.SELECTED_VALUE_CHANGED);

    // Nothing should have been spoken.
    assertEquals(undefined, nextSpeech);
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AriaLeaves', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="radio"><p>PM</p></div>
    <div role="switch"><p>Agree</p></div>
    <div role="checkbox"><p>Agree</p></div>
    <script>
      const p = document.getElementsByTagName('p')[0];
      p.addEventListener('click', () => {});
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('PM, radio button unselected')
        .call(doCmd('nextObject'))
        .expectSpeech('PM')
        .call(
            () => assertEquals(
                RoleType.STATIC_TEXT,
                ChromeVoxState.instance.currentRange.start.node.role))

        .call(doCmd('nextObject'))
        .expectSpeech('Agree, switch off')
        .call(
            () => assertEquals(
                RoleType.SWITCH,
                ChromeVoxState.instance.currentRange.start.node.role))

        .call(doCmd('nextObject'))
        .expectSpeech('Agree', 'Check box')
        .call(
            () => assertEquals(
                RoleType.CHECK_BOX,
                ChromeVoxState.instance.currentRange.start.node.role))

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'MarkedContent', function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <span>This is </span><span role="mark">my</span><span> text.</span>
    <br>
    <span>This is </span><span role="mark"
        aria-roledescription="Comment">your</span><span> text.</span>
    <br>
    <span>This is </span><span role="suggestion"><span
        role="insertion">their</span></span><span> text.</span>
    <br>
    <span>This is </span><span role="suggestion"><span
        role="deletion">everyone's</span></span><span> text.</span>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    mockFeedback.expectSpeech('Start')
        .call(doCmd('nextObject'))
        .expectSpeech('This is ')
        .call(doCmd('nextObject'))
        .expectSpeech('Marked content', 'my', 'Marked content end')
        .expectBraille('Marked content my Marked content end')
        .call(doCmd('nextObject'))
        .expectSpeech(' text.')
        .expectBraille(' text.')
        .call(doCmd('nextObject'))
        .expectSpeech('This is ')
        .call(doCmd('nextObject'))
        .expectSpeech('Comment', 'your', 'Comment end')
        .expectBraille('Comment your Comment end')
        .call(doCmd('nextObject'))
        .expectSpeech(' text.')
        .expectBraille(' text.')
        .call(doCmd('nextObject'))
        .expectSpeech('This is ')
        .call(doCmd('nextObject'))
        .expectSpeech('Suggest', 'Insert', 'their', 'Insert end', 'Suggest end')
        .expectBraille('Suggest Insert their Insert end Suggest end')
        .call(doCmd('nextObject'))
        .expectSpeech(' text.')
        .expectBraille(' text.')
        .call(doCmd('nextObject'))
        .expectSpeech('This is ')
        .call(doCmd('nextObject'))
        .expectSpeech(
            'Suggest', 'Delete', `everyone's`, 'Delete end', 'Suggest end')
        .expectBraille(`Suggest Delete everyone's Delete end Suggest end`)
        .call(doCmd('nextObject'))
        .expectSpeech(' text.')
        .expectBraille(' text.')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ClickAncestorAreNotActionable', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <div id="button1" role="button" aria-label="OK">
      <div role="group">OK</div>
    </div>
    <div id="button2" role="button" aria-label="cancel">
      <a href="#cancel">more info</a>
    </div>
    <p>end</p>
    <script>
      document.getElementById('button1').addEventListener('click', () => {});
      document.getElementById('button2').addEventListener('click', () => {});
    </script>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    mockFeedback.expectSpeech('Start')
        .call(doCmd('nextObject'))
        .expectSpeech('OK')
        .call(doCmd('nextObject'))
        .expectSpeech('cancel')
        .call(doCmd('nextObject'))
        .expectSpeech('more info')
        .call(doCmd('nextObject'))
        .expectSpeech('end')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'TouchEditingState', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <input type="text"></input>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    const bounds = rootNode.find({role: RoleType.TEXT_FIELD}).location;
    mockFeedback.expectSpeech('Start')
        .call(doGesture(
            chrome.accessibilityPrivate.Gesture.TOUCH_EXPLORE, bounds.left,
            bounds.top))
        .expectSpeech('Edit text', 'Double tap to start editing')
        .call(doGesture(
            chrome.accessibilityPrivate.Gesture.CLICK, bounds.left, bounds.top))
        .expectSpeech('Edit text', 'is editing')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'TouchGesturesProducesEarcons', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <button>ok</button>
    <a href="chromevox.com">cancel</a>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    mockFeedback.expectSpeech('Start')
        .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
        .expectSpeech('ok', 'Button')
        .expectEarcon(Earcon.BUTTON)
        .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
        .expectSpeech('cancel', 'Link')
        .expectEarcon(Earcon.LINK)
        .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_LEFT1))
        .expectSpeech('ok', 'Button')
        .expectEarcon(Earcon.BUTTON)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'Separator', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <p><span>Hello</span></p>
    <p><span role="separator">Separator content should be read</span></p>
    <p><span>World</span></p>
  `;
  this.runWithLoadedTree(site, function(rootNode) {
    mockFeedback.expectSpeech('Start')
        .call(doCmd('nextObject'))
        .expectSpeech('Hello')
        .call(doCmd('nextObject'))
        .expectSpeech('Separator content should be read', 'Separator')
        .expectBraille('Separator content should be read seprtr')
        .call(doCmd('nextObject'))
        .expectSpeech('World')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'FocusAfterClick', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <button id="clickMe">Click me</button>
    <h1 id="focusMe" tabindex=-1>Focus me</h1>
    <script>
      document.getElementById('clickMe').addEventListener('click', function() {
        document.getElementById('focusMe').focus();
      }, false);
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    DesktopAutomationHandler.announceActions = false;
    mockFeedback.expectSpeech('Start')
        .call(doCmd('nextObject'))
        .expectSpeech('Click me')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Focus me')
        .call(() => {
          assertEquals(
              'Focus me',
              ChromeVoxState.instance.getCurrentRange().start.node.name);
        })
        .replay();
  });
});

SYNC_TEST_F('ChromeVoxBackgroundTest', 'EarconPlayback', function() {
  const engine = ChromeVoxState.instance.earcons_.engine_;
  assertTrue(engine !== undefined);

  // We only test a few earcons here. Not all earcons prevent parallel playback
  // or have mappings into the earcon engine.

  // There are no tracked sources yet.
  assertEquals(0, Object.keys(engine.lastEarconSources_).length);

  // Note that alert modal vs nonmodal would be allowed to play in parallel (as
  // do wrap / wrap edge) because they are different events even though they
  // really play the same sound.
  ChromeVox.earcons.playEarcon(Earcon.ALERT_MODAL);
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);
  const lastAlertSource = engine.lastEarconSources_[Earcon.ALERT_MODAL];
  assertTrue(lastAlertSource !== undefined);

  ChromeVox.earcons.playEarcon(Earcon.ALERT_MODAL);
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);

  // The earcon for this stayed the same (above), so there's no duplicate
  // playback of two alerts.
  assertEquals(lastAlertSource, engine.lastEarconSources_[Earcon.ALERT_MODAL]);

  // This simulates a parallel playback of the button earcon which is allowed.
  ChromeVox.earcons.playEarcon(Earcon.BUTTON);
  assertEquals(2, Object.keys(engine.lastEarconSources_).length);
  assertTrue(engine.lastEarconSources_[Earcon.BUTTON] !== undefined);

  // This gets called by web audio when the earcon finishes.
  lastAlertSource.onended();

  // The button earcon is still playing.
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);
  assertTrue(engine.lastEarconSources_[Earcon.BUTTON] !== undefined);

  // Finish up the button earcon, too.
  engine.lastEarconSources_[Earcon.BUTTON].onended();
  assertEquals(0, Object.keys(engine.lastEarconSources_).length);
});

TEST_F('ChromeVoxBackgroundTest', 'MixedNavWithRangeInvalidation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <button>apple</button>
    <a href="google.com">grape</a>
    <button>banana</button>
  `;
  this.runWithLoadedTree(site, function(root) {
    // Different ways to navigate to the next object.
    const keyboardHandler = ChromeVoxState.instance.keyboardHandler_;
    const nextObjectKeyboard = keyboardHandler.onKeyDown.bind(keyboardHandler, {
      keyCode: KeyCode.RIGHT,
      metaKey: true,
      preventDefault: () => {},
      stopPropagation: () => {}
    });
    const nextObjectBraille = BrailleCommandHandler.onBrailleKeyEvent.bind(
        BrailleCommandHandler, {command: BrailleKeyCommand.PAN_RIGHT});
    const nextObjectGesture =
        GestureCommandHandler.onAccessibilityGesture_.bind(
            GestureCommandHandler, Gesture.SWIPE_RIGHT1);
    const clearCurrentRange = ChromeVoxState.instance.setCurrentRange.bind(
        ChromeVoxState.instance, null);
    const toggleTalkBack = () => {
      ChromeVoxState.instance.talkBackEnabled =
          !ChromeVoxState.instance.talkBackEnabled;
    };

    mockFeedback
        .expectSpeech('Start')

        .call(clearCurrentRange)
        .call(nextObjectKeyboard)
        .expectSpeech('apple', 'Button')

        .call(clearCurrentRange)
        .call(nextObjectBraille)
        .expectSpeech('grape', 'Link')

        .call(clearCurrentRange)
        .call(nextObjectGesture)
        .expectSpeech('banana', 'Button')

        .call(clearCurrentRange)
        .call(nextObjectKeyboard)
        .expectSpeech('Web Content')

        .call(clearCurrentRange)
        .call(toggleTalkBack)
        .call(nextObjectKeyboard)
        .call(() => assertFalse(!!ChromeVoxState.instance.currentRange))

        .call(nextObjectBraille)
        .call(() => assertFalse(!!ChromeVoxState.instance.currentRange))

        .call(nextObjectGesture)
        .call(() => assertFalse(!!ChromeVoxState.instance.currentRange))

        .call(toggleTalkBack)
        .call(nextObjectKeyboard)
        .call(() => assertTrue(!!ChromeVoxState.instance.currentRange))

        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'DetailsChanged', function() {
  const mockFeedback = this.createMockFeedback();

  // Make sure we're not testing reading of the hint from the button's output
  // below.
  localStorage['useVerboseMode'] = false;
  const site = `
    <button id="click">ok</button>
    <p id="details">hello</p>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', () => {
        button.setAttribute('aria-details', 'details');
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const button = root.find({role: RoleType.BUTTON});
    mockFeedback.expectSpeech('ok')
        .call(button.doDefault.bind(button))
        .expectSpeech('Press Search+A, J to jump to details')
        .replay();
  });
});

SYNC_TEST_F('ChromeVoxBackgroundTest', 'PageLoadEarcons', async function() {
  const sawEarcons = [];
  const fakeEarcons = {playEarcon: (earcon) => sawEarcons.push(earcon)};
  Object.defineProperty(ChromeVox, 'earcons', {get: () => fakeEarcons});
  AutomationUtil.getTopLevelRoot = (node) => node;

  const module = await import('./page_load_sound_handler.js');
  const PageLoadSoundHandler = module.PageLoadSoundHandler;

  // Use this specific object to control the load environment.
  const handler = new PageLoadSoundHandler();

  // Build up a fake automation node with a parent and root.
  const fakeNode = {};
  fakeNode.docUrl = 'foo';
  fakeNode.root = fakeNode;
  fakeNode.parent = {state: {focused: true}};

  handler.onLoadStart({target: fakeNode});
  assertEqualStringArrays([Earcon.PAGE_START_LOADING], sawEarcons);
  handler.onLoadComplete({target: fakeNode});
  assertEqualStringArrays(
      [Earcon.PAGE_START_LOADING, Earcon.PAGE_FINISH_LOADING], sawEarcons);

  // No extra earcons.
  handler.onLoadComplete({target: fakeNode});
  assertEqualStringArrays(
      [Earcon.PAGE_START_LOADING, Earcon.PAGE_FINISH_LOADING], sawEarcons);

  // Try a range change that finishes the load sound.
  sawEarcons.length = 0;
  handler.onLoadStart({target: fakeNode});
  fakeNode.docLoadingProgress = 1;
  handler.onCurrentRangeChanged({start: {node: fakeNode}});
  assertEqualStringArrays(
      [Earcon.PAGE_START_LOADING, Earcon.PAGE_FINISH_LOADING], sawEarcons);
});

TEST_F('ChromeVoxBackgroundTest', 'NewTabRead', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<p>start</p><p>end</p>`;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech('end')
        .call(press(KeyCode.T, {ctrl: true}))
        .expectSpeech(/New Tab/)
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NestedMenuHints', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="menu" aria-orientation="vertical">
      <div role="menu" aria-orientation="horizontal">
        <div tabindex="0" role="menuitem">hello</div>
        <div tabindex="0" role="menuitem">bro</div>
      </div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback
        .expectSpeech(
            'Press left or right arrow to navigate; enter to activate')
        .call(
            () => assertFalse(mockFeedback.utteranceInQueue(
                'Press up or down arrow to navigate; enter to activate')))
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SkipLabelDescriptionFor', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <label>
      <input type="checkbox" name="enableSpeechLogging">
      <span>Enable speech logging</span>
    </label>
    <p>end</p>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('start')
        .call(doCmd('nextObject'))
        .expectSpeech('Enable speech logging', 'Check box')
        .call(doCmd('nextObject'))
        .expectSpeech('end')
        .call(doCmd('previousObject'))
        .expectSpeech('Enable speech logging', 'Check box')
        .call(doCmd('previousObject'))
        .expectSpeech('start')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'Abbreviation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <abbr title="uniform resource locator">URL</abbr>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.expectSpeech('URL', 'uniform resource locator', 'Abbreviation')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'EndOfText', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div tabindex=0 role="textbox" contenteditable><p>abc</p><p>123</p></div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const contentEditable = root.find({role: RoleType.TEXT_FIELD});

    this.listenOnce(contentEditable, EventType.FOCUS, function() {
      mockFeedback.call(press(KeyCode.RIGHT))
          .expectSpeech('b')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('c')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('\n')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('1')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('2')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('3')
          .call(press(KeyCode.RIGHT))
          .expectSpeech('End of text')

          .call(press(KeyCode.LEFT))
          .expectSpeech('3')
          .call(press(KeyCode.LEFT))
          .expectSpeech('2')
          .call(press(KeyCode.LEFT))
          .expectSpeech('1')
          .call(press(KeyCode.LEFT))
          .expectSpeech('\n')
          .call(press(KeyCode.LEFT))
          .expectSpeech('c')
          .call(press(KeyCode.LEFT))
          .expectSpeech('b')
          .call(press(KeyCode.LEFT))
          .expectSpeech('a')

          .replay();
    }.bind(this));
    contentEditable.focus();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'ShowContextMenuOnViewsTab', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<p>test</p>`;
  this.runWithLoadedTree(site, function(root) {
    const tabs = root.findAll({Role: RoleType.TAB});
    assertTrue(tabs.length > 0);
    tabs[0].showContextMenu();
    mockFeedback.expectSpeech(/menu opened/).replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'SelectWithOptGroup', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <select>
      <optgroup label="Theropods">
          <option>Tyrannosaurus</option>
          <option>Velociraptor</option>
          <option>Deinonychus</option>
      </optgroup>
    </select>
  `;
  this.runWithLoadedTree(site, function() {
    mockFeedback.expectSpeech('Tyrannosaurus', 'has pop up', 'Collapsed')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Tyrannosaurus')
        .call(press(KeyCode.DOWN))
        .expectSpeech('Velociraptor')
        .call(press(KeyCode.DOWN))
        .expectSpeech('Deinonychus')
        .call(press(KeyCode.UP))
        .expectSpeech('Velociraptor')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'GroupNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p><span>hello</span><a href="a.com">hi</a><a href="a.com">hey</a></p>
    <p><span>goodbye</span><a href="a.com">bye</a><a href="a.com">chow</a></p>
  `;
  this.runWithLoadedTree(site, function() {
    mockFeedback.call(doCmd('nextGroup'))
        .expectSpeech('goodbye', 'bye', 'Link', 'chow', 'Link')
        .call(doCmd('nextObject'))
        .expectSpeech('goodbye')
        .call(doCmd('nextObject'))
        .expectSpeech('bye', 'Link')
        .call(doCmd('previousGroup'))
        .expectSpeech('hello', 'hi', 'Link', 'hey', 'Link')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'AllowIframeToBeFocused', function(root) {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>hello</p>
    <iframe id="frame" tabindex=-1
        srcdoc="<title>test title</title><p>world</p>"></iframe>
    <button id="click"></button>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', () => {
        document.getElementById('frame').focus();
      });
    </script>
  `;
  this.runWithLoadedTree(site, function(root) {
    const button = root.find({role: RoleType.BUTTON});
    mockFeedback.expectSpeech('hello')
        .call(button.doDefault.bind(button))
        .expectSpeech('test title')
        .replay();
  });
});

TEST_F('ChromeVoxBackgroundTest', 'NewWindowWebSpeech', function() {
  this.newCallback(async () => {
    const speech = [];
    let onSpeech;
    ChromeVox.tts.speak = (textString) => {
      speech.push(textString);
      if (onSpeech) {
        onSpeech(textString);
      }
    };

    chrome.runtime.openOptionsPage();

    await new Promise(resolve => {
      onSpeech = (textString) => {
        if (textString === 'ChromeVox Options') {
          resolve();
        }
      };
    });

    press(KeyCode.TAB)();

    await new Promise(resolve => {
      onSpeech = resolve;
    });

    // Check to ensure there are no duplicate announcements.
    assertEquals(
        speech.indexOf('ChromeVox Options'),
        speech.lastIndexOf('ChromeVox Options'));

    // Ensure there are no announcements about the Tab role.
    assertTrue(speech.every(text => {
      return text.indexOf('Tab') !== 0;
    }));
  })();
});

TEST_F('ChromeVoxBackgroundTest', 'MultipleListBoxes', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="listbox" aria-expanded="false" aria-label="Configuration 1">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>

    <div role="listbox" aria-expanded="false" aria-label="Configuration 2">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>

    <div role="listbox" aria-expanded="false" aria-label="Configuration 3">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    mockFeedback.call(press(KeyCode.TAB))
        .expectSpeech(
            'Listbox item 1', ' 1 of 3 ', 'Configuration 1', 'List box')
        .call(press(KeyCode.TAB))
        .expectSpeech(
            'Listbox item 2', ' 2 of 3 ', 'Configuration 2', 'List box')
        .call(press(KeyCode.TAB))
        .expectSpeech(
            'Listbox item 3', ' 3 of 3 ', 'Configuration 3', 'List box')
        .replay();
  });
});

// Make sure linear navigation does not go inside ListBox's options.
TEST_F('ChromeVoxBackgroundTest', 'ListBoxLinearNavigation', function() {
  const mockFeedback = this.createMockFeedback();
  const site =

      this.runWithLoadedTree(this.listBoxDoc, function(root) {
        mockFeedback.call(doCmd('nextObject'))
            .expectSpeech('Select an item', 'List box')
            .call(doCmd('nextObject'))
            .expectSpeech('Click', 'Button')
            .call(doCmd('previousObject'))
            .expectSpeech('Select an item', 'List box')
            .replay();
      });
});

// Make sure navigation with Tab to ListBox lands on options.
TEST_F('ChromeVoxBackgroundTest', 'ListBoxItemsNavigation', function() {
  const mockFeedback = this.createMockFeedback();

  this.runWithLoadedTree(this.listBoxDoc, function(root) {
    mockFeedback.call(press(KeyCode.TAB))
        .expectSpeech(
            'Listbox item one', ' 1 of 3 ', 'Select an item', 'List box')
        .call(doCmd('nextObject'))
        .expectSpeech('Listbox item two', ' 2 of 3 ')
        .call(doCmd('nextObject'))
        .expectSpeech('Listbox item three', ' 3 of 3 ')
        .replay();
  });
});
