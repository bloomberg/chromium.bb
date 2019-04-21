// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web_resource/web_resource_util.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"

namespace web_resource {

namespace {

const char kInvalidDataTypeError[] =
    "Data from web resource server is missing or not valid JSON.";

const char kUnexpectedJSONFormatError[] =
    "Data from web resource server does not have expected format.";

// Parses |data| as a JSON string and calls back on |task_runner|.
// Must not be called on the UI thread, for performance reasons.
void ParseJSONOnBackgroundThread(
    base::TaskRunner* task_runner,
    const std::string& data,
    const WebResourceService::SuccessCallback& success_callback,
    const WebResourceService::ErrorCallback& error_callback) {
  if (data.empty()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(error_callback, kInvalidDataTypeError));
    return;
  }

  base::Optional<base::Value> value(base::JSONReader::Read(data));
  if (!value) {
    // Page information not properly read, or corrupted.
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(error_callback, kInvalidDataTypeError));
    return;
  }

  if (!value->is_dict()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(error_callback, kUnexpectedJSONFormatError));
    return;
  }

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(success_callback,
                     base::Value::ToUniquePtrValue(std::move(value).value())));
}

// Starts the parsing of |data| as a JSON string asynchronously on a background
// thread. Can be called from the UI thread.
void StartParseJSONAsync(
    const std::string& data,
    const WebResourceService::SuccessCallback& success_callback,
    const WebResourceService::ErrorCallback& error_callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseJSONOnBackgroundThread,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     data, success_callback, error_callback));
}

}  // namespace

WebResourceService::ParseJSONCallback GetIOSChromeParseJSONCallback() {
  return base::Bind(&StartParseJSONAsync);
}

}  // namespace web_resource
