/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/CreateWindow.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/frame/FrameClient.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/KeyboardCodes.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebKeyboardEvent.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebWindowFeatures.h"

namespace blink {

namespace {

void UpdatePolicyForEvent(const WebInputEvent* input_event,
                          NavigationPolicy* policy) {
  if (!input_event)
    return;

  unsigned short button_number = 0;
  if (input_event->GetType() == WebInputEvent::kMouseUp) {
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(input_event);

    switch (mouse_event->button) {
      case WebMouseEvent::Button::kLeft:
        button_number = 0;
        break;
      case WebMouseEvent::Button::kMiddle:
        button_number = 1;
        break;
      case WebMouseEvent::Button::kRight:
        button_number = 2;
        break;
      default:
        return;
    }
  } else if ((WebInputEvent::IsKeyboardEventType(input_event->GetType()) &&
              static_cast<const WebKeyboardEvent*>(input_event)
                      ->windows_key_code == VKEY_RETURN) ||
             WebInputEvent::IsGestureEventType(input_event->GetType())) {
    // Keyboard and gesture events can simulate mouse events.
    button_number = 0;
  } else {
    return;
  }

  bool ctrl = input_event->GetModifiers() & WebInputEvent::kControlKey;
  bool shift = input_event->GetModifiers() & WebInputEvent::kShiftKey;
  bool alt = input_event->GetModifiers() & WebInputEvent::kAltKey;
  bool meta = input_event->GetModifiers() & WebInputEvent::kMetaKey;

  NavigationPolicy user_policy = *policy;
  NavigationPolicyFromMouseEvent(button_number, ctrl, shift, alt, meta,
                                 &user_policy);

  // When the input event suggests a download, but the navigation was initiated
  // by script, we should not override it.
  if (user_policy == kNavigationPolicyDownload &&
      *policy != kNavigationPolicyIgnore)
    return;

  // User and app agree that we want a new window; let the app override the
  // decorations.
  if (user_policy == kNavigationPolicyNewWindow &&
      *policy == kNavigationPolicyNewPopup)
    return;
  *policy = user_policy;
}

NavigationPolicy GetNavigationPolicy(const WebInputEvent* current_event,
                                     const WebWindowFeatures& features) {
  // If our default configuration was modified by a script or wasn't
  // created by a user gesture, then show as a popup. Else, let this
  // new window be opened as a toplevel window.
  bool as_popup = !features.tool_bar_visible || !features.status_bar_visible ||
                  !features.scrollbars_visible || !features.menu_bar_visible ||
                  !features.resizable;
  NavigationPolicy policy =
      as_popup ? kNavigationPolicyNewPopup : kNavigationPolicyNewForegroundTab;
  UpdatePolicyForEvent(current_event, &policy);
  return policy;
}

}  // anonymous namespace

NavigationPolicy EffectiveNavigationPolicy(NavigationPolicy policy,
                                           const WebInputEvent* current_event,
                                           const WebWindowFeatures& features) {
  if (policy == kNavigationPolicyIgnore)
    return GetNavigationPolicy(current_event, features);
  if (policy == kNavigationPolicyNewBackgroundTab &&
      GetNavigationPolicy(current_event, features) !=
          kNavigationPolicyNewBackgroundTab &&
      !UIEventWithKeyState::NewTabModifierSetFromIsolatedWorld()) {
    return kNavigationPolicyNewForegroundTab;
  }
  return policy;
}

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't when
// parsing window features.
static bool IsWindowFeaturesSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\0';
}

WebWindowFeatures GetWindowFeaturesFromString(const String& feature_string) {
  WebWindowFeatures window_features;

  // The IE rule is: all features except for channelmode default
  // to YES, but if the user specifies a feature string, all features default to
  // NO. (There is no public standard that applies to this method.)
  // <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
  if (feature_string.IsEmpty())
    return window_features;

  window_features.menu_bar_visible = false;
  window_features.status_bar_visible = false;
  window_features.tool_bar_visible = false;
  window_features.scrollbars_visible = false;

  // Tread lightly in this code -- it was specifically designed to mimic Win
  // IE's parsing behavior.
  unsigned key_begin, key_end;
  unsigned value_begin, value_end;

  String buffer = feature_string.DeprecatedLower();
  unsigned length = buffer.length();
  for (unsigned i = 0; i < length;) {
    // skip to first non-separator, but don't skip past the end of the string
    while (i < length && IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_begin = i;

    // skip to first separator
    while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_end = i;

    SECURITY_DCHECK(i <= length);

    // skip to first '=', but don't skip past a ',' or the end of the string
    while (i < length && buffer[i] != '=') {
      if (buffer[i] == ',')
        break;
      i++;
    }

    SECURITY_DCHECK(i <= length);

    // Skip to first non-separator, but don't skip past a ',' or the end of the
    // string.
    while (i < length && IsWindowFeaturesSeparator(buffer[i])) {
      if (buffer[i] == ',')
        break;
      i++;
    }
    value_begin = i;

    SECURITY_DCHECK(i <= length);

    // skip to first separator
    while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
      i++;
    value_end = i;

    SECURITY_DCHECK(i <= length);

    String key_string(buffer.Substring(key_begin, key_end - key_begin));
    String value_string(buffer.Substring(value_begin, value_end - value_begin));

    // Listing a key with no value is shorthand for key=yes
    int value;
    if (value_string.IsEmpty() || value_string == "yes")
      value = 1;
    else
      value = value_string.ToInt();

    if (key_string == "left" || key_string == "screenx") {
      window_features.x_set = true;
      window_features.x = value;
    } else if (key_string == "top" || key_string == "screeny") {
      window_features.y_set = true;
      window_features.y = value;
    } else if (key_string == "width" || key_string == "innerwidth") {
      window_features.width_set = true;
      window_features.width = value;
    } else if (key_string == "height" || key_string == "innerheight") {
      window_features.height_set = true;
      window_features.height = value;
    } else if (key_string == "menubar") {
      window_features.menu_bar_visible = value;
    } else if (key_string == "toolbar" || key_string == "location") {
      window_features.tool_bar_visible |= static_cast<bool>(value);
    } else if (key_string == "status") {
      window_features.status_bar_visible = value;
    } else if (key_string == "scrollbars") {
      window_features.scrollbars_visible = value;
    } else if (key_string == "resizable") {
      window_features.resizable = value;
    } else if (key_string == "noopener") {
      window_features.noopener = true;
    } else if (key_string == "background") {
      window_features.background = true;
    } else if (key_string == "persistent") {
      window_features.persistent = true;
    }
  }

  return window_features;
}

static Frame* ReuseExistingWindow(LocalFrame& active_frame,
                                  LocalFrame& lookup_frame,
                                  const AtomicString& frame_name,
                                  NavigationPolicy policy) {
  if (!frame_name.IsEmpty() && !EqualIgnoringASCIICase(frame_name, "_blank") &&
      policy == kNavigationPolicyIgnore) {
    if (Frame* frame =
            lookup_frame.FindFrameForNavigation(frame_name, active_frame)) {
      if (!EqualIgnoringASCIICase(frame_name, "_self")) {
        if (Page* page = frame->GetPage()) {
          if (page == active_frame.GetPage())
            page->GetFocusController().SetFocusedFrame(frame);
          else
            page->GetChromeClient().Focus();
        }
      }
      return frame;
    }
  }
  return nullptr;
}

static Frame* CreateNewWindow(LocalFrame& opener_frame,
                              const FrameLoadRequest& request,
                              const WebWindowFeatures& features,
                              NavigationPolicy policy,
                              bool& created) {
  Page* old_page = opener_frame.GetPage();
  if (!old_page)
    return nullptr;

  policy = EffectiveNavigationPolicy(
      policy, old_page->GetChromeClient().GetCurrentInputEvent(), features);

  const SandboxFlags sandbox_flags =
      opener_frame.GetDocument()->IsSandboxed(
          kSandboxPropagatesToAuxiliaryBrowsingContexts)
          ? opener_frame.GetSecurityContext()->GetSandboxFlags()
          : kSandboxNone;

  Page* page = old_page->GetChromeClient().CreateWindow(
      &opener_frame, request, features, policy, sandbox_flags);
  if (!page)
    return nullptr;

  if (page == old_page)
    return &opener_frame.Tree().Top();

  DCHECK(page->MainFrame());
  LocalFrame& frame = *ToLocalFrame(page->MainFrame());

  page->SetWindowFeatures(features);

  frame.View()->SetCanHaveScrollbars(features.scrollbars_visible);

  // 'x' and 'y' specify the location of the window, while 'width' and 'height'
  // specify the size of the viewport. We can only resize the window, so adjust
  // for the difference between the window size and the viewport size.

  IntRect window_rect = page->GetChromeClient().RootWindowRect();
  IntSize viewport_size = page->GetChromeClient().PageRect().Size();

  if (features.x_set)
    window_rect.SetX(features.x);
  if (features.y_set)
    window_rect.SetY(features.y);
  if (features.width_set)
    window_rect.SetWidth(features.width +
                         (window_rect.Width() - viewport_size.Width()));
  if (features.height_set)
    window_rect.SetHeight(features.height +
                          (window_rect.Height() - viewport_size.Height()));

  page->GetChromeClient().SetWindowRectWithAdjustment(window_rect, frame);
  page->GetChromeClient().Show(policy);

  // This call may suspend the execution by running nested run loop.
  probe::windowCreated(&opener_frame, &frame);
  created = true;
  return &frame;
}

static Frame* CreateWindowHelper(LocalFrame& opener_frame,
                                 LocalFrame& active_frame,
                                 LocalFrame& lookup_frame,
                                 const FrameLoadRequest& request,
                                 const WebWindowFeatures& features,
                                 NavigationPolicy policy,
                                 bool& created) {
  DCHECK(request.GetResourceRequest().RequestorOrigin() ||
         opener_frame.GetDocument()->Url().IsEmpty());
  DCHECK_EQ(request.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeAuxiliary);

  created = false;

  Frame* window = features.noopener
                      ? nullptr
                      : ReuseExistingWindow(active_frame, lookup_frame,
                                            request.FrameName(), policy);

  if (!window) {
    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (opener_frame.GetDocument()->IsSandboxed(kSandboxPopups)) {
      // FIXME: This message should be moved off the console once a solution to
      // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
      opener_frame.GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
          kSecurityMessageSource, kErrorMessageLevel,
          "Blocked opening '" +
              request.GetResourceRequest().Url().ElidedString() +
              "' in a new window because the request was made in a sandboxed "
              "frame whose 'allow-popups' permission is not set."));
      return nullptr;
    }
  }

  if (window) {
    // JS can run inside reuseExistingWindow (via onblur), which can detach
    // the target window.
    if (!window->Client())
      return nullptr;
    if (request.GetShouldSetOpener() == kMaybeSetOpener)
      window->Client()->SetOpener(&opener_frame);
    return window;
  }

  return CreateNewWindow(opener_frame, request, features, policy, created);
}

DOMWindow* CreateWindow(const String& url_string,
                        const AtomicString& frame_name,
                        const String& window_features_string,
                        LocalDOMWindow& calling_window,
                        LocalFrame& first_frame,
                        LocalFrame& opener_frame,
                        ExceptionState& exception_state) {
  LocalFrame* active_frame = calling_window.GetFrame();
  DCHECK(active_frame);

  KURL completed_url = url_string.IsEmpty()
                           ? KURL(kParsedURLString, g_empty_string)
                           : first_frame.GetDocument()->CompleteURL(url_string);
  if (!completed_url.IsEmpty() && !completed_url.IsValid()) {
    UseCounter::Count(active_frame, WebFeature::kWindowOpenWithInvalidURL);
    exception_state.ThrowDOMException(
        kSyntaxError, "Unable to open a window with invalid URL '" +
                          completed_url.GetString() + "'.\n");
    return nullptr;
  }

  WebWindowFeatures window_features =
      GetWindowFeaturesFromString(window_features_string);

  FrameLoadRequest frame_request(calling_window.document(),
                                 ResourceRequest(completed_url), frame_name);
  frame_request.SetShouldSetOpener(window_features.noopener ? kNeverSetOpener
                                                            : kMaybeSetOpener);
  frame_request.GetResourceRequest().SetFrameType(
      WebURLRequest::kFrameTypeAuxiliary);
  frame_request.GetResourceRequest().SetRequestorOrigin(
      SecurityOrigin::Create(active_frame->GetDocument()->Url()));

  // Normally, FrameLoader would take care of setting the referrer for a
  // navigation that is triggered from javascript. However, creating a window
  // goes through sufficient processing that it eventually enters FrameLoader as
  // an embedder-initiated navigation.  FrameLoader assumes no responsibility
  // for generating an embedder-initiated navigation's referrer, so we need to
  // ensure the proper referrer is set now.
  frame_request.GetResourceRequest().SetHTTPReferrer(
      SecurityPolicy::GenerateReferrer(
          active_frame->GetDocument()->GetReferrerPolicy(), completed_url,
          active_frame->GetDocument()->OutgoingReferrer()));

  // Records HasUserGesture before the value is invalidated inside
  // createWindow(LocalFrame& openerFrame, ...).
  // This value will be set in ResourceRequest loaded in a new LocalFrame.
  bool has_user_gesture = UserGestureIndicator::ProcessingUserGesture();

  // We pass the opener frame for the lookupFrame in case the active frame is
  // different from the opener frame, and the name references a frame relative
  // to the opener frame.
  bool created;
  Frame* new_frame = CreateWindowHelper(
      opener_frame, *active_frame, opener_frame, frame_request, window_features,
      kNavigationPolicyIgnore, created);
  if (!new_frame)
    return nullptr;
  if (new_frame->DomWindow()->IsInsecureScriptAccess(calling_window,
                                                     completed_url))
    return window_features.noopener ? nullptr : new_frame->DomWindow();

  // TODO(dcheng): Special case for window.open("about:blank") to ensure it
  // loads synchronously into a new window. This is our historical behavior, and
  // it's consistent with the creation of a new iframe with src="about:blank".
  // Perhaps we could get rid of this if we started reporting the initial empty
  // document's url as about:blank? See crbug.com/471239.
  // TODO(japhet): This special case is also necessary for behavior asserted by
  // some extensions tests.  Using NavigationScheduler::scheduleNavigationChange
  // causes the navigation to be flagged as a client redirect, which is
  // observable via the webNavigation extension api.
  if (created) {
    FrameLoadRequest request(calling_window.document(),
                             ResourceRequest(completed_url));
    request.GetResourceRequest().SetHasUserGesture(has_user_gesture);
    new_frame->Navigate(request);
  } else if (!url_string.IsEmpty()) {
    new_frame->Navigate(*calling_window.document(), completed_url, false,
                        has_user_gesture ? UserGestureStatus::kActive
                                         : UserGestureStatus::kNone);
  }
  return window_features.noopener ? nullptr : new_frame->DomWindow();
}

void CreateWindowForRequest(const FrameLoadRequest& request,
                            LocalFrame& opener_frame,
                            NavigationPolicy policy) {
  DCHECK(request.GetResourceRequest().RequestorOrigin() ||
         (opener_frame.GetDocument() &&
          opener_frame.GetDocument()->Url().IsEmpty()));

  if (opener_frame.GetDocument()->PageDismissalEventBeingDispatched() !=
      Document::kNoDismissal)
    return;

  if (opener_frame.GetDocument() &&
      opener_frame.GetDocument()->IsSandboxed(kSandboxPopups))
    return;

  if (policy == kNavigationPolicyCurrentTab)
    policy = kNavigationPolicyNewForegroundTab;

  WebWindowFeatures features;
  features.noopener = request.GetShouldSetOpener() == kNeverSetOpener;
  bool created;
  Frame* new_frame =
      CreateWindowHelper(opener_frame, opener_frame, opener_frame, request,
                         features, policy, created);
  if (!new_frame)
    return;
  if (request.GetShouldSendReferrer() == kMaybeSendReferrer) {
    // TODO(japhet): Does ReferrerPolicy need to be proagated for RemoteFrames?
    if (new_frame->IsLocalFrame())
      ToLocalFrame(new_frame)->GetDocument()->SetReferrerPolicy(
          opener_frame.GetDocument()->GetReferrerPolicy());
  }

  // TODO(japhet): Form submissions on RemoteFrames don't work yet.
  FrameLoadRequest new_request(0, request.GetResourceRequest());
  new_request.SetForm(request.Form());
  if (new_frame->IsLocalFrame())
    ToLocalFrame(new_frame)->Loader().Load(new_request);
}

}  // namespace blink
