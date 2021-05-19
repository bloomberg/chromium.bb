// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_java_script_feature.h"

#import "ios/web/find_in_page/find_in_page_constants.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "find_in_page_js";
const char kEventListenersScriptName[] = "find_in_page_event_listeners_js";

// Timeout for the find within JavaScript in milliseconds.
const double kFindInPageFindTimeout = 100.0;

// The timeout for JavaScript function calls in milliseconds. Important that
// this is longer than |kFindInPageFindTimeout| to allow for incomplete find to
// restart again. If this timeout hits, then something went wrong with the find
// and find in page should not continue.
const double kJavaScriptFunctionCallTimeout = 200.0;
}  // namespace

namespace web {

namespace find_in_page {

const int kFindInPagePending = -1;

}  // namespace find_in_page

// static
FindInPageJavaScriptFeature* FindInPageJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FindInPageJavaScriptFeature> instance;
  return instance.get();
}

FindInPageJavaScriptFeature::FindInPageJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
               kScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kEventListenersScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetBaseJavaScriptFeature()}) {}

FindInPageJavaScriptFeature::~FindInPageJavaScriptFeature() = default;

bool FindInPageJavaScriptFeature::Search(
    WebFrame* frame,
    const std::string& query,
    base::OnceCallback<void(base::Optional<int>)> callback) {
  std::vector<base::Value> params;
  params.push_back(base::Value(query));
  params.push_back(base::Value(kFindInPageFindTimeout));
  return CallJavaScriptFunction(
      frame, kFindInPageSearch, params,
      base::BindOnce(&FindInPageJavaScriptFeature::ProcessSearchResult,
                     base::Unretained(GetInstance()), std::move(callback)),
      base::TimeDelta::FromMilliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::Pump(
    WebFrame* frame,
    base::OnceCallback<void(base::Optional<int>)> callback) {
  std::vector<base::Value> params;
  params.push_back(base::Value(kFindInPageFindTimeout));
  CallJavaScriptFunction(
      frame, kFindInPagePump, params,
      base::BindOnce(&FindInPageJavaScriptFeature::ProcessSearchResult,
                     base::Unretained(GetInstance()), std::move(callback)),
      base::TimeDelta::FromMilliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::SelectMatch(
    WebFrame* frame,
    int index,
    base::OnceCallback<void(const base::Value*)> callback) {
  std::vector<base::Value> params;
  params.push_back(base::Value(index));
  CallJavaScriptFunction(
      frame, kFindInPageSelectAndScrollToMatch, params, std::move(callback),
      base::TimeDelta::FromMilliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::Stop(WebFrame* frame) {
  std::vector<base::Value> params;
  CallJavaScriptFunction(frame, kFindInPageStop, params);
}

void FindInPageJavaScriptFeature::ProcessSearchResult(
    base::OnceCallback<void(const base::Optional<int>)> callback,
    const base::Value* result) {
  base::Optional<int> match_count;
  if (result && result->is_double()) {
    // Valid match number returned. If not, match count will be 0 in order to
    // zero-out count from previous find.
    match_count = static_cast<int>(result->GetDouble());
  }
  std::move(callback).Run(match_count);
}

}  // namespace web
