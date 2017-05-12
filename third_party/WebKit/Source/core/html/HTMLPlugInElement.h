/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights
 * reserved.
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
 *
 */

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "platform/bindings/SharedPersistent.h"
#include "v8/include/v8.h"

namespace blink {

class HTMLImageLoader;
class LayoutPart;
class LayoutEmbeddedItem;
class PluginView;

enum PreferPlugInsForImagesOption {
  kShouldPreferPlugInsForImages,
  kShouldNotPreferPlugInsForImages
};

class CORE_EXPORT HTMLPlugInElement : public HTMLFrameOwnerElement {
 public:
  ~HTMLPlugInElement() override;
  DECLARE_VIRTUAL_TRACE();

  void SetFocused(bool, WebFocusType) override;
  void ResetInstance();
  // TODO(dcheng): Consider removing this, since HTMLEmbedElementLegacyCall
  // and HTMLObjectElementLegacyCall usage is extremely low.
  v8::Local<v8::Object> PluginWrapper();
  // TODO(joelhockey): Clean up PluginWidget and OwnedPlugin (maybe also
  // PluginWrapper).  It would be good to remove and/or rename some of these.
  // PluginWidget and OwnedPlugin both return the plugin that is stored as
  // widget in HTMLFrameOwnerElement.  However PluginWidget will synchronously
  // create the plugin if required by calling LayoutPartForJSBindings.
  // Possibly the PluginWidget code can be inlined into PluginWrapper.
  PluginView* PluginWidget() const;
  PluginView* OwnedPlugin() const;
  bool CanProcessDrag() const;
  const String& Url() const { return url_; }

  // Public for FrameView::addPartToUpdate()
  bool NeedsPluginUpdate() const { return needs_plugin_update_; }
  void SetNeedsPluginUpdate(bool needs_plugin_update) {
    needs_plugin_update_ = needs_plugin_update;
  }
  void UpdatePlugin();

  bool ShouldAccelerate() const;

  void RequestPluginCreationWithoutLayoutObjectIfPossible();
  void CreatePluginWithoutLayoutObject();

 protected:
  HTMLPlugInElement(const QualifiedName& tag_name,
                    Document&,
                    bool created_by_parser,
                    PreferPlugInsForImagesOption);

  // Node functions:
  void RemovedFrom(ContainerNode* insertion_point) override;
  void DidMoveToNewDocument(Document& old_document) override;

  // Element functions:
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  virtual bool HasFallbackContent() const;
  virtual bool UseFallbackContent() const;
  // Create or update the LayoutPart and return it, triggering layout if
  // necessary.
  virtual LayoutPart* LayoutPartForJSBindings() const;

  bool IsImageType();
  bool ShouldPreferPlugInsForImages() const {
    return should_prefer_plug_ins_for_images_;
  }
  LayoutEmbeddedItem GetLayoutEmbeddedItem() const;
  bool AllowedToLoadFrameURL(const String& url);
  bool RequestObject(const String& url,
                     const String& mime_type,
                     const Vector<String>& param_names,
                     const Vector<String>& param_values);
  bool ShouldUsePlugin(const KURL&,
                       const String& mime_type,
                       bool has_fallback,
                       bool& use_fallback);

  void DispatchErrorEvent();
  bool IsErrorplaceholder();
  void LazyReattachIfNeeded();

  String service_type_;
  String url_;
  KURL loaded_url_;
  Member<HTMLImageLoader> image_loader_;
  bool is_delaying_load_event_;

 private:
  // EventTarget overrides:
  void RemoveAllEventListeners() final;

  // Node overrides:
  bool CanContainRangeEndPoint() const override { return false; }
  bool CanStartSelection() const override;
  bool WillRespondToMouseClickEvents() final;
  void DefaultEventHandler(Event*) final;
  void AttachLayoutTree(const AttachContext& = AttachContext()) final;
  void DetachLayoutTree(const AttachContext& = AttachContext()) final;
  void FinishParsingChildren() final;

  // Element overrides:
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  bool SupportsFocus() const final { return true; }
  bool LayoutObjectIsFocusable() const final;
  bool IsKeyboardFocusable() const final;
  void DidAddUserAgentShadowRoot(ShadowRoot&) final;

  // HTMLElement overrides:
  bool HasCustomFocusLogic() const override;
  bool IsPluginElement() const final;

  // HTMLFrameOwnerElement overrides:
  void DisconnectContentFrame() override;

  // Return any existing LayoutPart without triggering relayout, or 0 if it
  // doesn't yet exist.
  virtual LayoutPart* ExistingLayoutPart() const = 0;
  virtual void UpdatePluginInternal() = 0;

  bool LoadPlugin(const KURL&,
                  const String& mime_type,
                  const Vector<String>& param_names,
                  const Vector<String>& param_values,
                  bool use_fallback,
                  bool require_layout_object);
  // Perform checks after we have determined that a plugin will be used to
  // show the object (i.e after allowedToLoadObject).
  bool AllowedToLoadPlugin(const KURL&, const String& mime_type);
  // Perform checks based on the URL and MIME-type of the object to load.
  bool AllowedToLoadObject(const KURL&, const String& mime_type);
  bool WouldLoadAsNetscapePlugin(const String& url, const String& service_type);

  void SetPersistedPlugin(PluginView*);

  bool RequestObjectInternal(const String& url,
                             const String& mime_type,
                             const Vector<String>& param_names,
                             const Vector<String>& param_values);

  v8::Global<v8::Object> plugin_wrapper_;
  bool needs_plugin_update_;
  bool should_prefer_plug_ins_for_images_;
  // Represents |layoutObject() && layoutObject()->isEmbeddedObject() &&
  // !layoutEmbeddedItem().showsUnavailablePluginIndicator()|.  We want to
  // avoid accessing |layoutObject()| in layoutObjectIsFocusable().
  bool plugin_is_available_ = false;

  // Normally the plugin is stored in HTMLFrameOwnerElement::widget_.
  // However, plugins can persist even when not rendered. In order to
  // prevent confusing code which may assume that OwnedWidget() != null
  // means the frame is active, we save off widget_ here while
  // the plugin is persisting but not being displayed.
  Member<PluginView> persisted_plugin_;
};

inline bool IsHTMLPlugInElement(const HTMLElement& element) {
  return element.IsPluginElement();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLPlugInElement);

}  // namespace blink

#endif  // HTMLPlugInElement_h
