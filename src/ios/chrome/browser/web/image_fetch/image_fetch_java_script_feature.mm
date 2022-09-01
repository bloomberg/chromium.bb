// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/image_fetch/image_fetch_java_script_feature.h"

#include "base/base64.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "image_fetch";
const char kScriptHandlerName[] = "ImageFetchMessageHandler";

ImageFetchJavaScriptFeature::Handler* GetHandlerFromWebState(
    web::WebState* web_state) {
  return ImageFetchTabHelper::FromWebState(web_state);
}

}  // namespace

ImageFetchJavaScriptFeature::ImageFetchJavaScriptFeature(
    base::RepeatingCallback<Handler*(web::WebState*)> handler_factory)
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}),
      handler_factory_(std::move(handler_factory)) {}

ImageFetchJavaScriptFeature::~ImageFetchJavaScriptFeature() = default;

ImageFetchJavaScriptFeature* ImageFetchJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ImageFetchJavaScriptFeature> instance(
      base::BindRepeating(&GetHandlerFromWebState));

  return instance.get();
}

void ImageFetchJavaScriptFeature::GetImageData(web::WebState* web_state,
                                               int call_id,
                                               const GURL& url) {
  web::WebFrame* main_frame = GetMainFrame(web_state);
  if (!main_frame) {
    return;
  }

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(call_id));
  parameters.push_back(base::Value(url.spec()));
  CallJavaScriptFunction(main_frame, "imageFetch.getImageData", parameters);
}

absl::optional<std::string>
ImageFetchJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ImageFetchJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  Handler* handler = handler_factory_.Run(web_state);
  if (!handler) {
    return;
  }

  // Verify that the message is well-formed before using it.
  base::Value* message = script_message.body();
  if (!message || !message->is_dict()) {
    return;
  }

  const base::Value* id_key = message->FindKey("id");
  if (!id_key || !id_key->is_double()) {
    return;
  }
  int call_id = static_cast<int>(id_key->GetDouble());

  std::string decoded_data;
  const base::Value* data = message->FindKey("data");
  if (!data || !data->is_string() ||
      !base::Base64Decode(data->GetString(), &decoded_data)) {
    handler->HandleJsFailure(call_id);
    return;
  }

  std::string from;
  const base::Value* from_value = message->FindKey("from");
  if (from_value && from_value->is_string()) {
    from = from_value->GetString();
  }

  DCHECK(!decoded_data.empty());
  handler->HandleJsSuccess(call_id, decoded_data, from);
}
