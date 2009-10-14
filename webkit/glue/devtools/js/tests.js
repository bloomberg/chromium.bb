// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This file contains small testing framework along with the
 * test suite for the frontend. These tests are a part of the continues build
 * and are executed by the devtools_sanity_unittest.cc as a part of the
 * Interactive UI Test suite.
 */

if (window.domAutomationController) {

var ___interactiveUiTestsMode = true;

/**
 * Test suite for interactive UI tests.
 * @constructor
 */
TestSuite = function() {
  this.controlTaken_ = false;
  this.timerId_ = -1;
};


/**
 * Reports test failure.
 * @param {string} message Failure description.
 */
TestSuite.prototype.fail = function(message) {
  if (this.controlTaken_) {
    this.reportFailure_(message);
  } else {
    throw message;
  }
};


/**
 * Equals assertion tests that expected == actual.
 * @param {Object} expected Expected object.
 * @param {Object} actual Actual object.
 * @param {string} opt_message User message to print if the test fails.
 */
TestSuite.prototype.assertEquals = function(expected, actual, opt_message) {
  if (expected != actual) {
    var message = 'Expected: "' + expected + '", but was "' + actual + '"';
    if (opt_message) {
      message = opt_message + '(' + message + ')';
    }
    this.fail(message);
  }
};


/**
 * True assertion tests that value == true.
 * @param {Object} value Actual object.
 * @param {string} opt_message User message to print if the test fails.
 */
TestSuite.prototype.assertTrue = function(value, opt_message) {
  this.assertEquals(true, !!value, opt_message);
};


/**
 * Contains assertion tests that string contains substring.
 * @param {string} string Outer.
 * @param {string} substring Inner.
 */
TestSuite.prototype.assertContains = function(string, substring) {
  if (string.indexOf(substring) == -1) {
    this.fail('Expected to: "' + string + '" to contain "' + substring + '"');
  }
};


/**
 * Takes control over execution.
 */
TestSuite.prototype.takeControl = function() {
  this.controlTaken_ = true;
  // Set up guard timer.
  var self = this;
  this.timerId_ = setTimeout(function() {
    self.reportFailure_('Timeout exceeded: 20 sec');
  }, 20000);
};


/**
 * Releases control over execution.
 */
TestSuite.prototype.releaseControl = function() {
  if (this.timerId_ != -1) {
    clearTimeout(this.timerId_);
    this.timerId_ = -1;
  }
  this.reportOk_();
};


/**
 * Async tests use this one to report that they are completed.
 */
TestSuite.prototype.reportOk_ = function() {
  window.domAutomationController.send('[OK]');
};


/**
 * Async tests use this one to report failures.
 */
TestSuite.prototype.reportFailure_ = function(error) {
  if (this.timerId_ != -1) {
    clearTimeout(this.timerId_);
    this.timerId_ = -1;
  }
  window.domAutomationController.send('[FAILED] ' + error);
};


/**
 * Runs all global functions starting with 'test' as unit tests.
 */
TestSuite.prototype.runTest = function(testName) {
  try {
    this[testName]();
    if (!this.controlTaken_) {
      this.reportOk_();
    }
  } catch (e) {
    this.reportFailure_(e);
  }
};


/**
 * @param {string} panelName Name of the panel to show.
 */
TestSuite.prototype.showPanel = function(panelName) {
  // Open Scripts panel.
  var toolbar = document.getElementById('toolbar');
  var button = toolbar.getElementsByClassName(panelName)[0];
  button.click();
  this.assertEquals(WebInspector.panels[panelName],
      WebInspector.currentPanel);
};


/**
 * Overrides the method with specified name until it's called first time.
 * @param {Object} receiver An object whose method to override.
 * @param {string} methodName Name of the method to override.
 * @param {Function} override A function that should be called right after the
 *     overriden method returns.
 * @param {boolean} opt_sticky Whether restore original method after first run
 *     or not.
 */
TestSuite.prototype.addSniffer = function(receiver, methodName, override,
                                          opt_sticky) {
  var orig = receiver[methodName];
  if (typeof orig != 'function') {
    this.fail('Cannot find method to override: ' + methodName);
  }
  var test = this;
  receiver[methodName] = function(var_args) {
    try {
      var result = orig.apply(this, arguments);
    } finally {
      if (!opt_sticky) {
        receiver[methodName] = orig;
      }
    }
    // In case of exception the override won't be called.
    try {
      override.apply(this, arguments);
    } catch (e) {
      test.fail('Exception in overriden method "' + methodName + '": ' + e);
    }
    return result;
  };
};


// UI Tests


/**
 * Tests that the real injected host is present in the context.
 */
TestSuite.prototype.testHostIsPresent = function() {
  this.assertTrue(typeof DevToolsHost == 'object' && !DevToolsHost.isStub);
};


/**
 * Tests elements tree has an 'HTML' root.
 */
TestSuite.prototype.testElementsTreeRoot = function() {
  var doc = WebInspector.domAgent.document;
  this.assertEquals('HTML', doc.documentElement.nodeName);
  this.assertTrue(doc.documentElement.hasChildNodes());
};


/**
 * Tests that main resource is present in the system and that it is
 * the only resource.
 */
TestSuite.prototype.testMainResource = function() {
  var tokens = [];
  var resources = WebInspector.resources;
  for (var id in resources) {
    tokens.push(resources[id].lastPathComponent);
  }
  this.assertEquals('simple_page.html', tokens.join(','));
};


/**
 * Tests that resources tab is enabled when corresponding item is selected.
 */
TestSuite.prototype.testEnableResourcesTab = function() {
  this.showPanel('resources');

  var test = this;
  this.addSniffer(WebInspector, 'addResource',
      function(identifier, payload) {
        test.assertEquals('simple_page.html', payload.lastPathComponent);
        WebInspector.panels.resources.refresh();
        WebInspector.resources[identifier]._resourcesTreeElement.select();

        test.releaseControl();
      });

  // Following call should lead to reload that we capture in the
  // addResource override.
  WebInspector.panels.resources._enableResourceTracking();

  // We now have some time to report results to controller.
  this.takeControl();
};


/**
 * Tests resource headers.
 */
TestSuite.prototype.testResourceHeaders = function() {
  this.showPanel('resources');

  var test = this;

  var requestOk = false;
  var responseOk = false;
  var timingOk = false;

  this.addSniffer(WebInspector, 'addResource',
      function(identifier, payload) {
        var resource = this.resources[identifier];
        if (resource.mainResource) {
          // We are only interested in secondary resources in this test.
          return;
        }

        var requestHeaders = JSON.stringify(resource.requestHeaders);
        test.assertContains(requestHeaders, 'Accept');
        requestOk = true;
      }, true);

  this.addSniffer(WebInspector, 'updateResource',
      function(identifier, payload) {
        var resource = this.resources[identifier];
        if (resource.mainResource) {
          // We are only interested in secondary resources in this test.
          return;
        }

        if (payload.didResponseChange) {
          var responseHeaders = JSON.stringify(resource.responseHeaders);
          test.assertContains(responseHeaders, 'Content-type');
          test.assertContains(responseHeaders, 'Content-Length');
          test.assertTrue(typeof resource.responseReceivedTime != 'undefnied');
          responseOk = true;
        }

        if (payload.didTimingChange) {
          test.assertTrue(typeof resource.startTime != 'undefnied');
          timingOk = true;
        }

        if (payload.didCompletionChange) {
          test.assertTrue(requestOk);
          test.assertTrue(responseOk);
          test.assertTrue(timingOk);
          test.assertTrue(typeof resource.endTime != 'undefnied');
          test.releaseControl();
        }
      }, true);

  WebInspector.panels.resources._enableResourceTracking();
  this.takeControl();
};


/**
 * Test that profiler works.
 */
TestSuite.prototype.testProfilerTab = function() {
  this.showPanel('profiles');

  var test = this;
  this.addSniffer(WebInspector, 'addProfile',
      function(profile) {
        var panel = WebInspector.panels.profiles;
        panel.showProfile(profile);
        var node = panel.visibleView.profileDataGridTree.children[0];
        // Iterate over displayed functions and search for a function
        // that is called 'fib' or 'eternal_fib'. If found, it will mean
        // that we actually have profiled page's code.
        while (node) {
          if (node.functionName.indexOf('fib') != -1) {
            test.releaseControl();
          }
          node = node.traverseNextNode(true, null, true);
        }

        test.fail();
      });
  var ticksCount = 0;
  var tickRecord = '\nt,';
  this.addSniffer(RemoteDebuggerAgent, 'DidGetNextLogLines',
      function(log) {
        var pos = 0;
        while ((pos = log.indexOf(tickRecord, pos)) != -1) {
          pos += tickRecord.length;
          ticksCount++;
        }
        if (ticksCount > 100) {
          InspectorController.stopProfiling();
        }
      }, true);

  InspectorController.startProfiling();
  this.takeControl();
};


/**
 * Tests that scripts tab can be open and populated with inspected scripts.
 */
TestSuite.prototype.testShowScriptsTab = function() {
  var parsedDebuggerTestPageHtml = false;

  // Intercept parsedScriptSource calls to check that all expected scripts are
  // added to the debugger.
  var test = this;
  var receivedConsoleApiSource = false;
  this.addSniffer(WebInspector, 'parsedScriptSource',
      function(sourceID, sourceURL, source, startingLine) {
        if (sourceURL == undefined) {
          if (receivedConsoleApiSource) {
            test.fail('Unexpected script without URL');
          } else {
            receivedConsoleApiSource = true;
          }
        } else if (sourceURL.search(/debugger_test_page.html$/) != -1) {
          if (parsedDebuggerTestPageHtml) {
            test.fail('Unexpected parse event: ' + sourceURL);
          }
          parsedDebuggerTestPageHtml = true;
        } else {
          test.fail('Unexpected script URL: ' + sourceURL);
        }

        if (!WebInspector.panels.scripts.visibleView) {
          test.fail('No visible script view: ' + sourceURL);
        }

        // There should be two scripts: one for the main page and another
        // one which is source of console API(see
        // InjectedScript._ensureCommandLineAPIInstalled).
        if (parsedDebuggerTestPageHtml && receivedConsoleApiSource) {
           test.releaseControl();
        }
      }, true /* sticky */);

  this.showPanel('scripts');

  // Wait until all scripts are added to the debugger.
  this.takeControl();
};


/**
 * Tests that scripts are not duplicaed on Scripts tab switch.
 */
TestSuite.prototype.testNoScriptDuplicatesOnPanelSwitch = function() {
  var test = this;

  // There should be two scripts: one for the main page and another
  // one which is source of console API(see
  // InjectedScript._ensureCommandLineAPIInstalled).
  var expectedScriptsCount = 2;
  var parsedScripts = [];


  function switchToElementsTab() {
    test.showPanel('elements');
    setTimeout(switchToScriptsTab, 0);
  }

  function switchToScriptsTab() {
    test.showPanel('scripts');
    setTimeout(checkScriptsPanel, 0);
  }

  function checkScriptsPanel() {
    test.assertTrue(!!WebInspector.panels.scripts.visibleView,
                    'No visible script view.');
    var select = WebInspector.panels.scripts.filesSelectElement;
    test.assertEquals(expectedScriptsCount, select.options.length,
                      'Unexpected options count');
    test.releaseControl();
  }

  this.addSniffer(WebInspector, 'parsedScriptSource',
      function(sourceID, sourceURL, source, startingLine) {
        test.assertTrue(
            parsedScripts.indexOf(sourceURL) == -1,
            'Duplicated script: ' + sourceURL);
        test.assertTrue(
           parsedScripts.length < expectedScriptsCount,
           'Too many scripts: ' + sourceURL);
        parsedScripts.push(sourceURL);

        if (parsedScripts.length == expectedScriptsCount) {
          setTimeout(switchToElementsTab, 0);
        }
      }, true /* sticky */);

  this.showPanel('scripts');

  // Wait until all scripts are added to the debugger.
  this.takeControl();
};


/**
 * Tests that a breakpoint can be set.
 */
TestSuite.prototype.testSetBreakpoint = function() {
  var parsedDebuggerTestPageHtml = false;
  var parsedDebuggerTestJs = false;

  this.showPanel('scripts');

  var scriptUrl = null;
  var breakpointLine = 12;

  var test = this;
  this.addSniffer(devtools.DebuggerAgent.prototype, 'handleScriptsResponse_',
      function(msg) {
        var scriptSelect = document.getElementById('scripts-files');
        var options = scriptSelect.options;

        // There should be console API source (see
        // InjectedScript._ensureCommandLineAPIInstalled) and the page script.
        test.assertEquals(2, options.length, 'Unexpected number of scripts(' +
            test.optionsToString_(options) + ')');

        test.showMainPageScriptSource_(
            'debugger_test_page.html',
            function(view, url) {
              view._addBreakpoint(breakpointLine);
              // Force v8 execution.
              RemoteToolsAgent.ExecuteVoidJavaScript();
              test.waitForSetBreakpointResponse_(url, breakpointLine,
                  function() {
                    test.releaseControl();
                  });
            });
      });


  this.takeControl();
};


/**
 * Serializes options collection to string.
 * @param {HTMLOptionsCollection} options
 * @return {string}
 */
TestSuite.prototype.optionsToString_ = function(options) {
  var names = [];
  for (var i = 0; i < options.length; i++) {
    names.push('"' + options[i].text + '"');
  }
  return names.join(',');
};


/**
 * Ensures that main HTML resource is selected in Scripts panel and that its
 * source frame is setup. Invokes the callback when the condition is satisfied.
 * @param {HTMLOptionsCollection} options
 * @param {function(WebInspector.SourceView,string)} callback
 */
TestSuite.prototype.showMainPageScriptSource_ = function(scriptName, callback) {
  var test = this;

  var scriptSelect = document.getElementById('scripts-files');
  var options = scriptSelect.options;

  // There should be console API source (see
  // InjectedScript._ensureCommandLineAPIInstalled) and the page script.
  test.assertEquals(2, options.length,
      'Unexpected number of scripts(' + test.optionsToString_(options) + ')');

  // Select page's script if it's not current option.
  var scriptResource;
  if (options[scriptSelect.selectedIndex].text === scriptName) {
    scriptResource = options[scriptSelect.selectedIndex].representedObject;
  } else {
    var pageScriptIndex = -1;
    for (var i = 0; i < options.length; i++) {
      if (options[i].text === scriptName) {
        pageScriptIndex = i;
        break;
      }
    }
    test.assertTrue(-1 !== pageScriptIndex,
                    'Script with url ' + scriptName + ' not found among ' +
                        test.optionsToString_(options));
    scriptResource = options[pageScriptIndex].representedObject;

    // Current panel is 'Scripts'.
    WebInspector.currentPanel._showScriptOrResource(scriptResource);
    test.assertEquals(pageScriptIndex, scriptSelect.selectedIndex,
                      'Unexpected selected option index.');
  }

  test.assertTrue(scriptResource instanceof WebInspector.Resource,
                  'Unexpected resource class.');
  test.assertTrue(!!scriptResource.url, 'Resource URL is null.');
  test.assertTrue(
      scriptResource.url.search(scriptName + '$') != -1,
      'Main HTML resource should be selected.');

  var scriptsPanel = WebInspector.panels.scripts;

  var view = scriptsPanel.visibleView;
  test.assertTrue(view instanceof WebInspector.SourceView);

  if (!view.sourceFrame._isContentLoaded()) {
    test.addSniffer(view, '_sourceFrameSetupFinished', function(event) {
      callback(view, scriptResource.url);
    });
  } else {
    callback(view, scriptResource.url);
  }
};


/*
 * Evaluates the code in the console as if user typed it manually and invokes
 * the callback when the result message is received and added to the console.
 * @param {string} code
 * @param {function(string)} callback
 */
TestSuite.prototype.evaluateInConsole_ = function(code, callback) {
  WebInspector.console.visible = true;
  WebInspector.console.prompt.text = code;
  WebInspector.console.promptElement.handleKeyEvent(
      new TestSuite.KeyEvent('Enter'));

  this.addSniffer(WebInspector.ConsoleView.prototype, 'addMessage',
      function(commandResult) {
        callback(commandResult.toMessageElement().textContent);
      });
};


/*
 * Waits for 'setbreakpoint' response, checks that corresponding breakpoint
 * was successfully set and invokes the callback if it was.
 * @param {string} scriptUrl
 * @param {number} breakpointLine
 * @param {function()} callback
 */
TestSuite.prototype.waitForSetBreakpointResponse_ = function(scriptUrl,
                                                             breakpointLine,
                                                             callback) {
  var test = this;
  test.addSniffer(
      devtools.DebuggerAgent.prototype,
      'handleSetBreakpointResponse_',
      function(msg) {
        var bps = this.urlToBreakpoints_[scriptUrl];
        test.assertTrue(!!bps, 'No breakpoints for line ' + breakpointLine);
        var line = devtools.DebuggerAgent.webkitToV8LineNumber_(breakpointLine);
        test.assertTrue(!!bps[line].getV8Id(),
                        'Breakpoint id was not assigned.');
        callback();
      });
};


/**
 * Tests eval on call frame.
 */
TestSuite.prototype.testEvalOnCallFrame = function() {
  this.showPanel('scripts');

  var breakpointLine = 16;

  var test = this;
  this.addSniffer(devtools.DebuggerAgent.prototype, 'handleScriptsResponse_',
      function(msg) {
        test.showMainPageScriptSource_(
            'debugger_test_page.html',
            function(view, url) {
              view._addBreakpoint(breakpointLine);
              // Force v8 execution.
              RemoteToolsAgent.ExecuteVoidJavaScript();
              test.waitForSetBreakpointResponse_(url, breakpointLine,
                                                 setBreakpointCallback);
            });
      });

  function setBreakpointCallback() {
    // Since breakpoints are ignored in evals' calculate() function is
    // execute after zero-timeout so that the breakpoint is hit.
    test.evaluateInConsole_(
        'setTimeout("calculate(123)" , 0)',
        function(resultText) {
          test.assertTrue(!isNaN(resultText),
                          'Failed to get timer id: ' + resultText);
          waitForBreakpointHit();
        });
  }

  function waitForBreakpointHit() {
    test.addSniffer(
        devtools.DebuggerAgent.prototype,
        'handleBacktraceResponse_',
        function(msg) {
          test.assertEquals(2, this.callFrames_.length,
                            'Unexpected stack depth on the breakpoint. ' +
                                JSON.stringify(msg));
          test.assertEquals('calculate', this.callFrames_[0].functionName,
                            'Unexpected top frame function.');
          // Evaluate 'e+1' where 'e' is an argument of 'calculate' function.
          test.evaluateInConsole_(
              'e+1',
              function(resultText) {
                test.assertEquals('124', resultText, 'Unexpected "e+1" value.');
                test.releaseControl();
              });
        });
  }

  this.takeControl();
};


/**
 * Tests that console auto completion works when script execution is paused.
 */
TestSuite.prototype.testCompletionOnPause = function() {
  this.showPanel('scripts');
  var test = this;
  this._executeCodeWhenScriptsAreParsed(
      'handleClick()',
      ['completion_on_pause.html$']);

  this._waitForScriptPause(
      {
        functionsOnStack: ['innerFunction', 'handleClick',
                           '(anonymous function)'],
        lineNumber: 9,
        lineText: '    debugger;'
      },
      showConsole);

  function showConsole() {
    test.addSniffer(WebInspector.console, 'afterShow', testLocalsCompletion);
    WebInspector.showConsole();
  }

  function testLocalsCompletion() {
    checkCompletions(
        'th',
        ['parameter1', 'closureLocal', 'p', 'createClosureLocal'],
        testThisCompletion);
  }

  function testThisCompletion() {
    checkCompletions(
        'this.',
        ['field1', 'field2', 'm'],
        testFieldCompletion);
  }

  function testFieldCompletion() {
    checkCompletions(
        'this.field1.',
        ['id', 'name'],
        function() {
          test.releaseControl();
        });
  }

  function checkCompletions(expression, expectedProperties, callback) {
      test.addSniffer(WebInspector.console, '_reportCompletions',
        function(bestMatchOnly, completionsReadyCallback, dotNotation,
            bracketNotation, prefix, result, isException) {
          test.assertTrue(!isException,
                          'Exception while collecting completions');
          for (var i = 0; i < expectedProperties.length; i++) {
            var name = expectedProperties[i];
            test.assertTrue(result[name], 'Name ' + name +
                ' not found among the completions: ' +
                JSON.stringify(result));
          }
          test.releaseControl();
        });
    WebInspector.console.prompt.text = expression;
    WebInspector.console.prompt.autoCompleteSoon();
  }

  this.takeControl();
};


/**
 * Tests that inspected page doesn't hang on reload if it contains a syntax
 * error and DevTools window is open.
 */
TestSuite.prototype.testAutoContinueOnSyntaxError = function() {
  this.showPanel('scripts');
  var test = this;

  function checkScriptsList() {
    var scriptSelect = document.getElementById('scripts-files');
    var options = scriptSelect.options;
    // There should be only console API source (see
    // InjectedScript._ensureCommandLineAPIInstalled) since the page script
    // contains a syntax error.
    for (var i = 0 ; i < options.length; i++) {
      if (options[i].text.search('script_syntax_error.html$') != -1) {
        test.fail('Script with syntax error should not be in the list of ' +
                  'parsed scripts.');
      }
    }
  }

  this.addSniffer(devtools.DebuggerAgent.prototype, 'handleScriptsResponse_',
      function(msg) {
        checkScriptsList();

        // Reload inspected page.
        test.evaluateInConsole_(
            'window.location.reload(true);',
            function(resultText) {
              test.assertEquals('undefined', resultText,
                                'Unexpected result of reload().');
              waitForExceptionEvent();
            });
      });

  function waitForExceptionEvent() {
    var exceptionCount = 0;
    test.addSniffer(
        devtools.DebuggerAgent.prototype,
        'handleExceptionEvent_',
        function(msg) {
          exceptionCount++;
          test.assertEquals(1, exceptionCount, 'Too many exceptions.');
          test.assertEquals(undefined, msg.getBody().script,
                            'Unexpected exception: ' + JSON.stringify(msg));
          test.releaseControl();
        });

    // Check that the script is not paused on parse error.
    test.addSniffer(
        WebInspector,
        'pausedScript',
        function(callFrames) {
          test.fail('Script execution should not pause on syntax error.');
        });
  }

  this.takeControl();
};


/**
 * Checks current execution line against expectations.
 * @param {WebInspector.SourceFrame} sourceFrame
 * @param {number} lineNumber Expected line number
 * @param {string} lineContent Expected line text
 */
TestSuite.prototype._checkExecutionLine = function(sourceFrame, lineNumber,
                                                   lineContent) {
  var sourceRow = sourceFrame.sourceRow(lineNumber);

  var line = sourceRow.getElementsByClassName('webkit-line-content')[0];
  this.assertEquals(lineNumber, sourceFrame.executionLine,
                    'Unexpected execution line number.');
  this.assertEquals(lineContent, line.textContent,
                    'Unexpected execution line text.');

  this.assertTrue(!!sourceRow, 'Execution line not found');
  this.assertTrue(sourceRow.hasStyleClass('webkit-execution-line'),
      'Execution line ' + lineNumber + ' is not highlighted. Class: ' +
          sourceRow.className);
}


/**
 * Checks that all expected scripts are present in the scripts list
 * in the Scripts panel.
 * @param {Array.<string>} expected Regular expressions describing
 *     expected script names.
 * @return {boolean} Whether all the scripts are in 'scripts-files' select
 *     box
 */
TestSuite.prototype._scriptsAreParsed = function(expected) {
  var scriptSelect = document.getElementById('scripts-files');
  var options = scriptSelect.options;

  // Check that at least all the expected scripts are present.
  var missing = expected.slice(0);
  for (var i = 0 ; i < options.length; i++) {
    for (var j = 0; j < missing.length; j++) {
      if (options[i].text.search(missing[j]) != -1) {
        missing.splice(j, 1);
        break;
      }
    }
  }
  return missing.length == 0;
};


/**
 * Waits for script pause, checks expectations, and invokes the callback.
 * @param {Object} expectations  Dictionary of expectations
 * @param {function():void} callback
 */
TestSuite.prototype._waitForScriptPause = function(expectations, callback) {
  var test = this;
  // Wait until script is paused.
  test.addSniffer(
      WebInspector,
      'pausedScript',
      function(callFrames) {
        test.assertEquals(expectations.functionsOnStack.length,
                          callFrames.length,
                          'Unexpected stack depth');

        var functionsOnStack = [];
        for (var i = 0; i < callFrames.length; i++) {
          functionsOnStack.push(callFrames[i].functionName);
        }
        test.assertEquals(
            expectations.functionsOnStack.join(','),
            functionsOnStack.join(','), 'Unexpected stack.');

        checkSourceFrameWhenLoaded();
      });

  // Check that execution line where the script is paused is expected one.
  function checkSourceFrameWhenLoaded() {
    var frame = WebInspector.currentPanel.visibleView.sourceFrame;
    if (frame._isContentLoaded()) {
      checkExecLine();
    } else {
      frame.addEventListener('content loaded', checkExecLine);
    }
    function checkExecLine() {
      test._checkExecutionLine(frame, expectations.lineNumber,
                               expectations.lineText);
      // Make sure we don't listen anymore.
      frame.removeEventListener('content loaded', checkExecLine);
      callback();
    }
  }
};


/**
 * Performs sequence of steps.
 * @param {Array.<Object|Function>} Array [expectations1,action1,expectations2,
 *     action2,...,actionN].
 */
TestSuite.prototype._performSteps = function(actions) {
  var test = this;
  var i = 0;
  function doNextAction() {
    if (i > 0) {
      actions[i++]();
    }
    if (i < actions.length - 1) {
      test._waitForScriptPause(actions[i++], doNextAction);
    }
  }
  doNextAction();
};


/**
 * Waits until all the scripts are parsed and asynchronously executes the code
 * in the inspected page.
 */
TestSuite.prototype._executeCodeWhenScriptsAreParsed = function(
    code, expectedScripts) {
  var test = this;

  function waitForAllScripts() {
    if (test._scriptsAreParsed(expectedScripts)) {
      executeFunctionInInspectedPage();
    } else {
      test.addSniffer(WebInspector, 'parsedScriptSource', waitForAllScripts);
    }
  }

  function executeFunctionInInspectedPage() {
    // Since breakpoints are ignored in evals' calculate() function is
    // execute after zero-timeout so that the breakpoint is hit.
    test.evaluateInConsole_(
        'setTimeout("' + code + '" , 0)',
        function(resultText) {
          test.assertTrue(!isNaN(resultText),
                          'Failed to get timer id: ' + resultText);
        });
  }

  waitForAllScripts();
};


/**
 * Waits until all debugger scripts are parsed and executes 'a()' in the
 * inspected page.
 */
TestSuite.prototype._executeFunctionForStepTest = function() {
  this._executeCodeWhenScriptsAreParsed(
      'a()',
      ['debugger_step.html$', 'debugger_step.js$']);
};


/**
 * Tests step over in the debugger.
 */
TestSuite.prototype.testStepOver = function() {
  this.showPanel('scripts');
  var test = this;

  this._executeFunctionForStepTest();

  this._performSteps([
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 3,
        lineText: '    debugger;'
      },
      function() {
        document.getElementById('scripts-step-over').click();
      },
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 5,
        lineText: '  var y = fact(10);'
      },
      function() {
        document.getElementById('scripts-step-over').click();
      },
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 6,
        lineText: '  return y;'
      },
      function() {
        test.releaseControl();
      }
  ]);

  test.takeControl();
};


/**
 * Tests step out in the debugger.
 */
TestSuite.prototype.testStepOut = function() {
  this.showPanel('scripts');
  var test = this;

  this._executeFunctionForStepTest();

  this._performSteps([
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 3,
        lineText: '    debugger;'
      },
      function() {
        document.getElementById('scripts-step-out').click();
      },
      {
        functionsOnStack: ['a','(anonymous function)'],
        lineNumber: 8,
        lineText: '  printResult(result);'
      },
      function() {
        test.releaseControl();
      }
  ]);

  test.takeControl();
};


/**
 * Tests step in in the debugger.
 */
TestSuite.prototype.testStepIn = function() {
  this.showPanel('scripts');
  var test = this;

  this._executeFunctionForStepTest();

  this._performSteps([
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 3,
        lineText: '    debugger;'
      },
      function() {
        document.getElementById('scripts-step-over').click();
      },
      {
        functionsOnStack: ['d','a','(anonymous function)'],
        lineNumber: 5,
        lineText: '  var y = fact(10);'
      },
      function() {
        document.getElementById('scripts-step-into').click();
      },
      {
        functionsOnStack: ['fact','d','a','(anonymous function)'],
        lineNumber: 15,
        lineText: '  return r;'
      },
      function() {
        test.releaseControl();
      }
  ]);

  test.takeControl();
};


/**
 * Gets a XPathResult matching given xpath.
 * @param {string} xpath
 * @param {number} resultType
 * @param {Node} opt_ancestor Context node. If not specified documentElement
 *     will be used
 * @return {XPathResult} Type of returned value is determined by 'resultType' parameter
 */

TestSuite.prototype._evaluateXpath = function(
    xpath, resultType, opt_ancestor) {
  if (!opt_ancestor) {
    opt_ancestor = document.documentElement;
  }
  try {
    return document.evaluate(xpath, opt_ancestor, null, resultType, null);
  } catch(e) {
    this.fail('Error in expression: "' + xpath + '".' + e);
  }
};


/**
 * Gets first Node matching given xpath.
 * @param {string} xpath
 * @param {Node} opt_ancestor Context node. If not specified documentElement
 *     will be used
 * @return {?Node}
 */
TestSuite.prototype._findNode = function(xpath, opt_ancestor) {
  var result = this._evaluateXpath(xpath, XPathResult.FIRST_ORDERED_NODE_TYPE,
      opt_ancestor).singleNodeValue;
  this.assertTrue(!!result, 'Cannot find node on path: ' + xpath);
  return result;
};


/**
 * Gets a text matching given xpath.
 * @param {string} xpath
 * @param {Node} opt_ancestor Context node. If not specified documentElement
 *     will be used
 * @return {?string}
 */
TestSuite.prototype._findText = function(xpath, opt_ancestor) {
  var result = this._evaluateXpath(xpath, XPathResult.STRING_TYPE,
      opt_ancestor).stringValue;
  this.assertTrue(!!result, 'Cannot find text on path: ' + xpath);
  return result;
};


/**
 * Gets an iterator over nodes matching given xpath.
 * @param {string} xpath
 * @param {Node} opt_ancestor Context node. If not specified, documentElement
 *     will be used
 * @return {XPathResult} Iterator over the nodes
 */
TestSuite.prototype._nodeIterator = function(xpath, opt_ancestor) {
  return this._evaluateXpath(xpath, XPathResult.ORDERED_NODE_ITERATOR_TYPE,
      opt_ancestor);
};


/**
 * Checks the scopeSectionDiv against the expectations.
 * @param {Node} scopeSectionDiv The section div
 * @param {Object} expectations Expectations dictionary
 */
TestSuite.prototype._checkScopeSectionDiv = function(
    scopeSectionDiv, expectations) {
  var scopeTitle = this._findText(
      './div[@class="header"]/div[@class="title"]/text()', scopeSectionDiv);
  this.assertEquals(expectations.title, scopeTitle,
      'Unexpected scope section title.');
  if (!expectations.properties) {
    return;
  }
  this.assertTrue(scopeSectionDiv.hasStyleClass('expanded'), 'Section "' +
      scopeTitle + '" is collapsed.');

  var propertyIt = this._nodeIterator('./ol[@class="properties"]/li',
                                      scopeSectionDiv);
  var propertyLi;
  var foundProps = [];
  while (propertyLi = propertyIt.iterateNext()) {
    var name = this._findText('./span[@class="name"]/text()', propertyLi);
    var value = this._findText('./span[@class="value"]/text()', propertyLi);
    this.assertTrue(!!name, 'Invalid variable name: "' + name + '"');
    this.assertTrue(name in expectations.properties,
                    'Unexpected property: ' + name);
    this.assertEquals(expectations.properties[name], value,
                      'Unexpected "' + name + '" property value.');
    delete expectations.properties[name];
    foundProps.push(name + ' = ' + value);
  }

  // Check that all expected properties were found.
  for (var p in expectations.properties) {
    this.fail('Property "' + p + '" was not found in scope "' + scopeTitle +
        '". Found properties: "' + foundProps.join(',') + '"');
  }
};


/**
 * Expands scope sections matching the filter and invokes the callback on
 * success.
 * @param {function(WebInspector.ObjectPropertiesSection, number):boolean}
 *     filter
 * @param {Function} callback
 */
TestSuite.prototype._expandScopeSections = function(filter, callback) {
  var sections = WebInspector.currentPanel.sidebarPanes.scopechain.sections;

  var toBeUpdatedCount = 0;
  function updateListener() {
    --toBeUpdatedCount;
    if (toBeUpdatedCount == 0) {
      // Report when all scopes are expanded and populated.
      callback();
    }
  }

  // Global scope is always the last one.
  for (var i = 0; i < sections.length - 1; i++) {
    var section = sections[i];
    if (!filter(sections, i)) {
      continue;
    }
    ++toBeUpdatedCount;
    var populated = section.populated;

    this._hookGetPropertiesCallback(updateListener,
      function() {
        section.expand();
        if (populated) {
          // Make sure 'updateProperties' callback will be called at least once
          // after it was overridden.
          section.update();
        }
      });
  }
};


/**
 * Tests that scopes can be expanded and contain expected data.
 */
TestSuite.prototype.testExpandScope = function() {
  this.showPanel('scripts');
  var test = this;

  this._executeCodeWhenScriptsAreParsed(
      'handleClick()',
      ['debugger_closure.html$']);

  this._waitForScriptPause(
      {
        functionsOnStack: ['innerFunction', 'handleClick',
                           '(anonymous function)'],
        lineNumber: 8,
        lineText: '    debugger;'
      },
      expandAllSectionsExceptGlobal);

  // Expanding Global scope takes for too long so we skeep it.
  function expandAllSectionsExceptGlobal() {
      test._expandScopeSections(function(sections, i) {
        return i < sections.length - 1;
      },
      examineScopes /* When all scopes are expanded and populated check
          them. */);
  }

  // Check scope sections contents.
  function examineScopes() {
    var scopeVariablesSection = test._findNode('//div[@id="scripts-sidebar"]/' +
        'div[div[@class="title"]/text()="Scope Variables"]');
    var expectedScopes = [
      {
        title: 'Local',
        properties: {
          x:2009,
          innerFunctionLocalVar:2011,
          'this': 'global',
        }
      },
      {
        title: 'Closure',
        properties: {
          n:'TextParam',
          makeClosureLocalVar:'local.TextParam',
        }
      },
      {
        title: 'Global',
      },
    ];
    var it = test._nodeIterator('./div[@class="body"]/div',
                                scopeVariablesSection);
    var scopeIndex = 0;
    var scopeDiv;
    while (scopeDiv = it.iterateNext()) {
       test.assertTrue(scopeIndex < expectedScopes.length,
                       'Too many scopes.');
       test._checkScopeSectionDiv(scopeDiv, expectedScopes[scopeIndex]);
       ++scopeIndex;
    }
    test.assertEquals(expectedScopes.length, scopeIndex,
                      'Unexpected number of scopes.');

    test.releaseControl();
  }

  test.takeControl();
};


/**
 * Returns child tree element for a property with given name.
 * @param {TreeElement} parent Parent tree element.
 * @param {string} childName
 * @return {TreeElement}
 */
TestSuite.prototype._findChildProperty = function(parent, childName) {
  var children = parent.children;
  for (var i = 0; i < children.length; i++) {
    var treeElement = children[i];
    var property = treeElement.property;
    if (property.name == childName) {
      return treeElement;
    }
  }
  this.fail('Cannot find ' + childName);
};


/**
 * Executes the 'code' with InjectedScriptAccess.getProperties overriden
 * so that all callbacks passed to InjectedScriptAccess.getProperties are
 * extended with the 'hook'.
 * @param {Function} hook The hook function.
 * @param {Function} code A code snippet to be executed.
 */
TestSuite.prototype._hookGetPropertiesCallback = function(hook, code) {
  var orig = InjectedScriptAccess.getProperties;
  InjectedScriptAccess.getProperties = function(objectProxy,
      ignoreHasOwnProperty, callback) {
    orig.call(InjectedScriptAccess, objectProxy, ignoreHasOwnProperty,
        function() {
          callback.apply(this, arguments);
          hook();
        });
  };
  try {
    code();
  } finally {
    InjectedScriptAccess.getProperties = orig;
  }
};


/**
 * Tests that all elements in prototype chain of an object have expected
 * intrinic proprties(__proto__, constructor, prototype).
 */
TestSuite.prototype.testDebugIntrinsicProperties = function() {
  this.showPanel('scripts');
  var test = this;

  this._executeCodeWhenScriptsAreParsed(
      'handleClick()',
      ['debugger_intrinsic_properties.html$']);

  this._waitForScriptPause(
      {
        functionsOnStack: ['callDebugger', 'handleClick',
                           '(anonymous function)'],
        lineNumber: 29,
        lineText: '  debugger;'
      },
      expandLocalScope);

  var localScopeSection = null;
  function expandLocalScope() {
    test._expandScopeSections(function(sections, i) {
        if (i == 0) {
          test.assertTrue(sections[i].object.isLocal, 'Scope #0 is not Local.');
          localScopeSection = sections[i];
          return true;
        }
        return false;
      },
      examineLocalScope);
  }

  function examineLocalScope() {
    var aTreeElement = test._findChildProperty(
        localScopeSection.propertiesTreeOutline, 'a');
    test.assertTrue(!!aTreeElement, 'Not found');

    test._hookGetPropertiesCallback(
        checkA.bind(null, aTreeElement),
        function () {
          aTreeElement.expand();
        });

  }

  function checkA(aTreeElement) {
    checkIntrinsicProperties(aTreeElement,
        [
          'constructor', 'function Child()',
          '__proto__', 'Object',
          'prototype', 'undefined',
          'parentField', '10',
          'childField', '20',
        ]);
    expandProto(aTreeElement, checkAProto);
  }

  function checkAProto(treeElement) {
    checkIntrinsicProperties(treeElement,
        [
          'constructor', 'function Child()',
          '__proto__', 'Object',
          'prototype', 'undefined',
          'childProtoField', '21',
        ]);
    expandProto(treeElement, checkAProtoProto);
  }

  function checkAProtoProto(treeElement) {
    checkIntrinsicProperties(treeElement,
        [
          'constructor', 'function Parent()',
          '__proto__', 'Object',
          'prototype', 'undefined',
          'parentProtoField', '11',
        ]);
    expandProto(treeElement, checkAProtoProtoProto);
  }

  function checkAProtoProtoProto(treeElement) {
    checkIntrinsicProperties(treeElement,
        [
          'constructor', 'function Object()',
          '__proto__', 'null',
          'prototype', 'undefined',
        ]);
    test.releaseControl();
  }

  function expandProto(treeElement, callback) {
    var proto = test._findChildProperty(treeElement, '__proto__');
    test.assertTrue(proto, '__proro__ not found');

    test._hookGetPropertiesCallback(callback.bind(null, proto),
        function() {
          proto.expand();
        });
  }

  function checkIntrinsicProperties(treeElement, expectations) {
    for (var i = 0; i < expectations.length; i += 2) {
      var name = expectations[i];
      var value = expectations[i+1];

      var propertyTreeElement = test._findChildProperty(treeElement, name);
      test.assertTrue(propertyTreeElement,
                      'Property "' + name + '" not found.');
      test.assertEquals(value,
          propertyTreeElement.property.value.description,
          'Unexpected "' + name + '" value.');
    }
  }

  test.takeControl();
};


/**
 * Tests 'Pause' button will pause debugger when a snippet is evaluated.
 */
TestSuite.prototype.testPauseInEval = function() {
  this.showPanel('scripts');

  var test = this;

  var pauseButton = document.getElementById('scripts-pause');
  pauseButton.click();

  devtools.tools.evaluateJavaScript('fib(10)');

  this.addSniffer(WebInspector, 'pausedScript',
      function() {
        test.releaseControl();
      });

  test.takeControl();
};


/**
 * Key event with given key identifier.
 */
TestSuite.KeyEvent = function(key) {
  this.keyIdentifier = key;
};
TestSuite.KeyEvent.prototype.preventDefault = function() {};
TestSuite.KeyEvent.prototype.stopPropagation = function() {};


/**
 * Tests console eval.
 */
TestSuite.prototype.testConsoleEval = function() {
  var test = this;
  this.evaluateInConsole_('123',
      function(resultText) {
        test.assertEquals('123', resultText);
        test.releaseControl();
      });

  this.takeControl();
};


/**
 * Tests console log.
 */
TestSuite.prototype.testConsoleLog = function() {
  WebInspector.console.visible = true;
  var messages = WebInspector.console.messages;
  var index = 0;

  var test = this;
  var assertNext = function(line, message, opt_class, opt_count, opt_substr) {
    var elem = messages[index++].toMessageElement();
    var clazz = elem.getAttribute('class');
    var expectation = (opt_count || '') + 'console_test_page.html:' +
        line + message;
    if (opt_substr) {
      test.assertContains(elem.textContent, expectation);
    } else {
      test.assertEquals(expectation, elem.textContent);
    }
    if (opt_class) {
      test.assertContains(clazz, 'console-' + opt_class);
    }
  };

  assertNext('5', 'log', 'log-level');
  assertNext('7', 'debug', 'log-level');
  assertNext('9', 'info', 'log-level');
  assertNext('11', 'warn', 'warning-level');
  assertNext('13', 'error', 'error-level');
  assertNext('15', 'Message format number 1, 2 and 3.5');
  assertNext('17', 'Message format for string');
  assertNext('19', 'Object Object');
  assertNext('22', 'repeated', 'log-level', 5);
  assertNext('26', 'count: 1');
  assertNext('26', 'count: 2');
  assertNext('29', 'group', 'group-title');
  index++;
  assertNext('33', 'timer:', 'log-level', '', true);
  assertNext('35', '1 2 3', 'log-level');
  assertNext('37', 'HTMLDocument', 'log-level');
  assertNext('39', '<html>', 'log-level', '', true);
};


/**
 * Tests eval of global objects.
 */
TestSuite.prototype.testEvalGlobal = function() {
  WebInspector.console.visible = true;

  var inputs = ['foo', 'foobar'];
  var expectations = ['foo', 'fooValue',
                      'foobar', 'ReferenceError: foobar is not defined'];

  // Do not change code below - simply add inputs and expectations above.
  var initEval = function(input) {
    WebInspector.console.prompt.text = input;
    WebInspector.console.promptElement.handleKeyEvent(
        new TestSuite.KeyEvent('Enter'));
  };
  var test = this;
  var messagesCount = 0;
  var inputIndex = 0;
  this.addSniffer(WebInspector.ConsoleView.prototype, 'addMessage',
      function(commandResult) {
        messagesCount++;
        if (messagesCount == expectations.length) {
          var messages = WebInspector.console.messages;
          for (var i = 0; i < expectations; ++i) {
            var elem = messages[i++].toMessageElement();
            test.assertEquals(elem.textContent, expectations[i]);
          }
          test.releaseControl();
        } else if (messagesCount % 2 == 0) {
          initEval(inputs[inputIndex++]);
        }
      }, true);

  initEval(inputs[inputIndex++]);
  this.takeControl();
};


/**
 * Test runner for the test suite.
 */
var uiTests = {};


/**
 * Run each test from the test suit on a fresh instance of the suite.
 */
uiTests.runAllTests = function() {
  // For debugging purposes.
  for (var name in TestSuite.prototype) {
    if (name.substring(0, 4) == 'test' &&
        typeof TestSuite.prototype[name] == 'function') {
      uiTests.runTest(name);
    }
  }
};


/**
 * Run specified test on a fresh instance of the test suite.
 * @param {string} name Name of a test method from TestSuite class.
 */
uiTests.runTest = function(name) {
  new TestSuite().runTest(name);
};


}
