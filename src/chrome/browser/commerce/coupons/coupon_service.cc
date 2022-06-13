// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/cart/cart_features.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/commerce/coupons/coupon_db_content.pb.h"

namespace {

void ConstructCouponProto(
    const GURL& origin,
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>& offers,
    const CouponService::CouponDisplayTimeMap& coupon_time_map,
    coupon_db::CouponContentProto* proto) {
  proto->set_key(origin.spec());
  for (const auto& offer : offers) {
    coupon_db::FreeListingCouponInfoProto* coupon_info_proto =
        proto->add_free_listing_coupons();
    coupon_info_proto->set_coupon_description(
        offer->display_strings.value_prop_text);
    coupon_info_proto->set_coupon_code(offer->promo_code);
    coupon_info_proto->set_coupon_id(offer->offer_id);
    coupon_info_proto->set_expiry_time(offer->expiry.ToDoubleT());
    std::pair<GURL, int64_t> key({origin, offer->offer_id});
    if (coupon_time_map.find(key) != coupon_time_map.end()) {
      coupon_info_proto->set_last_display_time(
          coupon_time_map.at(key).ToJavaTime());
    } else {
      // Unknown last display time; set to zero so the reminder bubble will
      // always appear.
      coupon_info_proto->set_last_display_time(base::Time().ToJavaTime());
    }
  }
}

bool CompareCouponList(
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>&
        coupon_list_a,
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>&
        coupon_list_b) {
  return std::equal(coupon_list_a.begin(), coupon_list_a.end(),
                    coupon_list_b.begin(),
                    [](const auto& coupon_a, const auto& coupon_b) {
                      return *coupon_a == *coupon_b;
                    });
}

}  // namespace

CouponService::CouponService(std::unique_ptr<CouponDB> coupon_db)
    : coupon_db_(std::move(coupon_db)) {
  InitializeCouponsMap();
}
CouponService::~CouponService() = default;

void CouponService::UpdateFreeListingCoupons(const CouponsMap& coupon_map) {
  if (!features_enabled_)
    return;
  // Identify origins whose coupon has changed in the new data.
  std::vector<GURL> invalid_coupon_origins;
  for (const auto& entry : coupon_map_) {
    const GURL& origin = entry.first;
    if (!coupon_map.contains(origin) ||
        !CompareCouponList(coupon_map.at(origin), coupon_map_.at(origin))) {
      invalid_coupon_origins.emplace_back(origin);
    }
  }
  for (const GURL& origin : invalid_coupon_origins) {
    NotifyObserversOfInvalidatedCoupon(origin);
    coupon_map_.erase(origin);
  }
  coupon_db_->DeleteAllCoupons();
  CouponDisplayTimeMap new_time_map;
  for (const auto& entry : coupon_map) {
    const GURL& origin(entry.first.DeprecatedGetOriginAsURL());
    for (const auto& coupon : entry.second) {
      if (!coupon_map_.contains(origin)) {
        auto new_coupon =
            std::make_unique<autofill::AutofillOfferData>(*coupon);
        coupon_map_[origin].emplace_back(std::move(new_coupon));
      }
      new_time_map[{origin, coupon->offer_id}] =
          coupon_time_map_[{origin, coupon->offer_id}];
    }
    coupon_db::CouponContentProto proto;
    ConstructCouponProto(origin, entry.second, coupon_time_map_, &proto);
    coupon_db_->AddCoupon(origin, proto);
  }
  coupon_time_map_ = new_time_map;
}

void CouponService::DeleteFreeListingCouponsForUrl(const GURL& url) {
  if (!url.is_valid())
    return;
  const GURL& origin(url.DeprecatedGetOriginAsURL());
  NotifyObserversOfInvalidatedCoupon(origin);
  coupon_map_.erase(origin);
  coupon_db_->DeleteCoupon(origin);
}

void CouponService::DeleteAllFreeListingCoupons() {
  for (const auto& entry : coupon_map_) {
    NotifyObserversOfInvalidatedCoupon(entry.first);
  }
  coupon_map_.clear();
  coupon_db_->DeleteAllCoupons();
}

base::Time CouponService::GetCouponDisplayTimestamp(
    const autofill::AutofillOfferData& offer) {
  DCHECK_EQ(offer.GetOfferType(),
            autofill::AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER);
  for (auto origin : offer.merchant_origins) {
    auto iter = coupon_time_map_.find(std::make_pair(origin, offer.offer_id));
    if (iter != coupon_time_map_.end())
      return iter->second;
  }
  return base::Time();
}

void CouponService::RecordCouponDisplayTimestamp(
    const autofill::AutofillOfferData& offer) {
  DCHECK_EQ(offer.GetOfferType(),
            autofill::AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER);
  base::Time timestamp = base::Time::Now();
  for (auto origin : offer.merchant_origins) {
    auto iter = coupon_time_map_.find(std::make_pair(origin, offer.offer_id));
    if (iter != coupon_time_map_.end()) {
      iter->second = timestamp;
      coupon_db_->LoadCoupon(
          origin, base::BindOnce(&CouponService::OnUpdateCouponTimestamp,
                                 weak_ptr_factory_.GetWeakPtr(), offer.offer_id,
                                 timestamp));
    }
  }
}

void CouponService::MaybeFeatureStatusChanged(bool enabled) {
  enabled &= (commerce::IsCouponWithCodeEnabled() ||
              cart_features::IsFakeDataEnabled());
  if (enabled == features_enabled_)
    return;
  features_enabled_ = enabled;
  if (!enabled)
    DeleteAllFreeListingCoupons();
}

CouponService::Coupons CouponService::GetFreeListingCouponsForUrl(
    const GURL& url) {
  if (!url.is_valid())
    return {};
  const GURL& origin(url.DeprecatedGetOriginAsURL());
  if (coupon_map_.find(origin) == coupon_map_.end()) {
    return {};
  }
  Coupons result;
  result.reserve(coupon_map_.at(origin).size());
  for (const auto& data : coupon_map_.at(origin))
    result.emplace_back(data.get());
  return result;
}

bool CouponService::IsUrlEligible(const GURL& url) {
  if (!url.is_valid())
    return false;
  return coupon_map_.find(url.DeprecatedGetOriginAsURL()) != coupon_map_.end();
}

void CouponService::AddObserver(CouponServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void CouponService::RemoveObserver(CouponServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

CouponService::CouponService() = default;

void CouponService::InitializeCouponsMap() {
  coupon_db_->LoadAllCoupons(base::BindOnce(
      &CouponService::OnInitializeCouponsMap, weak_ptr_factory_.GetWeakPtr()));
}

void CouponService::OnInitializeCouponsMap(
    bool success,
    std::vector<CouponDB::KeyAndValue> proto_pairs) {
  DCHECK(success);
  for (auto pair : proto_pairs) {
    const GURL origin(GURL(pair.first));
    for (auto coupon : pair.second.free_listing_coupons()) {
      auto offer = std::make_unique<autofill::AutofillOfferData>();
      offer->display_strings.value_prop_text = coupon.coupon_description();
      offer->promo_code = coupon.coupon_code();
      offer->merchant_origins.emplace_back(origin);
      offer->offer_id = coupon.coupon_id();
      offer->expiry = base::Time::FromDoubleT(coupon.expiry_time());
      coupon_map_[origin].emplace_back(std::move(offer));
      coupon_time_map_[{origin, coupon.coupon_id()}] =
          base::Time::FromJavaTime(coupon.last_display_time());
    }
  }
}

void CouponService::OnUpdateCouponTimestamp(
    int64_t coupon_id,
    const base::Time last_display_timestamp,
    bool success,
    std::vector<CouponDB::KeyAndValue> proto_pairs) {
  DCHECK(success);
  if (proto_pairs.empty())
    return;
  coupon_db::CouponContentProto proto = proto_pairs[0].second;
  for (int i = 0; i < proto.free_listing_coupons_size(); ++i) {
    if (proto.free_listing_coupons()[i].coupon_id() != coupon_id)
      continue;
    coupon_db::FreeListingCouponInfoProto* coupon_proto =
        proto.mutable_free_listing_coupons(i);
    coupon_proto->set_last_display_time(last_display_timestamp.ToJavaTime());
    coupon_db_->AddCoupon(GURL(proto_pairs[0].first), proto);
    return;
  }
}

CouponDB* CouponService::GetDB() {
  return coupon_db_.get();
}

void CouponService::NotifyObserversOfInvalidatedCoupon(const GURL& url) {
  for (const auto& offer : coupon_map_[url]) {
    for (CouponServiceObserver& observer : observers_) {
      observer.OnCouponInvalidated(*offer);
    }
  }
}
