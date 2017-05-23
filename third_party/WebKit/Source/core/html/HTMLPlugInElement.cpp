/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/html/HTMLPlugInElement.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/PluginDocument.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutEmbeddedItem.h"
#include "core/loader/MixedContentChecker.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/plugins/PluginView.h"
#include "platform/Histogram.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/mime/MIMETypeFromURL.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Used for histograms, do not change the order.
enum PluginRequestObjectResult {
  kPluginRequestObjectResultFailure = 0,
  kPluginRequestObjectResultSuccess = 1,
  // Keep at the end.
  kPluginRequestObjectResultMax
};

}  // anonymous namespace

HTMLPlugInElement::HTMLPlugInElement(
    const QualifiedName& tag_name,
    Document& doc,
    bool created_by_parser,
    PreferPlugInsForImagesOption prefer_plug_ins_for_images_option)
    : HTMLFrameOwnerElement(tag_name, doc),
      is_delaying_load_event_(false),
      // m_needsPluginUpdate(!createdByParser) allows HTMLObjectElement to delay
      // FrameViewBase updates until after all children are parsed. For
      // HTMLEmbedElement this delay is unnecessary, but it is simpler to make
      // both classes share the same codepath in this class.
      needs_plugin_update_(!created_by_parser),
      should_prefer_plug_ins_for_images_(prefer_plug_ins_for_images_option ==
                                         kShouldPreferPlugInsForImages) {}

HTMLPlugInElement::~HTMLPlugInElement() {
  DCHECK(plugin_wrapper_.IsEmpty());  // cleared in detachLayoutTree()
  DCHECK(!is_delaying_load_event_);
}

DEFINE_TRACE(HTMLPlugInElement) {
  visitor->Trace(image_loader_);
  visitor->Trace(persisted_plugin_);
  HTMLFrameOwnerElement::Trace(visitor);
}

void HTMLPlugInElement::SetPersistedPlugin(PluginView* plugin) {
  if (persisted_plugin_ == plugin)
    return;
  if (persisted_plugin_) {
    persisted_plugin_->Hide();
    DisposeFrameOrPluginSoon(persisted_plugin_.Release());
  }
  persisted_plugin_ = plugin;
}

void HTMLPlugInElement::SetFocused(bool focused, WebFocusType focus_type) {
  PluginView* plugin = OwnedPlugin();
  if (plugin)
    plugin->SetFocused(focused, focus_type);
  HTMLFrameOwnerElement::SetFocused(focused, focus_type);
}

bool HTMLPlugInElement::RequestObjectInternal(
    const String& url,
    const String& mime_type,
    const Vector<String>& param_names,
    const Vector<String>& param_values) {
  if (url.IsEmpty() && mime_type.IsEmpty())
    return false;

  if (ProtocolIsJavaScript(url))
    return false;

  KURL completed_url = url.IsEmpty() ? KURL() : GetDocument().CompleteURL(url);
  if (!AllowedToLoadObject(completed_url, mime_type))
    return false;

  bool use_fallback;
  if (!ShouldUsePlugin(completed_url, mime_type, HasFallbackContent(),
                       use_fallback)) {
    // If the plugin element already contains a subframe,
    // loadOrRedirectSubframe will re-use it. Otherwise, it will create a
    // new frame and set it as the LayoutPart's FrameViewBase, causing what was
    // previously in the FrameViewBase to be torn down.
    return LoadOrRedirectSubframe(completed_url, GetNameAttribute(), true);
  }

  return LoadPlugin(completed_url, mime_type, param_names, param_values,
                    use_fallback, true);
}

bool HTMLPlugInElement::CanProcessDrag() const {
  return PluginWidget() && PluginWidget()->CanProcessDrag();
}

bool HTMLPlugInElement::CanStartSelection() const {
  return UseFallbackContent() && Node::CanStartSelection();
}

bool HTMLPlugInElement::WillRespondToMouseClickEvents() {
  if (IsDisabledFormControl())
    return false;
  LayoutObject* r = GetLayoutObject();
  return r && (r->IsEmbeddedObject() || r->IsLayoutPart());
}

void HTMLPlugInElement::RemoveAllEventListeners() {
  HTMLFrameOwnerElement::RemoveAllEventListeners();
  PluginView* plugin = OwnedPlugin();
  if (plugin)
    plugin->EventListenersRemoved();
}

void HTMLPlugInElement::DidMoveToNewDocument(Document& old_document) {
  if (image_loader_)
    image_loader_->ElementDidMoveToNewDocument();
  HTMLFrameOwnerElement::DidMoveToNewDocument(old_document);
}

void HTMLPlugInElement::AttachLayoutTree(const AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);

  if (!GetLayoutObject() || UseFallbackContent()) {
    // If we don't have a layoutObject we have to dispose of any plugins
    // which we persisted over a reattach.
    if (persisted_plugin_) {
      HTMLFrameOwnerElement::UpdateSuspendScope
          suspend_widget_hierarchy_updates;
      SetPersistedPlugin(nullptr);
    }
    return;
  }

  if (IsImageType()) {
    if (!image_loader_)
      image_loader_ = HTMLImageLoader::Create(this);
    image_loader_->UpdateFromElement();
  } else if (NeedsPluginUpdate() && !GetLayoutEmbeddedItem().IsNull() &&
             !GetLayoutEmbeddedItem().ShowsUnavailablePluginIndicator() &&
             !WouldLoadAsNetscapePlugin(url_, service_type_) &&
             !is_delaying_load_event_) {
    is_delaying_load_event_ = true;
    GetDocument().IncrementLoadEventDelayCount();
    GetDocument().LoadPluginsSoon();
  }
}

void HTMLPlugInElement::UpdatePlugin() {
  UpdatePluginInternal();
  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }
}

void HTMLPlugInElement::RemovedFrom(ContainerNode* insertion_point) {
  // If we've persisted the plugin and we're removed from the tree then
  // make sure we cleanup the persistance pointer.
  if (persisted_plugin_) {
    // TODO(dcheng): This UpdateSuspendScope doesn't seem to provide much;
    // investigate removing it.
    HTMLFrameOwnerElement::UpdateSuspendScope suspend_widget_hierarchy_updates;
    SetPersistedPlugin(nullptr);
  }
  HTMLFrameOwnerElement::RemovedFrom(insertion_point);
}

void HTMLPlugInElement::RequestPluginCreationWithoutLayoutObjectIfPossible() {
  if (service_type_.IsEmpty())
    return;

  if (!GetDocument().GetFrame() ||
      !GetDocument()
           .GetFrame()
           ->Loader()
           .Client()
           ->CanCreatePluginWithoutRenderer(service_type_))
    return;

  if (GetLayoutObject() && GetLayoutObject()->IsLayoutPart())
    return;

  CreatePluginWithoutLayoutObject();
}

void HTMLPlugInElement::CreatePluginWithoutLayoutObject() {
  DCHECK(GetDocument()
             .GetFrame()
             ->Loader()
             .Client()
             ->CanCreatePluginWithoutRenderer(service_type_));

  KURL url;
  // CSP can block src-less objects.
  if (!AllowedToLoadObject(url, service_type_))
    return;

  Vector<String> param_names;
  Vector<String> param_values;

  param_names.push_back("type");
  param_values.push_back(service_type_);

  bool use_fallback = false;
  LoadPlugin(url, service_type_, param_names, param_values, use_fallback,
             false);
}

bool HTMLPlugInElement::ShouldAccelerate() const {
  PluginView* plugin = OwnedPlugin();
  return plugin && plugin->PlatformLayer();
}

void HTMLPlugInElement::DetachLayoutTree(const AttachContext& context) {
  // Update the FrameViewBase the next time we attach (detaching destroys the
  // plugin).
  // FIXME: None of this "needsPluginUpdate" related code looks right.
  if (GetLayoutObject() && !UseFallbackContent())
    SetNeedsPluginUpdate(true);

  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }

  // Only try to persist a plugin we actually own.
  PluginView* plugin = OwnedPlugin();
  if (plugin && context.performing_reattach) {
    SetPersistedPlugin(ToPluginView(ReleaseWidget()));
  } else {
    // Clear the plugin; will trigger disposal of it with Oilpan.
    SetWidget(nullptr);
  }

  ResetInstance();

  HTMLFrameOwnerElement::DetachLayoutTree(context);
}

LayoutObject* HTMLPlugInElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // Fallback content breaks the DOM->layoutObject class relationship of this
  // class and all superclasses because createObject won't necessarily return
  // a LayoutEmbeddedObject or LayoutPart.
  if (UseFallbackContent())
    return LayoutObject::CreateObject(this, style);

  if (IsImageType()) {
    LayoutImage* image = new LayoutImage(this);
    image->SetImageResource(LayoutImageResource::Create());
    return image;
  }

  plugin_is_available_ = true;
  return new LayoutEmbeddedObject(this);
}

void HTMLPlugInElement::FinishParsingChildren() {
  HTMLFrameOwnerElement::FinishParsingChildren();
  if (UseFallbackContent())
    return;

  SetNeedsPluginUpdate(true);
  if (isConnected())
    LazyReattachIfNeeded();
}

void HTMLPlugInElement::ResetInstance() {
  plugin_wrapper_.Reset();
}

v8::Local<v8::Object> HTMLPlugInElement::PluginWrapper() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return v8::Local<v8::Object>();

  // If the host dynamically turns off JavaScript (or Java) we will still
  // return the cached allocated Bindings::Instance. Not supporting this
  // edge-case is OK.
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  if (plugin_wrapper_.IsEmpty()) {
    PluginView* plugin;

    if (persisted_plugin_)
      plugin = persisted_plugin_;
    else
      plugin = PluginWidget();

    if (plugin)
      plugin_wrapper_.Reset(isolate, plugin->ScriptableObject(isolate));
  }
  return plugin_wrapper_.Get(isolate);
}

PluginView* HTMLPlugInElement::PluginWidget() const {
  if (LayoutPart* layout_part = LayoutPartForJSBindings())
    return layout_part->Plugin();
  return nullptr;
}

PluginView* HTMLPlugInElement::OwnedPlugin() const {
  FrameOrPlugin* frame_or_plugin = OwnedWidget();
  if (frame_or_plugin && frame_or_plugin->IsPluginView())
    return ToPluginView(frame_or_plugin);
  return nullptr;
}

bool HTMLPlugInElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == vspaceAttr ||
      name == hspaceAttr || name == alignAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLPlugInElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == vspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (name == hspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (name == alignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else {
    HTMLFrameOwnerElement::CollectStyleForPresentationAttribute(name, value,
                                                                style);
  }
}

void HTMLPlugInElement::DefaultEventHandler(Event* event) {
  // Firefox seems to use a fake event listener to dispatch events to plugin
  // (tested with mouse events only). This is observable via different order
  // of events - in Firefox, event listeners specified in HTML attributes
  // fires first, then an event gets dispatched to plugin, and only then
  // other event listeners fire. Hopefully, this difference does not matter in
  // practice.

  // FIXME: Mouse down and scroll events are passed down to plugin via custom
  // code in EventHandler; these code paths should be united.

  LayoutObject* r = GetLayoutObject();
  if (!r || !r->IsLayoutPart())
    return;
  if (r->IsEmbeddedObject()) {
    if (LayoutEmbeddedItem(ToLayoutEmbeddedObject(r))
            .ShowsUnavailablePluginIndicator())
      return;
  }
  PluginView* plugin = OwnedPlugin();
  if (!plugin)
    return;
  plugin->HandleEvent(event);
  if (event->DefaultHandled())
    return;
  HTMLFrameOwnerElement::DefaultEventHandler(event);
}

LayoutPart* HTMLPlugInElement::LayoutPartForJSBindings() const {
  // Needs to load the plugin immediatedly because this function is called
  // when JavaScript code accesses the plugin.
  // FIXME: Check if dispatching events here is safe.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets(
      Document::kRunPostLayoutTasksSynchronously);
  return ExistingLayoutPart();
}

bool HTMLPlugInElement::IsKeyboardFocusable() const {
  if (HTMLFrameOwnerElement::IsKeyboardFocusable())
    return true;
  return GetDocument().IsActive() && PluginWidget() &&
         PluginWidget()->SupportsKeyboardFocus();
}

bool HTMLPlugInElement::HasCustomFocusLogic() const {
  return !UseFallbackContent();
}

bool HTMLPlugInElement::IsPluginElement() const {
  return true;
}

bool HTMLPlugInElement::IsErrorplaceholder() {
  if (PluginWidget() && PluginWidget()->IsPluginContainer() &&
      PluginWidget()->IsErrorplaceholder())
    return true;
  return false;
}

void HTMLPlugInElement::DisconnectContentFrame() {
  HTMLFrameOwnerElement::DisconnectContentFrame();
  SetPersistedPlugin(nullptr);
}

bool HTMLPlugInElement::LayoutObjectIsFocusable() const {
  if (HTMLFrameOwnerElement::SupportsFocus() &&
      HTMLFrameOwnerElement::LayoutObjectIsFocusable())
    return true;

  if (UseFallbackContent() || !HTMLFrameOwnerElement::LayoutObjectIsFocusable())
    return false;
  return plugin_is_available_;
}

bool HTMLPlugInElement::IsImageType() {
  if (service_type_.IsEmpty() && ProtocolIs(url_, "data"))
    service_type_ = MimeTypeFromDataURL(url_);

  if (LocalFrame* frame = GetDocument().GetFrame()) {
    KURL completed_url = GetDocument().CompleteURL(url_);
    return frame->Loader().Client()->GetObjectContentType(
               completed_url, service_type_, ShouldPreferPlugInsForImages()) ==
           kObjectContentImage;
  }

  return Image::SupportsType(service_type_);
}

LayoutEmbeddedItem HTMLPlugInElement::GetLayoutEmbeddedItem() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!GetLayoutObject() || !GetLayoutObject()->IsEmbeddedObject())
    return LayoutEmbeddedItem(nullptr);
  return LayoutEmbeddedItem(ToLayoutEmbeddedObject(GetLayoutObject()));
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::AllowedToLoadFrameURL(const String& url) {
  KURL complete_url = GetDocument().CompleteURL(url);
  return !(ContentFrame() && complete_url.ProtocolIsJavaScript() &&
           !GetDocument().GetSecurityOrigin()->CanAccess(
               ContentFrame()->GetSecurityContext()->GetSecurityOrigin()));
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInElement::WouldLoadAsNetscapePlugin(const String& url,
                                                  const String& service_type) {
  DCHECK(GetDocument().GetFrame());
  KURL completed_url;
  if (!url.IsEmpty())
    completed_url = GetDocument().CompleteURL(url);
  return GetDocument().GetFrame()->Loader().Client()->GetObjectContentType(
             completed_url, service_type, ShouldPreferPlugInsForImages()) ==
         kObjectContentNetscapePlugin;
}

bool HTMLPlugInElement::RequestObject(const String& url,
                                      const String& mime_type,
                                      const Vector<String>& param_names,
                                      const Vector<String>& param_values) {
  bool result =
      RequestObjectInternal(url, mime_type, param_names, param_values);

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, result_histogram,
      ("Plugin.RequestObjectResult", kPluginRequestObjectResultMax));
  result_histogram.Count(result ? kPluginRequestObjectResultSuccess
                                : kPluginRequestObjectResultFailure);

  return result;
}

bool HTMLPlugInElement::LoadPlugin(const KURL& url,
                                   const String& mime_type,
                                   const Vector<String>& param_names,
                                   const Vector<String>& param_values,
                                   bool use_fallback,
                                   bool require_layout_object) {
  if (!AllowedToLoadPlugin(url, mime_type))
    return false;

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame->Loader().AllowPlugins(kAboutToInstantiatePlugin))
    return false;

  LayoutEmbeddedItem layout_item = GetLayoutEmbeddedItem();
  // FIXME: This code should not depend on layoutObject!
  if ((layout_item.IsNull() && require_layout_object) || use_fallback)
    return false;

  VLOG(1) << this << " Plugin URL: " << url_;
  VLOG(1) << "Loaded URL: " << url.GetString();
  loaded_url_ = url;

  if (persisted_plugin_) {
    SetWidget(persisted_plugin_.Release());
  } else {
    bool load_manually =
        GetDocument().IsPluginDocument() && !GetDocument().ContainsPlugins();
    LocalFrameClient::DetachedPluginPolicy policy =
        require_layout_object ? LocalFrameClient::kFailOnDetachedPlugin
                              : LocalFrameClient::kAllowDetachedPlugin;
    PluginView* plugin = frame->Loader().Client()->CreatePlugin(
        this, url, param_names, param_values, mime_type, load_manually, policy);
    if (!plugin) {
      if (!layout_item.IsNull() &&
          !layout_item.ShowsUnavailablePluginIndicator()) {
        plugin_is_available_ = false;
        layout_item.SetPluginAvailability(LayoutEmbeddedObject::kPluginMissing);
      }
      return false;
    }

    if (!layout_item.IsNull()) {
      SetWidget(plugin);
      layout_item.GetFrameView()->AddPlugin(plugin);
    } else {
      SetPersistedPlugin(plugin);
    }
  }

  GetDocument().SetContainsPlugins();
  // TODO(esprehn): WebPluginContainerImpl::setWebLayer also schedules a
  // compositing update, do we need both?
  SetNeedsCompositingUpdate();
  // Make sure any input event handlers introduced by the plugin are taken into
  // account.
  if (Page* page = GetDocument().GetFrame()->GetPage()) {
    if (ScrollingCoordinator* scrolling_coordinator =
            page->GetScrollingCoordinator())
      scrolling_coordinator->NotifyGeometryChanged();
  }
  return true;
}

bool HTMLPlugInElement::ShouldUsePlugin(const KURL& url,
                                        const String& mime_type,
                                        bool has_fallback,
                                        bool& use_fallback) {
  ObjectContentType object_type =
      GetDocument().GetFrame()->Loader().Client()->GetObjectContentType(
          url, mime_type, ShouldPreferPlugInsForImages());
  // If an object's content can't be handled and it has no fallback, let
  // it be handled as a plugin to show the broken plugin icon.
  use_fallback = object_type == kObjectContentNone && has_fallback;
  return object_type == kObjectContentNone ||
         object_type == kObjectContentNetscapePlugin;
}

void HTMLPlugInElement::DispatchErrorEvent() {
  if (GetDocument().IsPluginDocument() && GetDocument().LocalOwner())
    GetDocument().LocalOwner()->DispatchEvent(
        Event::Create(EventTypeNames::error));
  else
    DispatchEvent(Event::Create(EventTypeNames::error));
}

bool HTMLPlugInElement::AllowedToLoadObject(const KURL& url,
                                            const String& mime_type) {
  if (url.IsEmpty() && mime_type.IsEmpty())
    return false;

  LocalFrame* frame = GetDocument().GetFrame();
  Settings* settings = frame->GetSettings();
  if (!settings)
    return false;

  if (MIMETypeRegistry::IsJavaAppletMIMEType(mime_type))
    return false;

  AtomicString declared_mime_type = FastGetAttribute(HTMLNames::typeAttr);
  if (!GetDocument().GetContentSecurityPolicy()->AllowObjectFromSource(url) ||
      !GetDocument().GetContentSecurityPolicy()->AllowPluginTypeForDocument(
          GetDocument(), mime_type, declared_mime_type, url)) {
    if (LayoutEmbeddedItem layout_item = GetLayoutEmbeddedItem()) {
      plugin_is_available_ = false;
      layout_item.SetPluginAvailability(
          LayoutEmbeddedObject::kPluginBlockedByContentSecurityPolicy);
    }
    return false;
  }
  // If the URL is empty, a plugin could still be instantiated if a MIME-type
  // is specified.
  return (!mime_type.IsEmpty() && url.IsEmpty()) ||
         !MixedContentChecker::ShouldBlockFetch(
             frame, WebURLRequest::kRequestContextObject,
             WebURLRequest::kFrameTypeNone,
             ResourceRequest::RedirectStatus::kNoRedirect, url);
}

bool HTMLPlugInElement::AllowedToLoadPlugin(const KURL& url,
                                            const String& mime_type) {
  if (GetDocument().IsSandboxed(kSandboxPlugins)) {
    GetDocument().AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, kErrorMessageLevel,
                               "Failed to load '" + url.ElidedString() +
                                   "' as a plugin, because the "
                                   "frame into which the plugin "
                                   "is loading is sandboxed."));
    return false;
  }
  return true;
}

void HTMLPlugInElement::DidAddUserAgentShadowRoot(ShadowRoot&) {
  UserAgentShadowRoot()->AppendChild(HTMLContentElement::Create(GetDocument()));
}

bool HTMLPlugInElement::HasFallbackContent() const {
  return false;
}

bool HTMLPlugInElement::UseFallbackContent() const {
  return false;
}

void HTMLPlugInElement::LazyReattachIfNeeded() {
  if (!UseFallbackContent() && NeedsPluginUpdate() && GetLayoutObject() &&
      !IsImageType()) {
    LazyReattachIfAttached();
    SetPersistedPlugin(nullptr);
  }
}

}  // namespace blink
