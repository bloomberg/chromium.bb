// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/text_elider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a new unique ID for use in NavigationItem during construction.  The
// returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
static int GetUniqueIDInConstructor() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

namespace web {

// Value 50 was picked experimentally by examining Chrome for iOS UI. Tab strip
// on 12.9" iPad Pro trucates the title to less than 50 characters (title that
// only consists of letters "i"). Tab strip has the biggest surface to fit
// title.
const size_t kMaxTitleLength = 50;

// static
std::unique_ptr<NavigationItem> NavigationItem::Create() {
  return std::unique_ptr<NavigationItem>(new NavigationItemImpl());
}

NavigationItemImpl::NavigationItemImpl()
    : unique_id_(GetUniqueIDInConstructor()),
      transition_type_(ui::PAGE_TRANSITION_LINK),
      user_agent_type_(UserAgentType::NONE),
      is_created_from_hash_change_(false),
      should_skip_repost_form_confirmation_(false),
      should_skip_serialization_(false),
      navigation_initiation_type_(web::NavigationInitiationType::NONE),
      is_untrusted_(false),
      is_upgraded_to_https_(false) {}

NavigationItemImpl::~NavigationItemImpl() {
}

NavigationItemImpl::NavigationItemImpl(const NavigationItemImpl& item)
    : unique_id_(item.unique_id_),
      original_request_url_(item.original_request_url_),
      url_(item.url_),
      referrer_(item.referrer_),
      virtual_url_(item.virtual_url_),
      title_(item.title_),
      page_display_state_(item.page_display_state_),
      transition_type_(item.transition_type_),
      favicon_(item.favicon_),
      ssl_(item.ssl_),
      timestamp_(item.timestamp_),
      user_agent_type_(item.user_agent_type_),
      http_request_headers_([item.http_request_headers_ mutableCopy]),
      serialized_state_object_([item.serialized_state_object_ copy]),
      is_created_from_hash_change_(item.is_created_from_hash_change_),
      should_skip_repost_form_confirmation_(
          item.should_skip_repost_form_confirmation_),
      should_skip_serialization_(item.should_skip_serialization_),
      post_data_([item.post_data_ copy]),
      error_retry_state_machine_(item.error_retry_state_machine_),
      navigation_initiation_type_(item.navigation_initiation_type_),
      is_untrusted_(item.is_untrusted_),
      cached_display_title_(item.cached_display_title_),
      is_upgraded_to_https_(item.is_upgraded_to_https_) {}

int NavigationItemImpl::GetUniqueID() const {
  return unique_id_;
}

void NavigationItemImpl::SetOriginalRequestURL(const GURL& url) {
  original_request_url_ = url;
}

const GURL& NavigationItemImpl::GetOriginalRequestURL() const {
  return original_request_url_;
}

void NavigationItemImpl::SetURL(const GURL& url) {
  url_ = url;
  cached_display_title_.clear();
  error_retry_state_machine_.SetURL(url);
}

const GURL& NavigationItemImpl::GetURL() const {
  return url_;
}

void NavigationItemImpl::SetReferrer(const web::Referrer& referrer) {
  referrer_ = referrer;
}

const web::Referrer& NavigationItemImpl::GetReferrer() const {
  return referrer_;
}

void NavigationItemImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == url_) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationItemImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? url_ : virtual_url_;
}

void NavigationItemImpl::SetTitle(const std::u16string& title) {
  if (title_ == title)
    return;

  if (title.size() > kMaxTitleLength) {
    title_ = gfx::TruncateString(title, kMaxTitleLength, gfx::CHARACTER_BREAK);
  } else {
    title_ = title;
  }
  cached_display_title_.clear();
}

const std::u16string& NavigationItemImpl::GetTitle() const {
  return title_;
}

void NavigationItemImpl::SetPageDisplayState(
    const web::PageDisplayState& display_state) {
  page_display_state_ = display_state;
}

const PageDisplayState& NavigationItemImpl::GetPageDisplayState() const {
  return page_display_state_;
}

const std::u16string& NavigationItemImpl::GetTitleForDisplay() const {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // File urls have different display rules, so use one if it is present.
  cached_display_title_ = NavigationItemImpl::GetDisplayTitleForURL(
      GetURL().SchemeIsFile() ? GetURL() : GetVirtualURL());
  return cached_display_title_;
}

void NavigationItemImpl::SetTransitionType(ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationItemImpl::GetTransitionType() const {
  return transition_type_;
}

const FaviconStatus& NavigationItemImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationItemImpl::GetFavicon() {
  return favicon_;
}

const SSLStatus& NavigationItemImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationItemImpl::GetSSL() {
  return ssl_;
}

void NavigationItemImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationItemImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationItemImpl::SetUserAgentType(UserAgentType type) {
  user_agent_type_ = type;
  DCHECK_EQ(!wk_navigation_util::URLNeedsUserAgentType(GetURL()),
            user_agent_type_ == UserAgentType::NONE);
}

void NavigationItemImpl::SetUntrusted() {
  is_untrusted_ = true;
}

bool NavigationItemImpl::IsUntrusted() {
  return is_untrusted_;
}

UserAgentType NavigationItemImpl::GetUserAgentType() const {
  return user_agent_type_;
}

bool NavigationItemImpl::HasPostData() const {
  return post_data_ != nil;
}

NSDictionary* NavigationItemImpl::GetHttpRequestHeaders() const {
  return [http_request_headers_ copy];
}

void NavigationItemImpl::AddHttpRequestHeaders(
    NSDictionary* additional_headers) {
  if (!additional_headers)
    return;

  if (http_request_headers_)
    [http_request_headers_ addEntriesFromDictionary:additional_headers];
  else
    http_request_headers_ = [additional_headers mutableCopy];
}

void NavigationItemImpl::SetUpgradedToHttps() {
  is_upgraded_to_https_ = true;
}

bool NavigationItemImpl::IsUpgradedToHttps() const {
  return is_upgraded_to_https_;
}

void NavigationItemImpl::SetSerializedStateObject(
    NSString* serialized_state_object) {
  serialized_state_object_ = serialized_state_object;
}

NSString* NavigationItemImpl::GetSerializedStateObject() const {
  return serialized_state_object_;
}

void NavigationItemImpl::SetNavigationInitiationType(
    web::NavigationInitiationType navigation_initiation_type) {
  navigation_initiation_type_ = navigation_initiation_type;
}

web::NavigationInitiationType NavigationItemImpl::NavigationInitiationType()
    const {
  return navigation_initiation_type_;
}

void NavigationItemImpl::SetIsCreatedFromHashChange(bool hash_change) {
  is_created_from_hash_change_ = hash_change;
}

bool NavigationItemImpl::IsCreatedFromHashChange() const {
  return is_created_from_hash_change_;
}

void NavigationItemImpl::SetShouldSkipRepostFormConfirmation(bool skip) {
  should_skip_repost_form_confirmation_ = skip;
}

bool NavigationItemImpl::ShouldSkipRepostFormConfirmation() const {
  return should_skip_repost_form_confirmation_;
}

void NavigationItemImpl::SetShouldSkipSerialization(bool skip) {
  should_skip_serialization_ = skip;
}

bool NavigationItemImpl::ShouldSkipSerialization() const {
  return should_skip_serialization_;
}

void NavigationItemImpl::SetPostData(NSData* post_data) {
  post_data_ = post_data;
}

NSData* NavigationItemImpl::GetPostData() const {
  return post_data_;
}

void NavigationItemImpl::RemoveHttpRequestHeaderForKey(NSString* key) {
  DCHECK(key);
  [http_request_headers_ removeObjectForKey:key];
  if (![http_request_headers_ count])
    http_request_headers_ = nil;
}

void NavigationItemImpl::ResetHttpRequestHeaders() {
  http_request_headers_ = nil;
}

void NavigationItemImpl::ResetForCommit() {
  // Navigation initiation type is only valid for pending navigations, thus
  // always reset to NONE after the item is committed.
  SetNavigationInitiationType(web::NavigationInitiationType::NONE);
}

void NavigationItemImpl::RestoreStateFromItem(NavigationItem* other) {
  // Restore the UserAgent type in any case, as if the URLs are different it
  // might mean that |this| is a next navigation. The page display state and the
  // virtual URL only make sense if it is the same item. The other headers might
  // not make sense after creating a new navigation to the page.
  if (other->GetUserAgentType() != UserAgentType::NONE) {
    SetUserAgentType(other->GetUserAgentType());
  }
  if (url_ == other->GetURL()) {
    SetPageDisplayState(other->GetPageDisplayState());
    SetVirtualURL(other->GetVirtualURL());
  }
}

ErrorRetryStateMachine& NavigationItemImpl::error_retry_state_machine() {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  return error_retry_state_machine_;
}

// static
std::u16string NavigationItemImpl::GetDisplayTitleForURL(const GURL& url) {
  if (url.is_empty())
    return std::u16string();

  std::u16string title = url_formatter::FormatUrl(url);

  // For file:// URLs use the filename as the title, not the full path.
  if (url.SchemeIsFile()) {
    std::u16string::size_type slashpos = title.rfind('/');
    if (slashpos != std::u16string::npos && slashpos != (title.size() - 1))
      title = title.substr(slashpos + 1);
  }

  const size_t kMaxTitleChars = 4 * 1024;
  gfx::ElideString(title, kMaxTitleChars, &title);
  return title;
}

#ifndef NDEBUG
NSString* NavigationItemImpl::GetDescription() const {
  return [NSString
      stringWithFormat:
          @"url:%s virtual_url_:%s originalurl:%s referrer: %s title:%s "
          @"transition:%d "
           "displayState:%@ userAgent:%s "
           "is_created_from_hash_change: %@ "
           "navigation_initiation_type: %d "
           "is_upgraded_to_https: %@",
          url_.spec().c_str(), virtual_url_.spec().c_str(),
          original_request_url_.spec().c_str(), referrer_.url.spec().c_str(),
          base::UTF16ToUTF8(title_).c_str(), transition_type_,
          page_display_state_.GetDescription(),
          GetUserAgentTypeDescription(user_agent_type_).c_str(),
          is_created_from_hash_change_ ? @"true" : @"false",
          navigation_initiation_type_,
          is_upgraded_to_https_ ? @"true" : @"false"];
}
#endif

}  // namespace web
