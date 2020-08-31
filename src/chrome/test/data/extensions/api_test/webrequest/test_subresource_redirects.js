// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var dataURL = 'data:text/plain,redirected1';

function getURLNonWebAccessible() {
  return getURL('manifest.json');
}

function getURLWebAccessible() {
  return getURL('simpleLoad/a.html');
}

function assertRedirectSucceeds(url, redirectURL, callback) {
  // Load a page to be sure webRequest listeners are set up.
  navigateAndWait(getURL('simpleLoad/b.html'), function() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = pass(function() {
      if (callback) callback();
      chrome.test.assertEq(xhr.responseURL, redirectURL);
    });
    xhr.onerror = function() {
      if (callback) callback();
      chrome.test.fail();
    };
    xhr.send();
  });
}

function assertRedirectFails(url, callback) {
  // Load a page to be sure webRequest listeners are set up.
  navigateAndWait(getURL('simpleLoad/b.html'), function() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = function() {
      if (callback) callback();
      chrome.test.fail();
    };
    xhr.onerror = pass(function() {
      if (callback) callback();
    });
    xhr.send();
  });
}

chrome.test.getConfig(function(config) {
  var onHeadersReceivedExtraInfoSpec = ['blocking'];
  if (config.customArg === 'useExtraHeaders')
    onHeadersReceivedExtraInfoSpec.push('extraHeaders');

  runTests([
    function subresourceRedirectToDataUrlOnHeadersReceived() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: dataURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, dataURL, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function subresourceRedirectToNonWebAccessibleUrlOnHeadersReceived() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: getURLNonWebAccessible()};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function subresourceRedirectToServerRedirectOnHeadersReceived() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function subresourceRedirectToUnallowedServerRedirectOnHeadersReceived() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectFails(url, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function subresourceRedirectToDataUrlOnBeforeRequest() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: dataURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, dataURL, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function subresourceRedirectToNonWebAccessibleUrlOnBeforeRequest() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: getURLNonWebAccessible()};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function subresourceRedirectToServerRedirectOnBeforeRequest() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function subresourceRedirectToUnallowedServerRedirectOnBeforeRequest() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectFails(url, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function subresourceRedirectToDataUrlWithServerRedirect() {
      assertRedirectFails(getServerURL('server-redirect?' + dataURL));
    },

    function subresourceRedirectToNonWebAccessibleWithServerRedirect() {
      assertRedirectFails(
          getServerURL('server-redirect?' + getURLNonWebAccessible()));
    },

    function subresourceRedirectToWebAccessibleWithServerRedirect() {
      assertRedirectSucceeds(
          getServerURL('server-redirect?' + getURLWebAccessible()),
          getURLWebAccessible());
    },

    function subresourceRedirectHasSameRequestIdOnHeadersReceived() {
      var url = getServerURL('echo');
      var requestId;
      var onHeadersReceivedListener = function(details) {
        requestId = details.requestId;
        return {redirectUrl: getURLWebAccessible()};
      };
      chrome.webRequest.onHeadersReceived.addListener(onHeadersReceivedListener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      var onBeforeRequestListener = chrome.test.callbackPass(function(details) {
        chrome.test.assertEq(details.requestId, requestId);
      });
      chrome.webRequest.onBeforeRequest.addListener(onBeforeRequestListener,
          {urls: [getURLWebAccessible()]});

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(
            onHeadersReceivedListener);
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestListener);
      });
    },

    function subresourceRedirectHasSameRequestIdOnBeforeRequest() {
      var url = getServerURL('echo');
      var requestId;
      var onBeforeRequestRedirectListener = function(details) {
        requestId = details.requestId;
        return {redirectUrl: getURLWebAccessible()};
      };
      chrome.webRequest.onBeforeRequest.addListener(
          onBeforeRequestRedirectListener, {urls: [url]}, ['blocking']);

      var onBeforeRequestListener = chrome.test.callbackPass(function(details) {
        chrome.test.assertEq(details.requestId, requestId);
      });
      chrome.webRequest.onBeforeRequest.addListener(onBeforeRequestListener,
          {urls: [getURLWebAccessible()]});

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestRedirectListener);
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestListener);
      });
    },
  ]);
});
