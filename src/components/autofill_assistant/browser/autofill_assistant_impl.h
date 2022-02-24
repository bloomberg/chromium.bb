// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_

#include <vector>

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"

namespace autofill_assistant {

class AutofillAssistantImpl : public autofill_assistant::AutofillAssistant {
 public:
  static std::unique_ptr<AutofillAssistantImpl> Create(
      content::BrowserContext* browser_context,
      version_info::Channel channel,
      const std::string& country_code,
      const std::string& locale);

  AutofillAssistantImpl(std::unique_ptr<ServiceRequestSender> request_sender,
                        const GURL& script_server_url,
                        const std::string& country_code,
                        const std::string& locale);
  AutofillAssistantImpl(const AutofillAssistantImpl&) = delete;
  AutofillAssistantImpl& operator=(const AutofillAssistantImpl&) = delete;
  ~AutofillAssistantImpl() override;

  void GetCapabilitiesByHashPrefix(
      uint32_t hash_prefix_length,
      const std::vector<uint64_t>& hash_prefixes,
      const std::string& intent,
      GetCapabilitiesResponseCallback callback) override;

 private:
  // The request sender responsible for communicating with a remote endpoint.
  std::unique_ptr<ServiceRequestSender> request_sender_;
  // The RPC endpoint to send requests to.
  GURL script_server_url_;
  // The client's country code.
  std::string country_code_;
  // The client's locale.
  std::string locale_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_IMPL_H_
