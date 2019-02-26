// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
#define SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace data_decoder {

class DataDecoderService : public service_manager::Service {
 public:
  DataDecoderService();
  explicit DataDecoderService(service_manager::mojom::ServiceRequest request);
  ~DataDecoderService() override;

  // May be used to establish a latent Service binding for this instance. May
  // only be called once, and only if this instance was default-constructed.
  void BindRequest(service_manager::mojom::ServiceRequest request);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  void BindImageDecoder(mojom::ImageDecoderRequest request);
  void BindJsonParser(mojom::JsonParserRequest request);
  void BindXmlParser(mojom::XmlParserRequest request);

  service_manager::ServiceBinding binding_{this};
  service_manager::ServiceKeepalive keepalive_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(DataDecoderService);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
