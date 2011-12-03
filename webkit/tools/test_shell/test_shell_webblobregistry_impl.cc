// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using WebKit::WebBlobData;
using WebKit::WebURL;
using webkit_blob::BlobData;

namespace {

MessageLoop* g_io_thread;
webkit_blob::BlobStorageController* g_blob_storage_controller;

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
  GURL thread_safe_url = url;  // WebURL uses refcounted strings.
  g_io_thread->PostTask(FROM_HERE, base::Bind(
      &TestShellWebBlobRegistryImpl::AddFinishedBlob, this,
      thread_safe_url, make_scoped_refptr(new BlobData(data))));
}

void TestShellWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(g_io_thread);
  GURL thread_safe_url = url;
  GURL thread_safe_src_url = src_url;
  g_io_thread->PostTask(FROM_HERE,  base::Bind(
      &TestShellWebBlobRegistryImpl::CloneBlob, this,
      thread_safe_url, thread_safe_src_url));
}

void TestShellWebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(g_io_thread);
  GURL thread_safe_url = url;
  g_io_thread->PostTask(FROM_HERE, base::Bind(
      &TestShellWebBlobRegistryImpl::RemoveBlob, this,
      thread_safe_url));
}

void TestShellWebBlobRegistryImpl::AddFinishedBlob(
    const GURL& url, BlobData* blob_data) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->AddFinishedBlob(url, blob_data);
}

void TestShellWebBlobRegistryImpl::CloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->CloneBlob(url, src_url);
}

void TestShellWebBlobRegistryImpl::RemoveBlob(const GURL& url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->RemoveBlob(url);
}
