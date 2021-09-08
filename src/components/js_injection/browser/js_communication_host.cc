// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/browser/js_communication_host.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/js_injection/browser/js_to_browser_messaging.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "components/js_injection/common/origin_matcher.h"
#include "components/js_injection/common/origin_matcher_mojom_traits.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace js_injection {
namespace {

std::string ConvertToNativeAllowedOriginRulesWithSanityCheck(
    const std::vector<std::string>& allowed_origin_rules_strings,
    OriginMatcher& allowed_origin_rules) {
  for (auto& rule : allowed_origin_rules_strings) {
    if (!allowed_origin_rules.AddRuleFromString(rule))
      return "allowedOriginRules " + rule + " is invalid";
  }
  return std::string();
}

}  // namespace

struct JsObject {
  JsObject(const std::u16string& name,
           OriginMatcher allowed_origin_rules,
           std::unique_ptr<WebMessageHostFactory> factory)
      : name(std::move(name)),
        allowed_origin_rules(std::move(allowed_origin_rules)),
        factory(std::move(factory)) {}
  JsObject(JsObject&& other) = delete;
  JsObject& operator=(JsObject&& other) = delete;
  ~JsObject() = default;

  std::u16string name;
  OriginMatcher allowed_origin_rules;
  std::unique_ptr<WebMessageHostFactory> factory;
};

struct DocumentStartJavaScript {
  DocumentStartJavaScript(std::u16string script,
                          OriginMatcher allowed_origin_rules,
                          int32_t script_id)
      : script_(std::move(script)),
        allowed_origin_rules_(allowed_origin_rules),
        script_id_(script_id) {}

  DocumentStartJavaScript(DocumentStartJavaScript&) = delete;
  DocumentStartJavaScript& operator=(DocumentStartJavaScript&) = delete;
  DocumentStartJavaScript(DocumentStartJavaScript&&) = default;
  DocumentStartJavaScript& operator=(DocumentStartJavaScript&&) = default;

  std::u16string script_;
  OriginMatcher allowed_origin_rules_;
  int32_t script_id_;
};

JsCommunicationHost::AddScriptResult::AddScriptResult() = default;
JsCommunicationHost::AddScriptResult::AddScriptResult(
    const JsCommunicationHost::AddScriptResult&) = default;
JsCommunicationHost::AddScriptResult&
JsCommunicationHost::AddScriptResult::operator=(
    const JsCommunicationHost::AddScriptResult&) = default;
JsCommunicationHost::AddScriptResult::~AddScriptResult() = default;

JsCommunicationHost::JsCommunicationHost(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

JsCommunicationHost::~JsCommunicationHost() = default;

JsCommunicationHost::AddScriptResult
JsCommunicationHost::AddDocumentStartJavaScript(
    const std::u16string& script,
    const std::vector<std::string>& allowed_origin_rules) {
  OriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      allowed_origin_rules, origin_matcher);
  AddScriptResult result;
  if (!error_message.empty()) {
    result.error_message = std::move(error_message);
    return result;
  }

  scripts_.emplace_back(script, origin_matcher, next_script_id_++);

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsCommunicationHost::NotifyFrameForAddDocumentStartJavaScript,
      base::Unretained(this), &*scripts_.rbegin()));
  result.script_id = scripts_.rbegin()->script_id_;
  return result;
}

bool JsCommunicationHost::RemoveDocumentStartJavaScript(int script_id) {
  for (auto it = scripts_.begin(); it != scripts_.end(); ++it) {
    if (it->script_id_ == script_id) {
      scripts_.erase(it);
      web_contents()->ForEachFrame(base::BindRepeating(
          &JsCommunicationHost::NotifyFrameForRemoveDocumentStartJavaScript,
          base::Unretained(this), script_id));
      return true;
    }
  }
  return false;
}

std::u16string JsCommunicationHost::AddWebMessageHostFactory(
    std::unique_ptr<WebMessageHostFactory> factory,
    const std::u16string& js_object_name,
    const std::vector<std::string>& allowed_origin_rules) {
  OriginMatcher origin_matcher;
  std::string error_message = ConvertToNativeAllowedOriginRulesWithSanityCheck(
      allowed_origin_rules, origin_matcher);
  if (!error_message.empty())
    return base::UTF8ToUTF16(error_message);

  for (const auto& js_object : js_objects_) {
    if (js_object->name == js_object_name) {
      return u"jsObjectName " + js_object->name + u" was already added.";
    }
  }

  js_objects_.push_back(std::make_unique<JsObject>(
      js_object_name, origin_matcher, std::move(factory)));

  web_contents()->ForEachFrame(base::BindRepeating(
      &JsCommunicationHost::NotifyFrameForWebMessageListener,
      base::Unretained(this)));
  return std::u16string();
}

void JsCommunicationHost::RemoveWebMessageHostFactory(
    const std::u16string& js_object_name) {
  for (auto iterator = js_objects_.begin(); iterator != js_objects_.end();
       ++iterator) {
    if ((*iterator)->name == js_object_name) {
      js_objects_.erase(iterator);
      web_contents()->ForEachFrame(base::BindRepeating(
          &JsCommunicationHost::NotifyFrameForWebMessageListener,
          base::Unretained(this)));
      break;
    }
  }
}

std::vector<JsCommunicationHost::RegisteredFactory>
JsCommunicationHost::GetWebMessageHostFactories() {
  const size_t num_objects = js_objects_.size();
  std::vector<RegisteredFactory> factories(num_objects);
  for (size_t i = 0; i < num_objects; ++i) {
    factories[i].js_name = js_objects_[i]->name;
    factories[i].allowed_origin_rules = js_objects_[i]->allowed_origin_rules;
    factories[i].factory = js_objects_[i]->factory.get();
  }
  return factories;
}

void JsCommunicationHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  NotifyFrameForWebMessageListener(render_frame_host);
  NotifyFrameForAllDocumentStartJavaScripts(render_frame_host);
}

void JsCommunicationHost::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  js_to_browser_messagings_.erase(render_frame_host);
}

void JsCommunicationHost::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  auto iter = js_to_browser_messagings_.find(render_frame_host);
  if (iter == js_to_browser_messagings_.end())
    return;

  using LifecycleState = content::RenderFrameHost::LifecycleState;
  if (old_state == LifecycleState::kInBackForwardCache ||
      new_state == LifecycleState::kInBackForwardCache) {
    for (auto& js_to_browser_messaging_ptr : iter->second)
      js_to_browser_messaging_ptr->OnBackForwardCacheStateChanged();
  }
}

void JsCommunicationHost::NotifyFrameForAllDocumentStartJavaScripts(
    content::RenderFrameHost* render_frame_host) {
  for (const auto& script : scripts_) {
    NotifyFrameForAddDocumentStartJavaScript(&script, render_frame_host);
  }
}

void JsCommunicationHost::NotifyFrameForWebMessageListener(
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  std::vector<mojom::JsObjectPtr> js_objects;
  js_objects.reserve(js_objects_.size());
  for (const auto& js_object : js_objects_) {
    mojo::PendingAssociatedRemote<mojom::JsToBrowserMessaging> pending_remote;
    js_to_browser_messagings_[render_frame_host].emplace_back(
        std::make_unique<JsToBrowserMessaging>(
            render_frame_host,
            pending_remote.InitWithNewEndpointAndPassReceiver(),
            js_object->factory.get(), js_object->allowed_origin_rules));
    js_objects.push_back(mojom::JsObject::New(js_object->name,
                                              std::move(pending_remote),
                                              js_object->allowed_origin_rules));
  }
  configurator_remote->SetJsObjects(std::move(js_objects));
}

void JsCommunicationHost::NotifyFrameForAddDocumentStartJavaScript(
    const DocumentStartJavaScript* script,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(script);
  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->AddDocumentStartScript(
      mojom::DocumentStartJavaScript::New(script->script_id_, script->script_,
                                          script->allowed_origin_rules_));
}

void JsCommunicationHost::NotifyFrameForRemoveDocumentStartJavaScript(
    int32_t script_id,
    content::RenderFrameHost* render_frame_host) {
  mojo::AssociatedRemote<mojom::JsCommunication> configurator_remote;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &configurator_remote);
  configurator_remote->RemoveDocumentStartScript(script_id);
}

}  // namespace js_injection
