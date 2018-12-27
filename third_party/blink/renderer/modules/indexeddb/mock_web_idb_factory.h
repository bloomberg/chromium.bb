// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_

#include <gmock/gmock.h>
#include <memory>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace WTF {
class String;
}

namespace blink {

class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;

class MockWebIDBFactory : public testing::StrictMock<blink::WebIDBFactory> {
 public:
  ~MockWebIDBFactory() override;

  static std::unique_ptr<MockWebIDBFactory> Create();

  MOCK_METHOD2(GetDatabaseInfo,
               void(WebIDBCallbacks*,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD2(GetDatabaseNames,
               void(WebIDBCallbacks*,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD6(Open,
               void(const WTF::String& name,
                    long long version,
                    long long transaction_id,
                    WebIDBCallbacks*,
                    WebIDBDatabaseCallbacks*,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD4(DeleteDatabase,
               void(const WTF::String& name,
                    WebIDBCallbacks*,
                    bool force_close,
                    scoped_refptr<base::SingleThreadTaskRunner>));

 private:
  MockWebIDBFactory();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_FACTORY_H_
