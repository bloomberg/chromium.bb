// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"

namespace printing {

class PrintBackendServiceImpl : public mojom::PrintBackendService {
 public:
  explicit PrintBackendServiceImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceImpl(const PrintBackendServiceImpl&) = delete;
  PrintBackendServiceImpl& operator=(const PrintBackendServiceImpl&) = delete;
  ~PrintBackendServiceImpl() override;

 private:
  friend class PrintBackendServiceTestImpl;

  using GetDefaultPrinterCallback =
      base::OnceCallback<void(const base::Optional<std::string>& printer_name)>;

  // mojom::PrintBackendService implementation:
  void Init(const std::string& locale) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;

  scoped_refptr<PrintBackend> print_backend_;

  mojo::Receiver<mojom::PrintBackendService> receiver_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
