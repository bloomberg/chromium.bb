// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/test_data_decoder_service.h"

#include "services/data_decoder/data_decoder_service.h"

namespace data_decoder {

TestDataDecoderService::TestDataDecoderService()
    : connector_factory_(std::make_unique<DataDecoderService>()),
      connector_(connector_factory_.CreateConnector()) {}

TestDataDecoderService::~TestDataDecoderService() = default;

}  // namespace data_decoder