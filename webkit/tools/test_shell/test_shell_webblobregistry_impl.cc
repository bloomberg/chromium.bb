// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBlobData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using WebKit::WebBlobData;
using WebKit::WebBlobStorageData;
using WebKit::WebString;
using WebKit::WebURL;

MessageLoop* g_io_thread;
webkit_blob::BlobStorageController* g_blob_storage_controller;

/* static */
void TestShellWebBlobRegistryImpl::InitializeOnIOThread(
    webkit_blob::BlobStorageController* blob_storage_controller) {
  g_io_thread = MessageLoop::current();
  g_blob_storage_controller = blob_storage_controller;
}

/* static */
void TestShellWebBlobRegistryImpl::Cleanup() {
  g_io_thread = NULL;
  g_blob_storage_controller = NULL;
}

TestShellWebBlobRegistryImpl::TestShellWebBlobRegistryImpl() {
}

void TestShellWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  DCHECK(g_io_thread);
  scoped_refptr<webkit_blob::BlobData> blob_data(
      new webkit_blob::BlobData(data));
  blob_data->AddRef();  // Release on DoRegisterBlobURL.
  g_io_thread->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &TestShellWebBlobRegistryImpl::DoRegisterBlobUrl,
                        url,
                        blob_data.get()));
}

void TestShellWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(g_io_thread);
  g_io_thread->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &TestShellWebBlobRegistryImpl::DoRegisterBlobUrlFrom,
                        url,
                        src_url));
}

void TestShellWebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(g_io_thread);
  g_io_thread->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &TestShellWebBlobRegistryImpl::DoUnregisterBlobUrl,
                        url));
}

void TestShellWebBlobRegistryImpl::DoRegisterBlobUrl(
    const GURL& url, webkit_blob::BlobData* blob_data) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->RegisterBlobUrl(url, blob_data);
  blob_data->Release();
}

void TestShellWebBlobRegistryImpl::DoRegisterBlobUrlFrom(
    const GURL& url, const GURL& src_url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->RegisterBlobUrlFrom(url, src_url);
}

void TestShellWebBlobRegistryImpl::DoUnregisterBlobUrl(const GURL& url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->UnregisterBlobUrl(url);
}
