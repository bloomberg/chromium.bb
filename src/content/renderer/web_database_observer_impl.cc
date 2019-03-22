// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_database_observer_impl.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/sqlite/sqlite3.h"

using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

WebDatabaseObserverImpl::WebDatabaseObserverImpl(
    scoped_refptr<blink::mojom::ThreadSafeWebDatabaseHostPtr> web_database_host)
    : web_database_host_(std::move(web_database_host)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(main_thread_task_runner_);
}

WebDatabaseObserverImpl::~WebDatabaseObserverImpl() = default;

void WebDatabaseObserverImpl::DatabaseOpened(
    const WebSecurityOrigin& origin,
    const WebString& database_name,
    const WebString& database_display_name,
    unsigned long estimated_size) {
  (*web_database_host_)
      ->Opened(origin, database_name.Utf16(), database_display_name.Utf16(),
               estimated_size);
}

void WebDatabaseObserverImpl::DatabaseModified(const WebSecurityOrigin& origin,
                                               const WebString& database_name) {
  (*web_database_host_)->Modified(origin, database_name.Utf16());
}

void WebDatabaseObserverImpl::DatabaseClosed(const WebSecurityOrigin& origin,
                                             const WebString& database_name) {
  DCHECK(!main_thread_task_runner_->RunsTasksInCurrentSequence());
  (*web_database_host_)->Closed(origin, database_name.Utf16());
}

void WebDatabaseObserverImpl::ReportSqliteError(const WebSecurityOrigin& origin,
                                                const WebString& database_name,
                                                int error) {
  // We filter out errors which the backend doesn't act on to avoid
  // a unnecessary ipc traffic, this method can get called at a fairly
  // high frequency (per-sqlstatement).
  if (error != SQLITE_CORRUPT && error != SQLITE_NOTADB)
    return;

  (*web_database_host_)
      ->HandleSqliteError(origin, database_name.Utf16(), error);
}

}  // namespace content
