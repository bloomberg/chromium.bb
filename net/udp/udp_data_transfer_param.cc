// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_data_transfer_param.h"

namespace net {

UDPDataTransferNetLogParam::UDPDataTransferNetLogParam(
    int byte_count,
    const char* bytes,
    bool log_bytes,
    const IPEndPoint* address)
    : byte_count_(byte_count),
      hex_encoded_bytes_(log_bytes ? base::HexEncode(bytes, byte_count) : "") {
  if (address)
    address_.reset(new IPEndPoint(*address));
}

Value* UDPDataTransferNetLogParam::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("byte_count", byte_count_);
  if (!hex_encoded_bytes_.empty())
    dict->SetString("hex_encoded_bytes", hex_encoded_bytes_);
  if (address_.get())
    dict->SetString("address", address_->ToString());
  return dict;
}

UDPDataTransferNetLogParam::~UDPDataTransferNetLogParam() {}

}  // namespace net
