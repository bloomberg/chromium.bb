// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_json_parser.h"

#include "base/optional.h"
#include "base/token.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "services/data_decoder/public/cpp/safe_json_parser_android.h"
#else
#include "services/data_decoder/public/cpp/safe_json_parser_impl.h"
#endif

namespace data_decoder {

namespace {

SafeJsonParser::Factory g_factory = nullptr;

SafeJsonParser* Create(service_manager::Connector* connector,
                       const std::string& unsafe_json,
                       SafeJsonParser::SuccessCallback success_callback,
                       SafeJsonParser::ErrorCallback error_callback,
                       const base::Optional<base::Token>& batch_id) {
  if (g_factory)
    return g_factory(unsafe_json, std::move(success_callback),
                     std::move(error_callback));

#if defined(OS_IOS)
  NOTREACHED() << "SafeJsonParser is not supported on iOS (except in tests)";
  return nullptr;
#elif defined(OS_ANDROID)
  return new SafeJsonParserAndroid(unsafe_json, std::move(success_callback),
                                   std::move(error_callback));
#else
  return new SafeJsonParserImpl(connector, unsafe_json,
                                std::move(success_callback),
                                std::move(error_callback), batch_id);
#endif
}

}  // namespace

// static
void SafeJsonParser::SetFactoryForTesting(Factory factory) {
  g_factory = factory;
}

// static
void SafeJsonParser::Parse(service_manager::Connector* connector,
                           const std::string& unsafe_json,
                           SuccessCallback success_callback,
                           ErrorCallback error_callback) {
  SafeJsonParser* parser =
      Create(connector, unsafe_json, std::move(success_callback),
             std::move(error_callback), base::nullopt);
  parser->Start();
}

// static
void SafeJsonParser::ParseBatch(service_manager::Connector* connector,
                                const std::string& unsafe_json,
                                SuccessCallback success_callback,
                                ErrorCallback error_callback,
                                const base::Token& batch_id) {
  SafeJsonParser* parser =
      Create(connector, unsafe_json, std::move(success_callback),
             std::move(error_callback), batch_id);
  parser->Start();
}

}  // namespace data_decoder
