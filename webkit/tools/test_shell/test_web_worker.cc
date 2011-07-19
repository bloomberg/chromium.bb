// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_web_worker.h"

TestWebWorker::TestWebWorker() {
  AddRef();  // Adds the reference held for worker object.
  AddRef();  // Adds the reference held for worker context object.
}

void TestWebWorker::workerObjectDestroyed() {
  Release();  // Releases the reference held for worker object.
}

void TestWebWorker::workerContextDestroyed() {
  Release();    // Releases the reference held for worker context object.
}

WebKit::WebWorker* TestWebWorker::createWorker(
    WebKit::WebWorkerClient* client) {
  return NULL;
}

WebKit::WebNotificationPresenter* TestWebWorker::notificationPresenter() {
  return NULL;
}

WebKit::WebApplicationCacheHost* TestWebWorker::createApplicationCacheHost(
    WebKit::WebApplicationCacheHostClient*) {
  return NULL;
}

bool TestWebWorker::allowDatabase(WebKit::WebFrame* frame,
                                  const WebKit::WebString& name,
                                  const WebKit::WebString& display_name,
                                  unsigned long estimated_size) {
  return true;
}

TestWebWorker::~TestWebWorker() {}
