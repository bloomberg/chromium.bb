// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function getSetCookieUrl(name, value) {
  return getServerURL('set-header?Set-Cookie:%20' + name + '=' + value);
}

function checkHeaders(headers, requiredNames, disallowedNames) {
  var headerMap = {};
  for (var i = 0; i < headers.length; i++)
    headerMap[headers[i].name.toLowerCase()] = headers[i].value;

  for (var i = 0; i < requiredNames.length; i++) {
    chrome.test.assertTrue(!!headerMap[requiredNames[i]],
        'Missing header: ' + requiredNames[i]);
  }
  for (var i = 0; i < disallowedNames.length; i++) {
    chrome.test.assertFalse(!!headerMap[disallowedNames[i]],
        'Header should not be present: ' + disallowedNames[i]);
  }
}

function removeHeader(headers, name) {
  for (var i = 0; i < headers.length; i++) {
    if (headers[i].name.toLowerCase() == name) {
      headers.splice(i, 1);
      break;
    }
  }
}

runTests([
  function testSpecialRequestHeadersVisible() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echo');
      var extraHeadersListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders, ['user-agent', 'cookie'], []);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(extraHeadersListener,
          {urls: [url]}, ['requestHeaders', 'extraHeaders']);

      var standardListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders, ['user-agent'], ['cookie']);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(standardListener,
          {urls: [url]}, ['requestHeaders']);

      navigateAndWait(url, function() {
        chrome.webRequest.onBeforeSendHeaders.removeListener(
            extraHeadersListener);
        chrome.webRequest.onBeforeSendHeaders.removeListener(standardListener);
      });
    });
  },

  function testSpecialResponseHeadersVisible() {
    var url = getSetCookieUrl('foo', 'bar');
    var extraHeadersListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, ['set-cookie'], []);
    });
    chrome.webRequest.onHeadersReceived.addListener(extraHeadersListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);

    var standardListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, [], ['set-cookie']);
    });
    chrome.webRequest.onHeadersReceived.addListener(standardListener,
        {urls: [url]}, ['responseHeaders']);

    navigateAndWait(url, function() {
      chrome.webRequest.onHeadersReceived.removeListener(extraHeadersListener);
      chrome.webRequest.onHeadersReceived.removeListener(standardListener);
    });
  },

  function testModifySpecialRequestHeaders() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echoheader?Cookie');
      var listener = callbackPass(function(details) {
        removeHeader(details.requestHeaders, 'cookie');
        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(listener,
          {urls: [url]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(tab.id, {
          code: 'document.body.innerText'
        }, callbackPass(function(results) {
          chrome.test.assertTrue(results[0].indexOf('bar') == -1,
              'Header not removed.');
        }));
      });
    });
  },

  function testCannotModifySpecialRequestHeadersWithoutExtraHeaders() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echoheader?Cookie');
      var listener = callbackPass(function(details) {
        removeHeader(details.requestHeaders, 'cookie');
        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(listener,
          {urls: [url]}, ['requestHeaders', 'blocking']);

      // Add a no-op listener with extraHeaders to make sure it does not affect
      // the other listener.
      var noop = function() {};
      chrome.webRequest.onBeforeSendHeaders.addListener(noop,
          {urls: [url]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(noop);
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(tab.id, {
          code: 'document.body.innerText'
        }, callbackPass(function(results) {
          chrome.test.assertTrue(results[0].indexOf('bar') >= 0,
              'Header should not be removed.');
        }));
      });
    });
  },

  function testModifyUserAgentWithoutExtraHeaders() {
    var url = getServerURL('echoheader?User-Agent');
    var listener = callbackPass(function(details) {
      var headers = details.requestHeaders;
      for (var i = 0; i < headers.length; i++) {
        if (headers[i].name.toLowerCase() === 'user-agent') {
          headers[i].value = 'foo';
          break;
        }
      }
      return {requestHeaders: headers};
    });
    chrome.webRequest.onBeforeSendHeaders.addListener(listener,
        {urls: [url]}, ['requestHeaders', 'blocking']);

    navigateAndWait(url, function(tab) {
      chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
      chrome.tabs.executeScript(tab.id, {
        code: 'document.body.innerText'
      }, callbackPass(function(results) {
        chrome.test.assertTrue(results[0].indexOf('foo') >= 0,
            'User-Agent should be modified.');
      }));
    });
  },

  // Successful Set-Cookie modification is tested in test_blocking_cookie.js.
  function testCannotModifySpecialResponseHeadersWithoutExtraHeaders() {
    // Use unique name and value so other tests don't interfere.
    var url = getSetCookieUrl('theName', 'theValue');
    var listener = callbackPass(function(details) {
      removeHeader(details.responseHeaders, 'set-cookie');
      return {responseHeaders: details.responseHeaders};
    });
    chrome.webRequest.onHeadersReceived.addListener(listener,
        {urls: [url]}, ['responseHeaders', 'blocking']);

    // Add a no-op listener with extraHeaders to make sure it does not affect
    // the other listener.
    var noop = function() {};
    chrome.webRequest.onHeadersReceived.addListener(noop,
        {urls: [url]}, ['responseHeaders', 'blocking', 'extraHeaders']);

    navigateAndWait(url, function(tab) {
      chrome.webRequest.onHeadersReceived.removeListener(noop);
      chrome.webRequest.onHeadersReceived.removeListener(listener);
      chrome.tabs.executeScript(tab.id, {
        code: 'document.cookie'
      }, callbackPass(function(results) {
        chrome.test.assertTrue(results[0].indexOf('theValue') >= 0,
            'Header should not be removed.');
      }));
    });
  },
]);
