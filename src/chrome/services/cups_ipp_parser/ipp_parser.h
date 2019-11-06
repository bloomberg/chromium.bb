// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_H_
#define CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_H_

#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace chrome {

// chrome.mojom.IppParser handler.
//
// This handler accepts incoming IPP requests as arbitrary buffers, parses
// the contents using libCUPS, and yields a chrome::mojom::IppRequest. It is
// intended to operate under the heavily jailed, out-of-process CupsIppParser
// Service.
class IppParser : public chrome::mojom::IppParser {
 public:
  explicit IppParser(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~IppParser() override;

 private:
  // chrome::mojom::IppParser override.
  // Checks that |to_parse| is formatted as a valid IPP request, per RFC2910
  // Calls |callback| with a fully parsed IPP request on success, empty on
  // failure.
  void ParseIpp(const std::vector<uint8_t>& to_parse,
                ParseIppCallback callback) override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(IppParser);
};

}  // namespace chrome

#endif  // CHROME_SERVICES_CUPS_IPP_PARSER_IPP_PARSER_H_
