// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

// The WebUI for chrome://domain-reliability-internals
class DomainReliabilityInternalsUI : public content::WebUIController {
 public:
  explicit DomainReliabilityInternalsUI(content::WebUI* web_ui);

  DomainReliabilityInternalsUI(const DomainReliabilityInternalsUI&) = delete;
  DomainReliabilityInternalsUI& operator=(const DomainReliabilityInternalsUI&) =
      delete;

  ~DomainReliabilityInternalsUI() override;
};

class DomainReliabilityInternalsHandler : public content::WebUIMessageHandler {
 public:
  DomainReliabilityInternalsHandler();

  DomainReliabilityInternalsHandler(const DomainReliabilityInternalsHandler&) =
      delete;
  DomainReliabilityInternalsHandler& operator=(
      const DomainReliabilityInternalsHandler&) = delete;

  ~DomainReliabilityInternalsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleUpdateData(const base::ListValue* args);
  void OnDataUpdated(base::Value data);

  std::string callback_id_;
  base::WeakPtrFactory<DomainReliabilityInternalsHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOMAIN_RELIABILITY_INTERNALS_UI_H_
