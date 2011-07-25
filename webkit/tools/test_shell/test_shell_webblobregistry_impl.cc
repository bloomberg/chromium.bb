// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using WebKit::WebBlobData;
using WebKit::WebString;
using WebKit::WebURL;

namespace {

MessageLoop* g_io_thread;
webkit_blob::BlobStorageController* g_blob_storage_controller;

// WebURL contains a WebCString object that is ref-counted,
// but not thread-safe ref-counted.
// "Normal" copying of WebURL results in a copy that is not thread-safe.
// This method creates a deep copy of WebURL.
WebURL GetWebURLThreadsafeCopy(const WebURL& source) {
  const WebKit::WebCString spec(source.spec().data(), source.spec().length());
  const url_parse::Parsed& parsed(source.parsed());
  const bool is_valid = source.isValid();
  return WebURL(spec, parsed, is_valid);
}

}  // namespace

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
  CancelableTask* task;
  {
    scoped_refptr<webkit_blob::BlobData> blob_data(
      new webkit_blob::BlobData(data));
    WebURL url_copy = GetWebURLThreadsafeCopy(url);
    task =
      NewRunnableMethod(
          this, &TestShellWebBlobRegistryImpl::DoRegisterBlobUrl, url_copy,
          blob_data);
    // After this block exits, url_copy is disposed, and
    // the underlying WebCString will have a refcount=1 and will
    // only be accessible from the task object.
  }
  g_io_thread->PostTask(FROM_HERE, task);
}

void TestShellWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(g_io_thread);
  CancelableTask* task;
  {
    WebURL url_copy = GetWebURLThreadsafeCopy(url);
    WebURL src_url_copy = GetWebURLThreadsafeCopy(src_url);
    task =
      NewRunnableMethod(this,
                        &TestShellWebBlobRegistryImpl::DoRegisterBlobUrlFrom,
                        url_copy,
                        src_url_copy);
    // After this block exits, url_copy and src_url_copy are disposed, and
    // the underlying WebCStrings will have a refcount=1 and will
    // only be accessible from the task object.
  }
  g_io_thread->PostTask(FROM_HERE, task);
}

void TestShellWebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(g_io_thread);
  CancelableTask* task;
  {
    WebURL url_copy = GetWebURLThreadsafeCopy(url);
    task =
      NewRunnableMethod(this,
                        &TestShellWebBlobRegistryImpl::DoUnregisterBlobUrl,
                        url_copy);
    // After this block exits, url_copy is disposed, and
    // the underlying WebCString will have a refcount=1 and will
    // only be accessible from the task object.
  }
  g_io_thread->PostTask(FROM_HERE, task);
}

void TestShellWebBlobRegistryImpl::DoRegisterBlobUrl(
    const GURL& url, webkit_blob::BlobData* blob_data) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->RegisterBlobUrl(url, blob_data);
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
