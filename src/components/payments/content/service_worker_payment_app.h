// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/web_app_manifest.h"
#include "components/payments/core/payment_app.h"
#include "content/public/browser/stored_payment_app.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace payments {

class PaymentRequestDelegate;

// Represents a service worker based payment app.
class ServiceWorkerPaymentApp : public PaymentApp {
 public:
  using IdentityCallback =
      base::RepeatingCallback<void(const url::Origin&, int64_t)>;

  // This constructor is used for a payment app that has been installed in
  // Chrome.
  ServiceWorkerPaymentApp(
      content::BrowserContext* browser_context,
      const GURL& top_origin,
      const GURL& frame_origin,
      const PaymentRequestSpec* spec,
      std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info,
      PaymentRequestDelegate* payment_request_delegate,
      const IdentityCallback& identity_callback);

  // This constructor is used for a payment app that has not been installed in
  // Chrome but can be installed when paying with it.
  ServiceWorkerPaymentApp(
      content::WebContents* web_contents,
      const GURL& top_origin,
      const GURL& frame_origin,
      const PaymentRequestSpec* spec,
      std::unique_ptr<WebAppInstallationInfo> installable_payment_app_info,
      const std::string& enabled_methods,
      PaymentRequestDelegate* payment_request_delegate,
      const IdentityCallback& identity_callback);
  ~ServiceWorkerPaymentApp() override;

  // The callback for ValidateCanMakePayment.
  // The first return value is a pointer point to the corresponding
  // ServiceWorkerPaymentApp of the result. The second return value is
  // the result.
  using ValidateCanMakePaymentCallback =
      base::OnceCallback<void(ServiceWorkerPaymentApp*, bool)>;

  // Validates whether this payment app can be used for this payment request. It
  // fires CanMakePaymentEvent to the payment app to do validation. The result
  // is returned through callback. If the returned result is false, then this
  // app should not be used for this payment request. This interface must be
  // called before any other interfaces in this class.
  void ValidateCanMakePayment(ValidateCanMakePaymentCallback callback);

  // PaymentApp:
  void InvokePaymentApp(Delegate* delegate) override;
  void OnPaymentAppWindowClosed() override;
  bool IsCompleteForPayment() const override;
  uint32_t GetCompletenessScore() const override;
  bool CanPreselect() const override;
  base::string16 GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;
  bool IsValidForModifier(
      const std::string& method,
      bool supported_networks_specified,
      const std::set<std::string>& supported_networks) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  gfx::ImageSkia icon_image_skia() const override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;
  ukm::SourceId UkmSourceId() override;

  void set_payment_handler_host(
      mojo::PendingRemote<mojom::PaymentHandlerHost> payment_handler_host) {
    payment_handler_host_ = std::move(payment_handler_host);
  }

 private:
  friend class ServiceWorkerPaymentAppTest;

  void OnPaymentAppInvoked(mojom::PaymentHandlerResponsePtr response);
  mojom::PaymentRequestEventDataPtr CreatePaymentRequestEventData();

  mojom::CanMakePaymentEventDataPtr CreateCanMakePaymentEventData();
  void OnCanMakePaymentEventSkipped(ValidateCanMakePaymentCallback callback);
  void OnCanMakePaymentEventResponded(
      ValidateCanMakePaymentCallback callback,
      mojom::CanMakePaymentResponsePtr response);

  // Called from two places:
  // 1) From PaymentAppProvider after a just-in-time installable payment handler
  //    has been installed.
  // 2) From this class when an already installed payment handler is about to be
  //    invoked.
  void OnPaymentAppIdentity(const url::Origin& origin, int64_t registration_id);

  content::BrowserContext* browser_context_;
  GURL top_origin_;
  GURL frame_origin_;
  const PaymentRequestSpec* spec_;
  std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info_;
  gfx::ImageSkia icon_image_;

  // Weak pointer is fine here since the owner of this object is
  // PaymentRequestState which also owns PaymentResponseHelper.
  Delegate* delegate_;

  // Weak pointer that must outlive this object.
  PaymentRequestDelegate* payment_request_delegate_;

  // The callback that is notified of service worker registration identifier
  // after the service worker is installed.
  IdentityCallback identity_callback_;

  mojo::PendingRemote<mojom::PaymentHandlerHost> payment_handler_host_;

  // PaymentAppProvider::CanMakePayment result of this payment app.
  bool can_make_payment_result_;
  bool has_enrolled_instrument_result_;

  // Below variables are used for installable ServiceWorkerPaymentApp
  // specifically.
  bool needs_installation_;
  content::WebContents* web_contents_;
  std::unique_ptr<WebAppInstallationInfo> installable_web_app_info_;
  std::string installable_enabled_method_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  base::WeakPtrFactory<ServiceWorkerPaymentApp> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentApp);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_H_
