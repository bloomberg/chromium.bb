// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(
    ['assert_additions.js', 'callback_helper.js', 'common.js', 'doc_utils.js']);

/**
 * Base test fixture for end to end tests (tests that need a full extension
 * renderer) for accessibility component extensions. These tests run inside of
 * the extension's background page context.
 */
E2ETestBase = class extends testing.Test {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
    this.desktop_;
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
  #include "chrome/browser/ash/crosapi/browser_manager.h"
  #include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
  #include "chrome/browser/ui/browser.h"
  #include "content/public/test/browser_test_utils.h"
  #include "extensions/browser/extension_host.h"
  #include "extensions/browser/process_manager.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
    if (ash_starter()->HasLacrosArgument()) {
      crosapi::BrowserManager::Get()->NewTab();
      ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
    }
      `);
  }

  /** @override */
  testGenPostamble() {
    GEN(`
    if (fail_on_console_error) {
      EXPECT_EQ(0u, console_observer.messages().size())
          << "Found console.warn or console.error with message: "
          << console_observer.GetMessageAt(0);
    }
    `);
  }

  testGenPreambleCommon(extensionIdName, failOnConsoleError = true) {
    GEN(`
    WaitForExtension(extension_misc::${extensionIdName}, std::move(load_cb));

    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(
                extension_misc::${extensionIdName});

    bool fail_on_console_error = ${failOnConsoleError};
    content::WebContentsConsoleObserver console_observer(host->host_contents());
    // A11y extensions should not log warnings or errors: these should cause
    // test failures.
    auto filter =
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level ==
              blink::mojom::ConsoleMessageLevel::kWarning ||
              message.log_level == blink::mojom::ConsoleMessageLevel::kError;
        };
    if (fail_on_console_error) {
      console_observer.SetFilter(base::BindRepeating(filter));
    }
    `);
  }

  /**
   * Listens and waits for the first event on the given node of the given type.
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} eventType
   * @param {!function()} callback
   * @param {boolean} capture
   */
  listenOnce(node, eventType, callback, capture) {
    const innerCallback = this.newCallback(function() {
      node.removeEventListener(eventType, innerCallback, capture);
      callback.apply(this, arguments);
    });
    node.addEventListener(eventType, innerCallback, capture);
  }

  /**
   * Listens to and waits for the specified event type on the given node until
   * |predicate| is satisfied.
   * @param {!function(): boolean} predicate
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} eventType
   * @param {!function()} callback
   * @param {boolean} capture
   */
  listenUntil(predicate, node, eventType, callback, capture = false) {
    callback = this.newCallback(callback);
    if (predicate()) {
      callback();
      return;
    }

    const listener = () => {
      if (predicate()) {
        node.removeEventListener(eventType, listener, capture);
        callback.apply(this, arguments);
      }
    };
    node.addEventListener(eventType, listener, capture);
  }

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture. Optionally, return a promise to
   * defer completion.
   * @return {Function}
   */
  newCallback(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
  }

  /**
   * Gets the desktop from the automation API and runs |callback|.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |opt_callback| must be wrapped with
   * |this.newCallback| if passed to asynchronous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called with the desktop node once it's retrieved.
   */
  runWithLoadedDesktop(callback) {
    chrome.automation.getDesktop(this.newCallback(callback));
  }

  /**
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| when a load complete fires.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |callback| must be wrapped with
   * |this.newCallback| if passed to asynchronous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {string|function(): string} doc An HTML snippet, optionally wrapped
   *     inside of a function.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called with the root web area node once the document is ready.
   * @param {{url: (string=), returnDesktop: (boolean=)}}
   *     opt_params
   *           url Optional url to wait for. Defaults to undefined.
   */
  runWithLoadedTree(doc, callback, opt_params = {}) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop((desktop) => {
      const url = opt_params.url || DocUtils.createUrlForDoc(doc);

      chrome.commandLinePrivate.hasSwitch(
          'lacros-chrome-path', hasLacrosChromePath => {
            // The below block handles opening a url either in a Lacros tab or
            // Ash tab. For Lacros, we re-use an already open Lacros tab. For
            // Ash, we use the chrome.tabs api.

            // This flag controls whether we've requested navigation to |url|
            // within the open Lacros tab.
            let didNavigateForLacros = false;

            // Listens to both load completes and focus events to eventually
            // trigger the test callback.
            const listener = (event) => {
              if (hasLacrosChromePath && !didNavigateForLacros) {
                // We have yet to request navigation in the Lacros tab. Do so
                // now by getting the default focus (the address bar), setting
                // the value to the url and then performing do default on the
                // auto completion node. This is somewhat involved, so each step
                // is commented.
                chrome.automation.getFocus(focus => {
                  // It's possible focus is elsewhere; wait until it lands on
                  // the address bar text field.
                  if (focus.role !== chrome.automation.RoleType.TEXT_FIELD) {
                    return;
                  }

                  // Next, we want to validate we're actually within a Lacros
                  // window. Do so by scanning upward until we see the presence
                  // of an app id which indicates an app subtree. See
                  // go/lacros-accessibility for details.
                  let app = focus;
                  while (app && !app.appId) {
                    app = app.parent;
                  }

                  if (app) {
                    didNavigateForLacros = true;

                    // This populates the address bar as if we typed the url.
                    focus.setValue(url);

                    // Next, wait until the auto completion shows up.
                    const clickAutocomplete = () => {
                      focus.removeEventListener(
                          'controlsChanged', clickAutocomplete);

                      // The text field relates to the auto complete list box
                      // via controlledBy. The |controls| node structure here
                      // nests several levels until the listBoxOption we want.
                      const autoCompleteListBoxOption =
                          focus.controls[0].firstChild.firstChild;
                      assertEquals(
                          'listBoxOption', autoCompleteListBoxOption.role);
                      autoCompleteListBoxOption.doDefault();
                    };
                    focus.addEventListener(
                        'controlsChanged', clickAutocomplete);
                  }
                });
                return;
              }

              // Navigation has occurred, but we need to ensure the url we want
              // has loaded.
              if (event.target.root.url !== url ||
                  !event.target.root.docLoaded) {
                return;
              }

              // Finally, when we get here, we've successfully navigated to the
              // |url| in either Lacros or Ash.
              desktop.removeEventListener('focus', listener, true);
              desktop.removeEventListener('loadComplete', listener, true);
              callback && callback(event.target.root);
              callback = null;
            };

            // Setup the listener above for focus and load complete listening.
            this.desktop_ = desktop;
            desktop.addEventListener('focus', listener, true);
            desktop.addEventListener('loadComplete', listener, true);

            // The easy case -- just open the Ash tab.
            if (!hasLacrosChromePath) {
              const createParams = {active: true, url};
              chrome.tabs.create(createParams);
            }
          });
    });
  }

  /**
   * Opens the options page for the running extension and calls |callback| with
   * the options page root once ready.
   * @param {function(chrome.automation.AutomationNode)} callback
   * @param {!RegExp} matchUrlRegExp The url pattern of the options page if
   *     different than the supplied default pattern below.
   */
  runWithLoadedOptionsPage(callback, matchUrlRegExp = /options.html/) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop((desktop) => {
      const listener = (event) => {
        if (!matchUrlRegExp.test(event.target.docUrl) ||
            !event.target.docLoaded) {
          return;
        }

        desktop.removeEventListener(
            chrome.automation.EventType.LOAD_COMPLETE, listener);

        callback(event.target);
      };
      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, listener);
      chrome.runtime.openOptionsPage();
    });
  }

  /**
   * Finds one specific node in the automation tree.
   * This function is expected to run within a callback passed to
   *     runWithLoadedTree().
   * @param {function(chrome.automation.AutomationNode): boolean} predicate A
   *     predicate that uniquely specifies one automation node.
   * @param {string=} nodeDescription An optional description of what node was
   *     being looked for.
   * @return {!chrome.automation.AutomationNode}
   */
  findNodeMatchingPredicate(
      predicate, nodeDescription = 'node matching the predicate') {
    assertNotNullNorUndefined(
        this.desktop_,
        'findNodeMatchingPredicate called from invalid location.');
    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD, {visit: predicate});
    const node = treeWalker.next().node;
    assertNotNullNorUndefined(node, 'Could not find ' + nodeDescription + '.');
    assertNullOrUndefined(
        treeWalker.next().node, 'Found more than one ' + nodeDescription + '.');
    return node;
  }
};

/** @override */
E2ETestBase.prototype.isAsync = true;
/**
 * @override
 * No UI in the background context.
 */
E2ETestBase.prototype.runAccessibilityChecks = false;
