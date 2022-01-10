// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_impl.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/web_package/subresource_web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/reload_type.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

using base::UTF16ToUTF8;

namespace content {

namespace {

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
int CreateUniqueEntryID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

void RecursivelyGenerateFrameEntries(
    NavigationEntryRestoreContextImpl* context,
    const blink::ExplodedFrameState& state,
    const std::vector<absl::optional<std::u16string>>& referenced_files,
    NavigationEntryImpl::TreeNode* node) {
  DCHECK(context);
  // Set a single-frame PageState on the entry.
  blink::ExplodedPageState page_state;

  // Copy the incoming PageState's list of referenced files into the main
  // frame's PageState.  (We don't pass the list to subframes below.)
  // TODO(creis): Grant access to this list for each process that renders
  // this page, even for OOPIFs.  Eventually keep track of a verified list of
  // files per frame, so that we only grant access to processes that need it.
  if (referenced_files.size() > 0)
    page_state.referenced_files = referenced_files;

  page_state.top = state;
  std::string data;
  blink::EncodePageState(page_state, &data);
  DCHECK(!data.empty()) << "Shouldn't generate an empty PageState.";

  GURL state_url(state.url_string.value_or(std::u16string()));
  scoped_refptr<FrameNavigationEntry> entry = context->GetFrameNavigationEntry(
      state.item_sequence_number,
      state.target ? base::UTF16ToUTF8(*state.target) : "", state_url);
  DCHECK(!entry || entry->initiator_origin() == state.initiator_origin);
  if (!entry) {
    entry = base::MakeRefCounted<FrameNavigationEntry>(
        UTF16ToUTF8(state.target.value_or(std::u16string())),
        state.item_sequence_number, state.document_sequence_number,
        UTF16ToUTF8(state.app_history_key.value_or(std::u16string())), nullptr,
        nullptr, state_url,
        // TODO(nasko): Supply valid origin once the value is persisted across
        // session restore.
        absl::nullopt /* origin */,
        Referrer(GURL(state.referrer.value_or(std::u16string())),
                 state.referrer_policy),
        state.initiator_origin, std::vector<GURL>(),
        blink::PageState::CreateFromEncodedData(data), "GET", -1,
        nullptr /* blob_url_loader_factory */,
        nullptr /* web_bundle_navigation_info */,
        nullptr /* subresource_web_bundle_navigation_info */,
        // TODO(https://crbug.com/1140393): We should restore the policy
        // container.
        nullptr /* policy_container_policies */);
    context->AddFrameNavigationEntry(entry.get());
  }
  node->frame_entry = std::move(entry);

  // Don't pass the file list to subframes, since that would result in multiple
  // copies of it ending up in the combined list in GetPageState (via
  // RecursivelyGenerateFrameState).
  std::vector<absl::optional<std::u16string>> empty_file_list;

  for (const blink::ExplodedFrameState& child_state : state.children) {
    node->children.push_back(
        std::make_unique<NavigationEntryImpl::TreeNode>(node, nullptr));
    RecursivelyGenerateFrameEntries(context, child_state, empty_file_list,
                                    node->children.back().get());
  }
}

absl::optional<std::u16string> UrlToOptionalString16(const GURL& url) {
  if (!url.is_valid())
    return absl::nullopt;
  return base::UTF8ToUTF16(url.spec());
}

void RecursivelyGenerateFrameState(
    NavigationEntryImpl::TreeNode* node,
    blink::ExplodedFrameState* state,
    std::vector<absl::optional<std::u16string>>* referenced_files) {
  // The FrameNavigationEntry's PageState contains just the ExplodedFrameState
  // for that particular frame.
  blink::ExplodedPageState exploded_page_state;
  if (!blink::DecodePageState(node->frame_entry->page_state().ToEncodedData(),
                              &exploded_page_state)) {
    NOTREACHED();
    return;
  }
  blink::ExplodedFrameState frame_state = exploded_page_state.top;

  // Copy the FrameNavigationEntry's frame state into the destination state.
  *state = frame_state;

  // Some data is stored *both* in 1) PageState/ExplodedFrameState and 2)
  // FrameNavigationEntry.  We want to treat FrameNavigationEntry as the
  // authoritative source of the data, so we clobber the ExplodedFrameState with
  // the data taken from FrameNavigationEntry.
  //
  // The following ExplodedFrameState fields do not have an equivalent
  // FrameNavigationEntry field:
  // - target
  // - state_object
  // - document_state
  // - scroll_restoration_type
  // - did_save_scroll_or_scale_state
  // - visual_viewport_scroll_offset
  // - scroll_offset
  // - page_scale_factor
  // - http_body (FrameNavigationEntry::GetPostData extracts the body from
  //   the ExplodedFrameState)
  // - scroll_anchor_selector
  // - scroll_anchor_offset
  // - scroll_anchor_simhash
  state->url_string = UrlToOptionalString16(node->frame_entry->url());
  state->referrer = UrlToOptionalString16(node->frame_entry->referrer().url);
  state->referrer_policy = node->frame_entry->referrer().policy;
  state->item_sequence_number = node->frame_entry->item_sequence_number();
  state->document_sequence_number =
      node->frame_entry->document_sequence_number();
  state->initiator_origin = node->frame_entry->initiator_origin();

  // Copy the frame's files into the PageState's |referenced_files|.
  referenced_files->reserve(referenced_files->size() +
                            exploded_page_state.referenced_files.size());
  for (auto& file : exploded_page_state.referenced_files)
    referenced_files->emplace_back(file);

  state->children.resize(node->children.size());
  for (size_t i = 0; i < node->children.size(); ++i) {
    RecursivelyGenerateFrameState(node->children[i].get(), &state->children[i],
                                  referenced_files);
  }
}

// Walk the ancestor chain for both the |frame_tree_node| and the |node|.
// Comparing the inputs directly is not performed, as this method assumes they
// already match each other. Returns false if a mismatch in unique name or
// ancestor chain is detected, otherwise true.
bool InSameTreePosition(FrameTreeNode* frame_tree_node,
                        NavigationEntryImpl::TreeNode* node) {
  FrameTreeNode* ftn = FrameTreeNode::From(frame_tree_node->parent());
  NavigationEntryImpl::TreeNode* current_node = node->parent;
  while (ftn && current_node) {
    if (!current_node->MatchesFrame(ftn))
      return false;

    if ((!current_node->parent && ftn->parent()) ||
        (current_node->parent && !ftn->parent())) {
      return false;
    }

    ftn = FrameTreeNode::From(ftn->parent());
    current_node = current_node->parent;
  }
  return true;
}

void RegisterOriginsRecursive(NavigationEntryImpl::TreeNode* node,
                              const url::Origin& origin) {
  if (node->frame_entry->committed_origin().has_value()) {
    const url::Origin node_origin =
        node->frame_entry->committed_origin().value();
    SiteInstanceImpl* site_instance = node->frame_entry->site_instance();
    if (site_instance && origin == node_origin)
      site_instance->PreventOptInOriginIsolation(node_origin);
  }

  for (auto& child : node->children)
    RegisterOriginsRecursive(child.get(), origin);
}

}  // namespace

void NavigationEntryImpl::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& origin) {
  return RegisterOriginsRecursive(root_node(), origin);
}

NavigationEntryImpl::TreeNode::TreeNode(
    TreeNode* parent,
    scoped_refptr<FrameNavigationEntry> frame_entry)
    : parent(parent), frame_entry(std::move(frame_entry)) {}

NavigationEntryImpl::TreeNode::~TreeNode() {}

bool NavigationEntryImpl::TreeNode::MatchesFrame(
    FrameTreeNode* frame_tree_node) const {
  if (!frame_tree_node) {
    SCOPED_CRASH_KEY_BOOL("NoFTN", "is_main_frame", !parent);
    SCOPED_CRASH_KEY_NUMBER("NoFTN", "children_size", children.size());
    base::debug::DumpWithoutCrashing();
    return false;
  }
  // The root node is for the main frame whether the unique name matches or not.
  if (!parent)
    return frame_tree_node->IsMainFrame();

  // Otherwise check the unique name for subframes.
  return !frame_tree_node->IsMainFrame() &&
         frame_tree_node->unique_name() == frame_entry->frame_unique_name();
}

std::unique_ptr<NavigationEntryImpl::TreeNode>
NavigationEntryImpl::TreeNode::CloneAndReplace(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* current_frame_tree_node,
    TreeNode* parent_node,
    NavigationEntryRestoreContextImpl* restore_context,
    ClonePolicy clone_policy) const {
  // |restore_context| should only ever be used when doing a deep clone, and
  // when there is no target.
  if (restore_context) {
    DCHECK(!frame_navigation_entry && !target_frame_tree_node &&
           clone_policy == ClonePolicy::kCloneFrameEntries);
  }

  // Clone this TreeNode, possibly replacing its FrameNavigationEntry.
  bool is_target_frame =
      target_frame_tree_node && MatchesFrame(target_frame_tree_node);

  scoped_refptr<FrameNavigationEntry> new_entry;
  if (is_target_frame) {
    new_entry = frame_navigation_entry;
  } else if (clone_policy == ClonePolicy::kShareFrameEntries) {
    new_entry = frame_entry;
  } else {
    if (restore_context) {
      // If |restore_context| is given and already has a FrameNavigationEntry
      // for the given item sequence number and URL, share that
      // FrameNavigationEntry rather than creating a duplicate.
      new_entry = restore_context->GetFrameNavigationEntry(
          frame_entry->item_sequence_number(), frame_entry->frame_unique_name(),
          frame_entry->url());
    }
    if (!new_entry) {
      new_entry = frame_entry->Clone();
      if (restore_context)
        restore_context->AddFrameNavigationEntry(new_entry.get());
    }
  }

  auto copy = std::make_unique<NavigationEntryImpl::TreeNode>(
      parent_node, std::move(new_entry));

  // Recursively clone the children if needed.
  if (!is_target_frame || clone_children_of_target) {
    for (size_t i = 0; i < children.size(); i++) {
      const auto& child = children[i];

      // Don't check whether it's still in the tree if |current_frame_tree_node|
      // is null.
      if (!current_frame_tree_node) {
        copy->children.push_back(child->CloneAndReplace(
            frame_navigation_entry, clone_children_of_target,
            target_frame_tree_node, nullptr, copy.get(), restore_context,
            clone_policy));
        continue;
      }

      // Otherwise, make sure the frame is still in the tree before cloning it.
      // This is O(N^2) in the worst case because we need to look up each frame
      // (and since we want to avoid a map of unique names, which can be very
      // long).  To partly mitigate this, we add an optimization for the common
      // case that the two child lists are the same length and are likely in the
      // same order: we pick the starting offset of the inner loop to get O(N).
      size_t ftn_child_count = current_frame_tree_node->child_count();
      for (size_t j = 0; j < ftn_child_count; j++) {
        size_t index = j;
        // If the two lists of children are the same length, start looking at
        // the same index as |child|.
        if (children.size() == ftn_child_count)
          index = (i + j) % ftn_child_count;

        if (current_frame_tree_node->child_at(index)->unique_name() ==
            child->frame_entry->frame_unique_name()) {
          // Found |child| in the tree.  Clone it and break out of inner loop.
          copy->children.push_back(child->CloneAndReplace(
              frame_navigation_entry, clone_children_of_target,
              target_frame_tree_node, current_frame_tree_node->child_at(index),
              copy.get(), restore_context, clone_policy));
          break;
        }
      }
    }
  }

  return copy;
}

std::unique_ptr<NavigationEntry> NavigationEntry::Create() {
  return std::make_unique<NavigationEntryImpl>();
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

const NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    const NavigationEntry* entry) {
  return static_cast<const NavigationEntryImpl*>(entry);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::FromNavigationEntry(
    std::unique_ptr<NavigationEntry> entry) {
  return base::WrapUnique(FromNavigationEntry(entry.release()));
}

NavigationEntryImpl::NavigationEntryImpl()
    : NavigationEntryImpl(nullptr,
                          GURL(),
                          Referrer(),
                          absl::nullopt,
                          std::u16string(),
                          ui::PAGE_TRANSITION_LINK,
                          false,
                          nullptr,
                          /* is_initial_entry = */ false) {}

NavigationEntryImpl::NavigationEntryImpl(
    scoped_refptr<SiteInstanceImpl> instance,
    const GURL& url,
    const Referrer& referrer,
    const absl::optional<url::Origin>& initiator_origin,
    const std::u16string& title,
    ui::PageTransition transition_type,
    bool is_renderer_initiated,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_initial_entry)
    : frame_tree_(std::make_unique<TreeNode>(
          nullptr,
          base::MakeRefCounted<FrameNavigationEntry>(
              "",
              -1,
              -1,
              "",
              std::move(instance),
              nullptr,
              url,
              absl::nullopt /* origin */,
              referrer,
              initiator_origin,
              std::vector<GURL>(),
              blink::PageState(),
              "GET",
              -1,
              std::move(blob_url_loader_factory),
              nullptr /* web_bundle_navigation_info */,
              nullptr /* subresource_web_bundle_navigation_info */,
              nullptr /* policy_container_policies */))),
      unique_id_(CreateUniqueEntryID()),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      transition_type_(transition_type),
      restore_type_(RestoreType::kNotRestored),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(FrameTreeNode::kFrameTreeNodeInvalidId),
      has_user_gesture_(false),
      reload_type_(ReloadType::NONE),
      started_from_context_menu_(false),
      ssl_error_(false),
      should_skip_on_back_forward_ui_(false),
      is_initial_entry_(is_initial_entry) {}

NavigationEntryImpl::~NavigationEntryImpl() {}

int NavigationEntryImpl::GetUniqueID() {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() {
  return base_url_for_data_url_;
}

#if defined(OS_ANDROID)
void NavigationEntryImpl::SetDataURLAsString(
    scoped_refptr<base::RefCountedString> data_url) {
  if (data_url) {
    // A quick check that it's actually a data URL.
    DCHECK(base::StartsWith(data_url->front_as<char>(), url::kDataScheme,
                            base::CompareCase::SENSITIVE));
  }
  data_url_as_string_ = std::move(data_url);
}

const scoped_refptr<const base::RefCountedString>&
NavigationEntryImpl::GetDataURLAsString() {
  return data_url_as_string_;
}
#endif

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const std::u16string& title) {
  title_ = title;
  cached_display_title_.clear();
}

const std::u16string& NavigationEntryImpl::GetTitle() {
  return title_;
}

void NavigationEntryImpl::SetPageState(const blink::PageState& state,
                                       NavigationEntryRestoreContext* context) {
  DCHECK(state.IsValid());
  DCHECK(context);

  // SetPageState should only be called before the NavigationEntry has been
  // loaded, such as for restore (when there are no subframe
  // FrameNavigationEntries yet).  However, some callers expect to call this
  // after a Clone but before loading the page.  Clone will copy over the
  // subframe entries, and we should reset them before setting the state again.
  //
  // TODO(creis): It would be good to verify that this NavigationEntry hasn't
  // been loaded yet in cases that SetPageState is called while subframe
  // entries exist, but there's currently no way to check that.
  if (!frame_tree_->children.empty())
    frame_tree_->children.clear();

  // If the PageState can't be parsed, just store it on the main frame's
  // FrameNavigationEntry without recursively creating subframe entries.
  blink::ExplodedPageState exploded_state;
  if (!blink::DecodePageState(state.ToEncodedData(), &exploded_state)) {
    frame_tree_->frame_entry->SetPageState(state);
    return;
  }

  RecursivelyGenerateFrameEntries(
      static_cast<NavigationEntryRestoreContextImpl*>(context),
      exploded_state.top, exploded_state.referenced_files, frame_tree_.get());
}

blink::PageState NavigationEntryImpl::GetPageState() {
  // Each FrameNavigationEntry has a frame-specific PageState.  We combine these
  // into an ExplodedPageState tree and generate a full PageState from it.
  blink::ExplodedPageState exploded_state;
  RecursivelyGenerateFrameState(frame_tree_.get(), &exploded_state.top,
                                &exploded_state.referenced_files);

  std::string encoded_data;
  blink::EncodePageState(exploded_state, &encoded_data);
  return blink::PageState::CreateFromEncodedData(encoded_data);
}

void NavigationEntryImpl::set_site_instance(
    scoped_refptr<SiteInstanceImpl> site_instance) {
  // TODO(creis): Update all callers and remove this method.
  frame_tree_->frame_entry->set_site_instance(std::move(site_instance));
}

const std::u16string& NavigationEntryImpl::GetTitleForDisplay() {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  std::u16string title;
  if (!virtual_url_.is_empty()) {
    title = url_formatter::FormatUrl(virtual_url_);
  } else if (!GetURL().is_empty()) {
    title = url_formatter::FormatUrl(GetURL());
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    std::u16string::size_type refpos = title.find('#');
    std::u16string::size_type querypos = title.find('?');
    std::u16string::size_type lastpos;
    if (refpos == std::u16string::npos)
      lastpos = querypos;
    else if (querypos == std::u16string::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    std::u16string::size_type slashpos = title.rfind('/', lastpos);
    if (slashpos != std::u16string::npos)
      title = title.substr(slashpos + 1);

  } else if (GetURL().SchemeIs(kChromeUIUntrustedScheme)) {
    // For chrome-untrusted:// URLs, leave title blank until the page loads.
    title = std::u16string();

  } else if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

#if defined(OS_ANDROID)
  if (GetURL().SchemeIs(url::kContentScheme)) {
    std::u16string file_display_name;
    if (base::MaybeGetFileDisplayName(base::FilePath(GetURL().spec()),
                                      &file_display_name)) {
      title = file_display_name;
    }
  }
#endif

  gfx::ElideString(title, blink::mojom::kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  frame_tree_->frame_entry->set_method(has_post_data ? "POST" : "GET");
}

bool NavigationEntryImpl::GetHasPostData() {
  return frame_tree_->frame_entry->method() == "POST";
}

void NavigationEntryImpl::SetPostID(int64_t post_id) {
  frame_tree_->frame_entry->set_post_id(post_id);
}

int64_t NavigationEntryImpl::GetPostID() {
  return frame_tree_->frame_entry->post_id();
}

void NavigationEntryImpl::SetPostData(
    const scoped_refptr<network::ResourceRequestBody>& data) {
  post_data_ = static_cast<network::ResourceRequestBody*>(data.get());
}

scoped_refptr<network::ResourceRequestBody> NavigationEntryImpl::GetPostData() {
  return post_data_.get();
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override_ua) {
  is_overriding_user_agent_ = override_ua;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  root_node()->frame_entry->set_redirect_chain(redirect_chain);
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() {
  return root_node()->frame_entry->redirect_chain();
}

const absl::optional<ReplacedNavigationEntryData>&
NavigationEntryImpl::GetReplacedEntryData() {
  return replaced_entry_data_;
}

bool NavigationEntryImpl::IsRestored() {
  return restore_type_ == RestoreType::kRestored;
}

std::string NavigationEntryImpl::GetExtraHeaders() {
  return extra_headers_;
}

void NavigationEntryImpl::AddExtraHeaders(
    const std::string& more_extra_headers) {
  DCHECK(!more_extra_headers.empty());
  if (!extra_headers_.empty())
    extra_headers_ += "\r\n";
  extra_headers_ += more_extra_headers;
}

int64_t NavigationEntryImpl::GetMainFrameDocumentSequenceNumber() {
  return frame_tree_->frame_entry->document_sequence_number();
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() {
  return can_load_local_resources_;
}

bool NavigationEntryImpl::IsInitialEntry() {
  return is_initial_entry_;
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  std::unique_ptr<NavigationEntryImpl> entry =
      CloneAndReplaceInternal(nullptr, false, nullptr, nullptr, nullptr,
                              ClonePolicy::kShareFrameEntries);
  // When we are not deep-copying, the NavigationEntry is going to be used for
  // a new committed navigation, so it loses its "initial" status.
  entry->set_is_initial_entry(false);
  return entry;
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneWithoutSharing(
    NavigationEntryRestoreContextImpl* restore_context) const {
  DCHECK(restore_context);
  return CloneAndReplaceInternal(nullptr, false, nullptr, nullptr,
                                 restore_context,
                                 ClonePolicy::kCloneFrameEntries);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node) const {
  std::unique_ptr<NavigationEntryImpl> entry = CloneAndReplaceInternal(
      frame_navigation_entry, clone_children_of_target, target_frame_tree_node,
      root_frame_tree_node, nullptr, ClonePolicy::kShareFrameEntries);
  // When we are not deep-copying, the NavigationEntry is going to be used for
  // a new committed navigation, so it loses its "initial" status.
  entry->set_is_initial_entry(false);
  return entry;
}

std::unique_ptr<NavigationEntryImpl>
NavigationEntryImpl::CloneAndReplaceInternal(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node,
    NavigationEntryRestoreContextImpl* restore_context,
    ClonePolicy clone_policy) const {
  auto copy = std::make_unique<NavigationEntryImpl>();

  copy->frame_tree_ = frame_tree_->CloneAndReplace(
      std::move(frame_navigation_entry), clone_children_of_target,
      target_frame_tree_node, root_frame_tree_node, nullptr, restore_context,
      clone_policy);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
  copy->page_type_ = page_type_;
  copy->virtual_url_ = virtual_url_;
  copy->update_virtual_url_with_url_ = update_virtual_url_with_url_;
  copy->title_ = title_;
  copy->favicon_ = favicon_;
  copy->ssl_ = ssl_;
  copy->transition_type_ = transition_type_;
  copy->user_typed_url_ = user_typed_url_;
  copy->restore_type_ = restore_type_;
  copy->original_request_url_ = original_request_url_;
  copy->is_overriding_user_agent_ = is_overriding_user_agent_;
  copy->timestamp_ = timestamp_;
  copy->http_status_code_ = http_status_code_;
  // ResetForCommit: post_data_
  copy->extra_headers_ = extra_headers_;
  copy->base_url_for_data_url_ = base_url_for_data_url_;
#if defined(OS_ANDROID)
  copy->data_url_as_string_ = data_url_as_string_;
#endif
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: should_replace_entry_
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  copy->has_user_gesture_ = has_user_gesture_;
  // ResetForCommit: reload_type_
  copy->CloneDataFrom(*this);
  copy->replaced_entry_data_ = replaced_entry_data_;
  copy->should_skip_on_back_forward_ui_ = should_skip_on_back_forward_ui_;
  copy->is_initial_entry_ = is_initial_entry_;

  return copy;
}

blink::mojom::CommonNavigationParamsPtr
NavigationEntryImpl::ConstructCommonNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    const GURL& dest_url,
    blink::mojom::ReferrerPtr dest_referrer,
    blink::mojom::NavigationType navigation_type,
    blink::PreviewsState previews_state,
    base::TimeTicks navigation_start,
    base::TimeTicks input_start) {
  blink::NavigationDownloadPolicy download_policy;
  if (IsViewSourceMode())
    download_policy.SetDisallowed(blink::NavigationDownloadType::kViewSource);
  // `base_url_for_data_url` is saved in NavigationEntry but should only be used
  // by main frames, because loadData* navigations can only happen on the main
  // frame.
  bool is_for_main_frame = (root_node()->frame_entry == &frame_entry);
  return blink::mojom::CommonNavigationParams::New(
      dest_url, frame_entry.initiator_origin(), std::move(dest_referrer),
      GetTransitionType(), navigation_type, download_policy,
      // It's okay to pass false for `should_replace_entry` because we never
      // replace an entry on session history / reload / restore navigation. New
      // navigation that may use replacement create their CommonNavigationParams
      // via NavigationRequest, for example, instead of via NavigationEntry.
      false /* should_replace_entry */,
      is_for_main_frame ? GetBaseURLForDataURL() : GURL(), previews_state,
      navigation_start, frame_entry.method(),
      post_body ? post_body : post_data_, network::mojom::SourceLocation::New(),
      has_started_from_context_menu(), has_user_gesture(),
      false /* has_text_fragment_token */,
      network::mojom::CSPDisposition::CHECK, std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */, input_start,
      network::mojom::RequestDestination::kEmpty);
}

blink::mojom::CommitNavigationParamsPtr
NavigationEntryImpl::ConstructCommitNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const GURL& original_url,
    const absl::optional<url::Origin>& origin_to_commit,
    const std::string& original_method,
    const base::flat_map<std::string, bool>& subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    const blink::FramePolicy& frame_policy) {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  // Note that this is actually does not work as intended right now because
  // we're only copying the redirect URLs into the new CommitNavigationParams,
  // keeping redirect_response and redirect_infos as empty.
  // TODO(https://crbug.com/1171225): Save redirect_response & redirect_infos in
  // FNE and copy them too?
  std::vector<GURL> redirects;
  if (ui::PageTransitionIsNewNavigation(GetTransitionType())) {
    redirects = frame_entry.redirect_chain();
  }

  int pending_offset_to_send = pending_history_list_offset;
  int current_offset_to_send = current_history_list_offset;
  int current_length_to_send = current_history_list_length;
  if (should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    pending_offset_to_send = -1;
    current_offset_to_send = -1;
    current_length_to_send = 0;
  }

  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::mojom::CommitNavigationParams::New(
          origin_to_commit,
          // The correct storage key will be computed before committing the
          // navigation.
          blink::StorageKey(), network::mojom::WebSandboxFlags(),
          GetIsOverridingUserAgent(), redirects,
          std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(), std::string(), original_url,
          original_method, GetCanLoadLocalResources(),
          frame_entry.page_state().ToEncodedData(), GetUniqueID(),
          subframe_unique_names, intended_as_new_entry, pending_offset_to_send,
          current_offset_to_send, current_length_to_send, false,
          IsViewSourceMode(), should_clear_history_list(),
          blink::mojom::NavigationTiming::New(),
          blink::mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create(),
          std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          std::string(),
#endif
          false /* is_browser_initiated */,
          GURL() /* web_bundle_physical_url */,
          GURL() /* base_url_override_for_web_bundle */,
          ukm::kInvalidSourceId /* document_ukm_source_id */, frame_policy,
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_agent_cluster */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_browsing_instance */, nullptr /* old_page_info */,
          -1 /* http_response_code */,
          std::vector<blink::mojom::
                          AppHistoryEntryPtr>() /* app_history_back_entries */,
          std::vector<
              blink::mojom::
                  AppHistoryEntryPtr>() /* app_history_forward_entries */,
          std::vector<GURL>() /* early_hints_preloaded_resources */,
          absl::nullopt /* ad_auction_components */,
          // This timestamp will be populated when the commit IPC is sent.
          base::TimeTicks() /* commit_sent */);
#if defined(OS_ANDROID)
  // `data_url_as_string` is saved in NavigationEntry but should only be used by
  // main frames, because loadData* navigations can only happen on the main
  // frame.
  bool is_for_main_frame = (root_node()->frame_entry == &frame_entry);
  if (is_for_main_frame &&
      NavigationControllerImpl::ValidateDataURLAsString(GetDataURLAsString())) {
    commit_params->data_url_as_string = GetDataURLAsString()->data();
  }
#endif
  return commit_params;
}

void NavigationEntryImpl::ResetForCommit(FrameNavigationEntry* frame_entry) {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest.
  SetPostData(nullptr);
  set_is_renderer_initiated(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(FrameTreeNode::kFrameTreeNodeInvalidId);
  set_reload_type(ReloadType::NONE);

  if (frame_entry) {
    frame_entry->set_source_site_instance(nullptr);
    frame_entry->set_blob_url_loader_factory(nullptr);
  }
}

NavigationEntryImpl::TreeNode* NavigationEntryImpl::GetTreeNode(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* node = nullptr;
  // TODO(https://crbug.com/1279628): Remove the BFS depth from the queue once
  // we don't need to debug the crash anymore.
  base::queue<std::pair<NavigationEntryImpl::TreeNode*, int>> work_queue;
  work_queue.push(std::make_pair(root_node(), 0));
  while (!work_queue.empty()) {
    node = work_queue.front().first;
    int depth = work_queue.front().second;
    work_queue.pop();
    if (!node) {
      SCOPED_CRASH_KEY_BOOL("NoNode", "ftn_is_main_frame",
                            frame_tree_node->IsMainFrame());
      SCOPED_CRASH_KEY_NUMBER("NoNode", "ftn_child_count",
                              frame_tree_node->child_count());
      SCOPED_CRASH_KEY_NUMBER("NoNode", "bfs_depth", depth);
      base::debug::DumpWithoutCrashing();
      continue;
    }
    if (node->MatchesFrame(frame_tree_node))
      return node;

    // Enqueue any children and keep looking.
    for (const auto& child : node->children)
      work_queue.push(std::make_pair(child.get(), depth + 1));
  }
  return nullptr;
}

void NavigationEntryImpl::AddOrUpdateFrameEntry(
    FrameTreeNode* frame_tree_node,
    UpdatePolicy update_policy,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    const std::string& app_history_key,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const absl::optional<url::Origin>& origin,
    const Referrer& referrer,
    const absl::optional<url::Origin>& initiator_origin,
    const std::vector<GURL>& redirect_chain,
    const blink::PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info,
    std::unique_ptr<SubresourceWebBundleNavigationInfo>
        subresource_web_bundle_navigation_info,
    std::unique_ptr<PolicyContainerPolicies> policy_container_policies) {
  // If this is called for the main frame, the FrameNavigationEntry is
  // guaranteed to exist, so just update it directly and return.
  if (frame_tree_node->IsMainFrame()) {
    // If the document of the FrameNavigationEntry is changing, we must clear
    // any child FrameNavigationEntries.
    if (root_node()->frame_entry->document_sequence_number() !=
        document_sequence_number)
      root_node()->children.clear();

    if (!url.is_empty()) {
      // A navigation committed on the main frame, so the NavigationEntry loses
      // its "initial NavigationEntry" status. Note that the initial entry
      // creation path also goes through this function, but we know not to
      // remove the status in that case because it uses the empty URL.
      // TODO(https://crbug.com/1215096): Consider to remove the initial entry
      // status after subframe navigations too, when the initial empty document
      // replacement behavior is more consistent, and we've determined that
      // exposing more history entries to WebView is OK.
      is_initial_entry_ = false;
    }

    root_node()->frame_entry->UpdateEntry(
        frame_tree_node->unique_name(), item_sequence_number,
        document_sequence_number, app_history_key, site_instance,
        std::move(source_site_instance), url, origin, referrer,
        initiator_origin, redirect_chain, page_state, method, post_id,
        std::move(blob_url_loader_factory),
        std::move(web_bundle_navigation_info),
        std::move(subresource_web_bundle_navigation_info),
        std::move(policy_container_policies));
    return;
  }

  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  NavigationEntryImpl::TreeNode* parent_node =
      GetTreeNode(FrameTreeNode::From(frame_tree_node->parent()));
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  const std::string& unique_name = frame_tree_node->unique_name();
  for (const auto& child : parent_node->children) {
    if (child->frame_entry->frame_unique_name() == unique_name) {
      if (update_policy == UpdatePolicy::kReplace) {
        RemoveEntryForFrame(frame_tree_node, false);
        break;
      }
      // If the document of the FrameNavigationEntry is changing, we must clear
      // any child FrameNavigationEntries.
      if (child->frame_entry->document_sequence_number() !=
          document_sequence_number)
        child->children.clear();

      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(
          unique_name, item_sequence_number, document_sequence_number,
          app_history_key, site_instance, std::move(source_site_instance), url,
          origin, referrer, initiator_origin, redirect_chain, page_state,
          method, post_id, std::move(blob_url_loader_factory),
          std::move(web_bundle_navigation_info),
          std::move(subresource_web_bundle_navigation_info),
          std::move(policy_container_policies));
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      unique_name, item_sequence_number, document_sequence_number,
      app_history_key, site_instance, std::move(source_site_instance), url,
      origin, referrer, initiator_origin, redirect_chain, page_state, method,
      post_id, std::move(blob_url_loader_factory),
      std::move(web_bundle_navigation_info),
      std::move(subresource_web_bundle_navigation_info),
      std::move(policy_container_policies));
  parent_node->children.push_back(
      std::make_unique<NavigationEntryImpl::TreeNode>(parent_node,
                                                      std::move(frame_entry)));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

base::flat_map<std::string, bool> NavigationEntryImpl::GetSubframeUniqueNames(
    FrameTreeNode* frame_tree_node) const {
  base::flat_map<std::string, bool> names;
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  if (tree_node) {
    // Return the names of all immediate children.
    for (const auto& child : tree_node->children) {
      // Keep track of whether we would be loading about:blank, since the
      // renderer should be allowed to just commit the initial blank frame if
      // that was the default URL.  PageState doesn't matter there, because
      // content injected into about:blank frames doesn't use it.
      //
      // Be careful not to rely on FrameNavigationEntry's URLs in this check,
      // because the committed URL in the browser could be rewritten to
      // about:blank.
      // See RenderProcessHostImpl::FilterURL to know which URLs are rewritten.
      // See https://crbug.com/657896 for details.
      bool is_about_blank = false;
      blink::ExplodedPageState exploded_page_state;
      if (blink::DecodePageState(
              child->frame_entry->page_state().ToEncodedData(),
              &exploded_page_state)) {
        blink::ExplodedFrameState frame_state = exploded_page_state.top;
        if (UTF16ToUTF8(frame_state.url_string.value_or(std::u16string())) ==
            url::kAboutBlankURL)
          is_about_blank = true;
      }

      names[child->frame_entry->frame_unique_name()] = is_about_blank;
    }
  }
  return names;
}

void NavigationEntryImpl::RemoveEntryForFrame(FrameTreeNode* frame_tree_node,
                                              bool only_if_different_position) {
  DCHECK(!frame_tree_node->IsMainFrame());

  NavigationEntryImpl::TreeNode* node = GetTreeNode(frame_tree_node);
  if (!node)
    return;

  // Remove the |node| from the tree if either 1) |only_if_different_position|
  // was not asked for or 2) if it is not in the same position in the tree of
  // FrameNavigationEntries and the FrameTree.
  if (!only_if_different_position ||
      !InSameTreePosition(frame_tree_node, node)) {
    auto* frame_entry = node->frame_entry.get();
    if (frame_entry && frame_entry->committed_origin()) {
      // Normally non-isolated origins are tracked through their presence in
      // session history, which is consulted whenever an origin newly requests
      // isolation. If we remove a frame_entry, its origin won't be available
      // to any future global walk if the same origin later wants to opt-in. So
      // we add it to the non-opt-in list here to be spec compliant (unless it's
      // currently opted-in, in which case this call will do nothing).
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->AddNonIsolatedOriginIfNeeded(
              frame_entry->site_instance()->GetIsolationContext(),
              frame_entry->committed_origin().value(),
              true /* global_ walk_or_frame_removal */);
    }
    NavigationEntryImpl::TreeNode* parent_node = node->parent;
    auto it = std::find_if(
        parent_node->children.begin(), parent_node->children.end(),
        [node](const std::unique_ptr<NavigationEntryImpl::TreeNode>& item) {
          return item.get() == node;
        });
    CHECK(it != parent_node->children.end());
    parent_node->children.erase(it);
  }
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

}  // namespace content
