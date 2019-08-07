// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_DATABASE_OBSERVER_IMPL_H_
#define CONTENT_RENDERER_WEB_DATABASE_OBSERVER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom.h"
#include "third_party/blink/public/platform/web_database_observer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class WebDatabaseObserverImpl : public blink::WebDatabaseObserver {
 public:
  explicit WebDatabaseObserverImpl(
      scoped_refptr<blink::mojom::ThreadSafeWebDatabaseHostPtr>
          web_database_host);
  virtual ~WebDatabaseObserverImpl();

  void DatabaseOpened(const blink::WebSecurityOrigin& origin,
                      const blink::WebString& database_name,
                      const blink::WebString& database_display_name,
                      uint32_t estimated_size) override;
  void DatabaseModified(const blink::WebSecurityOrigin& origin,
                        const blink::WebString& database_name) override;
  void DatabaseClosed(const blink::WebSecurityOrigin& origin,
                      const blink::WebString& database_name) override;
  void ReportSqliteError(const blink::WebSecurityOrigin& origin,
                         const blink::WebString& database_name,
                         int error) override;

 private:
  scoped_refptr<blink::mojom::ThreadSafeWebDatabaseHostPtr> web_database_host_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseObserverImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_DATABASE_OBSERVER_IMPL_H_
