// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/data_decoder_util.h"

#include "services/service_manager/public/cpp/connector.h"

namespace media_router {

namespace {
// The batch ID used by data_decoder_util functions.
constexpr char kDataDecoderServiceBatchId[] = "media_router";
}  // namespace

DataDecoder::DataDecoder(service_manager::Connector* connector)
    : connector_(connector->Clone()) {}

DataDecoder::~DataDecoder() = default;

void DataDecoder::ParseXml(const std::string& unsafe_xml,
                           data_decoder::XmlParserCallback callback) {
  data_decoder::ParseXml(connector_.get(), unsafe_xml, std::move(callback),
                         kDataDecoderServiceBatchId);
}

void DataDecoder::ParseJson(
    const std::string& unsafe_json,
    const data_decoder::SafeJsonParser::SuccessCallback& success_callback,
    const data_decoder::SafeJsonParser::ErrorCallback& error_callback) {
  data_decoder::SafeJsonParser::ParseBatch(connector_.get(), unsafe_json,
                                           success_callback, error_callback,
                                           kDataDecoderServiceBatchId);
}

}  // namespace media_router
