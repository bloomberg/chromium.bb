// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/open_tab_result.h"

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/omnibox_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

constexpr char kOpenTabScheme[] = "opentab://";

constexpr char16_t kUrlDelimiter[] = u" - ";

absl::optional<std::string> GetDriveId(const GURL& url) {
  if (url.host() != "docs.google.com")
    return absl::nullopt;

  std::string path = url.path();

  const std::string kPathPrefix = "/document/d/";
  if (!base::StartsWith(path, kPathPrefix))
    return absl::nullopt;

  std::string id = path.substr(kPathPrefix.size());
  int suffix = id.find_first_of('/');
  return id.substr(0, suffix);
}

}  // namespace

OpenTabResult::OpenTabResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             FaviconCache* favicon_cache,
                             const TokenizedString& query,
                             const AutocompleteMatch& match)
    : profile_(profile),
      list_controller_(list_controller),
      favicon_cache_(favicon_cache),
      match_(match),
      drive_id_(GetDriveId(match.destination_url)) {
  // TODO(crbug.com/1293702): This may not be unique. Once we have a mechanism
  // for opening a specific tab, add that info too to ensure uniqueness.
  set_id(kOpenTabScheme + match.destination_url.spec());

  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOpenTab);
  SetMetricsType(ash::OPEN_TAB);
  SetCategory(Category::kWeb);

  UpdateRelevance(query);
  UpdateText();
  UpdateIcon();
}

OpenTabResult::~OpenTabResult() = default;

void OpenTabResult::Open(int event_flags) {
  list_controller_->OpenURL(
      profile_, match_.destination_url, match_.transition,
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::SWITCH_TO_TAB));
}

absl::optional<std::string> OpenTabResult::DriveId() const {
  return drive_id_;
}

void OpenTabResult::UpdateRelevance(const TokenizedString& query) {
  TokenizedString title(match_.contents, TokenizedString::Mode::kCamelCase);
  TokenizedStringMatch match;
  if (!match.Calculate(query, title)) {
    // TODO(crbug.com/1293702): Low-scoring results are filtered out here
    // because the browser-side provider currently only does very basic
    // filtering.
    scoring().filter = true;
  }
  set_relevance(match.relevance());
}

void OpenTabResult::UpdateText() {
  // URL results from the Omnibox have the page title stored in
  // |match.description|.
  TextItem contents_text(TextType::kString);
  contents_text.SetText(match_.description);
  contents_text.SetTextTags({});

  TextItem delimiter_text(TextType::kString);
  delimiter_text.SetText(kUrlDelimiter);
  delimiter_text.SetTextTags({});

  std::u16string url = base::UTF8ToUTF16(match_.destination_url.spec());
  TextItem url_text(TextType::kString);
  url_text.SetText(url);
  url_text.SetTextTags({Tag(Tag::URL, 0, url.length())});

  SetTitleTextVector({std::move(contents_text), std::move(delimiter_text),
                      std::move(url_text)});
  SetDetails(l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT));
}

void OpenTabResult::UpdateIcon() {
  // Use the favicon if it is in cache.
  if (favicon_cache_) {
    const auto icon = favicon_cache_->GetFaviconForPageUrl(
        match_.destination_url, base::BindOnce(&OpenTabResult::OnFaviconFetched,
                                               weak_factory_.GetWeakPtr()));
    if (!icon.IsEmpty()) {
      SetIcon(IconInfo(icon.AsImageSkia(), kFaviconDimension));
      return;
    }
  }

  // Otherwise, fall back to using a generic icon.
  // TODO(crbug.com/1293702): WIP. Decide on the right generic icon here.
  SetIcon(
      IconInfo(gfx::CreateVectorIcon(omnibox::kSwitchIcon, kSystemIconDimension,
                                     GetGenericIconColor()),
               kSystemIconDimension));
}

void OpenTabResult::OnFaviconFetched(const gfx::Image& icon) {
  // By contract, this is never called with an empty |icon|.
  DCHECK(!icon.IsEmpty());
  SetIcon(IconInfo(icon.AsImageSkia(), kFaviconDimension));
}

}  // namespace app_list
