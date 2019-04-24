// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace guest_view {

class GuestViewBase;

// A GuestViewManagerDelegate interface allows GuestViewManager to delegate
// responsibilities to other modules in Chromium. Different builds of Chromium
// may use different GuestViewManagerDelegate implementations. For example,
// mobile builds of Chromium do not include an extensions module and so
// permission checks would be different, and IsOwnedByExtension would always
// return false.
class GuestViewManagerDelegate {
 public:
  GuestViewManagerDelegate();
  virtual ~GuestViewManagerDelegate();

  // Invoked after |guest_web_contents| is added.
  virtual void OnGuestAdded(content::WebContents* guest_web_contents) const {}

  // Dispatches the event with |name| with the provided |args| to the embedder
  // of the given |guest| with |instance_id| for routing.
  virtual void DispatchEvent(const std::string& event_name,
                             std::unique_ptr<base::DictionaryValue> args,
                             GuestViewBase* guest,
                             int instance_id) {}

  // Indicates whether the |guest| can be used within the context of where it
  // was created.
  virtual bool IsGuestAvailableToContext(GuestViewBase* guest);

  // Indicates whether the |guest| is owned by an extension or Chrome App.
  virtual bool IsOwnedByExtension(GuestViewBase* guest);

  // Registers additional GuestView types the delegator (GuestViewManger) can
  // create.
  virtual void RegisterAdditionalGuestViewTypes() {}
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_
