// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkPreloadResourceClients_h
#define LinkPreloadResourceClients_h

#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/FontResource.h"
#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceOwner.h"

namespace blink {

class LinkLoader;

class LinkPreloadResourceClient
    : public GarbageCollectedFinalized<LinkPreloadResourceClient> {
 public:
  virtual ~LinkPreloadResourceClient() {}

  void TriggerEvents(const Resource*);
  virtual Resource* GetResource() = 0;
  virtual void Clear() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->Trace(loader_); }

 protected:
  explicit LinkPreloadResourceClient(LinkLoader* loader) : loader_(loader) {
    DCHECK(loader);
  }

 private:
  Member<LinkLoader> loader_;
};

class LinkPreloadScriptResourceClient
    : public LinkPreloadResourceClient,
      public ResourceOwner<ScriptResource, ScriptResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadScriptResourceClient);

 public:
  static LinkPreloadScriptResourceClient* Create(LinkLoader* loader,
                                                 ScriptResource* resource) {
    return new LinkPreloadScriptResourceClient(loader, resource);
  }

  virtual String DebugName() const { return "LinkPreloadScript"; }
  virtual ~LinkPreloadScriptResourceClient() {}

  Resource* GetResource() override {
    return ResourceOwner<ScriptResource>::GetResource();
  }
  void Clear() override { ClearResource(); }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(this->GetResource(), resource);
    TriggerEvents(resource);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    LinkPreloadResourceClient::Trace(visitor);
    ResourceOwner<ScriptResource, ScriptResourceClient>::Trace(visitor);
  }

 private:
  LinkPreloadScriptResourceClient(LinkLoader* loader, ScriptResource* resource)
      : LinkPreloadResourceClient(loader) {
    SetResource(resource, Resource::kDontMarkAsReferenced);
  }
};

class LinkPreloadStyleResourceClient
    : public LinkPreloadResourceClient,
      public ResourceOwner<CSSStyleSheetResource, StyleSheetResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadStyleResourceClient);

 public:
  static LinkPreloadStyleResourceClient* Create(
      LinkLoader* loader,
      CSSStyleSheetResource* resource) {
    return new LinkPreloadStyleResourceClient(loader, resource);
  }

  virtual String DebugName() const { return "LinkPreloadStyle"; }
  virtual ~LinkPreloadStyleResourceClient() {}

  Resource* GetResource() override {
    return ResourceOwner<CSSStyleSheetResource>::GetResource();
  }
  void Clear() override { ClearResource(); }

  void SetCSSStyleSheet(const String&,
                        const KURL&,
                        ReferrerPolicy,
                        const String&,
                        const CSSStyleSheetResource* resource) override {
    DCHECK_EQ(this->GetResource(), resource);
    TriggerEvents(static_cast<const Resource*>(resource));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    LinkPreloadResourceClient::Trace(visitor);
    ResourceOwner<CSSStyleSheetResource, StyleSheetResourceClient>::Trace(
        visitor);
  }

 private:
  LinkPreloadStyleResourceClient(LinkLoader* loader,
                                 CSSStyleSheetResource* resource)
      : LinkPreloadResourceClient(loader) {
    SetResource(resource, Resource::kDontMarkAsReferenced);
  }
};

class LinkPreloadImageResourceClient : public LinkPreloadResourceClient,
                                       public ResourceOwner<ImageResource> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadImageResourceClient);

 public:
  static LinkPreloadImageResourceClient* Create(LinkLoader* loader,
                                                ImageResource* resource) {
    return new LinkPreloadImageResourceClient(loader, resource);
  }

  virtual String DebugName() const { return "LinkPreloadImage"; }
  virtual ~LinkPreloadImageResourceClient() {}

  Resource* GetResource() override {
    return ResourceOwner<ImageResource>::GetResource();
  }
  void Clear() override { ClearResource(); }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(this->GetResource(), ToImageResource(resource));
    TriggerEvents(resource);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    LinkPreloadResourceClient::Trace(visitor);
    ResourceOwner<ImageResource>::Trace(visitor);
  }

 private:
  LinkPreloadImageResourceClient(LinkLoader* loader, ImageResource* resource)
      : LinkPreloadResourceClient(loader) {
    SetResource(resource, Resource::kDontMarkAsReferenced);
  }
};

class LinkPreloadFontResourceClient
    : public LinkPreloadResourceClient,
      public ResourceOwner<FontResource, FontResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadFontResourceClient);

 public:
  static LinkPreloadFontResourceClient* Create(LinkLoader* loader,
                                               FontResource* resource) {
    return new LinkPreloadFontResourceClient(loader, resource);
  }

  virtual String DebugName() const { return "LinkPreloadFont"; }
  virtual ~LinkPreloadFontResourceClient() {}

  Resource* GetResource() override {
    return ResourceOwner<FontResource>::GetResource();
  }
  void Clear() override { ClearResource(); }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(this->GetResource(), ToFontResource(resource));
    TriggerEvents(resource);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    LinkPreloadResourceClient::Trace(visitor);
    ResourceOwner<FontResource, FontResourceClient>::Trace(visitor);
  }

 private:
  LinkPreloadFontResourceClient(LinkLoader* loader, FontResource* resource)
      : LinkPreloadResourceClient(loader) {
    SetResource(resource, Resource::kDontMarkAsReferenced);
  }
};

class LinkPreloadRawResourceClient
    : public LinkPreloadResourceClient,
      public ResourceOwner<RawResource, RawResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkPreloadRawResourceClient);

 public:
  static LinkPreloadRawResourceClient* Create(LinkLoader* loader,
                                              RawResource* resource) {
    return new LinkPreloadRawResourceClient(loader, resource);
  }

  virtual String DebugName() const { return "LinkPreloadRaw"; }
  virtual ~LinkPreloadRawResourceClient() {}

  Resource* GetResource() override {
    return ResourceOwner<RawResource>::GetResource();
  }
  void Clear() override { ClearResource(); }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(this->GetResource(), ToRawResource(resource));
    TriggerEvents(resource);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    LinkPreloadResourceClient::Trace(visitor);
    ResourceOwner<RawResource, RawResourceClient>::Trace(visitor);
  }

 private:
  LinkPreloadRawResourceClient(LinkLoader* loader, RawResource* resource)
      : LinkPreloadResourceClient(loader) {
    SetResource(resource, Resource::kDontMarkAsReferenced);
  }
};

}  // namespace blink

#endif  // LinkPreloadResourceClients_h
