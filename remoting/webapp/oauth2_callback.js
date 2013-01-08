// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OAuth2 class that handles retrieval/storage of an OAuth2 token.
 *
 * Uses a content script to trampoline the OAuth redirect page back into the
 * extension context.  This works around the lack of native support for
 * chrome-extensions in OAuth2.
 */

'use strict';

function retrieveRefreshToken() {
  var query = window.location.search.substring(1);
  var parts = query.split('&');
  var queryArgs = {};
  for (var i = 0; i < parts.length; i++) {
    var pair = parts[i].split('=');
    queryArgs[pair[0]] = pair[1];
  }

  if ('code' in queryArgs && 'state' in queryArgs) {
    var oauth2 = new remoting.OAuth2();
    oauth2.exchangeCodeForToken(queryArgs['code'], queryArgs['state'],
      function() {
        window.location.replace(chrome.extension.getURL('main.html'));
      });
  } else {
    window.location.replace(chrome.extension.getURL('main.html'));
  }
}

window.addEventListener('load', retrieveRefreshToken, false);
