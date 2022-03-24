// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/commerce_hint_service.h"

#include <map>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_features.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search/ntp_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cart {

namespace {
// TODO(crbug/1164236): support multiple cart systems in the same domain.
// Returns eTLB+1 domain.
std::string GetDomain(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

void ConstructCartProto(cart_db::ChromeCartContentProto* proto,
                        const GURL& navigation_url,
                        std::vector<mojom::ProductPtr> products) {
  const std::string& domain = GetDomain(navigation_url);
  proto->set_key(domain);
  proto->set_merchant(domain);
  proto->set_merchant_cart_url(navigation_url.spec());
  proto->set_timestamp(base::Time::Now().ToDoubleT());
  for (auto& product : products) {
    if (product->image_url.spec().size() != 0) {
      proto->add_product_image_urls(product->image_url.spec());
    }
    if (!product->product_id.empty()) {
      cart_db::ChromeCartProductProto product_proto;
      product_proto.set_product_id(std::move(product->product_id));
      cart_db::ChromeCartProductProto* added_product =
          proto->add_product_infos();
      *added_product = std::move(product_proto);
    }
  }
}

}  // namespace

// Implementation of the Mojo CommerceHintObserver. This is called by the
// renderer to notify the browser that a commerce hint happens.
class CommerceHintObserverImpl
    : public content::DocumentService<mojom::CommerceHintObserver> {
 public:
  explicit CommerceHintObserverImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::CommerceHintObserver> receiver,
      base::WeakPtr<CommerceHintService> service)
      : DocumentService(render_frame_host, std::move(receiver)),
        binding_url_(render_frame_host->GetLastCommittedURL()),
        service_(std::move(service)) {}

  ~CommerceHintObserverImpl() override = default;

  void OnAddToCart(const absl::optional<GURL>& cart_url,
                   const std::string& product_id) override {
    DVLOG(1) << "Received OnAddToCart in the browser process on "
             << binding_url_;
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnAddToCart(binding_url_, cart_url, product_id);
  }

  void OnVisitCart() override {
    DVLOG(1) << "Received OnVisitCart in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnAddToCart(binding_url_, binding_url_);
  }

  void OnCartProductUpdated(std::vector<mojom::ProductPtr> products) override {
    DVLOG(1) << "Received OnCartProductUpdated in the browser process, with "
             << products.size() << " product(s).";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;

    if (products.empty()) {
      service_->OnRemoveCart(binding_url_);
    } else {
      service_->OnCartUpdated(binding_url_, std::move(products));
    }
  }

  void OnVisitCheckout() override {
    DVLOG(1) << "Received OnVisitCheckout in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnRemoveCart(binding_url_);
  }

  void OnPurchase() override {
    DVLOG(1) << "Received OnPurchase in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnRemoveCart(binding_url_);
  }

  void OnFormSubmit(bool is_purchase) override {
    DVLOG(1) << "Received OnFormSubmit in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnFormSubmit(binding_url_, is_purchase);
  }

  void OnWillSendRequest(bool is_addtocart) override {
    DVLOG(1) << "Received OnWillSendRequest in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnWillSendRequest(binding_url_, is_addtocart);
  }

  void OnNavigation(const GURL& url, OnNavigationCallback callback) override {
    std::move(callback).Run(service_->ShouldSkip(url));
  }

 private:
  GURL binding_url_;
  base::WeakPtr<CommerceHintService> service_;
};

CommerceHintService::CommerceHintService(content::WebContents* web_contents)
    : content::WebContentsUserData<CommerceHintService>(*web_contents) {
  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  service_ = CartServiceFactory::GetInstance()->GetForProfile(profile);
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::SHOPPING_PAGE_PREDICTOR});
  }
}

CommerceHintService::~CommerceHintService() = default;

content::WebContents* CommerceHintService::WebContents() {
  return &GetWebContents();
}

void CommerceHintService::BindCommerceHintObserver(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<mojom::CommerceHintObserver> receiver) {
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  new CommerceHintObserverImpl(host, std::move(receiver),
                               weak_factory_.GetWeakPtr());
}

bool CommerceHintService::ShouldSkip(const GURL& url) {
  if (!optimization_guide_decider_) {
    return false;
  }

  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, &metadata);
  DVLOG(1) << "SHOPPING_PAGE_PREDICTOR = " << static_cast<int>(decision);
  return optimization_guide::OptimizationGuideDecision::kFalse == decision;
}

void CommerceHintService::OnAddToCart(const GURL& navigation_url,
                                      const absl::optional<GURL>& cart_url,
                                      const std::string& product_id) {
  if (ShouldSkip(navigation_url))
    return;
  absl::optional<GURL> validated_cart = cart_url;
  if (cart_url && GetDomain(*cart_url) != GetDomain(navigation_url)) {
    DVLOG(1) << "Reject cart URL with different eTLD+1 domain.";
    validated_cart = absl::nullopt;
  }
  // When rule-based discount is enabled, do not accept cart page URLs from
  // partner merchants as there could be things like discount tokens in them.
  if (service_->IsCartDiscountEnabled() &&
      cart_features::IsRuleDiscountPartnerMerchant(navigation_url) &&
      product_id.empty()) {
    validated_cart = absl::nullopt;
  }
  cart_db::ChromeCartContentProto proto;
  std::vector<mojom::ProductPtr> products;
  if (!product_id.empty()) {
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->product_id = product_id;
    products.push_back(std::move(product_ptr));
  }
  ConstructCartProto(&proto, navigation_url, std::move(products));
  service_->AddCart(GetDomain(navigation_url), validated_cart,
                    std::move(proto));
}

void CommerceHintService::OnRemoveCart(const GURL& url) {
  service_->DeleteCart(url, false);
}

void CommerceHintService::OnCartUpdated(
    const GURL& cart_url,
    std::vector<mojom::ProductPtr> products) {
  if (ShouldSkip(cart_url))
    return;
  absl::optional<GURL> validated_cart = cart_url;
  // When rule-based discount is enabled, do not accept cart page URLs from
  // partner merchants as there could be things like discount tokens in them.
  if (service_->IsCartDiscountEnabled() &&
      cart_features::IsRuleDiscountPartnerMerchant(cart_url)) {
    validated_cart = absl::nullopt;
  }
  cart_db::ChromeCartContentProto proto;
  ConstructCartProto(&proto, cart_url, std::move(products));
  service_->AddCart(proto.key(), validated_cart, std::move(proto));
}

void CommerceHintService::OnFormSubmit(const GURL& navigation_url,
                                       bool is_purchase) {
  if (ShouldSkip(navigation_url))
    return;
  uint8_t bytes[1];
  crypto::RandBytes(bytes);
  bool report_truth = bytes[0] & 0x1;
  bool random = (bytes[0] >> 1) & 0x1;
  bool reported = report_truth ? is_purchase : random;
  ukm::builders::Shopping_FormSubmitted(
      ukm::GetSourceIdForWebContentsDocument(&GetWebContents()))
      .SetIsTransaction(reported)
      .Record(ukm::UkmRecorder::Get());
  base::UmaHistogramBoolean("Commerce.Carts.FormSubmitIsTransaction", reported);
}

void CommerceHintService::OnWillSendRequest(const GURL& navigation_url,
                                            bool is_addtocart) {
  if (ShouldSkip(navigation_url))
    return;
  uint8_t bytes[1];
  crypto::RandBytes(bytes);
  bool report_truth = bytes[0] & 0x1;
  bool random = (bytes[0] >> 1) & 0x1;
  bool reported = report_truth ? is_addtocart : random;
  ukm::builders::Shopping_WillSendRequest(
      ukm::GetSourceIdForWebContentsDocument(&GetWebContents()))
      .SetIsAddToCart(reported)
      .Record(ukm::UkmRecorder::Get());
  base::UmaHistogramBoolean("Commerce.Carts.XHRIsAddToCart", reported);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService);

}  // namespace cart
