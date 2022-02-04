// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/cart/commerce_renderer_feature_list.h"
#include "components/search/ntp_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/loader/http_body_element_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"

using base::UserMetricsAction;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebString;

namespace cart {

namespace {

constexpr unsigned kLengthLimit = 4096;
constexpr char kAmazonDomain[] = "amazon.com";
constexpr char kEbayDomain[] = "ebay.com";
constexpr char kElectronicExpressDomain[] = "electronicexpress.com";
constexpr char kGStoreHost[] = "store.google.com";

constexpr base::FeatureParam<std::string> kSkipPattern{
    &ntp_features::kNtpChromeCartModule, "product-skip-pattern",
    // This regex does not match anything.
    "\\b\\B"};

// This is based on top 30 US shopping sites.
// TODO(crbug/1164236): cover more shopping sites.
constexpr base::FeatureParam<std::string> kAddToCartPattern{
    &ntp_features::kNtpChromeCartModule, "add-to-cart-pattern",
    "(\\b|[^a-z])"
    "((add(ed)?(-|_|(%20)|\\s)?(item)?(-|_|(%20)|\\s)?to(-|_|(%20)|\\s)?(cart|"
    "basket|bag)"
    ")|(cart\\/add)|(checkout\\/basket)|(cart_type)|(isquickaddtocartbutton))"
    "(\\b|[^a-z])"};

constexpr base::FeatureParam<std::string> kSkipAddToCartMapping{
    &ntp_features::kNtpChromeCartModule, "skip-add-to-cart-mapping",
    // Empty JSON string.
    ""};

// The heuristics of cart pages are from top 100 US shopping domains.
// https://colab.corp.google.com/drive/1fTGE_SQw_8OG4ubzQvWcBuyHEhlQ-pwQ?usp=sharing
constexpr base::FeatureParam<std::string> kCartPattern{
    &ntp_features::kNtpChromeCartModule, "cart-pattern",
    // clang-format off
    "(^https?://cart\\.)"
    "|"
    "(/("
      "(((my|co|shopping|view)[-_]?)?(cart|bag)(view|display)?)"
      "|"
      "(checkout/([^/]+/)?(basket|bag))"
      "|"
      "(checkoutcart(display)?view)"
      "|"
      "(bundles/shop)"
      "|"
      "((ajax)?orderitemdisplay(view)?)"
      "|"
      "(cart-show)"
    ")(/|\\.|$))"
    // clang-format on
};

constexpr base::FeatureParam<std::string> kCartPatternMapping{
    &ntp_features::kNtpChromeCartModule, "cart-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kCheckoutPattern{
    &ntp_features::kNtpChromeCartModule, "checkout-pattern",
    // clang-format off
    "/("
    "("
      "("
        "(begin|billing|cart|payment|start|review|final|order|secure|new)"
        "[-_]?"
      ")?"
      "(checkout|chkout)(s)?"
      "([-_]?(begin|billing|cart|payment|start|review))?"
    ")"
    "|"
    "(\\w+(checkout|chkout)(s)?)"
    ")(/|\\.|$|\\?)"
    // clang-format on
};

constexpr base::FeatureParam<std::string> kCheckoutPatternMapping{
    &ntp_features::kNtpChromeCartModule, "checkout-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kPurchaseURLPatternMapping{
    &ntp_features::kNtpChromeCartModule, "purchase-url-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kPurchaseButtonPattern{
    &ntp_features::kNtpChromeCartModule, "purchase-button-pattern",
    // clang-format off
    "^("
    "("
      "(place|submit|complete|confirm|finalize|make)(\\s(an|your|my|this))?"
      "(\\ssecure)?\\s(order|purchase|checkout|payment)"
    ")"
    "|"
    "((pay|buy)(\\ssecurely)?(\\sUSD)?\\s(it|now|((\\$)?\\d+(\\.\\d+)?)))"
    "|"
    "((make|authorise|authorize|secure)\\spayment)"
    "|"
    "(confirm\\s(and|&)\\s(buy|purchase|order|pay|checkout))"
    "|"
    "((\\W)*(buy|purchase|order|pay|checkout)(\\W)*)"
    ")$"
    // clang-format on
};

constexpr base::FeatureParam<std::string> kPurchaseButtonPatternMapping{
    &ntp_features::kNtpChromeCartModule, "purchase-button-pattern-mapping",
    // Empty JSON map.
    "{}"};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionGapTime{
    &ntp_features::kNtpChromeCartModule, "cart-extraction-gap-time",
    base::Seconds(2)};

constexpr base::FeatureParam<int> kCartExtractionMaxCount{
    &ntp_features::kNtpChromeCartModule, "cart-extraction-max-count", 20};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionMinTaskTime{
    &ntp_features::kNtpChromeCartModule, "cart-extraction-min-task-time",
    base::Seconds(0.01)};

constexpr base::FeatureParam<double> kCartExtractionDutyCycle{
    &ntp_features::kNtpChromeCartModule, "cart-extraction-duty-cycle", 0.05};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionTimeout{
    &ntp_features::kNtpChromeCartModule, "cart-extraction-timeout",
    base::Seconds(0.25)};

constexpr base::FeatureParam<std::string> kProductIdPatternMapping{
    &ntp_features::kNtpChromeCartModule, "product-id-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kCouponProductIdPatternMapping{
    &commerce_renderer_feature::kRetailCoupons,
    "coupon-product-id-pattern-mapping",
    // Empty JSON string.
    ""};

std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsCartHeuristicsImprovementEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ntp_features::kNtpChromeCartModule,
      ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam, false);
}

enum class CommerceEvent {
  kAddToCartByForm,
  kAddToCartByURL,
  kVisitCart,
  kVisitCheckout,
  kPurchaseByForm,
  kPurchaseByURL,
};

void RecordCommerceEvent(CommerceEvent event) {
  switch (event) {
    case CommerceEvent::kAddToCartByForm:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.AddToCartByPOST", true);
      DVLOG(1) << "Commerce.AddToCart by POST form";
      break;
    case CommerceEvent::kAddToCartByURL:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.AddToCartByURL", true);
      DVLOG(1) << "Commerce.AddToCart by URL";
      break;
    case CommerceEvent::kVisitCart:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.VisitCart", true);
      DVLOG(1) << "Commerce.VisitCart";
      break;
    case CommerceEvent::kVisitCheckout:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.VisitCheckout", true);
      DVLOG(1) << "Commerce.VisitCheckout";
      break;
    case CommerceEvent::kPurchaseByForm:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.PurchaseByPOST", true);
      DVLOG(1) << "Commerce.Purchase by POST form";
      break;
    case CommerceEvent::kPurchaseByURL:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.PurchaseByURL", true);
      DVLOG(1) << "Commerce.Purchase by URL";
      break;
    default:
      NOTREACHED();
  }
}

mojo::Remote<mojom::CommerceHintObserver> GetObserver(
    content::RenderFrame* render_frame) {
  // Connect to Mojo service on browser to notify commerce signals.
  mojo::Remote<mojom::CommerceHintObserver> observer;

  // Subframes including fenced frames shouldn't be reached here.
  DCHECK(render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree());

  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      observer.BindNewPipeAndPassReceiver());
  return observer;
}

absl::optional<GURL> ScanCartURL(content::RenderFrame* render_frame) {
  blink::WebDocument doc = render_frame->GetWebFrame()->GetDocument();

  absl::optional<GURL> best;
  blink::WebVector<WebElement> elements =
      doc.QuerySelectorAll(WebString("a[href]"));
  for (WebElement element : elements) {
    GURL link = doc.CompleteURL(element.GetAttribute("href"));
    if (!link.is_valid())
      continue;
    link = link.GetAsReferrer();
    // Only keep the shortest match. First match or most frequent match might
    // work better, but we need larger validating corpus.
    if (best && link.spec().size() >= best->spec().size())
      continue;
    if (!CommerceHintAgent::IsVisitCart(link))
      continue;
    DVLOG(2) << "Cart link: " << link;
    best = link;
  }
  if (best)
    DVLOG(1) << "Best cart link: " << *best;
  return best;
}

void OnAddToCart(content::RenderFrame* render_frame,
                 const std::string& product_id = std::string()) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnAddToCart(ScanCartURL(render_frame), product_id);
}

void OnVisitCart(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnVisitCart();
}

void OnCartProductUpdated(content::RenderFrame* render_frame,
                          std::vector<mojom::ProductPtr> products) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnCartProductUpdated(std::move(products));
}

void OnVisitCheckout(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnVisitCheckout();
}

void OnPurchase(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnPurchase();
}

void OnFormSubmit(content::RenderFrame* render_frame, bool is_purchase) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnFormSubmit(is_purchase);
}

void OnWillSendRequest(content::RenderFrame* render_frame, bool is_addtocart) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnWillSendRequest(is_addtocart);
}

bool PartialMatch(base::StringPiece str, const re2::RE2& re) {
  return RE2::PartialMatch(re2::StringPiece(str.data(), str.size()), re);
}

const re2::RE2& GetAddToCartPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kAddToCartPattern.Get(),
                                               options);
  return *instance;
}

// TODO(crbug.com/1189786): Using per-site pattern and full URL matching could
// be unnecessary. Improve this later by using general pattern if possible and
// more flexible matching.
const re2::RE2& GetVisitCartPattern(const GURL& url) {
  static base::NoDestructor<std::map<std::string, std::string>>
      heuristic_string_map([] {
        const base::StringPiece json_resource(
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                IDR_CART_DOMAIN_CART_URL_REGEX_JSON));
        const std::string& finch_param = kCartPatternMapping.Get();
        const base::Value json(base::JSONReader::Read(finch_param.empty()
                                                          ? json_resource
                                                          : finch_param)
                                   .value());
        DCHECK(json.is_dict());
        std::map<std::string, std::string> map;
        for (auto item : json.DictItems()) {
          map.insert(
              {std::move(item.first), std::move(item.second.GetString())});
        }
        return map;
      }());
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      heuristic_regex_map;
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  const std::string& domain = eTLDPlusOne(url);
  if (heuristic_string_map->find(domain) == heuristic_string_map->end()) {
    static base::NoDestructor<re2::RE2> instance(kCartPattern.Get(), options);
    return *instance;
  }
  if (heuristic_regex_map->find(domain) == heuristic_regex_map->end()) {
    heuristic_regex_map->insert(
        {domain, std::make_unique<re2::RE2>(heuristic_string_map->at(domain),
                                            options)});
  }
  return *heuristic_regex_map->at(domain);
}

// TODO(crbug/1164236): cover more shopping sites.
const re2::RE2& GetVisitCheckoutPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kCheckoutPattern.Get(), options);
  return *instance;
}

const re2::RE2& GetSkipPattern() {
  static base::NoDestructor<re2::RE2> instance([] {
    const std::string& pattern = kSkipPattern.Get();
    DVLOG(1) << "SkipPattern = " << pattern;
    return pattern;
  }());
  return *instance;
}

// TODO(crbug/1164236): need i18n.
const re2::RE2& GetPurchaseTextPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kPurchaseButtonPattern.Get(),
                                               options);
  return *instance;
}

bool GetProductIdFromRequest(base::StringPiece request,
                             std::string* product_id) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> re("(product_id|pr1id)=(\\w+)", options);
  return RE2::PartialMatch(re2::StringPiece(request.data(), request.size()),
                           *re, nullptr, product_id);
}

bool IsSameDomainXHR(const std::string& host,
                     const blink::WebURLRequest& request) {
  // Only handle XHR POST requests here.
  // Other matches like navigation is handled in DidStartNavigation().
  if (!request.HttpMethod().Equals("POST"))
    return false;

  const GURL url = request.Url();
  return url.DomainIs(host);
}

const std::map<std::string, std::string>& GetSkipAddToCartMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> skip_map([] {
    const base::Value json(
        base::JSONReader::Read(
            kSkipAddToCartMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                      IDR_SKIP_ADD_TO_CART_REQUEST_DOMAIN_MAPPING_JSON)
                : kSkipAddToCartMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto item : json.DictItems()) {
      map.insert({std::move(item.first), std::move(item.second.GetString())});
    }
    return map;
  }());
  return *skip_map;
}

const std::map<std::string, std::string>& GetCheckoutPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    const base::Value json(
        base::JSONReader::Read(
            kCheckoutPatternMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                      IDR_CHECKOUT_URL_REGEX_DOMAIN_MAPPING_JSON)
                : kCheckoutPatternMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (const auto item : json.DictItems()) {
      map.insert({std::move(item.first), std::move(item.second.GetString())});
    }
    return map;
  }());
  return *pattern_map;
}

const std::map<std::string, std::string>& GetPurchaseURLPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    const base::Value json(
        base::JSONReader::Read(
            kPurchaseURLPatternMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                      IDR_PURCHASE_URL_REGEX_DOMAIN_MAPPING_JSON)
                : kPurchaseURLPatternMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (const auto item : json.DictItems()) {
      map.insert({std::move(item.first), std::move(item.second.GetString())});
    }
    return map;
  }());
  return *pattern_map;
}

const std::map<std::string, std::string>& GetPurchaseButtonPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    const base::Value json(
        base::JSONReader::Read(kPurchaseButtonPatternMapping.Get()).value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (const auto item : json.DictItems()) {
      map.insert({std::move(item.first), std::move(item.second.GetString())});
    }
    return map;
  }());
  return *pattern_map;
}

bool DetectAddToCart(content::RenderFrame* render_frame,
                     const blink::WebURLRequest& request) {
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  const GURL& navigation_url(frame->GetDocument().Url());
  const GURL& url = request.Url();

  bool is_add_to_cart = false;
  if (navigation_url.DomainIs("dickssportinggoods.com")) {
    is_add_to_cart = CommerceHintAgent::IsAddToCart(url.spec());
  } else if (url.DomainIs("rei.com")) {
    // TODO(crbug.com/1188143): There are other true positives like
    // 'neo-product/rs/cart/item' that are missed here. Figure out a more
    // comprehensive solution.
    is_add_to_cart = url.path_piece() == "/rest/cart/item";
  } else if (navigation_url.DomainIs(kElectronicExpressDomain)) {
    is_add_to_cart =
        CommerceHintAgent::IsAddToCart(url.spec()) &&
        GetProductIdFromRequest(url.spec().substr(0, kLengthLimit), nullptr);
  } else if (navigation_url.host() == kGStoreHost) {
    is_add_to_cart = url.spec().find("O2JPA") != std::string::npos;
  } else if (url.DomainIs("zappos.com")) {
    is_add_to_cart = url.spec().find("mobileapi/v1/cart?displayRewards=true") !=
                     std::string::npos;
  } else {
    is_add_to_cart = CommerceHintAgent::IsAddToCart(url.path_piece());
  }
  if (is_add_to_cart) {
    std::string url_product_id;
    if (commerce_renderer_feature::IsPartnerMerchant(navigation_url)) {
      GetProductIdFromRequest(url.spec().substr(0, kLengthLimit),
                              &url_product_id);
    }
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame, std::move(url_product_id));
    return true;
  }

  if (CommerceHintAgent::ShouldSkipAddToCartRequest(navigation_url, url)) {
    return false;
  }

  if (IsCartHeuristicsImprovementEnabled()) {
    if (navigation_url.DomainIs("abebooks.com"))
      return false;
    if (navigation_url.DomainIs("abercrombie.com"))
      return false;
    if (navigation_url.DomainIs(kAmazonDomain) &&
        url.host() != "fls-na.amazon.com")
      return false;
    if (navigation_url.DomainIs("bestbuy.com"))
      return false;
    if (navigation_url.DomainIs("containerstore.com"))
      return false;
    if (navigation_url.DomainIs("gap.com") && url.DomainIs("granify.com"))
      return false;
    if (navigation_url.DomainIs("kohls.com"))
      return false;
    if (navigation_url.DomainIs("officedepot.com") &&
        url.DomainIs("chatid.com"))
      return false;
    if (navigation_url.DomainIs("pier1.com"))
      return false;
  }

  blink::WebHTTPBody body = request.HttpBody();
  if (body.IsNull())
    return false;

  unsigned i = 0;
  blink::WebHTTPBody::Element element;
  while (body.ElementAt(i++, element)) {
    if (element.type != blink::HTTPBodyElementType::kTypeData)
      continue;

    // TODO(crbug/1168704): this copy is avoidable if element is guaranteed to
    // have contiguous buffer.
    std::vector<uint8_t> buf = element.data.Copy().ReleaseVector();
    base::StringPiece str(reinterpret_cast<char*>(buf.data()), buf.size());

    // Per-site hard-coded exclusion rules:
    if (navigation_url.DomainIs("groupon.com") && buf.size() > 10000)
      return false;

    // Per-site skipping length limit when checking request text.
    bool skip_length_limit = navigation_url.DomainIs("otterbox.com");

    if (CommerceHintAgent::IsAddToCart(str, skip_length_limit)) {
      std::string product_id;
      if (commerce_renderer_feature::IsPartnerMerchant(url)) {
        GetProductIdFromRequest(str.substr(0, kLengthLimit), &product_id);
      }
      RecordCommerceEvent(CommerceEvent::kAddToCartByForm);
      DVLOG(2) << "Matched add-to-cart. Request from \"" << navigation_url
               << "\" to \"" << url << "\" with payload (size = " << str.size()
               << ") \"" << str << "\"";
      OnAddToCart(render_frame, std::move(product_id));
      return true;
    }
  }
  return false;
}

std::string CanonicalURL(const GURL& url) {
  return base::JoinString({url.scheme_piece(), "://", url.host_piece(),
                           url.path_piece().substr(0, kLengthLimit)},
                          "");
}

const WebString& GetProductExtractionScript() {
  static base::NoDestructor<WebString> script([] {
    std::string script_string =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_CART_PRODUCT_EXTRACTION_JS);
    if (IsCartHeuristicsImprovementEnabled()) {
      script_string = "var isImprovementEnabled = true;\n" + script_string;
    }
    const std::string config =
        "var kSleeperMinTaskTimeMs = " +
        base::NumberToString(
            kCartExtractionMinTaskTime.Get().InMillisecondsF()) +
        ";\n" + "var kSleeperDutyCycle = " +
        base::NumberToString(kCartExtractionDutyCycle.Get()) + ";\n" +
        "var kTimeoutMs = " +
        base::NumberToString(kCartExtractionTimeout.Get().InMillisecondsF()) +
        ";\n";
    DVLOG(2) << config;
    script_string = config + script_string;

    const std::string id_extraction_map =
        kProductIdPatternMapping.Get().empty()
            ? std::string(
                  ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                      IDR_CART_DOMAIN_PRODUCT_ID_REGEX_JSON))
            : kProductIdPatternMapping.Get();
    script_string =
        "var idExtractionMap = " + id_extraction_map + ";\n" + script_string;

    const std::string coupon_id_extraction_map =
        kCouponProductIdPatternMapping.Get();
    if (!coupon_id_extraction_map.empty()) {
      script_string =
          "var couponIdExtractionMap = " + coupon_id_extraction_map + ";\n" +
          script_string;
    }
    return WebString::FromUTF8(std::move(script_string));
  }());
  return *script;
}

}  // namespace

CommerceHintAgent::CommerceHintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CommerceHintAgent>(render_frame) {
  DCHECK(render_frame);

  // Subframes including fenced frames shouldn't be reached here.
  DCHECK(render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree());
}

CommerceHintAgent::~CommerceHintAgent() = default;

bool CommerceHintAgent::IsAddToCart(base::StringPiece str,
                                    bool skip_length_limit) {
  return PartialMatch(skip_length_limit ? str : str.substr(0, kLengthLimit),
                      GetAddToCartPattern());
}

bool CommerceHintAgent::IsVisitCart(const GURL& url) {
  return PartialMatch(CanonicalURL(url).substr(0, kLengthLimit),
                      GetVisitCartPattern(url));
}

bool CommerceHintAgent::IsVisitCheckout(const GURL& url) {
  const std::map<std::string, std::string>& checkout_string_map =
      GetCheckoutPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      checkout_regex_map;
  std::string domain = eTLDPlusOne(url);
  std::string url_string = CanonicalURL(url).substr(0, kLengthLimit);
  if (checkout_string_map.find(domain) == checkout_string_map.end()) {
    return PartialMatch(url_string, GetVisitCheckoutPattern());
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (checkout_regex_map->find(domain) == checkout_regex_map->end()) {
    checkout_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(checkout_string_map.at(domain), options)});
  }
  return PartialMatch(url_string, *checkout_regex_map->at(domain));
}

bool CommerceHintAgent::IsPurchase(const GURL& url) {
  const std::map<std::string, std::string>& purchase_string_map =
      GetPurchaseURLPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      purchase_regex_map;
  std::string domain = eTLDPlusOne(url);
  std::string url_string = CanonicalURL(url).substr(0, kLengthLimit);
  if (purchase_string_map.find(domain) == purchase_string_map.end()) {
    return false;
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (purchase_regex_map->find(domain) == purchase_regex_map->end()) {
    purchase_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(purchase_string_map.at(domain), options)});
  }
  return PartialMatch(url_string, *purchase_regex_map->at(domain));
}

bool CommerceHintAgent::IsPurchase(const GURL& url,
                                   base::StringPiece button_text) {
  const std::map<std::string, std::string>& purchase_string_map =
      GetPurchaseButtonPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      purchase_regex_map;
  std::string domain = eTLDPlusOne(url);
  if (purchase_string_map.find(domain) == purchase_string_map.end()) {
    return PartialMatch(button_text, GetPurchaseTextPattern());
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (purchase_regex_map->find(domain) == purchase_regex_map->end()) {
    purchase_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(purchase_string_map.at(domain), options)});
  }
  return PartialMatch(button_text, *purchase_regex_map->at(domain));
}

bool CommerceHintAgent::ShouldSkip(base::StringPiece product_name) {
  return PartialMatch(product_name.substr(0, kLengthLimit), GetSkipPattern());
}

const std::vector<std::string> CommerceHintAgent::ExtractButtonTexts(
    const blink::WebFormElement& form) {
  static base::NoDestructor<WebString> kButton("button");

  const WebElementCollection& buttons = form.GetElementsByHTMLTagName(*kButton);

  std::vector<std::string> button_texts;
  for (WebElement button = buttons.FirstItem(); !button.IsNull();
       button = buttons.NextItem()) {
    // TODO(crbug/1164236): emulate innerText to be more robust.
    button_texts.push_back(base::UTF16ToUTF8(base::CollapseWhitespace(
        base::TrimWhitespace(button.TextContent().Utf16(),
                             base::TrimPositions::TRIM_ALL),
        true)));
  }
  return button_texts;
}

void CommerceHintAgent::MaybeExtractProducts() {
  // TODO(crbug/1241582): Add a test for rate control based on whether the
  // histogram is recorded.
  if (is_extraction_pending_) {
    DVLOG(1) << "Extraction is scheduled. Skip this request.";
    return;
  }
  if (extraction_count_ >= kCartExtractionMaxCount.Get()) {
    DVLOG(1) << "Extraction exceeds quota. Skip until navigation.";
    return;
  }
  is_extraction_pending_ = true;
  DVLOG(1) << "Scheduled extraction";
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CommerceHintAgent::ExtractProducts,
                     weak_factory_.GetWeakPtr()),
      kCartExtractionGapTime.Get());
}

void CommerceHintAgent::ExtractProducts() {
  is_extraction_pending_ = false;
  if (is_extraction_running_) {
    DVLOG(1) << "Extraction is running. Try again later.";
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CommerceHintAgent::MaybeExtractProducts,
                       weak_factory_.GetWeakPtr()),
        kCartExtractionGapTime.Get());
    return;
  }
  is_extraction_running_ = true;
  DVLOG(2) << "is_extraction_running_ = " << is_extraction_running_;

  blink::WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  blink::WebScriptSource source =
      blink::WebScriptSource(GetProductExtractionScript());

  if (!javascript_request_) {
    // This singleton never gets deleted, and will out-live CommerceHintAgen.
    javascript_request_ = new JavaScriptRequest(weak_factory_.GetWeakPtr());
  }
  main_frame->RequestExecuteScript(
      ISOLATED_WORLD_ID_CHROME_INTERNAL, base::make_span(&source, 1), false,
      blink::WebLocalFrame::kAsynchronous, javascript_request_,
      blink::BackForwardCacheAware::kAllow,
      blink::WebLocalFrame::PromiseBehavior::kAwait);
}

CommerceHintAgent::JavaScriptRequest::JavaScriptRequest(
    base::WeakPtr<CommerceHintAgent> agent)
    : agent_(std::move(agent)) {}

CommerceHintAgent::JavaScriptRequest::~JavaScriptRequest() = default;

void CommerceHintAgent::JavaScriptRequest::WillExecute() {
  start_time_ = base::TimeTicks::Now();
}

void CommerceHintAgent::JavaScriptRequest::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  // Only record when the start time is correctly captured.
  DCHECK(!start_time_.is_null());
  if (!start_time_.is_null()) {
    base::UmaHistogramTimes("Commerce.Carts.ExtractionExecutionTime",
                            base::TimeTicks::Now() - start_time_);
  }
  if (!agent_)
    return;
  if (result.empty() || result[0].IsEmpty())
    return;
  blink::WebLocalFrame* main_frame = agent_->render_frame()->GetWebFrame();
  v8::Local<v8::Context> context = main_frame->MainWorldScriptContext();

  agent_->OnProductsExtracted(
      content::V8ValueConverter::Create()->FromV8Value(result[0], context));
}

void CommerceHintAgent::OnProductsExtracted(
    std::unique_ptr<base::Value> results) {
  if (!results) {
    DLOG(ERROR) << "OnProductsExtracted() got empty results";
    return;
  }
  DVLOG(2) << "OnProductsExtracted: " << *results;
  if (!results->is_dict()) {
    DLOG(ERROR) << "OnProductsExtracted() result is not dict";
    return;
  }

  auto record_time = [&](const std::string& key,
                         const std::string& histograme_name) {
    auto* value = results->FindKey(key);
    if (!value)
      return;
    if (value->is_int() || value->is_double()) {
      base::UmaHistogramTimes(histograme_name,
                              base::Milliseconds(value->GetDouble()));
    }
  };
  record_time("longest_task_ms", "Commerce.Carts.ExtractionLongestTaskTime");
  record_time("total_tasks_ms", "Commerce.Carts.ExtractionTotalTasksTime");
  record_time("elapsed_ms", "Commerce.Carts.ExtractionElapsedTime");

  auto* timedout = results->FindKey("timedout");
  if (timedout && timedout->is_bool()) {
    base::UmaHistogramBoolean("Commerce.Carts.ExtractionTimedOut",
                              timedout->GetBool());
  }

  auto* extracted_products = results->FindKey("products");
  if (!extracted_products)
    return;
  // Don't update cart when the return value is not a list. This could be due to
  // that the cart is not loaded.
  if (!extracted_products->is_list())
    return;
  bool is_partner = commerce_renderer_feature::IsPartnerMerchant(
      GURL(render_frame()->GetWebFrame()->GetDocument().Url()));
  std::vector<mojom::ProductPtr> products;
  for (const auto& product : extracted_products->GetList()) {
    if (!product.is_dict())
      continue;
    const auto* image_url = product.FindKey("imageUrl");
    const auto* product_name = product.FindKey("title");
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->image_url = GURL(image_url->GetString());
    product_ptr->name = product_name->GetString();
    DVLOG(1) << "image_url = " << product_ptr->image_url;
    DVLOG(1) << "name = " << product_ptr->name;
    if (ShouldSkip(product_ptr->name)) {
      DVLOG(1) << "skipped";
      continue;
    }
    if (is_partner) {
      std::string product_id;
      const auto* extracted_id = product.FindKey("productId");
      if (extracted_id) {
        product_id = extracted_id->GetString();
      }
      DVLOG(1) << "product_id = " << product_id;
      DCHECK(!product_id.empty());
      product_ptr->product_id = std::move(product_id);
    }
    products.push_back(std::move(product_ptr));
  }
  OnCartProductUpdated(render_frame(), std::move(products));

  is_extraction_running_ = false;
  extraction_count_++;
  DVLOG(2) << "is_extraction_running_ = " << is_extraction_running_;
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  if (should_skip_) {
    return;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL& url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  // Only check XHR POST requests for add-to-cart.
  // Other add-to-cart matches like navigation is handled in
  // DidStartNavigation(). Some sites use GET requests though, so special-case
  // them here.
  GURL request_url = request.Url();
  if (request.HttpMethod().Equals("POST") ||
      request_url.DomainIs(kEbayDomain) ||
      url.DomainIs(kElectronicExpressDomain)) {
    bool is_add_to_cart = DetectAddToCart(render_frame(), request);
    OnWillSendRequest(render_frame(), is_add_to_cart);
  }

  // TODO(crbug/1164236): use MutationObserver on cart instead.
  // Detect XHR in cart page.
  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  if (!url.SchemeIs(url::kHttpsScheme))
    return;
  if (IsVisitCart(url) && IsSameDomainXHR(url.host(), request)) {
    DVLOG(1) << "In-cart XHR: " << request.Url();
    MaybeExtractProducts();
  }
}

void CommerceHintAgent::OnNavigation(const GURL& url,
                                     OnNavigationCallback callback) {
  if (!commerce_renderer_feature::kOptimizeRendererSignal.Get()) {
    std::move(callback).Run(false);
    return;
  }
  if (!navigation_observer_.is_bound() ||
      !navigation_observer_.is_connected()) {
    navigation_observer_ = GetObserver(render_frame());
  }
  navigation_observer_->OnNavigation(url, std::move(callback));
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    absl::optional<blink::WebNavigationType> navigation_type) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  has_finished_loading_ = false;
  OnNavigation(url,
               base::BindOnce(&CommerceHintAgent::DidStartNavigationCallback,
                              weak_factory_.GetWeakPtr(), url,
                              std::move(navigation_type)));
}

void CommerceHintAgent::DidStartNavigationCallback(
    const GURL& url,
    absl::optional<blink::WebNavigationType> navigation_type,
    bool should_skip) {
  should_skip_ = should_skip;
  starting_url_ = url;
}

void CommerceHintAgent::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (should_skip_) {
    return;
  }
  if (!starting_url_.is_valid())
    return;
  if (IsAddToCart(starting_url_.PathForRequestPiece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame());
  }
  if (!IsVisitCart(starting_url_) && IsVisitCheckout(starting_url_)) {
    RecordCommerceEvent(CommerceEvent::kVisitCheckout);
    OnVisitCheckout(render_frame());
  }
  if (IsPurchase(starting_url_)) {
    RecordCommerceEvent(CommerceEvent::kPurchaseByURL);
    OnPurchase(render_frame());
  }

  starting_url_ = GURL();
}

void CommerceHintAgent::DidFinishLoad() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL& url(frame->GetDocument().Url());
  if (!url.SchemeIs(url::kHttpsScheme))
    return;
  OnNavigation(url, base::BindOnce(&CommerceHintAgent::DidFinishLoadCallback,
                                   weak_factory_.GetWeakPtr()));
}

void CommerceHintAgent::DidFinishLoadCallback(bool should_skip) {
  should_skip_ = should_skip;
  if (should_skip_) {
    return;
  }
  has_finished_loading_ = true;
  extraction_count_ = 0;
  const GURL& url(render_frame()->GetWebFrame()->GetDocument().Url());
  // Some URLs might satisfy the patterns for both cart and checkout (e.g.
  // https://www.foo.com/cart/checkout). In those cases, cart has higher
  // priority.
  if (IsVisitCart(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCart);
    OnVisitCart(render_frame());
    DVLOG(1) << "Extract products after loading";
    MaybeExtractProducts();
  } else if (IsVisitCheckout(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCheckout);
    OnVisitCheckout(render_frame());
  }
}

void CommerceHintAgent::WillSubmitForm(const blink::WebFormElement& form) {
  if (should_skip_) {
    return;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  bool is_purchase = false;
  for (const std::string& button_text : ExtractButtonTexts(form)) {
    if (IsPurchase(url, button_text)) {
      RecordCommerceEvent(CommerceEvent::kPurchaseByForm);
      OnPurchase(render_frame());
      is_purchase = true;
      break;
    }
  }
  OnFormSubmit(render_frame(), is_purchase);
}

// TODO(crbug/1164236): use MutationObserver on cart instead.
void CommerceHintAgent::ExtractCartFromCurrentFrame() {
  if (should_skip_) {
    return;
  }
  if (!has_finished_loading_)
    return;
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIs(url::kHttpsScheme))
    return;

  if (IsVisitCart(url)) {
    DVLOG(1) << "Extract products due to layout shift or intersection change";
    MaybeExtractProducts();
  }
}

void CommerceHintAgent::DidObserveLayoutShift(double score,
                                              bool after_input_or_scroll) {
  DVLOG(1) << "Layout shift " << score << " " << after_input_or_scroll;
  ExtractCartFromCurrentFrame();
}

void CommerceHintAgent::OnMainFrameIntersectionChanged(
    const gfx::Rect& intersect_rect) {
  DVLOG(1) << "Intersection changed " << intersect_rect.x() << " "
           << intersect_rect.y() << " " << intersect_rect.width() << " "
           << intersect_rect.height();
  ExtractCartFromCurrentFrame();
}

bool CommerceHintAgent::ShouldSkipAddToCartRequest(const GURL& navigation_url,
                                                   const GURL& request_url) {
  const std::map<std::string, std::string>& skip_string_map =
      GetSkipAddToCartMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      skip_regex_map;
  const std::string& navigation_domain = eTLDPlusOne(navigation_url);
  if (skip_string_map.find(navigation_domain) == skip_string_map.end()) {
    return false;
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (skip_regex_map->find(navigation_domain) == skip_regex_map->end()) {
    skip_regex_map->insert(
        {navigation_domain,
         std::make_unique<re2::RE2>(skip_string_map.at(navigation_domain),
                                    options)});
  }
  return PartialMatch(request_url.spec().substr(0, kLengthLimit),
                      *skip_regex_map->at(navigation_domain));
}
}  // namespace cart
