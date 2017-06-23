/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef Frame_h
#define Frame_h

#include "core/CoreExport.h"
#include "core/frame/FrameLifecycle.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/FrameView.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/page/FrameTree.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebFeaturePolicy.h"

namespace blink {

class ChromeClient;
class DOMWindow;
class DOMWrapperWorld;
class Document;
class FrameClient;
class FrameOwner;
class HTMLFrameOwnerElement;
class LayoutEmbeddedContent;
class LayoutEmbeddedContentItem;
class LocalFrame;
class KURL;
class Page;
class SecurityContext;
class Settings;
class WindowProxy;
class WindowProxyManager;
struct FrameLoadRequest;

enum class FrameDetachType { kRemove, kSwap };

// Status of user gesture.
enum class UserGestureStatus { kActive, kNone };

// Frame is the base class of LocalFrame and RemoteFrame and should only contain
// functionality shared between both. In particular, any method related to
// input, layout, or painting probably belongs on LocalFrame.
class CORE_EXPORT Frame : public GarbageCollectedFinalized<Frame> {
 public:
  virtual ~Frame();

  DECLARE_VIRTUAL_TRACE();

  virtual bool IsLocalFrame() const = 0;
  virtual bool IsRemoteFrame() const = 0;

  virtual void Navigate(Document& origin_document,
                        const KURL&,
                        bool replace_current_item,
                        UserGestureStatus) = 0;
  // This version of Frame::navigate assumes the resulting navigation is not
  // to be started on a timer. Use the method above in such cases.
  virtual void Navigate(const FrameLoadRequest&) = 0;
  virtual void Reload(FrameLoadType, ClientRedirectPolicy) = 0;

  virtual void Detach(FrameDetachType);
  void DisconnectOwnerElement();
  virtual bool ShouldClose() = 0;

  FrameClient* Client() const;

  Page* GetPage() const;  // Null when the frame is detached.
  virtual FrameView* View() const = 0;

  bool IsMainFrame() const;
  bool IsLocalRoot() const;

  FrameOwner* Owner() const;
  void SetOwner(FrameOwner*);
  HTMLFrameOwnerElement* DeprecatedLocalOwner() const;

  DOMWindow* DomWindow() const { return dom_window_; }

  FrameTree& Tree() const;
  ChromeClient& GetChromeClient() const;

  virtual SecurityContext* GetSecurityContext() const = 0;

  Frame* FindUnsafeParentScrollPropagationBoundary();

  // This prepares the Frame for the next commit. It will detach children,
  // dispatch unload events, abort XHR requests and detach the document.
  // Returns true if the frame is ready to receive the next commit, or false
  // otherwise.
  virtual bool PrepareForCommit() = 0;

  // TODO(japhet): These should all move to LocalFrame.
  virtual void PrintNavigationErrorMessage(const Frame&,
                                           const char* reason) = 0;
  virtual void PrintNavigationWarning(const String&) = 0;

  // TODO(pilgrim): Replace all instances of ownerLayoutObject() with
  // ownerLayoutItem(), https://crbug.com/499321
  LayoutEmbeddedContent* OwnerLayoutObject()
      const;  // LayoutObject for the element that contains this frame.
  LayoutEmbeddedContentItem OwnerLayoutItem() const;

  Settings* GetSettings() const;  // can be null

  // isLoading() is true when the embedder should think a load is in progress.
  // In the case of LocalFrames, it means that the frame has sent a
  // didStartLoading() callback, but not the matching didStopLoading(). Inside
  // blink, you probably want Document::loadEventFinished() instead.
  void SetIsLoading(bool is_loading) { is_loading_ = is_loading; }
  bool IsLoading() const { return is_loading_; }

  WindowProxyManager* GetWindowProxyManager() const {
    return window_proxy_manager_;
  }
  WindowProxy* GetWindowProxy(DOMWrapperWorld&);

  virtual void DidChangeVisibilityState();

  void SetDocumentHasReceivedUserGesture();
  bool HasReceivedUserGesture() const { return has_received_user_gesture_; }

  bool IsAttached() const {
    return lifecycle_.GetState() == FrameLifecycle::kAttached;
  }

  // Tests whether the feature-policy controlled feature is enabled by policy in
  // the given frame.
  bool IsFeatureEnabled(WebFeaturePolicyFeature) const;

  // Called to make a frame inert or non-inert. A frame is inert when there
  // is a modal dialog displayed within an ancestor frame, and this frame
  // itself is not within the dialog.
  virtual void SetIsInert(bool) = 0;
  void UpdateInertIfPossible();

 protected:
  Frame(FrameClient*, Page&, FrameOwner*, WindowProxyManager*);

  mutable FrameTree tree_node_;

  Member<Page> page_;
  Member<FrameOwner> owner_;
  Member<DOMWindow> dom_window_;

  bool has_received_user_gesture_ = false;

  FrameLifecycle lifecycle_;

  // This is set to true if this is a subframe, and the frame element in the
  // parent frame's document becomes inert. This should always be false for
  // the main frame.
  bool is_inert_ = false;

 private:
  Member<FrameClient> client_;
  const Member<WindowProxyManager> window_proxy_manager_;
  // TODO(sashab): Investigate if this can be represented with m_lifecycle.
  bool is_loading_;
};

inline FrameClient* Frame::Client() const {
  return client_;
}

inline FrameOwner* Frame::Owner() const {
  return owner_;
}

inline FrameTree& Frame::Tree() const {
  return tree_node_;
}

// Allow equality comparisons of Frames by reference or pointer,
// interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Frame)

}  // namespace blink

#endif  // Frame_h
