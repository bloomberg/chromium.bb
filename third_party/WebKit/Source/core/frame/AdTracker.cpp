// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/AdTracker.h"

#include "core/CoreProbeSink.h"
#include "core/frame/LocalFrame.h"
#include "core/probe/CoreProbes.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"

namespace blink {

AdTracker::AdTracker(LocalFrame* local_root) : local_root_(local_root) {
  local_root_->GetProbeSink()->addAdTracker(this);
}

AdTracker::~AdTracker() {
  DCHECK(!local_root_);
}

void AdTracker::Shutdown() {
  if (!local_root_)
    return;
  local_root_->GetProbeSink()->removeAdTracker(this);
  local_root_ = nullptr;
}

void AdTracker::WillExecuteScript(const String& script_url) {
  bool is_ad =
      script_url.IsEmpty() ? false : known_ad_scripts_.Contains(script_url);
  ExecutingScript script(script_url, is_ad);
  executing_scripts_.push_back(script);
}

void AdTracker::DidExecuteScript() {
  executing_scripts_.pop_back();
}

void AdTracker::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.script_url);
}

void AdTracker::Did(const probe::ExecuteScript& probe) {
  DidExecuteScript();
}

void AdTracker::Will(const probe::CallFunction& probe) {
  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (probe.depth)
    return;

  v8::Local<v8::Value> resource_name =
      probe.function->GetScriptOrigin().ResourceName();
  String script_url;
  if (!resource_name.IsEmpty())
    script_url = ToCoreString(resource_name->ToString());
  WillExecuteScript(script_url);
}

void AdTracker::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;

  DidExecuteScript();
}

void AdTracker::WillSendRequest(ExecutionContext* execution_context,
                                unsigned long identifier,
                                DocumentLoader* loader,
                                ResourceRequest& request,
                                const ResourceResponse& redirect_response,
                                const FetchInitiatorInfo& initiator_info,
                                Resource::Type resource_type) {
  // If the resource is not already marked as an ad, check if any executing
  // script is an ad. If yes, mark this as an ad.
  if (!request.IsAdResource() && AnyExecutingScriptsTaggedAsAdResource())
    request.SetIsAdResource();

  // If it is a script marked as an ad, append it to the known ad scripts set.
  if (resource_type != Resource::kScript || !request.IsAdResource()) {
    return;
  }
  AppendToKnownAdScripts(request.Url());
}

// Keeping a separate function to easily access from tests.
void AdTracker::AppendToKnownAdScripts(const KURL& url) {
  known_ad_scripts_.insert(url.GetString());
}

bool AdTracker::AnyExecutingScriptsTaggedAsAdResource() {
  for (const auto& executing_script : executing_scripts_) {
    if (executing_script.is_ad)
      return true;
  }
  return false;
}

void AdTracker::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_root_);
}

}  // namespace blink
