// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_PAY_ARC_PAYMENT_APP_BRIDGE_H_
#define COMPONENTS_ARC_PAY_ARC_PAYMENT_APP_BRIDGE_H_

#include <string>

#include "base/callback_forward.h"
#include "components/arc/mojom/payment_app.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Invokes the TWA payment app.
class ArcPaymentAppBridge : public KeyedService {
 public:
  using IsPaymentImplementedCallback =
      base::OnceCallback<void(mojom::IsPaymentImplementedResultPtr)>;
  using IsReadyToPayCallback =
      base::OnceCallback<void(mojom::IsReadyToPayResultPtr)>;
  using InvokePaymentAppCallback =
      base::OnceCallback<void(mojom::InvokePaymentAppResultPtr)>;

  // Returns the instance owned by the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcPaymentAppBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Used only in testing with a TestBrowserContext.
  static ArcPaymentAppBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcPaymentAppBridge(content::BrowserContext* browser_context,
                      ArcBridgeService* bridge_service);
  ~ArcPaymentAppBridge() override;

  // Disallow copy and assign.
  ArcPaymentAppBridge(const ArcPaymentAppBridge& other) = delete;
  ArcPaymentAppBridge& operator=(const ArcPaymentAppBridge& other) = delete;

  // Checks whether the TWA has the ability to perform payments. May be invoked
  // when off the record, e.g., incognito mode or guest mode.
  void IsPaymentImplemented(const std::string& package_name,
                            IsPaymentImplementedCallback callback);

  // Queries the TWA payment app whether it is able to perform a payment. Should
  // not be invoked when off the record, e.g., incognito mode or guest mode.
  void IsReadyToPay(mojom::PaymentParametersPtr parameters,
                    IsReadyToPayCallback callback);

  // Invokes the TWA payment app flow.
  void InvokePaymentApp(mojom::PaymentParametersPtr parameters,
                        InvokePaymentAppCallback callback);

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // COMPONENTS_ARC_PAY_ARC_PAYMENT_APP_BRIDGE_H_
