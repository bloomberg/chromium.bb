// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"

#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_discount_metric_collector.h"
#include "chrome/browser/cart/cart_features.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
constexpr char kFakeDataPrefix[] = "Fake:";
constexpr char kNoRbdUtmTag[] = "chrome_cart_no_rbd";
constexpr char kRbdUtmTag[] = "chrome_cart_rbd";
constexpr char kCartPrefsKey[] = "chrome_cart";

constexpr base::FeatureParam<std::string> kSkipCartExtractionPattern{
    &ntp_features::kNtpChromeCartModule, "skip-cart-extraction-pattern",
    // This regex does not match anything.
    "\\b\\B"};

constexpr base::FeatureParam<bool> kRbdUtmParam{
    &ntp_features::kNtpChromeCartModule,
    ntp_features::kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam, false};

constexpr base::FeatureParam<bool> kBypassDisocuntFetchingThreshold{
    &commerce::kCommerceDeveloper, "bypass-discount-fetching-threshold", false};

std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsFakeDataEnabled() {
  return base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleDataParam) == "fake";
}

std::string GetKeyForURL(const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  return IsFakeDataEnabled() ? std::string(kFakeDataPrefix) + domain : domain;
}

bool CompareTimeStampForProtoPair(const CartDB::KeyAndValue pair1,
                                  CartDB::KeyAndValue pair2) {
  return pair1.second.timestamp() > pair2.second.timestamp();
}

absl::optional<base::Value> JSONToDictionary(int resource_id) {
  base::StringPiece json_resource(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  absl::optional<base::Value> value = base::JSONReader::Read(json_resource);
  DCHECK(value && value.has_value() && value->is_dict());
  return value;
}

bool IsExpired(const cart_db::ChromeCartContentProto& proto) {
  return (base::Time::Now() - base::Time::FromDoubleT(proto.timestamp()))
             .InDays() > 14;
}

const re2::RE2& GetSkipCartExtractionPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kSkipCartExtractionPattern.Get(),
                                               options);
  return *instance;
}

// Check if any product in existing_proto is no longer in new_proto.
bool ProductsRemoved(cart_db::ChromeCartContentProto existing_proto,
                     cart_db::ChromeCartContentProto new_proto) {
  if (existing_proto.product_infos_size() == 0)
    return false;
  if (existing_proto.product_infos_size() > new_proto.product_infos_size())
    return true;
  std::set<std::string> new_proto_product_ids;
  for (auto new_product : new_proto.product_infos()) {
    new_proto_product_ids.insert(new_product.product_id());
  }
  for (auto existing_product : existing_proto.product_infos()) {
    bool product_remains =
        new_proto_product_ids.find(existing_product.product_id()) !=
        new_proto_product_ids.end();
    if (!product_remains)
      return true;
  }
  return false;
}
}  // namespace

CartService::CartService(Profile* profile)
    : profile_(profile),
      cart_db_(std::make_unique<CartDB>(profile_)),
      domain_name_mapping_(JSONToDictionary(IDR_CART_DOMAIN_NAME_MAPPING_JSON)),
      domain_cart_url_mapping_(
          JSONToDictionary(IDR_CART_DOMAIN_CART_URL_MAPPING_JSON)),
      discount_link_fetcher_(std::make_unique<CartDiscountLinkFetcher>()),
      metrics_tracker_(std::make_unique<CartMetricsTracker>(
          chrome::FindTabbedBrowser(profile, false))),
      coupon_service_(CouponServiceFactory::GetForProfile(profile)) {
  history_service_observation_.Observe(HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS));
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    AddCartsWithFakeData();
  } else {
    // In case last deconstruction is interrupted and fake data is not deleted.
    DeleteCartsWithFakeData();
  }

  if (IsCartDiscountEnabled()) {
    StartGettingDiscount();
  }
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::SHOPPING_PAGE_PREDICTOR});
  }

  pref_change_registrar_.Init(profile->GetPrefs());
  auto callback = base::BindRepeating(&CartService::OnCartFeaturesChanged,
                                      weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Add(prefs::kNtpDisabledModules, callback);
  pref_change_registrar_.Add(prefs::kCartDiscountEnabled, callback);
  coupon_service_->MaybeFeatureStatusChanged(IsCartAndDiscountEnabled());
}

CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleHidden, false);
  registry->RegisterIntegerPref(prefs::kCartModuleWelcomeSurfaceShownTimes, 0);
  registry->RegisterBooleanPref(prefs::kCartDiscountAcknowledged, false);
  registry->RegisterBooleanPref(prefs::kCartDiscountEnabled, false);
  registry->RegisterDictionaryPref(prefs::kCartUsedDiscounts);
  registry->RegisterTimePref(prefs::kCartDiscountLastFetchedTime, base::Time());
}

GURL CartService::AppendUTM(const GURL& base_url, bool is_discount_enabled) {
  DCHECK(base_url.is_valid() &&
         cart_features::IsRuleDiscountPartnerMerchant(base_url));

  if (kRbdUtmParam.Get()) {
    return net::AppendOrReplaceQueryParameter(
        base_url, "utm_source",
        is_discount_enabled ? kRbdUtmTag : kNoRbdUtmTag);
  } else {
    return base_url;
  }
}

void CartService::Hide() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, true);
}

void CartService::RestoreHidden() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, false);
}

bool CartService::IsHidden() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleHidden);
}

void CartService::LoadCart(const std::string& domain,
                           CartDB::LoadCallback callback) {
  cart_db_->LoadCart(domain, std::move(callback));
}

void CartService::LoadAllActiveCarts(CartDB::LoadCallback callback) {
  cart_db_->LoadAllCarts(base::BindOnce(&CartService::OnLoadCarts,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void CartService::AddCart(const std::string& domain,
                          const absl::optional<GURL>& cart_url,
                          const cart_db::ChromeCartContentProto& proto) {
  cart_db_->LoadCart(domain, base::BindOnce(&CartService::OnAddCart,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            domain, cart_url, proto));
}

void CartService::DeleteCart(const GURL& url, bool ignore_remove_status) {
  coupon_service_->DeleteFreeListingCouponsForUrl(url);
  if (ignore_remove_status) {
    cart_db_->DeleteCart(eTLDPlusOne(url),
                         base::BindOnce(&CartService::OnOperationFinished,
                                        weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  cart_db_->LoadCart(eTLDPlusOne(url),
                     base::BindOnce(&CartService::OnDeleteCart,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CartService::HideCart(const GURL& cart_url,
                           CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartHiddenStatus,
                                    weak_ptr_factory_.GetWeakPtr(), true,
                                    std::move(callback)));
}

void CartService::RestoreHiddenCart(const GURL& cart_url,
                                    CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartHiddenStatus,
                                    weak_ptr_factory_.GetWeakPtr(), false,
                                    std::move(callback)));
}

void CartService::RemoveCart(const GURL& cart_url,
                             CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartRemovedStatus,
                                    weak_ptr_factory_.GetWeakPtr(), true,
                                    std::move(callback)));
}

void CartService::RestoreRemovedCart(const GURL& cart_url,
                                     CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartRemovedStatus,
                                    weak_ptr_factory_.GetWeakPtr(), false,
                                    std::move(callback)));
}

void CartService::IncreaseWelcomeSurfaceCounter() {
  if (!ShouldShowWelcomeSurface())
    return;
  int times = profile_->GetPrefs()->GetInteger(
      prefs::kCartModuleWelcomeSurfaceShownTimes);
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   times + 1);
}

bool CartService::ShouldShowWelcomeSurface() {
  return profile_->GetPrefs()->GetInteger(
             prefs::kCartModuleWelcomeSurfaceShownTimes) <
         kWelcomSurfaceShowLimit;
}

void CartService::AcknowledgeDiscountConsent(bool should_enable) {
  if (IsFakeDataEnabled()) {
    return;
  }
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, should_enable);

  if (should_enable &&
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) ==
          "true") {
    StartGettingDiscount();
  }
}

void CartService::ShouldShowDiscountConsent(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (IsFakeDataEnabled()) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE, base::BindOnce(
                                  [](base::OnceCallback<void(bool)> callback) {
                                    std::move(callback).Run(true);
                                  },
                                  std::move(callback)));
    return;
  }
  bool should_not_show =
      ShouldShowWelcomeSurface() ||
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) !=
          "true" ||
      profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged);
  if (should_not_show) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE, base::BindOnce(
                                  [](base::OnceCallback<void(bool)> callback) {
                                    std::move(callback).Run(false);
                                  },
                                  std::move(callback)));
  } else {
    LoadAllActiveCarts(base::BindOnce(&CartService::HasPartnerCarts,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback)));
  }
}

void CartService::HasPartnerCarts(
    base::OnceCallback<void(bool)> callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  for (auto proto_pair : proto_pairs) {
    if (cart_features::IsPartnerMerchant(
            GURL(proto_pair.second.merchant_cart_url()))) {
      std::move(callback).Run(true);
      return;
    }
  }
  std::move(callback).Run(false);
}

bool CartService::IsCartDiscountEnabled() {
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) !=
      "true") {
    return false;
  }
  return profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled);
}

void CartService::SetCartDiscountEnabled(bool enabled) {
  DCHECK(base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) ==
         "true");
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, enabled);

  if (enabled) {
    StartGettingDiscount();
  } else {
    // TODO(crbug.com/1207197): Use sequence checker instead.
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    fetch_discount_worker_.reset();
  }
}

void CartService::GetDiscountURL(
    const GURL& cart_url,
    base::OnceCallback<void(const ::GURL&)> callback) {
  auto url = cart_url;
  if (cart_features::IsRuleDiscountPartnerMerchant(cart_url)) {
    url = AppendUTM(cart_url, IsCartDiscountEnabled());
  }

  if (!cart_features::IsRuleDiscountPartnerMerchant(cart_url) ||
      !IsCartDiscountEnabled()) {
    std::move(callback).Run(url);
    CartDiscountMetricCollector::RecordClickedOnDiscount(false);
    return;
  }
  LoadCart(eTLDPlusOne(cart_url), base::BindOnce(&CartService::OnGetDiscountURL,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 url, std::move(callback)));
}

void CartService::OnGetDiscountURL(
    const GURL& default_cart_url,
    base::OnceCallback<void(const ::GURL&)> callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK_EQ(proto_pairs.size(), 1U);
  if (proto_pairs.size() != 1U) {
    std::move(callback).Run(default_cart_url);
    CartDiscountMetricCollector::RecordClickedOnDiscount(false);
    return;
  }
  auto& cart_proto = proto_pairs[0].second;
  if (!IsCartDiscountEnabled() ||
      cart_proto.discount_info().rule_discount_info().empty()) {
    std::move(callback).Run(default_cart_url);
    CartDiscountMetricCollector::RecordClickedOnDiscount(false);
    return;
  }
  auto pending_factory = profile_->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess()
                             ->Clone();

  discount_link_fetcher_->Fetch(
      std::move(pending_factory), cart_proto,
      base::BindOnce(&CartService::OnDiscountURLFetched,
                     weak_ptr_factory_.GetWeakPtr(), default_cart_url,
                     std::move(callback), cart_proto));

  CartDiscountMetricCollector::RecordClickedOnDiscount(true);
}

void CartService::OnDiscountURLFetched(
    const GURL& default_cart_url,
    base::OnceCallback<void(const ::GURL&)> callback,
    const cart_db::ChromeCartContentProto& cart_proto,
    const GURL& discount_url) {
  std::move(callback).Run(discount_url.is_valid()
                              ? AppendUTM(discount_url, IsCartDiscountEnabled())
                              : default_cart_url);
  if (discount_url.is_valid()) {
    CacheUsedDiscounts(cart_proto);
    CleanUpDiscounts(cart_proto);
    CartDiscountMetricCollector::RecordAppliedDiscount();
  }
}

void CartService::PrepareForNavigation(const GURL& cart_url,
                                       bool is_navigating) {
  metrics_tracker_->PrepareToRecordUKM(cart_url);
  if (is_navigating ||
      !cart_features::IsRuleDiscountPartnerMerchant(cart_url) ||
      !IsCartDiscountEnabled()) {
    return;
  }
  if (!discount_url_loader_) {
    discount_url_loader_ = std::make_unique<DiscountURLLoader>(
        chrome::FindTabbedBrowser(profile_, false), profile_);
  }
  discount_url_loader_->PrepareURLForDiscountLoad(cart_url);
}

void CartService::LoadCartsWithFakeData(CartDB::LoadCallback callback) {
  cart_db_->LoadCartsWithPrefix(
      kFakeDataPrefix,
      base::BindOnce(&CartService::OnLoadCarts, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CartService::OnOperationFinished(bool success) {
  DCHECK(success) << "database operation failed.";
}

void CartService::OnOperationFinishedWithCallback(
    CartDB::OperationCallback callback,
    bool success) {
  DCHECK(success) << "database operation failed.";
  std::move(callback).Run(success);
}

void CartService::Shutdown() {
  history_service_observation_.Reset();
  DeleteCartsWithFakeData();
  // Delete content of all carts that are removed.
  cart_db_->LoadAllCarts(base::BindOnce(&CartService::DeleteRemovedCartsContent,
                                        weak_ptr_factory_.GetWeakPtr()));
  metrics_tracker_->ShutDown();
  if (discount_url_loader_) {
    discount_url_loader_->ShutDown();
  }
}

void CartService::OnURLsDeleted(history::HistoryService* history_service,
                                const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/1157892): Add more fine-grained deletion of cart data when
  // history deletion happens.
  cart_db_->DeleteAllCarts(base::BindOnce(&CartService::OnOperationFinished,
                                          weak_ptr_factory_.GetWeakPtr()));
  coupon_service_->DeleteAllFreeListingCoupons();
}

CartDB* CartService::GetDB() {
  return cart_db_.get();
}

void CartService::AddCartsWithFakeData() {
  DeleteCartsWithFakeData();
  // Polulate and add some carts with fake data.
  double time_now = base::Time::Now().ToDoubleT();
  cart_db::ChromeCartContentProto dummy_proto1;
  GURL dummy_url1 = GURL("https://shopping.google.com");
  dummy_proto1.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url1));
  dummy_proto1.set_merchant("Cart Foo");
  dummy_proto1.set_merchant_cart_url(dummy_url1.spec());
  dummy_proto1.set_timestamp(time_now + 6);
  dummy_proto1.mutable_discount_info()->set_discount_text(
      l10n_util::GetStringFUTF8(IDS_NTP_MODULES_CART_DISCOUNT_CHIP_AMOUNT,
                                u"15%"));
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn0.gstatic.com/"
      "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
      "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
      "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn1.gstatic.com/"
      "shopping?q=tbn:ANd9GcT2ew6Aydzu5VzRV756ORGha6fyjKp_On7iTlr_"
      "tL9vODnlNtFo_xsxj6_lCop-3J0Vk44lHfk-AxoBJDABVHPVFN-"
      "EiWLcZvzkdpHFqcurm7fBVmWtYKo2rg&usqp=CAY");
  cart_db_->AddCart(dummy_proto1.key(), dummy_proto1,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto2;
  GURL dummy_url2 = GURL("https://www.amazon.com/");
  dummy_proto2.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url2));
  dummy_proto2.set_merchant("Cart Bar");
  dummy_proto2.set_merchant_cart_url(dummy_url2.spec());
  dummy_proto2.set_timestamp(time_now + 5);
  dummy_proto2.mutable_discount_info()->set_discount_text(
      l10n_util::GetStringFUTF8(IDS_NTP_MODULES_CART_DISCOUNT_CHIP_AMOUNT,
                                u"20%"));
  dummy_proto2.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  dummy_proto2.add_product_image_urls(
      "https://encrypted-tbn0.gstatic.com/"
      "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
      "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
      "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
  cart_db_->AddCart(dummy_proto2.key(), dummy_proto2,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto3;
  GURL dummy_url3 = GURL("https://www.ebay.com/");
  dummy_proto3.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url3));
  dummy_proto3.set_merchant("Cart Baz");
  dummy_proto3.set_merchant_cart_url(dummy_url3.spec());
  dummy_proto3.set_timestamp(time_now + 4);
  dummy_proto3.mutable_discount_info()->set_discount_text(
      l10n_util::GetStringFUTF8(IDS_NTP_MODULES_CART_DISCOUNT_CHIP_UP_TO_AMOUNT,
                                u"$50"));
  dummy_proto3.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  cart_db_->AddCart(dummy_proto3.key(), dummy_proto3,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto4;
  GURL dummy_url4 = GURL("https://www.walmart.com/");
  dummy_proto4.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url4));
  dummy_proto4.set_merchant("Cart Qux");
  dummy_proto4.set_merchant_cart_url(dummy_url4.spec());
  dummy_proto4.set_timestamp(time_now + 3);
  dummy_proto4.add_product_image_urls(
      "https://encrypted-tbn0.gstatic.com/"
      "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
      "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
      "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
  dummy_proto4.add_product_image_urls(
      "https://encrypted-tbn1.gstatic.com/"
      "shopping?q=tbn:ANd9GcT2ew6Aydzu5VzRV756ORGha6fyjKp_On7iTlr_"
      "tL9vODnlNtFo_xsxj6_lCop-3J0Vk44lHfk-AxoBJDABVHPVFN-"
      "EiWLcZvzkdpHFqcurm7fBVmWtYKo2rg&usqp=CAY");
  cart_db_->AddCart(dummy_proto4.key(), dummy_proto4,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto5;
  GURL dummy_url5 = GURL("https://www.bestbuy.com/");
  dummy_proto5.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url5));
  dummy_proto5.set_merchant("Cart Corge");
  dummy_proto5.set_merchant_cart_url(dummy_url5.spec());
  dummy_proto5.set_timestamp(time_now + 2);
  dummy_proto5.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  cart_db_->AddCart(dummy_proto5.key(), dummy_proto5,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto6;
  GURL dummy_url6 = GURL("https://www.nike.com/");
  dummy_proto6.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url6));
  dummy_proto6.set_merchant("Cart Flob");
  dummy_proto6.set_merchant_cart_url(dummy_url6.spec());
  dummy_proto6.set_timestamp(time_now + 1);
  dummy_proto6.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  dummy_proto6.add_product_image_urls(
      "https://encrypted-tbn0.gstatic.com/"
      "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
      "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
      "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
  dummy_proto6.add_product_image_urls(
      "https://encrypted-tbn1.gstatic.com/"
      "shopping?q=tbn:ANd9GcT2ew6Aydzu5VzRV756ORGha6fyjKp_On7iTlr_"
      "tL9vODnlNtFo_xsxj6_lCop-3J0Vk44lHfk-AxoBJDABVHPVFN-"
      "EiWLcZvzkdpHFqcurm7fBVmWtYKo2rg&usqp=CAY");
  cart_db_->AddCart(dummy_proto6.key(), dummy_proto6,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto7;
  GURL dummy_url7 = GURL("https://www.bestbuy.com/");
  dummy_proto7.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url7));
  dummy_proto7.set_merchant("Cart Gob");
  dummy_proto7.set_merchant_cart_url(dummy_url7.spec());
  dummy_proto7.set_timestamp(time_now + 2);
  cart_db_->AddCart(dummy_proto7.key(), dummy_proto7,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::DeleteCartsWithFakeData() {
  cart_db_->DeleteCartsWithPrefix(
      kFakeDataPrefix, base::BindOnce(&CartService::OnOperationFinished,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void CartService::DeleteRemovedCartsContent(
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  for (CartDB::KeyAndValue proto_pair : proto_pairs) {
    if (proto_pair.second.is_removed()) {
      // Delete removed cart content by overwriting it with an entry with only
      // removed status data.
      cart_db::ChromeCartContentProto empty_proto;
      empty_proto.set_key(proto_pair.first);
      empty_proto.set_is_removed(true);
      cart_db_->AddCart(proto_pair.first, empty_proto,
                        base::BindOnce(&CartService::OnOperationFinished,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

bool CartService::ShouldSkip(const GURL& url) {
  if (!optimization_guide_decider_) {
    return false;
  }
  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, &metadata);
  DVLOG(1) << "SHOPPING_PAGE_PREDICTOR = " << static_cast<int>(decision);
  return optimization_guide::OptimizationGuideDecision::kFalse == decision;
}

void CartService::OnLoadCarts(CartDB::LoadCallback callback,
                              bool success,
                              std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (IsFakeDataEnabled()) {
    std::move(callback).Run(success, std::move(proto_pairs));
    return;
  }
  if (IsHidden()) {
    std::move(callback).Run(success, {});
    return;
  }
  std::set<std::string> merchants_to_erase;
  for (CartDB::KeyAndValue kv : proto_pairs) {
    const GURL& cart_url(GURL(kv.second.merchant_cart_url()));
    if (IsExpired(kv.second) || ShouldSkip(cart_url)) {
      // Removed carts should remain removed.
      if (!kv.second.is_removed()) {
        DeleteCart(cart_url, true);
      }
      merchants_to_erase.emplace(kv.second.key());
    }
  }
  proto_pairs.erase(
      std::remove_if(proto_pairs.begin(), proto_pairs.end(),
                     [merchants_to_erase](CartDB::KeyAndValue kv) {
                       return kv.second.is_hidden() || kv.second.is_removed() ||
                              merchants_to_erase.find(kv.second.key()) !=
                                  merchants_to_erase.end();
                     }),
      proto_pairs.end());
  for (auto proto_pair : proto_pairs) {
    if (RE2::FullMatch(re2::StringPiece(proto_pair.first),
                       GetSkipCartExtractionPattern())) {
      proto_pair.second.clear_product_image_urls();
      cart_db_->AddCart(proto_pair.first, proto_pair.second,
                        base::BindOnce(&CartService::OnOperationFinished,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }
  // Sort items in timestamp descending order.
  std::sort(proto_pairs.begin(), proto_pairs.end(),
            CompareTimeStampForProtoPair);
  std::move(callback).Run(success, std::move(proto_pairs));
}

void CartService::SetCartHiddenStatus(
    bool isHidden,
    CartDB::OperationCallback callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  CartDB::KeyAndValue proto_pair = proto_pairs[0];
  proto_pair.second.set_is_hidden(isHidden);
  cart_db_->AddCart(
      proto_pair.first, proto_pair.second,
      base::BindOnce(&CartService::OnOperationFinishedWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CartService::SetCartRemovedStatus(
    bool isRemoved,
    CartDB::OperationCallback callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  CartDB::KeyAndValue proto_pair = proto_pairs[0];
  proto_pair.second.set_is_removed(isRemoved);
  cart_db_->AddCart(
      proto_pair.first, proto_pair.second,
      base::BindOnce(&CartService::OnOperationFinishedWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CartService::OnAddCart(const std::string& domain,
                            const absl::optional<GURL>& cart_url,
                            cart_db::ChromeCartContentProto proto,
                            bool success,
                            std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  // Restore module visibility anytime a cart-related action happens.
  RestoreHidden();
  std::string* merchant_name = domain_name_mapping_->FindStringKey(domain);
  if (merchant_name) {
    proto.set_merchant(*merchant_name);
  }
  if (cart_url) {
    proto.set_merchant_cart_url(cart_url->spec());
  } else {
    std::string* fallback_url = domain_cart_url_mapping_->FindStringKey(domain);
    if (fallback_url) {
      proto.set_merchant_cart_url(*fallback_url);
    }
  }

  // Skip extracting the block list.
  if (RE2::FullMatch(re2::StringPiece(domain),
                     GetSkipCartExtractionPattern())) {
    proto.clear_product_image_urls();
    proto.clear_product_infos();
    cart_db_->AddCart(domain, std::move(proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (proto_pairs.size() == 0) {
    cart_db_->AddCart(domain, std::move(proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  cart_db::ChromeCartContentProto existing_proto = proto_pairs[0].second;
  // Do nothing for carts that has been explicitly removed.
  if (existing_proto.is_removed()) {
    return;
  }

  bool has_product_image = false;
  // If the new proto has product images, we can copy the product images to the
  // existing proto without worrying about overwriting as it reflects the latest
  // state.
  if (proto.product_image_urls().size()) {
    *(existing_proto.mutable_product_image_urls()) =
        std::move(proto.product_image_urls());
    has_product_image = true;
  }
  existing_proto.set_is_hidden(false);
  existing_proto.set_timestamp(proto.timestamp());
  if (cart_url) {
    existing_proto.set_merchant_cart_url(cart_url->spec());
  }
  // If some products in the existing cart are no longer in the new cart, remove
  // the corresponding coupons.
  if (has_product_image && ProductsRemoved(existing_proto, proto)) {
    coupon_service_->DeleteFreeListingCouponsForUrl(
        GURL(existing_proto.merchant_cart_url()));
  }

  if (proto.product_infos().size()) {
    // If no product images, this addition comes from AddToCart detection and
    // should have only one product (if any). Add this product to the existing
    // cart if not included already.
    if (!has_product_image) {
      DCHECK_EQ(1, proto.product_infos().size());
      auto new_product_info = std::move(proto.product_infos().at(0));
      bool is_included = false;
      for (auto product_proto : existing_proto.product_infos()) {
        is_included |=
            (product_proto.product_id() == new_product_info.product_id());
        if (is_included)
          break;
      }
      if (!is_included) {
        auto* added_product = existing_proto.add_product_infos();
        *added_product = std::move(new_product_info);
      }
    } else {
      *(existing_proto.mutable_product_infos()) =
          std::move(proto.product_infos());
    }
  }
  cart_db_->AddCart(domain, std::move(existing_proto),
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::UpdateDiscounts(const GURL& cart_url,
                                  cart_db::ChromeCartContentProto new_proto,
                                  const bool is_tester) {
  if (!cart_url.is_valid()) {
    VLOG(1) << __func__
            << "update discounts with invalid cart_url: " << cart_url;
    return;
  }

  if (new_proto.has_discount_info() &&
      !new_proto.discount_info().rule_discount_info().empty()) {
    // Filter used rule_based discounts.
    std::vector<cart_db::RuleDiscountInfoProto> rule_discount_info_protos;
    for (const cart_db::RuleDiscountInfoProto& proto :
         new_proto.discount_info().rule_discount_info()) {
      if (is_tester || !IsDiscountUsed(proto.rule_id())) {
        rule_discount_info_protos.emplace_back(proto);
      }
    }
    if (rule_discount_info_protos.empty()) {
      new_proto.clear_discount_info();
    } else {
      *new_proto.mutable_discount_info()->mutable_rule_discount_info() = {
          rule_discount_info_protos.begin(), rule_discount_info_protos.end()};
    }
  }

  std::string domain = eTLDPlusOne(cart_url);
  cart_db_->AddCart(domain, std::move(new_proto),
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::StartGettingDiscount() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(IsCartDiscountEnabled())
      << "Should be called only if the discount feature is enabled.";
  DCHECK(!fetch_discount_worker_)
      << "fetch_discount_worker_ should not be valid at this point.";

  base::Time last_fetched_time =
      profile_->GetPrefs()->GetTime(prefs::kCartDiscountLastFetchedTime);
  base::TimeDelta fetch_delay = cart_features::kDiscountFetchDelayParam.Get() -
                                (base::Time::Now() - last_fetched_time);
  if (last_fetched_time == base::Time() || fetch_delay < base::TimeDelta() ||
      kBypassDisocuntFetchingThreshold.Get()) {
    fetch_delay = base::TimeDelta();
  }

  if (fetch_discount_worker_for_testing_) {
    fetch_discount_worker_for_testing_->Start(fetch_delay);
    return;
  }

  fetch_discount_worker_ = std::make_unique<FetchDiscountWorker>(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      std::make_unique<CartDiscountFetcherFactory>(),
      std::make_unique<CartServiceDelegate>(this),
      IdentityManagerFactory::GetForProfile(profile_),
      profile_->GetVariationsClient());

  fetch_discount_worker_->Start(fetch_delay);
}

bool CartService::IsDiscountUsed(const std::string& rule_id) {
  return profile_->GetPrefs()
             ->GetDictionary(prefs::kCartUsedDiscounts)
             ->FindBoolKey(rule_id) != absl::nullopt;
}

void CartService::RecordFetchTimestamp() {
  profile_->GetPrefs()->SetTime(prefs::kCartDiscountLastFetchedTime,
                                base::Time::Now());
}

void CartService::UpdateFreeListingCoupons(
    const CouponService::CouponsMap& map) {
  coupon_service_->UpdateFreeListingCoupons(map);
}

void CartService::CacheUsedDiscounts(
    const cart_db::ChromeCartContentProto& proto) {
  if (!proto.has_discount_info() ||
      proto.discount_info().rule_discount_info().empty()) {
    VLOG(1) << "Empty rule based discounts, cache nothing";
    return;
  }
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kCartUsedDiscounts);
  for (auto discount_info : proto.discount_info().rule_discount_info()) {
    update->SetBoolKey(discount_info.rule_id(), true);
  }
}

void CartService::CleanUpDiscounts(cart_db::ChromeCartContentProto proto) {
  if (proto.merchant_cart_url().empty()) {
    NOTREACHED() << "proto does not have merchant_cart_url";
    return;
  }
  if (!proto.has_discount_info()) {
    NOTREACHED() << "proto does not have discount_info";
    return;
  }

  // Clean up the rule-based discounts.
  if (!proto.discount_info().rule_discount_info().empty()) {
    proto.clear_discount_info();
  }

  cart_db_->AddCart(eTLDPlusOne(GURL(proto.merchant_cart_url())), proto,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::OnDeleteCart(bool success,
                               std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (proto_pairs.empty())
    return;
  DCHECK_EQ(1U, proto_pairs.size());
  if (proto_pairs[0].second.is_removed())
    return;
  cart_db_->DeleteCart(proto_pairs[0].first,
                       base::BindOnce(&CartService::OnOperationFinished,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void CartService::OnCartFeaturesChanged(const std::string& pref_name) {
  coupon_service_->MaybeFeatureStatusChanged(IsCartAndDiscountEnabled());
}

bool CartService::IsCartAndDiscountEnabled() {
  auto* list = profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules);
  if (list && base::Contains(list->GetList(), base::Value(kCartPrefsKey))) {
    return false;
  }
  return profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled);
}

void CartService::SetCartDiscountLinkFetcherForTesting(
    std::unique_ptr<CartDiscountLinkFetcher> discount_link_fetcher) {
  discount_link_fetcher_ = std::move(discount_link_fetcher);
}

void CartService::SetFetchDiscountWorkerForTesting(
    std::unique_ptr<FetchDiscountWorker> fetch_discount_worker) {
  fetch_discount_worker_for_testing_ = std::move(fetch_discount_worker);
}

void CartService::SetCouponServiceForTesting(CouponService* coupon_service) {
  coupon_service_ = coupon_service;
}
