// Copyright 2010 The Native Client Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.


function $(id) {
  return document.getElementById(id);
}

// Logs test results to the server using URL-encoded RPC.
// Also logs the same test results locally into the DOM.
function RPCWrapper() {
  // Workaround how JS binds 'this'
  var this_ = this;
  // It is assumed RPC will work unless proven otherwise.
  this.rpc_available = true;
  // Set to true if any test fails.
  this.ever_failed = false;
  // Async calls can make it faster, but it can also change order of events.
  this.async = false;

  // Called if URL-encoded RPC gets a 404, can't find the server, etc. 
  function handleRPCFailure(name, message) {
    this_.logLocalError('RPC failure for ' + name + ': ' + message);
    this_.disableRPC();
  }

  // URL-encoded RPC always responds 'OK'.  If we get anything else, worry.
  function handleRPCResponse(name, message) {
    if (message != 'OK') {
      this_.logLocalError('Unexpected RPC response to ' + name +
                          ': \'' + message + '\'');
    }
  }

  // Performs a URL-encoded RPC call, given a function name and a dictionary
  // (actually just an object - it's a JS idiom) of parameters.
  function rpcCall(name, params) {
    if (this_.rpc_available) {
      // Construct the URL for the RPC request.
      var args = [];
      for (var pname in params) {
        pvalue = params[pname];
        args.push(encodeURIComponent(pname) + '=' + encodeURIComponent(pvalue));
      }
      var url = '/TESTER/' + name + '?' + args.join('&');
      var req = new XMLHttpRequest();
      // Async result handler
      req.onreadystatechange = function() {
        if (req.readyState == 4) {
          if (req.status == 200) {
            handleRPCResponse(name, req.responseText);
          } else {
            handleRPCFailure(name, req.status.toString());
          }
        }
      }
      try {
        req.open('GET', url, this_.async);
        req.send();
        if (!this_.async) {
          handleRPCResponse(name, req.responseText);
        }
      } catch (err) {
        handleRPCFailure(name, err.toString());
      }
    }
  }

  // Pretty prints an error into the DOM.
  this.logLocalError = function(message) {
    this.logLocalNewline();
    this.logLocal(message, 'red');
    this.visualError();
  }

  // If RPC isn't working, disable it to stop error message spam.
  this.disableRPC = function() {
    if (this.rpc_available) {
      this.rpc_available = false;
      this.logLocalError('Disabling RPC');
    }
  }

  this.startup = function() {
    this.logLocal('[STARTUP]');
    rpcCall('Startup', {});
  }

  this.shutdown = function() {
    this.logLocalNewline();
    this.logLocal('[SHUTDOWN]');
    rpcCall('Shutdown', {});

    if (this.ever_failed) {
      this.localOutput.style.border = '2px solid #FF0000';
    } else {
      this.localOutput.style.border = '2px solid #00FF00';
    }
  }

  this.client_error = function(message) {
    this.logLocalNewline();
    this.logLocalException('[CLIENT_ERROR] ', message);
    this.visualError();

    try {
      rpcCall('ClientError', {message: message});
    } catch (err) {
      // There's not much that can be done, at this point.
    }
  }

  this.begin = function(test_name) {
    this.logLocalNewline();
    this.logLocal('[' + test_name + ' BEGIN]');
    rpcCall('TestBegin', {test_name: test_name});
  }

  this.log = function(test_name, message) {
    if (typeof(message) != 'string') {
      message = toString(message);
    }
    this.logLocal('[' + test_name + ' LOG] ' + message);
    rpcCall('TestLog', {test_name: test_name, message: message});
  }

  this.fail = function(test_name, message) {
    this.logLocal('[' + test_name + ' FAIL] ' + message, 'red');
    this.visualError();
    rpcCall('TestFailed', {test_name: test_name, message: message});
  }

  this.exception = function(test_name, message, stack) {
    this.logLocal('[' + test_name + ' EXCEPTION] ' + message, 'red');
    this.logLocalPre(stack);
    this.visualError();
    rpcCall('TestException', {test_name: test_name, message: message,
      stack: stack});
  }

  this.error = function(test_name, message) {
    this.logLocal('[' + test_name + ' ERROR] ' + message, 'red');
    this.visualError();
    rpcCall('TestError', {test_name: test_name, message: message});
  }

  this.pass = function(test_name) {
    this.logLocal('[' + test_name + ' PASS]', 'green');
    rpcCall('TestPassed', {test_name: test_name});
  }

  this.visualError = function() {
    // Changing the color is defered until testing is done
    this.ever_failed = true;
  }

  this.logLocal = function(message, color) {
    var mNode = document.createTextNode(message);
    var div = document.createElement('div');
    if (color != undefined) {
      div.style.color = color;
    }
    div.appendChild(mNode);
    this.localOutput.appendChild(div);
  }

  this.logLocalNewline = function() {
    var br = document.createElement('br');
    this.localOutput.appendChild(br);
  }

  this.logLocalPre = function(message) {
    var mNode = document.createTextNode(message);
    var div = document.createElement('pre');
    div.appendChild(mNode);
    this.localOutput.appendChild(div);
  }

  this.logLocalException = function(prefix, err) {
    // Exception handling in JS is very ad-hoc.  If we can recognize the
    // structure of the exception, pretty print it, otherwise just punt.
    if (typeof err == 'object' && 'message' in err && 'stack' in err) {
      this.logLocal(prefix + err.message, 'red');
      this.logLocalPre(err.stack);
    } else {
      this.logLocal(prefix + err, 'red');
    }
  }

  // Create a place in the page to output test results
  this.localOutput = document.createElement('div');
  this.localOutput.style.border = '2px solid #0000FF';
  this.localOutput.style.padding = '10px';
  document.body.appendChild(this.localOutput);
}


// Begin functions for testing 

function fail(message, info) {
  var parts = [];
  if (message != undefined) {
    parts.push(message);
  }
  if (info != undefined) {
    parts.push('(' + info + ')');
  }
  throw {type: 'test_fail', message: parts.join(' ')};
}


function assert(condition, message) {
  if (!condition) {
    fail(message, toString(condition));
  }
}


// This is accepted best practice for checking if an object is an array.
function isArray(obj) {
  return Object.prototype.toString.call(obj) === '[object Array]';
}


function toString(obj) {
  if (typeof(obj) == 'string') {
    return '\'' + obj + '\'';
  }
  try {
    return obj.toString();
  } catch (err) {
    try {
      // Arrays should do this automatically, but there is a known bug where 
      // NaCl gets array types wrong.  .toString will fail on these objects.
      return obj.join(',');
    } catch (err) {
      if (obj == undefined) {
        return 'undefined';
      } else {
        // There is no way to create a textual representation of this object.
        return '[UNPRINTABLE]';
      }
    }
  }
}


function assertEqual(a, b, message) {
  if (isArray(a) && isArray(b)) {
    assertArraysEqual(a, b, message);
  } else if (a !== b) {
    fail(message, toString(a) + ' != ' + toString(b));
  }
}


function assertArraysEqual(a, b, message) {
  var dofail = function() {
    fail(message, toString(a) + ' != ' + toString(b));
  }
  if (a.length != b.length) {
    dofail();
  }
  for (var i = 0; i < a.length; i++) {
    if(a[i] != b[i]) {
      dofail();
    }
  }
}


// Ideally there'd be some way to identify what exception was thrown, but JS
// exceptions are fairly ad-hoc.
// TODO(ncbray) allow manual validation of exception types?
function assertRaises(func, message) {
  try {
    func();
  } catch (err) {
    return;
  }
  fail(message, 'did not raise');
}


function log(message) {
  tester.log(message);
}


function halt_and_pass() {
  throw {type: 'test_pass'};
}


function halt_and_callback(time, callback) {
  throw {type: 'test_wait', time: time, callback: callback};
}

// End functions for testing


function begins_with(s, prefix) {
  if (s.length >= prefix.length) {
    return s.substr(0, prefix.length) == prefix;
  } else {
    return false;
  }
}


function ends_with(s, suffix) {
  if (s.length >= suffix.length) {
    return s.substr(s.length - suffix.length, suffix.length) == suffix;
  } else {
    return false;
  }
}


// All of these methods for identifying a nexe are not bomb proof.
// One of them should work, however.
function is_nacl_embed(embed) {
  if (embed.type != undefined) {
    var type = embed.type.toLowerCase();
    if (begins_with(type, 'application/') && type.indexOf('-nacl') != -1) {
      return true;
    }
  }
  if (embed.src != undefined) {
    var src = embed.src.toLowerCase();
    if (ends_with(src, '.nexe')) {
      return true;
    } else if (ends_with(src, '.nmf')) {
      return true;
    }
  }
  if (embed.nexes != undefined) {
    return true;
  }
  return false;
}

function is_loading(embed) {
  return embed.__moduleReady == 0;
}

function is_broken(embed) {
  return embed.__moduleReady == undefined;
}

function embed_name(embed) {
  if (embed.name != undefined) {
    if (embed.id != undefined) {
      return embed.name + ' / ' + embed.id;
    } else {
      return embed.name;
    }
  } else if (embed.id != undefined) {
    return embed.id;
  } else {
    return '[no name]';
  }
}

function Tester() {
  // Workaround how JS binds 'this'
  var this_ = this;
  // The tests being run.
  var tests = [];
  // Log messages that occur before RPC has been set up
  var backlog = [];

  this.initRPC = function() {
    this.rpc = new RPCWrapper();
    this.rpc.startup();
  }

  this.log = function(message) {
    if (this.rpc == undefined) {
      backlog.push(message);
    } else {
      this.rpc.log(this.currentTest, message);
    }
  }

  this.run = function() {
    this.initRPC();
    this.reportExternalErrors('#setup#');

    // Wait up to a second.
    this.retries = 100;
    this.retryWait = 10;
    setTimeout(function() { this_.waitForPlugins(); }, 0);
  }

  // Find all the plugins in the DOM and make sure they've all loaded.
  this.waitForPlugins = function() {
    var loaded = true;
    var waiting = [];
    var loaded = [];

    var embeds = document.getElementsByTagName('embed');
    for (var i = 0; i < embeds.length; i++) {
      if (is_nacl_embed(embeds[i])) {
        if (is_broken(embeds[i])) {
          this.rpc.client_error('nacl embed is broken: ' +
                                embed_name(embeds[i]));
        } else if (is_loading(embeds[i])) {
          waiting.push(embeds[i]);
        } else {
          loaded.push(embeds[i]);
        }
      }
    }

    function doStart() {
      for (var i = 0; i < loaded.length; i++) {
        log(embed_name(loaded[i]) + ' loaded');
      }
      this_.startTesting();
    }

    if (waiting.length == 0) {
      doStart();
    } else {
      this.retries -= 1;
      if (this.retries <= 0) {
        for (var j = 0; j < waiting.length; j++) {
          this.rpc.client_error(embed_name(waiting[j]) +
                                ' took too long to load');
          doStart();
        }
      } else {
        setTimeout(function() { this_.waitForPlugins(); }, this.retryWait);
      }
    }
  }

  this.initTest = function() {
    if (this.testIndex < tests.length) {
      // Setup for the next test.
      var test = tests[this.testIndex];
      this.currentTest = test.name;
      this.rpc.begin(this.currentTest);
      this.setNextStep(0, test.callback);
    } else {
      // There are no more test suites.
      this.reportExternalErrors('#shutdown#');
      this.rpc.shutdown();
    }
  }

  this.setNextStep = function(time, callback) {
    // Give up control
    // There are three reasons for this:
    // 1) It allows the render thread to update (can't see the log, otherwise)
    // 2) It breaks up a "long running process" so the browser
    //    won't think it crashed
    // 3) It allows us to sleep for a specified interval
    // TODO it might not be desirable to relinquish control between each test.
    // Running several tests in a row might make things faster.
    this.nextStep = callback;
    setTimeout(function() { this_.runTestStep(); }, time);
  }

  this.startTesting = function() {
    // if tests[0] does not exist (no tests), initTest will shut down testing.
    this.testIndex = 0;
    this.initTest();
  }

  this.nextTest = function() {
    // Advance to the next test suite
    this.testIndex += 1;
    this.initTest()
  }

  this.runTestStep = function() {
    callback = this.nextStep;
    // Be paranoid - avoid accidental infinite loops
    this.nextStep = null;
    // Should we move on to the next test?
    var done = true;
    // Did the text exit abnormally
    var exceptionHandled = false;

    try {
      callback();
    } catch (err) {
      if (typeof err == 'object') {
        if ('type' in err) {
          if (err.type == 'test_fail') {
            // A special exception that terminates the test with a failure
            this.rpc.fail(this.currentTest, err.message);
            exceptionHandled = true;
          } else if (err.type == 'test_pass') {
            // A special exception that terminates the test without failing
            this.rpc.pass(this.currentTest);
            exceptionHandled = true;
          } else if (err.type == 'test_wait') {
            // A special exception that indicates the test wants a callback
            this.setNextStep(err.time, err.callback);
            done = false;
            exceptionHandled = true;
          }
        }
        if ('message' in err && 'stack' in err) {
          // An exception with a stack trace thrown by Javascript
          this.rpc.exception(this.currentTest, err.message, err.stack);
          exceptionHandled = true;
        }
      }
      if (!exceptionHandled) {
        // No idea how to decode the exception
        this.rpc.error(this.currentTest, err.toString());
      }
      exceptionHandled = true;
    }

    if (done) {
      if (!exceptionHandled) {
        // No exception, the test passed
        this.rpc.pass(this.currentTest);
      }
      this.nextTest();
    }
  }

  this.addTest = function(name, callback) {
    tests.push({name: name, callback: callback});
  }

  var errors = [];

  // Defer handling the error
  this.externalError = function(err) {
    errors.push(err);
  }

  this.handleExternalError = function(err) {
    this.rpc.client_error(err.toString());
  }

  // Report log messages / exceptions that occur outside of the tests
  this.reportExternalErrors = function(psedo_test) {
    this.currentTest = psedo_test;

    // Log messages that occurred before rpc was set up
    for (var i = 0; i < backlog.length; i++) {
      this.log(backlog[i]);
    }
    backlog = [];

    // Exceptions that occurred outside of tests
    for (var i = 0; i < errors.length; i++) {
      this.handleExternalError(errors[i]);
    }
    errors = [];
  }
}


tester = new Tester();

// Using onload guarantees we'll be in a reasonable state before we start
// testing.  Unfortunately, if the user redefines onload, testing will not start
// automatically.
window.onload = function() { tester.run(); };


// Not fully supported on Chrome - only some errors will be caught.
window.onerror = function(err, url, line) {
  if (url == undefined || line == undefined) {
    tester.externalError(err.toString());
  } else {
    tester.externalError(err.toString() + ' - ' + url.toString() + '(' +
      line.toString() + ')');
  }

  // Don't eat it - let the browser log it
  return false;
};
