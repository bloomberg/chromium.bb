// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Integration module for QUnit tests running in browser tests.
 * Specifically it:
 * - Sets QUnit.autostart to false, so that the browser test can hook the test
 *   results callback before the test starts.
 * - Implements a text-based test reporter to report test results back to the
 *   browser test.
 */

(function(QUnit, automationController, exports) {

'use strict';

var TestReporter = function() {
  this.errorMessage_ = '';
  this.failedTestsCount_ = 0;
  this.failedAssertions_ = [];
};

TestReporter.prototype.init = function(qunit) {
  qunit.testDone(this.onTestDone_.bind(this));
  qunit.log(this.onAssertion_.bind(this));
};

TestReporter.prototype.onTestDone_ = function(details) {
  if (this.failedAssertions_.length > 0) {
    this.errorMessage_ += '  ' + details.module + '.' + details.name + '\n';
    this.errorMessage_ += this.failedAssertions_.map(
        function(assertion, index){
          return '    ' + (index + 1) + '. ' + assertion.message + '\n' +
                 '    ' + assertion.source;
        }).join('\n');
    this.failedAssertions_ = [];
    this.failedTestsCount_++;
  }
};

TestReporter.prototype.onAssertion_ = function(details) {
  if (!details.result) {
    this.failedAssertions_.push(details);
  }
};

TestReporter.prototype.getErrorMessage = function(){
  var errorMessage = '';
  if (this.failedTestsCount_ > 0) {
    var test = (this.failedTestsCount_ > 1) ? 'tests' : 'test';
    errorMessage = this.failedTestsCount_  + ' ' + test + ' failed:\n';
    errorMessage += this.errorMessage_;
  }
  return errorMessage;
};

var BrowserTestHarness = function(qunit, domAutomationController, reporter) {
  this.qunit_ = qunit;
  this.automationController_ = domAutomationController;
  this.reporter_ = reporter;
};

BrowserTestHarness.prototype.init = function() {
  this.qunit_.config.autostart = false;
};

BrowserTestHarness.prototype.run = function() {
  this.reporter_.init(this.qunit_);
  this.qunit_.start();
  this.qunit_.done(function(details){
    this.automationController_.send(JSON.stringify({
      passed: details.passed == details.total,
      errorMessage: this.reporter_.getErrorMessage()
    }));
  }.bind(this));
};

// The browser test runs chrome with the flag --dom-automation, which creates
// the window.domAutomationController object.  This allows the test suite to
// JS-encoded data back to the browser test.
if (automationController) {
  if (!QUnit) {
    console.error('browser_test_harness.js must be included after QUnit.js.');
    return;
  }

  var testHarness = new BrowserTestHarness(
      QUnit,
      automationController,
      new TestReporter());
  testHarness.init();
  exports.browserTestHarness = testHarness;
}

})(window.QUnit, window.domAutomationController, window);