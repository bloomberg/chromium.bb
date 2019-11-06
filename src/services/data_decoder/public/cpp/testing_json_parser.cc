// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/testing_json_parser.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"

namespace data_decoder {

namespace {

SafeJsonParser* CreateTestingJsonParser(
    const std::string& unsafe_json,
    SafeJsonParser::SuccessCallback success_callback,
    SafeJsonParser::ErrorCallback error_callback) {
  return new TestingJsonParser(unsafe_json, std::move(success_callback),
                               std::move(error_callback));
}

}  // namespace

TestingJsonParser::ScopedFactoryOverride::ScopedFactoryOverride() {
  SafeJsonParser::SetFactoryForTesting(&CreateTestingJsonParser);
}

TestingJsonParser::ScopedFactoryOverride::~ScopedFactoryOverride() {
  SafeJsonParser::SetFactoryForTesting(nullptr);
}

TestingJsonParser::TestingJsonParser(const std::string& unsafe_json,
                                     SuccessCallback success_callback,
                                     ErrorCallback error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

TestingJsonParser::~TestingJsonParser() {}

void TestingJsonParser::Start() {
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(unsafe_json_,
                                                    base::JSON_PARSE_RFC);

  // Run the callback asynchronously. Post the delete task first, so that the
  // completion callbacks may quit the run loop without leaking |this|.
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, value_with_error.value
                     ? base::BindOnce(std::move(success_callback_),
                                      std::move(*value_with_error.value))
                     : base::BindOnce(std::move(error_callback_),
                                      value_with_error.error_message));
}

}  // namespace data_decoder
