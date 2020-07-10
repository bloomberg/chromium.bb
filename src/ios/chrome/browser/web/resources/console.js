// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts to allow page console.log() etc. output to be seen on the console
// of the host application.

goog.provide('__crWeb.console');

// Requires __crWeb.message provided by __crWeb.allFramesWebBundle.

/**
 * Namespace for this module.
 */
__gCrWeb.console = {};

/* Beginning of anonymous object. */
(function() {
function sendConsoleMessage(method, originalArgs) {
  var message, slicedArgs = Array.prototype.slice.call(originalArgs);
  try {
    message = slicedArgs.join(' ');
  } catch (err) {
  }
  __gCrWeb.message.invokeOnHost({
    'command': 'console.msg',
    'method': method,
    'message': message,
    'url': document.location.href
  });
}

var originalConsoleLog = console.log;
console.log = function() {
  sendConsoleMessage('log', arguments);
  return originalConsoleLog.apply(this, arguments);
};

var originalConsoleDebug = console.debug;
console.debug = function() {
  sendConsoleMessage('debug', arguments);
  return originalConsoleDebug.apply(this, arguments);
};

var originalConsoleInfo = console.info;
console.info = function() {
  sendConsoleMessage('info', arguments);
  return originalConsoleInfo.apply(this, arguments);
};

var originalConsoleWarn = console.warn;
console.warn = function() {
  sendConsoleMessage('warn', arguments);
  return originalConsoleWarn.apply(this, arguments);
};

var originalConsoleError = console.error;
console.error = function() {
  sendConsoleMessage('error', arguments);
  return originalConsoleError.apply(this, arguments);
};
}());
