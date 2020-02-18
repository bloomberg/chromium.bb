// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_ITEM_H_
#define IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_ITEM_H_

#include <memory>

#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/ui/page_display_state.h"
#include "ui/base/page_transition_types.h"

class GURL;

@class NSDictionary;

namespace web {
struct FaviconStatus;
struct Referrer;
struct SSLStatus;

// A NavigationItem is a data structure that captures all the information
// required to recreate a browsing state. It represents one point in the
// chain of navigation managed by a NavigationManager.
class NavigationItem : public base::SupportsUserData {
 public:
  // Creates a new NavigationItem.
  static std::unique_ptr<NavigationItem> Create();

  // Page-related stuff --------------------------------------------------------

  // A unique ID is preserved across commits and redirects, which means that
  // sometimes a NavigationEntry's unique ID needs to be set (e.g. when
  // creating a committed entry to correspond to a to-be-deleted pending entry,
  // the pending entry's ID must be copied).
  virtual int GetUniqueID() const = 0;

  // The original URL for the navigation request.  This may differ from GetURL()
  // if a redirect occurs after attempting to load this original URL.
  virtual void SetOriginalRequestURL(const GURL& url) = 0;
  virtual const GURL& GetOriginalRequestURL() const = 0;

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use GetVirtualURL() below for showing to
  // the user.
  virtual void SetURL(const GURL& url) = 0;
  virtual const GURL& GetURL() const = 0;

  // The referring URL. Can be empty.
  virtual void SetReferrer(const Referrer& referrer) = 0;
  virtual const Referrer& GetReferrer() const = 0;

  // The virtual URL, when nonempty, will override the actual URL of the page
  // when we display it to the user. This allows us to have nice and friendly
  // URLs that the user sees for things like about: URLs, but actually feed
  // the renderer a data URL that results in the content loading.
  //
  // GetVirtualURL() will return the URL to display to the user in all cases, so
  // if there is no overridden display URL, it will return the actual one.
  virtual void SetVirtualURL(const GURL& url) = 0;
  virtual const GURL& GetVirtualURL() const = 0;

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  virtual void SetTitle(const base::string16& title) = 0;
  virtual const base::string16& GetTitle() const = 0;

  // Stores the NavigationItem's last recorded scroll offset and zoom scale.
  virtual void SetPageDisplayState(const PageDisplayState& page_state) = 0;
  virtual const PageDisplayState& GetPageDisplayState() const = 0;

  // Page-related helpers ------------------------------------------------------

  // Returns the title to be displayed on the tab. This could be the title of
  // the page if it is available or the URL.
  virtual const base::string16& GetTitleForDisplay() const = 0;

  // Tracking stuff ------------------------------------------------------------

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  virtual void SetTransitionType(ui::PageTransition transition_type) = 0;
  virtual ui::PageTransition GetTransitionType() const = 0;

  // The favicon data and tracking information. See web::FaviconStatus.
  virtual const FaviconStatus& GetFavicon() const = 0;
  virtual FaviconStatus& GetFavicon() = 0;

  // All the SSL flags and state. See web::SSLStatus.
  virtual const SSLStatus& GetSSL() const = 0;
  virtual SSLStatus& GetSSL() = 0;

  // The time at which the last known local navigation has
  // completed. (A navigation can be completed more than once if the
  // page is reloaded.)
  //
  // If GetTimestamp() returns a null time, that means that either:
  //
  //   - this navigation hasn't completed yet;
  //   - this navigation was restored and for some reason the
  //     timestamp wasn't available;
  //   - or this navigation was copied from a foreign session.
  virtual void SetTimestamp(base::Time timestamp) = 0;
  virtual base::Time GetTimestamp() const = 0;

  // The type of user agent requested for the navigation.
  // If |update_inherited_user_agent| is false, then the user agent type used
  // for inheritance won't be changed. If it is true, the the user agent for
  // inheritance will be updated to |type|.
  // See NavigationManager UserAgentOverrideOption::INHERIT.
  // TODO(crbug.com/697512): Create equivalent enum type for WebContents.
  virtual void SetUserAgentType(UserAgentType type,
                                bool update_inherited_user_agent) = 0;
  virtual UserAgentType GetUserAgentType() const = 0;

  // Returns the type of user agent to be used for inheritance. Inheritance is
  // used to know the User Agent type when a new navigation is started. This
  // type could be different from GetUserAgentType() as if the Inherited user
  // agent could be the automatic one, whereas the GetUserAgentType is the one
  // actually used by this item.
  virtual UserAgentType GetUserAgentForInheritance() const = 0;

  // |true| if this item is the result of a POST request with data.
  virtual bool HasPostData() const = 0;

  // Returns the item's current http request headers.
  virtual NSDictionary* GetHttpRequestHeaders() const = 0;

  // Adds headers from |additional_headers| to the item's http request headers.
  // Existing headers with the same key will be overridden.
  virtual void AddHttpRequestHeaders(NSDictionary* additional_headers) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_ITEM_H_
