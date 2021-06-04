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
#include "chrome/browser/cart/fetch_discount_worker.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
constexpr char kFakeDataPrefix[] = "Fake:";
const int kDelayStartMs = 10;

constexpr base::FeatureParam<std::string> kPartnerMerchantPattern{
    &ntp_features::kNtpChromeCartModule, "partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

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

const re2::RE2& GetPartnerMerchantPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kPartnerMerchantPattern.Get(),
                                               options);
  return *instance;
}

bool IsPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetPartnerMerchantPattern());
}
}  // namespace

CartService::CartService(Profile* profile)
    : profile_(profile),
      cart_db_(std::make_unique<CartDB>(profile_)),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)),
      domain_name_mapping_(JSONToDictionary(IDR_CART_DOMAIN_NAME_MAPPING_JSON)),
      domain_cart_url_mapping_(
          JSONToDictionary(IDR_CART_DOMAIN_CART_URL_MAPPING_JSON)),
      discount_link_fetcher_(std::make_unique<CartDiscountLinkFetcher>()) {
  if (history_service_) {
    history_service_observation_.Observe(history_service_);
  }
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
}

CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleHidden, false);
  registry->RegisterIntegerPref(prefs::kCartModuleWelcomeSurfaceShownTimes, 0);
  registry->RegisterBooleanPref(prefs::kCartDiscountAcknowledged, false);
  registry->RegisterBooleanPref(prefs::kCartDiscountEnabled, false);
  registry->RegisterDictionaryPref(prefs::kCartUsedDiscounts);
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

void CartService::DeleteCart(const std::string& domain) {
  cart_db_->DeleteCart(domain, base::BindOnce(&CartService::OnOperationFinished,
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
    if (IsPartnerMerchant(GURL(proto_pair.second.merchant_cart_url()))) {
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
  if (!IsPartnerMerchant(cart_url) || !IsCartDiscountEnabled()) {
    std::move(callback).Run(cart_url);
    return;
  }
  LoadCart(eTLDPlusOne(cart_url),
           base::BindOnce(&CartService::OnGetDiscountURL,
                          weak_ptr_factory_.GetWeakPtr(), cart_url,
                          std::move(callback)));
}

void CartService::OnGetDiscountURL(
    const GURL& default_cart_url,
    base::OnceCallback<void(const ::GURL&)> callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK_EQ(proto_pairs.size(), 1U);
  if (proto_pairs.size() != 1U) {
    std::move(callback).Run(default_cart_url);
    return;
  }
  auto& cart_proto = proto_pairs[0].second;
  if (cart_proto.discount_info().discount_info().empty()) {
    std::move(callback).Run(default_cart_url);
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
}

void CartService::OnDiscountURLFetched(
    const GURL& default_cart_url,
    base::OnceCallback<void(const ::GURL&)> callback,
    const cart_db::ChromeCartContentProto& cart_proto,
    const GURL& discount_url) {
  std::move(callback).Run(discount_url.is_valid() ? discount_url
                                                  : default_cart_url);
  if (discount_url.is_valid()) {
    CacheUsedDiscounts(cart_proto);
    CleanUpDiscounts(cart_proto);
  }
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
  if (history_service_) {
    history_service_observation_.Reset();
  }
  DeleteCartsWithFakeData();
  // Delete content of all carts that are removed.
  cart_db_->LoadAllCarts(base::BindOnce(&CartService::DeleteRemovedCartsContent,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CartService::OnURLsDeleted(history::HistoryService* history_service,
                                const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/1157892): Add more fine-grained deletion of cart data when
  // history deletion happens.
  cart_db_->DeleteAllCarts(base::BindOnce(&CartService::OnOperationFinished,
                                          weak_ptr_factory_.GetWeakPtr()));
}

CartDB* CartService::GetDB() {
  return cart_db_.get();
}

void CartService::AddCartsWithFakeData() {
  DeleteCartsWithFakeData();
  // Polulate and add some carts with fake data.
  double time_now = base::Time::Now().ToDoubleT();
  cart_db::ChromeCartContentProto dummy_proto1;
  GURL dummy_url1 = GURL("https://www.google.com/");
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
  cart_db_->AddCart(dummy_proto3.key(), dummy_proto3,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto4;
  GURL dummy_url4 = GURL("https://www.walmart.com/");
  dummy_proto4.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url4));
  dummy_proto4.set_merchant("Cart Qux");
  dummy_proto4.set_merchant_cart_url(dummy_url4.spec());
  dummy_proto4.set_timestamp(time_now + 3);
  cart_db_->AddCart(dummy_proto4.key(), dummy_proto4,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto5;
  GURL dummy_url5 = GURL("https://www.bestbuy.com/");
  dummy_proto5.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url5));
  dummy_proto5.set_merchant("Cart Corge");
  dummy_proto5.set_merchant_cart_url(dummy_url5.spec());
  dummy_proto5.set_timestamp(time_now + 2);
  cart_db_->AddCart(dummy_proto5.key(), dummy_proto5,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto6;
  GURL dummy_url6 = GURL("https://www.nike.com/");
  dummy_proto6.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url6));
  dummy_proto6.set_merchant("Cart Flob");
  dummy_proto6.set_merchant_cart_url(dummy_url6.spec());
  dummy_proto6.set_timestamp(time_now + 1);
  cart_db_->AddCart(dummy_proto6.key(), dummy_proto6,
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

void CartService::OnLoadCarts(CartDB::LoadCallback callback,
                              bool success,
                              std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (IsHidden() && !IsFakeDataEnabled()) {
    std::move(callback).Run(success, {});
    return;
  }
  std::set<std::string> expired_merchants;
  for (CartDB::KeyAndValue kv : proto_pairs) {
    if (IsExpired(kv.second)) {
      DeleteCart(kv.second.key());
      expired_merchants.emplace(kv.second.key());
    }
  }
  proto_pairs.erase(
      std::remove_if(proto_pairs.begin(), proto_pairs.end(),
                     [expired_merchants](CartDB::KeyAndValue kv) {
                       return kv.second.is_hidden() || kv.second.is_removed() ||
                              expired_merchants.find(kv.second.key()) !=
                                  expired_merchants.end();
                     }),
      proto_pairs.end());
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
  // If the new proto has product images, we can add it to the database without
  // worrying about overwriting as it reflects the latest state; if not, we keep
  // the existing proto while updating timestamp, hidden status and product
  // information if any.
  if (proto.product_image_urls().size()) {
    cart_db_->AddCart(domain, std::move(proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  existing_proto.set_is_hidden(false);
  existing_proto.set_timestamp(proto.timestamp());
  if (cart_url) {
    existing_proto.set_merchant_cart_url(cart_url->spec());
  }
  // If no product images, this addition comes from AddToCart detection and
  // should have only one product (if any). Add this product to the existing
  // cart if not included already.
  if (proto.product_infos().size()) {
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
  }
  cart_db_->AddCart(domain, std::move(existing_proto),
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::UpdateDiscounts(const GURL& cart_url,
                                  cart_db::ChromeCartContentProto new_proto) {
  if (!cart_url.is_valid()) {
    VLOG(1) << __func__
            << "update discounts with invalid cart_url: " << cart_url;
    return;
  }

  if (new_proto.has_discount_info() &&
      !new_proto.discount_info().discount_info().empty()) {
    // Filter used discounts.
    std::vector<cart_db::DiscountInfoProto> discount_info_protos;
    for (const cart_db::DiscountInfoProto& proto :
         new_proto.discount_info().discount_info()) {
      if (!IsDiscountUsed(proto.rule_id())) {
        discount_info_protos.emplace_back(proto);
      }
    }
    if (discount_info_protos.empty()) {
      new_proto.clear_discount_info();
    } else {
      *new_proto.mutable_discount_info()->mutable_discount_info() = {
          discount_info_protos.begin(), discount_info_protos.end()};
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

  fetch_discount_worker_ = std::make_unique<FetchDiscountWorker>(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      std::make_unique<CartDiscountFetcherFactory>(),
      std::make_unique<CartLoaderAndUpdaterFactory>(profile_));

  fetch_discount_worker_->Start(
      base::TimeDelta::FromMilliseconds(kDelayStartMs));
}

bool CartService::IsDiscountUsed(const std::string& rule_id) {
  return profile_->GetPrefs()
             ->GetDictionary(prefs::kCartUsedDiscounts)
             ->FindBoolKey(rule_id) != absl::nullopt;
}

void CartService::CacheUsedDiscounts(
    const cart_db::ChromeCartContentProto& proto) {
  if (!proto.has_discount_info() ||
      proto.discount_info().discount_info().empty()) {
    NOTREACHED() << "Empty discounts";
    return;
  }
  DictionaryPrefUpdate update(profile_->GetPrefs(), prefs::kCartUsedDiscounts);
  for (auto discount_info : proto.discount_info().discount_info()) {
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

  proto.clear_discount_info();
  cart_db_->AddCart(eTLDPlusOne(GURL(proto.merchant_cart_url())), proto,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::SetCartDiscountLinkFetcherForTesting(
    std::unique_ptr<CartDiscountLinkFetcher> discount_link_fetcher) {
  discount_link_fetcher_ = std::move(discount_link_fetcher);
}
