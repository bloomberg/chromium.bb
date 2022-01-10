// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/web_contents_observer.h"

namespace payments {

struct SecurePaymentConfirmationCredential;

class SecurePaymentConfirmationAppFactory
    : public PaymentAppFactory,
      public WebDataServiceConsumer,
      public content::WebContentsObserver {
 public:
  SecurePaymentConfirmationAppFactory();
  ~SecurePaymentConfirmationAppFactory() override;

  SecurePaymentConfirmationAppFactory(
      const SecurePaymentConfirmationAppFactory& other) = delete;
  SecurePaymentConfirmationAppFactory& operator=(
      const SecurePaymentConfirmationAppFactory& other) = delete;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override;

 private:
  struct Request;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void OnIsUserVerifyingPlatformAuthenticatorAvailable(
      std::unique_ptr<Request> request,
      bool is_available);

  void OnAppIcon(
      std::unique_ptr<SecurePaymentConfirmationCredential> credential,
      std::unique_ptr<Request> request,
      const SkBitmap& icon);

  // Called after downloading the icon whose URL was passed into PaymentRequest
  // API.
  void DidDownloadIcon(
      std::unique_ptr<SecurePaymentConfirmationCredential> credential,
      std::unique_ptr<Request> request,
      int request_id,
      int unused_http_status_code,
      const GURL& unused_image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& unused_sizes);

  std::map<WebDataServiceBase::Handle, std::unique_ptr<Request>> requests_;
  base::WeakPtrFactory<SecurePaymentConfirmationAppFactory> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_FACTORY_H_
