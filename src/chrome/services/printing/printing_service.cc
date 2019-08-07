// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/printing_service.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/services/printing/pdf_nup_converter.h"
#include "chrome/services/printing/pdf_to_pwg_raster_converter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if defined(OS_WIN)
#include "chrome/services/printing/pdf_to_emf_converter.h"
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

namespace printing {

namespace {

#if defined(OS_WIN)
void OnPdfToEmfConverterFactoryRequest(
    service_manager::ServiceKeepalive* keepalive,
    printing::mojom::PdfToEmfConverterFactoryRequest request) {
  mojo::MakeStrongBinding(std::make_unique<printing::PdfToEmfConverterFactory>(
                              keepalive->CreateRef()),
                          std::move(request));
}
#endif

void OnPdfToPwgRasterConverterRequest(
    service_manager::ServiceKeepalive* keepalive,
    printing::mojom::PdfToPwgRasterConverterRequest request) {
  mojo::MakeStrongBinding(std::make_unique<printing::PdfToPwgRasterConverter>(
                              keepalive->CreateRef()),
                          std::move(request));
}

void OnPdfNupConverterRequest(service_manager::ServiceKeepalive* keepalive,
                              printing::mojom::PdfNupConverterRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<printing::PdfNupConverter>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

PrintingService::PrintingService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

PrintingService::~PrintingService() = default;

void PrintingService::OnStart() {
#if defined(OS_WIN)
  registry_.AddInterface(base::BindRepeating(&OnPdfToEmfConverterFactoryRequest,
                                             &service_keepalive_));
#endif
  registry_.AddInterface(base::BindRepeating(&OnPdfToPwgRasterConverterRequest,
                                             &service_keepalive_));
  registry_.AddInterface(
      base::BindRepeating(&OnPdfNupConverterRequest, &service_keepalive_));
}

void PrintingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace printing
