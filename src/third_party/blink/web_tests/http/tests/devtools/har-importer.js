// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Verifies that imported HAR files create matching NetworkRequests');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('network_test_runner');
  const harRoot = new HARImporter.HARRoot(harJson);
  const requests = HARImporter.Importer.requestsFromHARLog(harRoot.log);
  const formattedRequests = await Promise.all(requests.map(async request => {
    return {
      url: request.url(),
      documentURL: request.documentURL,
      initiator: request.initiator(),
      requestFormData: await(request.requestFormData()),
      connectionId: request.connectionId,
      requestMethod: request.requestMethod,
      requestHeaders: request.requestHeaders(),
      mimeType: request.mimeType,
      responseHeaders: request.responseHeaders,
      statusCode: request.statusCode,
      statusText: request.statusText,
      protocol: request.protocol,
      resourceSize: request.resourceSize,
      transferSize: request.transferSize,
      cached: request.cached(),
      cachedInMemory: request.cachedInMemory(),
      contentData: await(request.contentData()),
      remoteAddress: request.remoteAddress(),
      resourceType: request.resourceType(),
      priority: request.priority(),
      finished: request.finished,
      timing: request.timing,
      endTime: request.endTime
    };
  }));
  TestRunner.addResult(
      'requests: ' + JSON.stringify(formattedRequests, null, 2));
  TestRunner.completeTest();
})();

const harJson = {
  'log': {
    'version': '1.2',
    'creator': {'name': 'WebInspector', 'version': '537.36'},
    'pages': [{
      'startedDateTime': '2018-11-20T20:43:07.756Z',
      'id': 'page_1',
      'title': 'http://localhost:8000/',
      'pageTimings':
          {'onContentLoad': 67.84599996171892, 'onLoad': 112.05600015819073}
    }],
    'entries': [
      {
        'startedDateTime': '2018-11-20T20:43:07.755Z',
        'time': 11.14144808263518,
        'request': {
          'method': 'GET',
          'url': 'http://localhost:8000/',
          'httpVersion': 'HTTP/1.1',
          'headers': [{'name': 'Host', 'value': 'localhost:8000'}],
          'queryString': [],
          'cookies': [],
          'headersSize': 418,
          'bodySize': 0
        },
        'response': {
          'status': 200,
          'statusText': 'OK',
          'httpVersion': 'HTTP/1.1',
          'headers': [
            {'name': 'Content-Type', 'value': 'text/html;charset=ISO-8859-1'}
          ],
          'cookies': [{
            'name': 'test-cookie-name',
            'value': '1',
            'path': '/',
            'domain': '.localhost',
            'expires': '2018-11-19T18:17:22.000Z',
            'httpOnly': false,
            'secure': false
          }],
          'content':
              {'size': 4633, 'mimeType': 'text/html', 'text': 'fake page data'},
          'redirectURL': '',
          'headersSize': 188,
          'bodySize': 4633,
          '_transferSize': 4821
        },
        'cache': {},
        'timings': {
          'blocked': 2.4644479999188333,
          'dns': -1,
          'ssl': -1,
          'connect': -1,
          'send': 0.06999999999999984,
          'wait': 3.089999983072281,
          'receive': 5.517000099644065,
          '_blocked_queueing': 0.447999918833375,
          '_blocked_proxy': 0.44899999999999984
        },
        'serverIPAddress': '[::1]',
        '_initiator': {'type': 'other'},
        '_priority': 'VeryHigh',
        'connection': '2945',
        'pageref': 'page_1'
      },
      {
        'startedDateTime': '2018-11-20T20:43:07.870Z',
        'time': 3.8945360814686865,
        'request': {
          'method': 'POST',
          'url': 'http://localhost:8000/post-endpoint',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'queryString': [],
          'cookies': [],
          'headersSize': 386,
          'bodySize': 0,
          'postData': {
            'mimeType': 'application/x-www-form-urlencoded',
            'text': 'one=urlencodedvalueone&two=urlencodedvaluetwo',
            'params': [
              {'name': 'one', 'value': 'urlencodedvalueone'},
              {'name': 'two', 'value': 'urlencodedvaluetwo'}
            ]
          }
        },
        'response': {
          'status': 200,
          'statusText': 'OK',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'cookies': [],
          'content':
              {'size': 1150, 'mimeType': 'image/x-icon', 'compression': 0},
          'redirectURL': '',
          'headersSize': 267,
          'bodySize': 1150,
          '_transferSize': 1417
        },
        'cache': {},
        'timings': {
          'blocked': 2.2485360001232477,
          'dns': -1,
          'ssl': -1,
          'connect': -1,
          'send': 0.06099999999999994,
          'wait': 0.5190001133680342,
          'receive': 1.0659999679774046,
          '_blocked_queueing': 0.5360001232475042,
          '_blocked_proxy': 0.4910000000000001
        },
        'serverIPAddress': '[::1]',
        '_initiator':
            {'type': 'parser', 'url': 'http://localhost/', 'lineNumber': 1},
        '_priority': 'Low',
        'connection': '2945',
        'pageref': 'page_1'
      },
      {
        'startedDateTime': '2018-11-20T20:43:07.870Z',
        'time': 3.8945360814686865,
        'request': {
          'method': 'GET',
          'url': 'http://localhost:8000/js_file.js',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'queryString': [],
          'cookies': [],
          'headersSize': 386,
          'bodySize': 0
        },
        'response': {
          'status': 200,
          'statusText': 'OK',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'cookies': [],
          'content': {'size': 1150, 'compression': 0},
          'redirectURL': '',
          'headersSize': 267,
          'bodySize': 1150,
          '_transferSize': 1417
        },
        'cache': {},
        'timings': {
          'blocked': 2.2485360001232477,
          'dns': -1,
          'ssl': -1,
          'connect': -1,
          'send': 0.06099999999999994,
          'wait': 0.5190001133680342,
          'receive': 1.0659999679774046,
          '_blocked_queueing': 0.5360001232475042,
          '_blocked_proxy': 0.4910000000000001
        },
        'serverIPAddress': '[::1]',
        '_initiator':
            {'type': 'parser', 'url': 'http://localhost/', 'lineNumber': 1},
        '_priority': 'Low',
        'connection': '2945',
        'pageref': 'page_1'
      },
      {
        'startedDateTime': '2018-11-20T20:43:07.870Z',
        'time': 3.8945360814686865,
        'request': {
          'method': 'GET',
          'url': 'http://localhost:8000/endpoint',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'queryString': [],
          'cookies': [],
          'headersSize': 386,
          'bodySize': 0
        },
        'response': {
          'status': 200,
          'statusText': 'OK',
          'httpVersion': 'HTTP/1.1',
          'headers': [],
          'cookies': [],
          'content': {'size': 1150, 'compression': 0},
          'redirectURL': '',
          'headersSize': 267,
          'bodySize': 1150,
          '_transferSize': 1417
        },
        'cache': {},
        'timings': {
          'blocked': 2.2485360001232477,
          'dns': -1,
          'ssl': -1,
          'connect': -1,
          'send': 0.06099999999999994,
          'wait': 0.5190001133680342,
          'receive': 1.0659999679774046,
          '_blocked_queueing': 0.5360001232475042,
          '_blocked_proxy': 0.4910000000000001
        },
        'serverIPAddress': '[::1]',
        '_initiator': {
          'type': 'script',
          'stack': {
            'callFrames': {
              'functionName': '',
              'scriptId': '32',
              'url': 'http://localhost:8000/script.js',
              'lineNumber': 5,
              'colNumber': 2
            }
          }
        },
        '_priority': 'Low',
        '_resourceType': 'fetch',
        'connection': '2945',
        'pageref': 'page_1'
      }
    ]
  }
};
