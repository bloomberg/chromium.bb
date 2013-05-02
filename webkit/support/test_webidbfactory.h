// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBIDBFACTORY_H_
#define WEBKIT_SUPPORT_TEST_WEBIDBFACTORY_H_

#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_local.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIDBFactory.h"

// Wrap a WebKit::WebIDBFactory to rewrite the data directory to
// a scoped temp directory. In multiprocess Chromium this is rewritten
// to a real profile directory during IPC.
class TestWebIDBFactory : public WebKit::WebIDBFactory {
 public:
  TestWebIDBFactory();
  virtual ~TestWebIDBFactory();

  // WebIDBFactory methods:
  virtual void getDatabaseNames(WebKit::WebIDBCallbacks*,
                                const WebKit::WebString& database_identifier,
                                const WebKit::WebString& data_dir);
  virtual void open(const WebKit::WebString& name,
                    long long version,
                    long long transaction_id,
                    WebKit::WebIDBCallbacks*,
                    WebKit::WebIDBDatabaseCallbacks*,
                    const WebKit::WebString& database_identifier,
                    const WebKit::WebString& data_dir);
  virtual void deleteDatabase(const WebKit::WebString& name,
                              WebKit::WebIDBCallbacks*,
                              const WebKit::WebString& database_identifier,
                              const WebKit::WebString& data_dir);

 private:
  // Returns the WebIDBFactory implementation to use for the current thread.
  WebKit::WebIDBFactory* GetFactory();

  // Returns the data directory to use.
  WebKit::WebString GetDataDir() const;

  // We allocate a separate WebIDBFactory instance per thread since the
  // implementation is not thread-safe.  We also intentionally leak the
  // factory instances to avoid shutdown races.  TODO(darin): Can we
  // avoid leaking these?
  base::ThreadLocalPointer<WebKit::WebIDBFactory> factories_;

  base::ScopedTempDir indexed_db_dir_;
};

#endif  // WEBKIT_SUPPORT_TEST_WEBIDBFACTORY_H_
