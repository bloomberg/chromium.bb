/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/clipboard/DataTransferItem.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/clipboard/DataObjectItem.h"
#include "core/clipboard/DataTransfer.h"
#include "core/dom/StringCallback.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/probe/CoreProbes.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

DataTransferItem* DataTransferItem::Create(DataTransfer* data_transfer,
                                           DataObjectItem* item) {
  return new DataTransferItem(data_transfer, item);
}

String DataTransferItem::kind() const {
  DEFINE_STATIC_LOCAL(const String, kind_string, ("string"));
  DEFINE_STATIC_LOCAL(const String, kind_file, ("file"));
  if (!data_transfer_->CanReadTypes())
    return String();
  switch (item_->Kind()) {
    case DataObjectItem::kStringKind:
      return kind_string;
    case DataObjectItem::kFileKind:
      return kind_file;
  }
  ASSERT_NOT_REACHED();
  return String();
}

String DataTransferItem::type() const {
  if (!data_transfer_->CanReadTypes())
    return String();
  return item_->GetType();
}

static void RunGetAsStringTask(ExecutionContext* context,
                               StringCallback* callback,
                               const String& data) {
  probe::AsyncTask async_task(context, callback);
  if (context)
    callback->handleEvent(data);
}

void DataTransferItem::getAsString(ScriptState* script_state,
                                   StringCallback* callback) const {
  if (!data_transfer_->CanReadData())
    return;
  if (!callback || item_->Kind() != DataObjectItem::kStringKind)
    return;

  ExecutionContext* context = script_state->GetExecutionContext();
  probe::AsyncTaskScheduled(context, "DataTransferItem.getAsString", callback);
  TaskRunnerHelper::Get(TaskType::kUserInteraction, script_state)
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&RunGetAsStringTask, WrapWeakPersistent(context),
                           WrapPersistent(callback), item_->GetAsString()));
}

File* DataTransferItem::getAsFile() const {
  if (!data_transfer_->CanReadData())
    return nullptr;

  return item_->GetAsFile();
}

DataTransferItem::DataTransferItem(DataTransfer* data_transfer,
                                   DataObjectItem* item)
    : data_transfer_(data_transfer), item_(item) {}

DEFINE_TRACE(DataTransferItem) {
  visitor->Trace(data_transfer_);
  visitor->Trace(item_);
}

}  // namespace blink
