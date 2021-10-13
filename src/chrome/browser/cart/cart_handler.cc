// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_features.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "components/search/ntp_features.h"

CartHandler::CartHandler(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartHandler::~CartHandler() = default;

void CartHandler::GetMerchantCarts(GetMerchantCartsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule));
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    cart_service_->LoadCartsWithFakeData(
        base::BindOnce(&CartHandler::GetCartDataCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    cart_service_->LoadAllActiveCarts(
        base::BindOnce(&CartHandler::GetCartDataCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void CartHandler::HideCartModule() {
  cart_service_->Hide();
}

void CartHandler::RestoreHiddenCartModule() {
  cart_service_->RestoreHidden();
}

void CartHandler::HideCart(const GURL& cart_url, HideCartCallback callback) {
  cart_service_->HideCart(cart_url, std::move(callback));
}

void CartHandler::RestoreHiddenCart(const GURL& cart_url,
                                    RestoreHiddenCartCallback callback) {
  cart_service_->RestoreHiddenCart(cart_url, std::move(callback));
}

void CartHandler::RemoveCart(const GURL& cart_url,
                             RemoveCartCallback callback) {
  cart_service_->RemoveCart(cart_url, std::move(callback));
}

void CartHandler::RestoreRemovedCart(const GURL& cart_url,
                                     RestoreRemovedCartCallback callback) {
  cart_service_->RestoreRemovedCart(cart_url, std::move(callback));
}

void CartHandler::GetCartDataCallback(GetMerchantCartsCallback callback,
                                      bool success,
                                      std::vector<CartDB::KeyAndValue> res) {
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  bool show_discount = cart_service_->IsCartDiscountEnabled();
  for (CartDB::KeyAndValue proto_pair : res) {
    auto cart = chrome_cart::mojom::MerchantCart::New();
    cart->merchant = std::move(proto_pair.second.merchant());

    if (cart_features::IsRuleDiscountPartnerMerchant(
            GURL(proto_pair.second.merchant_cart_url()))) {
      cart->cart_url = CartService::AppendUTM(
          GURL(std::move(proto_pair.second.merchant_cart_url())),
          show_discount);
    } else {
      cart->cart_url = GURL(std::move(proto_pair.second.merchant_cart_url()));
    }

    std::vector<std::string> image_urls;
    // Not show product images when showing welcome surface.
    if (!cart_service_->ShouldShowWelcomeSurface()) {
      for (std::string image_url : proto_pair.second.product_image_urls()) {
        cart->product_image_urls.emplace_back(std::move(image_url));
      }
      if (show_discount &&
          (proto_pair.second.discount_info().rule_discount_info_size() > 0 ||
           proto_pair.second.discount_info().has_coupons())) {
        cart->discount_text =
            std::move(proto_pair.second.discount_info().discount_text());
      }
    }
    carts.push_back(std::move(cart));
  }
  if (carts.size() > 0) {
    cart_service_->IncreaseWelcomeSurfaceCounter();
  }
  std::move(callback).Run(std::move(carts));
}

void CartHandler::GetWarmWelcomeVisible(
    GetWarmWelcomeVisibleCallback callback) {
  std::move(callback).Run(cart_service_->ShouldShowWelcomeSurface());
}

void CartHandler::GetDiscountURL(const GURL& cart_url,
                                 GetDiscountURLCallback callback) {
  cart_service_->GetDiscountURL(cart_url, std::move(callback));
}

void CartHandler::GetDiscountConsentCardVisible(
    GetDiscountConsentCardVisibleCallback callback) {
  cart_service_->ShouldShowDiscountConsent(std::move(callback));
}

void CartHandler::OnDiscountConsentAcknowledged(bool accept) {
  cart_service_->AcknowledgeDiscountConsent(accept);
}

void CartHandler::GetDiscountEnabled(GetDiscountEnabledCallback callback) {
  std::move(callback).Run(cart_service_->IsCartDiscountEnabled());
}

void CartHandler::SetDiscountEnabled(bool enabled) {
  cart_service_->SetCartDiscountEnabled(enabled);
}

void CartHandler::PrepareForNavigation(const GURL& cart_url,
                                       bool is_navigating) {
  cart_service_->PrepareForNavigation(cart_url, is_navigating);
}
