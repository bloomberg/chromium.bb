// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getPageWithOSDDURL() {
  // A URL with path "/" (this points to webrequest/osdd/index.html).
  // The path is "/" because OSDD are only loaded from "/".
  return getServerURL('');
}

function getOSDDURL() {
  // The URL of the OpenSearch description document that is referenced by the
  // page from |getPageWithOSDDURL()|. The actual response is not important, so
  // the server will reply with the default 404 handler.
  return getServerURL('opensearch-dont-ignore-me.xml');
}


runTests([
  function test_linked_open_search_description() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'other',
          url: getOSDDURL(),
          // Not getPageWithOSDDURL() because we are only listening to requests
          // of type "other".
          frameUrl: 'unknown frame URL',
          tabId: 0,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'other',
          url: getOSDDURL(),
          tabId: 0,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'other',
          url: getOSDDURL(),
          tabId: 0,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'other',
          url: getOSDDURL(),
          tabId: 0,
          statusLine: 'HTTP/1.1 404 Not Found',
          statusCode: 404,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'other',
          url: getOSDDURL(),
          tabId: 0,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 404 Not Found',
          statusCode: 404,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'other',
          url: getOSDDURL(),
          tabId: 0,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 404 Not Found',
          statusCode: 404,
          initiator: getServerDomain(initiators.WEB_INITIATED)
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']],
        {urls: ['<all_urls>'], types: ['other']});

    // This page must be opened in the main frame, because OSDD requests are
    // only generated for main frame documents.
    navigateAndWait(getPageWithOSDDURL(), function() {
      console.log('Navigated to ' + getPageWithOSDDURL());
    });
  },
]);
