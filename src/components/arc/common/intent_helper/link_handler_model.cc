// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/link_handler_model.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "components/google/core/common/google_util.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#endif

namespace arc {

namespace {

constexpr int kMaxValueLen = 2048;

// Not owned. Must outlive all LinkHandlerModel instances.
LinkHandlerModelDelegate* g_link_handler_model_delegate = nullptr;

bool GetQueryValue(const GURL& url,
                   const std::string& key_to_find,
                   std::u16string* out) {
  const std::string str(url.query());

  url::Component query(0, str.length());
  url::Component key;
  url::Component value;

  while (url::ExtractQueryKeyValue(str.c_str(), &query, &key, &value)) {
    if (!value.is_nonempty())
      continue;
    if (str.substr(key.begin, key.len) == key_to_find) {
      if (value.len >= kMaxValueLen)
        return false;
      url::RawCanonOutputW<kMaxValueLen> output;
      url::DecodeURLEscapeSequences(str.c_str() + value.begin, value.len,
                                    url::DecodeURLMode::kUTF8OrIsomorphic,
                                    &output);
      *out = std::u16string(output.data(), output.length());
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<LinkHandlerModel> LinkHandlerModel::Create(
    content::BrowserContext* context,
    const GURL& link_url) {
  auto impl = base::WrapUnique(new LinkHandlerModel());
  if (!impl->Init(context, link_url))
    return nullptr;
  return impl;
}

LinkHandlerModel::~LinkHandlerModel() = default;

void LinkHandlerModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LinkHandlerModel::OpenLinkWithHandler(uint32_t handler_id) {
  if (handler_id >= handlers_.size())
    return;

  if (!g_link_handler_model_delegate) {
    LOG(DFATAL) << "LinkHandlerModelDelegate is not set.";
    return;
  }

  if (!g_link_handler_model_delegate->HandleUrl(
          url_.spec(), handlers_[handler_id].package_name)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1275075): Take metrics in Lacros as well.
  ArcMetricsService::RecordArcUserInteraction(
      context_, arc::UserInteractionType::APP_STARTED_FROM_LINK_CONTEXT_MENU);
#endif
}

// static
void LinkHandlerModel::SetDelegate(LinkHandlerModelDelegate* delegate) {
  // SetDelegate should be called only when overwriting nullptr or unsetting to
  // nullptr except for testing.
  if (g_link_handler_model_delegate && delegate) {
    LOG(ERROR) << "g_link_handler_model_delegate is modified. "
               << "This should not happen except for testing.";
  }
  g_link_handler_model_delegate = delegate;
}

LinkHandlerModel::LinkHandlerModel() = default;

bool LinkHandlerModel::Init(content::BrowserContext* context, const GURL& url) {
  DCHECK(context);
  context_ = context;

  // Check if ARC apps can handle the |url|. Since the information is held in
  // a different (ARC) process, issue a mojo IPC request. Usually, the
  // callback function, OnUrlHandlerList, is called within a few milliseconds
  // even on the slowest Chromebook we support.
  url_ = RewriteUrlFromQueryIfAvailable(url);

  if (!g_link_handler_model_delegate) {
    // g_link_handler_model_delegate should be already set on the product.
    // It is not set for some tests such as browser_tests since crosapi is
    // disabled.
    LOG(ERROR) << "LinkHandlerModelDelegate is not set. "
               << "This should not happen except for testing.";
    return false;
  }

  return g_link_handler_model_delegate->RequestUrlHandlerList(
      url_.spec(), base::BindOnce(&LinkHandlerModel::OnUrlHandlerList,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void LinkHandlerModel::OnUrlHandlerList(
    std::vector<LinkHandlerModelDelegate::IntentHandlerInfo> handlers) {
  for (auto& handler : handlers) {
    if (handler.package_name == "org.chromium.arc.intent_helper")
      continue;
    handlers_.push_back(std::move(handler));
  }

  bool icon_info_notified = false;
  if (!g_link_handler_model_delegate) {
    // g_link_handler_model_delegate should be already set on the product.
    // It is not set for some tests such as browser_tests since crosapi is
    // disabled.
    LOG(ERROR) << "LinkHandlerModelDelegate is not set. "
               << "This should not happen except for testing.";
  }

  std::vector<LinkHandlerModelDelegate::ActivityName> activities;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    activities.emplace_back(handlers_[i].package_name,
                            handlers_[i].activity_name);
  }
  const LinkHandlerModelDelegate::GetResult result =
      g_link_handler_model_delegate->GetActivityIcons(
          activities, base::BindOnce(&LinkHandlerModel::NotifyObserver,
                                     weak_ptr_factory_.GetWeakPtr()));
  icon_info_notified =
      LinkHandlerModelDelegate::ActivityIconLoader::HasIconsReadyCallbackRun(
          result);

  if (!icon_info_notified) {
    // Call NotifyObserver() without icon information, unless
    // GetActivityIcons has already called it. Otherwise if we delay the
    // notification waiting for all icons, context menu may flicker.
    NotifyObserver(nullptr);
  }
}

void LinkHandlerModel::NotifyObserver(
    std::unique_ptr<LinkHandlerModelDelegate::ActivityToIconsMap> icons) {
  if (icons) {
    icons_.insert(icons->begin(), icons->end());
    icons.reset();
  }

  std::vector<LinkHandlerInfo> handlers;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    gfx::Image icon;
    const LinkHandlerModelDelegate::ActivityName activity(
        handlers_[i].package_name, handlers_[i].activity_name);
    const auto it = icons_.find(activity);
    if (it != icons_.end())
      icon = it->second.icon16;
    // Use the handler's index as an ID.
    LinkHandlerInfo handler = {base::UTF8ToUTF16(handlers_[i].name), icon,
                               static_cast<uint32_t>(i)};
    handlers.push_back(handler);
  }
  for (auto& observer : observer_list_)
    observer.ModelChanged(handlers);
}

// static
GURL LinkHandlerModel::RewriteUrlFromQueryIfAvailableForTesting(
    const GURL& url) {
  return RewriteUrlFromQueryIfAvailable(url);
}

// static
GURL LinkHandlerModel::RewriteUrlFromQueryIfAvailable(const GURL& url) {
  static const char kPathToFind[] = "/url";
  static const char kKeyToFind[] = "url";

  if (!google_util::IsGoogleHostname(url.host_piece(),
                                     google_util::DISALLOW_SUBDOMAIN)) {
    return url;
  }
  if (!url.has_path() || url.path() != kPathToFind)
    return url;

  std::u16string value;
  if (!GetQueryValue(url, kKeyToFind, &value))
    return url;

  const GURL new_url(value);
  if (!new_url.is_valid())
    return url;
  return new_url;
}

}  // namespace arc
