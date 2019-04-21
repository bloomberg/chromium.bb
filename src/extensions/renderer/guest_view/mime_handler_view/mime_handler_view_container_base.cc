// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_base.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/mojo/guest_view.mojom.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "ipc/ipc_sync_channel.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"

namespace extensions {
using UMAType = MimeHandlerViewUMATypes::Type;

namespace {

const char kPostMessageName[] = "postMessage";

base::LazyInstance<mojom::GuestViewAssociatedPtr>::Leaky g_guest_view;

mojom::GuestView* GetGuestView() {
  if (!g_guest_view.Get()) {
    content::RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &g_guest_view.Get());
  }

  return g_guest_view.Get().get();
}

// The gin-backed scriptable object which is exposed by the BrowserPlugin for
// MimeHandlerViewContainerBase. This currently only implements "postMessage".
class ScriptableObject : public gin::Wrappable<ScriptableObject>,
                         public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static v8::Local<v8::Object> Create(
      v8::Isolate* isolate,
      base::WeakPtr<MimeHandlerViewContainerBase> container) {
    ScriptableObject* scriptable_object =
        new ScriptableObject(isolate, container);
    return gin::CreateHandle(isolate, scriptable_object)
        .ToV8()
        .As<v8::Object>();
  }

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(
      v8::Isolate* isolate,
      const std::string& identifier) override {
    if (identifier == kPostMessageName) {
      if (post_message_function_template_.IsEmpty()) {
        post_message_function_template_.Reset(
            isolate,
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(
                    &MimeHandlerViewContainerBase::PostJavaScriptMessage,
                    container_, isolate)));
      }
      v8::Local<v8::FunctionTemplate> function_template =
          v8::Local<v8::FunctionTemplate>::New(isolate,
                                               post_message_function_template_);
      v8::Local<v8::Function> function;
      if (function_template->GetFunction(isolate->GetCurrentContext())
              .ToLocal(&function))
        return function;
    }
    return v8::Local<v8::Value>();
  }

 private:
  ScriptableObject(v8::Isolate* isolate,
                   base::WeakPtr<MimeHandlerViewContainerBase> container)
      : gin::NamedPropertyInterceptor(isolate, this), container_(container) {}

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<ScriptableObject>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor();
  }

  base::WeakPtr<MimeHandlerViewContainerBase> container_;
  v8::Persistent<v8::FunctionTemplate> post_message_function_template_;
};

// static
gin::WrapperInfo ScriptableObject::kWrapperInfo = {gin::kEmbedderNativeGin};

// Maps from content::RenderFrame to the set of MimeHandlerViewContainerBases
//  within it.
base::LazyInstance<
    std::map<content::RenderFrame*, std::set<MimeHandlerViewContainerBase*>>>::
    DestructorAtExit g_mime_handler_view_container_base_map =
        LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Stores a raw pointer to MimeHandlerViewContainerBase since this throttle's
// lifetime is shorter (it matches |container|'s loader_).
class MimeHandlerViewContainerBase::PluginResourceThrottle
    : public content::URLLoaderThrottle {
 public:
  explicit PluginResourceThrottle(
      base::WeakPtr<MimeHandlerViewContainerBase> container)
      : container_(container) {}
  ~PluginResourceThrottle() override {}

 private:
  // content::URLLoaderThrottle overrides;
  void WillProcessResponse(const GURL& response_url,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override {
    if (!container_) {
      // In the embedder case if the plugin element is removed right after an
      // ongoing request is made, MimeHandlerViewContainerBase is destroyed
      // synchronously but the WebURLLoaderImpl corresponding to this throttle
      // goes away asynchronously when ResourceLoader::CancelTimerFired() is
      // called (see https://crbug.com/878359).
      return;
    }
    network::mojom::URLLoaderPtr dummy_new_loader;
    mojo::MakeRequest(&dummy_new_loader);
    network::mojom::URLLoaderClientPtr new_client;
    network::mojom::URLLoaderClientRequest new_client_request =
        mojo::MakeRequest(&new_client);

    network::mojom::URLLoaderPtr original_loader;
    network::mojom::URLLoaderClientRequest original_client;
    delegate_->InterceptResponse(std::move(dummy_new_loader),
                                 std::move(new_client_request),
                                 &original_loader, &original_client);

    auto transferrable_loader = content::mojom::TransferrableURLLoader::New();
    transferrable_loader->url_loader = original_loader.PassInterface();
    transferrable_loader->url_loader_client = std::move(original_client);

    // Make a deep copy of ResourceResponseHead before passing it cross-thread.
    auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
    resource_response->head = *response_head;
    auto deep_copied_response = resource_response->DeepCopy();
    transferrable_loader->head = std::move(deep_copied_response->head);
    container_->SetEmbeddedLoader(std::move(transferrable_loader));
  }

  base::WeakPtr<MimeHandlerViewContainerBase> container_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceThrottle);
};

MimeHandlerViewContainerBase::MimeHandlerViewContainerBase(
    content::RenderFrame* embedder_render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url)
    : original_url_(original_url),
      plugin_path_(info.path.MaybeAsASCII()),
      mime_type_(mime_type),
      embedder_render_frame_routing_id_(embedder_render_frame->GetRoutingID()),
      before_unload_control_binding_(this),
      resource_access_type_(
          embedder_render_frame->GetWebFrame()
                  ->GetDocument()
                  .GetSecurityOrigin()
                  .CanAccess(blink::WebSecurityOrigin::Create(original_url))
              ? ResourceAccessType::kAccessible
              : ResourceAccessType::kInaccessible),
      weak_factory_(this) {
  DCHECK(!mime_type_.empty());
  g_mime_handler_view_container_base_map.Get()[embedder_render_frame].insert(
      this);
}

MimeHandlerViewContainerBase::~MimeHandlerViewContainerBase() {
  if (loader_) {
    DCHECK(is_embedded_);
    loader_->Cancel();
  }

  if (auto* rf = GetEmbedderRenderFrame()) {
    g_mime_handler_view_container_base_map.Get()[rf].erase(this);
    if (g_mime_handler_view_container_base_map.Get()[rf].empty())
      g_mime_handler_view_container_base_map.Get().erase(rf);
  }
}

// static
std::vector<MimeHandlerViewContainerBase*>
MimeHandlerViewContainerBase::FromRenderFrame(
    content::RenderFrame* render_frame) {
  auto it = g_mime_handler_view_container_base_map.Get().find(render_frame);
  if (it == g_mime_handler_view_container_base_map.Get().end())
    return std::vector<MimeHandlerViewContainerBase*>();

  return std::vector<MimeHandlerViewContainerBase*>(it->second.begin(),
                                                    it->second.end());
}

std::unique_ptr<content::URLLoaderThrottle>
MimeHandlerViewContainerBase::MaybeCreatePluginThrottle(const GURL& url) {
  if (!waiting_to_create_throttle_ || url != original_url_)
    return nullptr;

  waiting_to_create_throttle_ = false;
  return std::make_unique<PluginResourceThrottle>(weak_factory_.GetWeakPtr());
}

void MimeHandlerViewContainerBase::PostJavaScriptMessage(
    v8::Isolate* isolate,
    v8::Local<v8::Value> message) {
  if (should_report_internal_messages_)
    RecordUMAForPostMessage(message);
  if (!guest_loaded_) {
    pending_messages_.push_back(v8::Global<v8::Value>(isolate, message));
    return;
  }

  auto* guest_proxy_frame = GetGuestProxyFrame();

  v8::Context::Scope context_scope(
      GetEmbedderRenderFrame()->GetWebFrame()->MainWorldScriptContext());

  v8::Local<v8::Object> guest_proxy_window = guest_proxy_frame->GlobalProxy();
  gin::Dictionary window_object(isolate, guest_proxy_window);
  v8::Local<v8::Function> post_message;
  if (!window_object.Get(std::string(kPostMessageName), &post_message))
    return;

  v8::Local<v8::Value> args[] = {
      message,
      // Post the message to any domain inside the browser plugin. The embedder
      // should already know what is embedded.
      gin::StringToV8(isolate, "*")};
  GetEmbedderRenderFrame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      post_message.As<v8::Function>(), guest_proxy_window, base::size(args),
      args);
}

void MimeHandlerViewContainerBase::PostMessageFromValue(
    const base::Value& message) {
  blink::WebLocalFrame* frame = GetEmbedderRenderFrame()->GetWebFrame();
  if (!frame)
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->MainWorldScriptContext());
  base::AutoReset<bool> avoid_recording_internal_messages(
      &should_report_internal_messages_, false);
  PostJavaScriptMessage(isolate,
                        content::V8ValueConverter::Create()->ToV8Value(
                            &message, frame->MainWorldScriptContext()));
}

void MimeHandlerViewContainerBase::DidReceiveData(const char* data,
                                                  int data_length) {
  view_id_ += std::string(data, data_length);
}

void MimeHandlerViewContainerBase::DidFinishLoading() {
  DCHECK(is_embedded_);
  // Warning: It is possible that |this| gets destroyed after this line (when
  // the MHVCB is of the frame type and the associated plugin element does not
  // have a content frame).
  CreateMimeHandlerViewGuestIfNecessary();
}

content::RenderFrame* MimeHandlerViewContainerBase::GetEmbedderRenderFrame()
    const {
  DCHECK_NE(embedder_render_frame_routing_id_, MSG_ROUTING_NONE);
  return content::RenderFrame::FromRoutingID(embedder_render_frame_routing_id_);
}

void MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary() {
  if (guest_created_)
    return;
  auto* embedder_render_frame = GetEmbedderRenderFrame();
  if (!embedder_render_frame) {
    // TODO(ekaramad): How can this happen? We should destroy the container if
    // this happens at all. The process is however different for a plugin-based
    // container.
    return;
  }

  auto* guest_view = GetGuestView();
  // When the network service is enabled, subresource requests like plugins are
  // made directly from the renderer to the network service. So we need to
  // intercept the URLLoader and send it to the browser so that it can forward
  // it to the plugin.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      is_embedded_) {
    if (transferrable_url_loader_.is_null())
      return;

    auto* extension_frame_helper =
        ExtensionFrameHelper::Get(embedder_render_frame);
    if (!extension_frame_helper)
      return;

    guest_view->CreateEmbeddedMimeHandlerViewGuest(
        embedder_render_frame->GetRoutingID(), extension_frame_helper->tab_id(),
        original_url_, GetInstanceId(), GetElementSize(),
        std::move(transferrable_url_loader_), plugin_frame_routing_id_);
    guest_created_ = true;
    return;
  }

  if (view_id_.empty())
    return;

  // The loader has completed loading |view_id_| so we can dispose it.
  if (loader_) {
    DCHECK(is_embedded_);
    loader_.reset();
  }

  DCHECK_NE(GetInstanceId(), guest_view::kInstanceIDNone);

  mime_handler::BeforeUnloadControlPtr before_unload_control;
  if (!is_embedded_) {
    before_unload_control_binding_.Bind(
        mojo::MakeRequest(&before_unload_control));
  }
  guest_view->CreateMimeHandlerViewGuest(
      embedder_render_frame->GetRoutingID(), view_id_, GetInstanceId(),
      GetElementSize(), std::move(before_unload_control),
      plugin_frame_routing_id_);

  guest_created_ = true;
}

void MimeHandlerViewContainerBase::DidLoadInternal() {
  RecordInteraction(UMAType::kDidLoadExtension);
  if (!GetEmbedderRenderFrame())
    return;

  guest_loaded_ = true;
  if (pending_messages_.empty())
    return;

  // Now that the guest has loaded, flush any unsent messages.
  blink::WebLocalFrame* frame = GetEmbedderRenderFrame()->GetWebFrame();
  if (!frame)
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->MainWorldScriptContext());
  for (const auto& pending_message : pending_messages_)
    PostJavaScriptMessage(isolate,
                          v8::Local<v8::Value>::New(isolate, pending_message));

  pending_messages_.clear();
}

void MimeHandlerViewContainerBase::SendResourceRequest() {
  blink::WebLocalFrame* frame = GetEmbedderRenderFrame()->GetWebFrame();

  blink::WebAssociatedURLLoaderOptions options;
  DCHECK(!loader_);
  loader_.reset(frame->CreateAssociatedURLLoader(options));

  // The embedded plugin is allowed to be cross-origin and we should always
  // send credentials/cookies with the request. So, use the default mode
  // "no-cors" and credentials mode "include".
  blink::WebURLRequest request(original_url_);
  request.SetRequestContext(blink::mojom::RequestContextType::OBJECT);
  // The plugin resource request should skip service workers since "plug-ins
  // may get their security origins from their own urls".
  // https://w3c.github.io/ServiceWorker/#implementer-concerns
  request.SetSkipServiceWorker(true);

  waiting_to_create_throttle_ = true;
  loader_->LoadAsynchronously(request, this);
}

void MimeHandlerViewContainerBase::EmbedderRenderFrameWillBeGone() {
  g_mime_handler_view_container_base_map.Get().erase(GetEmbedderRenderFrame());
}

void MimeHandlerViewContainerBase::SetEmbeddedLoader(
    content::mojom::TransferrableURLLoaderPtr transferrable_url_loader) {
  transferrable_url_loader_ = std::move(transferrable_url_loader);
  transferrable_url_loader_->url = GURL(plugin_path_ + base::GenerateGUID());
  // Warning: It is possible that |this| gets destroyed after this line (when
  // the MHVCB is of the frame type and the associated plugin element does not
  // have a content frame).
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewContainerBase::RecordUMAForPostMessage(
    v8::Local<v8::Value>& message) {
  auto data = content::V8ValueConverter::Create()->FromV8Value(
      message,
      GetEmbedderRenderFrame()->GetWebFrame()->MainWorldScriptContext());
  std::string message_type;
  if (data->is_dict()) {
    base::DictionaryValue::From(std::move(data))
        ->GetString("type", &message_type);
  }

  bool accessible = resource_access_type_ == ResourceAccessType::kAccessible;
  MimeHandlerViewUMATypes::Type post_message_type;
  if (message_type == "getSelectedText") {
    post_message_type = accessible ? UMAType::kAccessibleGetSelectedText
                                   : UMAType::kInaccessibleGetSelectedText;
  } else if (message_type == "print") {
    post_message_type =
        accessible ? UMAType::kAccessiblePrint : UMAType::kInaccessiblePrint;
  } else if (message_type == "selectAll") {
    post_message_type = accessible ? UMAType::kAccessibleSelectAll
                                   : UMAType::kInaccessibleSelectAll;
  } else {
    post_message_type = accessible ? UMAType::kAccessibleInvalid
                                   : UMAType::kInaccessibleInvalid;
  }
  DCHECK_NE(post_message_type, UMAType::kDidCreateMimeHandlerViewContainerBase);
  RecordInteraction(post_message_type);
  if (is_embedded_)
    RecordInteraction(UMAType::kPostMessageToEmbeddedMimeHandlerView);
}

void MimeHandlerViewContainerBase::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  DCHECK(!is_embedded_);
  GetEmbedderRenderFrame()
      ->GetWebFrame()
      ->GetDocument()
      .SetShowBeforeUnloadDialog(show_dialog);
  std::move(callback).Run();
}

v8::Local<v8::Object> MimeHandlerViewContainerBase::GetScriptableObjectInternal(
    v8::Isolate* isolate) {
  if (scriptable_object_.IsEmpty()) {
    v8::Local<v8::Object> object =
        ScriptableObject::Create(isolate, weak_factory_.GetWeakPtr());
    scriptable_object_.Reset(isolate, object);
  }
  return v8::Local<v8::Object>::New(isolate, scriptable_object_);
}

void MimeHandlerViewContainerBase::RecordInteraction(UMAType uma_type) {
  base::UmaHistogramEnumeration(MimeHandlerViewUMATypes::kUMAName, uma_type);
}

}  // namespace extensions
