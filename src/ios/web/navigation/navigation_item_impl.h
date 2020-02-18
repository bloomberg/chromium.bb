// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/string16.h"
#include "ios/web/navigation/error_retry_state_machine.h"
#include "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/security/ssl_status.h"
#include "url/gurl.h"

namespace web {

class NavigationItemStorageBuilder;
enum class NavigationInitiationType;

// Implementation of NavigationItem.
class NavigationItemImpl : public web::NavigationItem {
 public:
  // Creates a default NavigationItemImpl.
  NavigationItemImpl();
  ~NavigationItemImpl() override;

  // Since NavigationItemImpls own their facade delegates, there is no implicit
  // copy constructor (scoped_ptrs can't be copied), so one is defined here.
  NavigationItemImpl(const NavigationItemImpl& item);

  // NavigationItem implementation:
  int GetUniqueID() const override;
  void SetOriginalRequestURL(const GURL& url) override;
  const GURL& GetOriginalRequestURL() const override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;
  void SetReferrer(const web::Referrer& referrer) override;
  const web::Referrer& GetReferrer() const override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() const override;
  void SetTitle(const base::string16& title) override;
  const base::string16& GetTitle() const override;
  void SetPageDisplayState(const PageDisplayState& display_state) override;
  const PageDisplayState& GetPageDisplayState() const override;
  const base::string16& GetTitleForDisplay() const override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() const override;
  const FaviconStatus& GetFavicon() const override;
  FaviconStatus& GetFavicon() override;
  const SSLStatus& GetSSL() const override;
  SSLStatus& GetSSL() override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() const override;
  void SetUserAgentType(UserAgentType type,
                        bool update_inherited_user_agent) override;
  UserAgentType GetUserAgentType() const override;
  UserAgentType GetUserAgentForInheritance() const override;
  bool HasPostData() const override;
  NSDictionary* GetHttpRequestHeaders() const override;
  void AddHttpRequestHeaders(NSDictionary* additional_headers) override;

  // Serialized representation of the state object that was used in conjunction
  // with a JavaScript window.history.pushState() or
  // window.history.replaceState() call that created or modified this
  // NavigationItem. Intended to be used for JavaScript history operations and
  // will be nil in most cases.
  void SetSerializedStateObject(NSString* serialized_state_object);
  NSString* GetSerializedStateObject() const;

  // Whether or not this item was created by calling history.pushState().
  void SetIsCreatedFromPushState(bool push_state);
  bool IsCreatedFromPushState() const;

  // Whether the state for this navigation has been changed by
  // history.replaceState().
  // TODO(crbug.com/659816): This state is only tracked because of flaky early
  // page script injection.  Once the root cause of this flake is found, this
  // can be removed.
  void SetHasStateBeenReplaced(bool replace_state);
  bool HasStateBeenReplaced() const;

  // Whether this navigation is the result of a hash change.
  void SetIsCreatedFromHashChange(bool hash_change);
  bool IsCreatedFromHashChange() const;

  // Initiation type of this pending navigation. Resets to NONE after commit.
  void SetNavigationInitiationType(
      web::NavigationInitiationType navigation_initiation_type);
  web::NavigationInitiationType NavigationInitiationType() const;

  // Whether or not to bypass showing the repost form confirmation when loading
  // a POST request. Set to YES for browser-generated POST requests.
  void SetShouldSkipRepostFormConfirmation(bool skip);
  bool ShouldSkipRepostFormConfirmation() const;

  // Whether or not to bypass serializing this item to session storage.  Set to
  // YES to skip saving this page (and therefore restoring this page).
  void SetShouldSkipSerialization(bool skip);
  bool ShouldSkipSerialization() const;

  // Data submitted with a POST request, persisted for resubmits.
  void SetPostData(NSData* post_data);
  NSData* GetPostData() const;

  // Removes the header for |key| from |http_request_headers_|.
  void RemoveHttpRequestHeaderForKey(NSString* key);

  // Removes all http headers from |http_request_headers_|.
  void ResetHttpRequestHeaders();

  // Once a navigation item is committed, we should no longer track
  // non-persisted state, as documented on the members below.
  void ResetForCommit();

  // Returns the state machine that manages the displaying and retrying of load
  // error for this item.
  ErrorRetryStateMachine& error_retry_state_machine();

  // Returns the title string to be used for a page with |url| if that page
  // doesn't specify a title.
  static base::string16 GetDisplayTitleForURL(const GURL& url);

  // Used only by WKBasedNavigationManager.  SetUntrusted() is only used for
  // Visible or LastCommitted NavigationItems where the |url_| may be incorrect
  // due to timining problems or bugs in WKWebView.
  void SetUntrusted();
  bool IsUntrusted();

#ifndef NDEBUG
  // Returns a human-readable description of the state for debugging purposes.
  NSString* GetDescription() const;
#endif

 private:
  // The NavigationManItemStorageBuilder functions require access to
  // private variables of NavigationItemImpl.
  friend NavigationItemStorageBuilder;

  int unique_id_;
  GURL original_request_url_;
  GURL url_;
  Referrer referrer_;
  GURL virtual_url_;
  base::string16 title_;
  PageDisplayState page_display_state_;
  ui::PageTransition transition_type_;
  FaviconStatus favicon_;
  SSLStatus ssl_;
  base::Time timestamp_;
  UserAgentType user_agent_type_;
  UserAgentType user_agent_type_inheritance_;
  NSMutableDictionary* http_request_headers_;

  NSString* serialized_state_object_;
  bool is_created_from_push_state_;
  bool has_state_been_replaced_;
  bool is_created_from_hash_change_;
  bool should_skip_repost_form_confirmation_;
  bool should_skip_serialization_;
  NSData* post_data_;
  ErrorRetryStateMachine error_retry_state_machine_;

  // The navigation initiation type of the item.  This decides whether the URL
  // should be displayed before the navigation commits.  It is cleared in
  // |ResetForCommit| and not persisted.
  web::NavigationInitiationType navigation_initiation_type_;

  // Used only by WKBasedNavigationManager.  |is_untrusted_| is only |true| for
  // Visible or LastCommitted NavigationItems where the |url_| may be incorrect
  // due to timining problems or bugs in WKWebView.
  bool is_untrusted_;

  // This is a cached version of the result of GetTitleForDisplay. When the URL,
  // virtual URL, or title is set, this should be cleared to force a refresh.
  mutable base::string16 cached_display_title_;

  // Copy and assignment is explicitly allowed for this class.
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
