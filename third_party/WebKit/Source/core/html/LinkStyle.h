// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkStyle_h
#define LinkStyle_h

#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/html/LinkResource.h"
#include "core/loader/resource/StyleSheetResource.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/wtf/Forward.h"

namespace blink {

class HTMLLinkElement;
class KURL;

// LinkStyle handles dynamically change-able link resources, which is
// typically @rel="stylesheet".
//
// It could be @rel="shortcut icon" or something else though. Each of
// types might better be handled by a separate class, but dynamically
// changing @rel makes it harder to move such a design so we are
// sticking current way so far.
class LinkStyle final : public LinkResource, ResourceOwner<StyleSheetResource> {
  USING_GARBAGE_COLLECTED_MIXIN(LinkStyle);

 public:
  static LinkStyle* Create(HTMLLinkElement* owner);

  explicit LinkStyle(HTMLLinkElement* owner);
  ~LinkStyle() override;

  LinkResourceType GetType() const override { return kStyle; }
  void Process() override;
  void OwnerRemoved() override;
  bool HasLoaded() const override { return loaded_sheet_; }
  DECLARE_VIRTUAL_TRACE();

  void StartLoadingDynamicSheet();
  void NotifyLoadedSheetAndAllCriticalSubresources(
      Node::LoadedSheetErrorStatus);
  bool SheetLoaded();

  void SetDisabledState(bool);
  void SetSheetTitle(const String&);

  bool StyleSheetIsLoading() const;
  bool HasSheet() const { return sheet_; }
  bool IsDisabled() const { return disabled_state_ == kDisabled; }
  bool IsEnabledViaScript() const {
    return disabled_state_ == kEnabledViaScript;
  }
  bool IsUnset() const { return disabled_state_ == kUnset; }

  CSSStyleSheet* Sheet() const { return sheet_.Get(); }

 private:
  // From StyleSheetResourceClient
  void SetCSSStyleSheet(const String& href,
                        const KURL& base_url,
                        ReferrerPolicy,
                        const WTF::TextEncoding&,
                        const CSSStyleSheetResource*) override;
  String DebugName() const override { return "LinkStyle"; }
  enum LoadReturnValue { kLoaded, kNotNeeded, kBail };
  LoadReturnValue LoadStylesheetIfNeeded(const KURL&,
                                         const WTF::TextEncoding&,
                                         const String& type);

  enum DisabledState { kUnset, kEnabledViaScript, kDisabled };

  enum PendingSheetType { kNone, kNonBlocking, kBlocking };

  void ClearSheet();
  void AddPendingSheet(PendingSheetType);
  void RemovePendingSheet();

  void SetCrossOriginStylesheetStatus(CSSStyleSheet*);
  void SetFetchFollowingCORS() {
    DCHECK(!fetch_following_cors_);
    fetch_following_cors_ = true;
  }
  void ClearFetchFollowingCORS() { fetch_following_cors_ = false; }

  Member<CSSStyleSheet> sheet_;
  DisabledState disabled_state_;
  PendingSheetType pending_sheet_type_;
  StyleEngineContext style_engine_context_;
  bool loading_;
  bool fired_load_;
  bool loaded_sheet_;
  bool fetch_following_cors_;
};

}  // namespace blink

#endif
