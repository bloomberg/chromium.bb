// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"

class GURL;

namespace client_hints {

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate {
 public:
  explicit ClientHints(content::BrowserContext* context);
  ~ClientHints() override;

  // content::ClientHintsControllerDelegate:
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  std::string GetAcceptLanguageString() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

 private:
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ClientHints);
};

}  // namespace client_hints

#endif  // CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
