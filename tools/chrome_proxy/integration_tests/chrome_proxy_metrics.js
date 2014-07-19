// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains common utilities to find chrome proxy related elements on
// a page and collect info from them.

(function() {
  var PROXY_VIEW_ID = 'proxy-view-tab-content';
  var PROXY_VIEW_EFFECTIVE_SETTINGS_ID = 'proxy-view-effective-settings';
  var PROXY_VIEW_BAD_PROXIES_ID = 'proxy-view-bad-proxies-div';
  var PROXY_VIEW_BAD_PROXIES_TBODY = 'proxy-view-bad-proxies-tbody';
  var PRXOY_SETTINGS_PREFIX = 'Proxy server for HTTP: ['
  var PROXY_SETTINGS_SIGNATURE = 'proxy.googlezip.net:443, ' +
    'compress.googlezip.net:80, direct://';

  // Returns the effective proxy in an array from settings.
  // An example of the settings is:
  // "Proxy server for HTTP: [proxy.googlezip.net:443, " +
  // "compress.googlezip.net:80, direct://]"
  function getEffectiveProxies(doc) {
    var settings = doc.getElementById(PROXY_VIEW_EFFECTIVE_SETTINGS_ID);
    if (settings && settings.innerHTML &&
      settings.innerHTML.indexOf(PRXOY_SETTINGS_PREFIX) == 0) {
      var left = settings.innerHTML.indexOf('[');
      var right = settings.innerHTML.indexOf(']');
      if (left >= 0 && right > left) {
        return settings.innerHTML.substring(left + 1, right).split(/[ ,]+/);
      }
    }
    return [];
  }

  // Returns an array of bad proxies. Each element is a bad proxy with
  // attribute 'proxy' as the proxy name and attribute 'retry' as the
  // next retry time.
  function getBadProxyList(doc) {
    var bad_proxies = doc.getElementById(PROXY_VIEW_BAD_PROXIES_ID);
    if (bad_proxies.hasAttribute('style') &&
        ('cssText' in bad_proxies.style) &&
        bad_proxies.style.cssText == 'display: none;') {
          return null;
        }
    var tbody = doc.getElementById(PROXY_VIEW_BAD_PROXIES_TBODY);
    results = [];
    for (var r = 0, n = tbody.rows.length; r < n; r++) {
      results[r] = {};
      results[r].proxy = tbody.rows[r].cells[0].innerHTML;
      timeSpan = tbody.rows[r].cells[1].getElementsByTagName('span')[0];
      if (timeSpan.hasAttribute('title') && timeSpan.title.indexOf('t=') == 0) {
        results[r].retry = timeSpan.title.substr(2);
      } else {
        results[r].retry = '-1';
      }
    }
    return results;
  }

  function getChromeProxyInfo() {
    if (!document.getElementById(PROXY_VIEW_ID)) {
      return null;
    }
    info = {};
    info.proxies = getEffectiveProxies(document);
    info.enabled = (info.proxies.length > 1 &&
        info.proxies[info.proxies.length - 1] == 'direct://' &&
        info.proxies[info.proxies.length - 2] != 'direct://');
    info.badProxies = getBadProxyList(document);
    return info;
  };
  window.__getChromeProxyInfo = getChromeProxyInfo;
})();
