'use strict';

/**
  ResizeTestHelper is a framework to test ResizeObserver
  notifications. Use it to make assertions about ResizeObserverEntries.
  This framework is needed because ResizeObservations are
  delivered asynchronously inside the event loop.

  It handles timeouts, and queueing of multiple steps in a test.

  Usage:

  Use createTest() to create tests.
  Make assertions inside entries, timeout callbacks.
  Every test should clean up after itself by calling helper.observer.disconnect();
  Chain tests together with nextTest(), or nextTestRaf()

  Example:

  var helper = new ResizeTestHelper();

  function test0() {
    helper.createTest(
      "test0: test name here",
      setup => { // setup gets called when test starts
        helper.observer.observe(t3);
      },
      entries => { // This is ResizeObserver callback
        // kick off next test by calling nextTestRaf
        helper.nextTestRaf();
      },
      timeout => { // timeout gets called on timeout
        // if timeout happens, and timeout is not defined, test will fail
      }
    );
*/

function ResizeTestHelper() {
  this._pendingTests = [];
  this._observer = new ResizeObserver(this._handleNotification.bind(this));
}
ResizeTestHelper.TIMEOUT = 500;

ResizeTestHelper.prototype = {

  // @return ResizeObserver
  get observer() {
    return this._observer;
  },

  _handleNotification: function(entries) {
    if (this._currentTest) {
      // console.log("notification");
      let current = this._currentTest;
      delete this._currentTest;
      window.clearTimeout(current.timeoutId);
      current.test.step(_ => {
        // console.log("step");
        let caughtEx = false;
        try {
          current.completion(entries);
          current.test.done();
        }
        catch(ex) {
          caughtEx = ex;
        }
        if (caughtEx)
          throw caughtEx;
      });
    }
  },
  _handleTimeout: function() {
    if (this._currentTest) {
      let current  = this._currentTest;
      delete this._currentTest;
      if (current.timeout) { // timeout is not an error
        current.timeout();
        current.test.done();
      }
      else {
        current.test.step(_ => {
          assert_unreached("Timed out waiting for notification. (" + ResizeTestHelper.TIMEOUT + "ms)");
          current.test.done();
        });
      }
    }
  },

  // Start the next test
  nextTest: function() {
    if (this._currentTest) // only one test at a time
      return;
    if (this._pendingTests.length > 0) {
      this._currentTest = this._pendingTests.shift();
      // console.log("executing ", this._currentTest.name);
      this._currentTest.setup();
      this._currentTest.timeoutId = this._currentTest.test.step_timeout(this._handleTimeout.bind(this), ResizeTestHelper.TIMEOUT);
    }
  },

  // Fires nextTest on rAF. Use it to trigger next test.
  nextTestRaf: function() {
    window.requestAnimationFrame( () => this.nextTest() );
  },

  // Adds new test to _pendingTests.
  createTest: function(name, setup, completion, timeoutCb) {
    // console.log('setup ', name);
    this._pendingTests.push( {
      name: name,
      test: async_test(name),
      setup: setup,
      completion: completion,
      timeout: timeoutCb });
  }

}
