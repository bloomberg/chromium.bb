// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_java_script_feature.h"

#include <vector>

#include "base/values.h"
#import "ios/web/favicon/favicon_util.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/script_message.h"
#include "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "favicon";
const char kEventListenersScriptName[] = "favicon_event_listeners";

const char kFaviconScriptHandlerName[] = "FaviconUrlsHandler";

}  // namespace

namespace web {

FaviconJavaScriptFeature::FaviconJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
               kScriptName,
               FeatureScript::InjectionTime::kDocumentEnd,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kEventListenersScriptName,
               FeatureScript::InjectionTime::kDocumentEnd,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

FaviconJavaScriptFeature::~FaviconJavaScriptFeature() {}

absl::optional<std::string>
FaviconJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kFaviconScriptHandlerName;
}

void FaviconJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(message.is_main_frame());

  if (!message.body() || !message.body()->is_list() || !message.request_url()) {
    return;
  }

  const GURL url = message.request_url().value();

  std::vector<FaviconURL> urls;
  if (!ExtractFaviconURL(message.body()->GetListDeprecated(), url, &urls))
    return;

  if (!urls.empty())
    static_cast<WebStateImpl*>(web_state)->OnFaviconUrlUpdated(urls);
}

}  // namespace web
