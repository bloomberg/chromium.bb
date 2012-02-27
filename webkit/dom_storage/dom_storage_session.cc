// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"

namespace dom_storage {

DomStorageSession::DomStorageSession(DomStorageContext* context)
    : context_(context),
      namespace_id_(context->AllocateSessionId()) {
  context->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DomStorageContext::CreateSessionNamespace,
                 context_, namespace_id_));
}

DomStorageSession* DomStorageSession::Clone() {
  int64 clone_id = context_->AllocateSessionId();
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DomStorageContext::CloneSessionNamespace,
                 context_, namespace_id_, clone_id));
  return new DomStorageSession(context_, clone_id);
}

DomStorageSession::DomStorageSession(DomStorageContext* context,
                                     int64 namespace_id)
    : context_(context),
      namespace_id_(namespace_id) {
  // This ctor is intended for use by the Clone() method.
}

DomStorageSession::~DomStorageSession() {
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DomStorageContext::DeleteSessionNamespace,
                 context_, namespace_id_));
}

}  // namespace dom_storage
