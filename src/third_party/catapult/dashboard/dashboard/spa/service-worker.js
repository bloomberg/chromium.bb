/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import DescribeCacheRequest from './describe-cache-request.js';
import ReportCacheRequest from './report-cache-request.js';
import ReportNamesCacheRequest from './report-names-cache-request.js';
import TestSuitesCacheRequest from './test-suites-cache-request.js';

self.addEventListener('activate', activateEvent => {
  activateEvent.waitUntil(self.clients.claim());
});

const FETCH_HANDLERS = {
  '/api/describe': DescribeCacheRequest,
  '/api/report/generate': ReportCacheRequest,
  '/api/report/names': ReportNamesCacheRequest,
  '/api/test_suites': TestSuitesCacheRequest,
};

self.addEventListener('fetch', fetchEvent => {
  const cls = FETCH_HANDLERS[new URL(fetchEvent.request.url).pathname];
  if (!cls) return;
  new cls(fetchEvent).respond();
});
