// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_json_parser_impl.h"

#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace data_decoder {

SafeJsonParserImpl::SafeJsonParserImpl(service_manager::Connector* connector,
                                       const std::string& unsafe_json,
                                       const SuccessCallback& success_callback,
                                       const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  connector->BindInterface(mojom::kServiceName, &json_parser_ptr_);
}

SafeJsonParserImpl::~SafeJsonParserImpl() = default;

void SafeJsonParserImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  json_parser_ptr_.set_connection_error_handler(base::Bind(
      &SafeJsonParserImpl::OnConnectionError, base::Unretained(this)));
  json_parser_ptr_->Parse(
      std::move(unsafe_json_),
      base::Bind(&SafeJsonParserImpl::OnParseDone, base::Unretained(this)));
}

void SafeJsonParserImpl::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shut down the utility process.
  json_parser_ptr_.reset();

  ReportResults(nullptr, "Connection error with the json parser process.");
}

void SafeJsonParserImpl::OnParseDone(std::unique_ptr<base::Value> result,
                                     const base::Optional<std::string>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shut down the utility process.
  json_parser_ptr_.reset();

  ReportResults(std::move(result), error.value_or(""));
}

void SafeJsonParserImpl::ReportResults(std::unique_ptr<base::Value> parsed_json,
                                       const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.empty() && parsed_json) {
    if (!success_callback_.is_null())
      success_callback_.Run(std::move(parsed_json));
  } else {
    if (!error_callback_.is_null())
      error_callback_.Run(error);
  }

  // The parsing is done whether an error occured or not, so this instance can
  // be cleaned up.
  delete this;
}

}  // namespace data_decoder
