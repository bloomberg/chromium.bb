// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_IPP_VALIDATOR_IPP_VALIDATOR_H_
#define CHROME_SERVICES_CUPS_IPP_VALIDATOR_IPP_VALIDATOR_H_

#include "chrome/services/cups_ipp_validator/public/mojom/ipp_validator.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace chrome {

// This class implements the chrome.mojom.IppValidator interface.
// It is intended to operate under the heavily jailed, out-of-process
// cups_ipp_validator service, validating incoming requests before passing
// them to CUPS.
class IppValidator : public chrome::mojom::IppValidator {
 public:
  explicit IppValidator(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~IppValidator() override;

 private:
  // chrome::mojom::IppValidator:
  // TODO(crbug.com/831913): implement, finalize wrapppers, lhchavez@
  // Checks that |request| is formatted as a valid IPP request, per RFC2910
  // Calls |callback| with true on success, else false
  // void ValidateIpp(const std::string& request, ValidateIppCallback callback)
  // override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(IppValidator);
};

}  // namespace chrome

#endif  // CHROME_SERVICES_CUPS_IPP_VALIDATOR_IPP_VALIDATOR_H_
