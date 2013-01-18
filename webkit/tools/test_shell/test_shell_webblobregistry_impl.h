// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBBLOBREGISTRY_IMPL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBBLOBREGISTRY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebBlobRegistry.h"

class GURL;

namespace webkit_blob {
class BlobData;
class BlobStorageController;
}

class TestShellWebBlobRegistryImpl
    : public WebKit::WebBlobRegistry,
      public base::RefCountedThreadSafe<TestShellWebBlobRegistryImpl> {
 public:
  static void InitializeOnIOThread(
      webkit_blob::BlobStorageController* blob_storage_controller);
  static void Cleanup();

  TestShellWebBlobRegistryImpl();

  // See WebBlobRegistry.h for documentation on these functions.
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               WebKit::WebBlobData& data);
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               const WebKit::WebURL& src_url);
  virtual void unregisterBlobURL(const WebKit::WebURL& url);

 protected:
  virtual ~TestShellWebBlobRegistryImpl() {}

 private:
  friend class base::RefCountedThreadSafe<TestShellWebBlobRegistryImpl>;

  // Run on I/O thread.
  void AddFinishedBlob(const GURL& url, webkit_blob::BlobData* blob_data);
  void CloneBlob(const GURL& url, const GURL& src_url);
  void RemoveBlob(const GURL& url);

  DISALLOW_COPY_AND_ASSIGN(TestShellWebBlobRegistryImpl);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBBLOBREGISTRY_IMPL_H_
