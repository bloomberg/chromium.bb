// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_RESULT_H_
#define UI_APP_LIST_SEARCH_RESULT_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace ui {
class MenuModel;
}

namespace app_list {

class SearchResultObserver;
class TokenizedString;
class TokenizedStringMatch;

// SearchResult consists of an icon, title text and details text. Title and
// details text can have tagged ranges that are displayed differently from
// default style.
class APP_LIST_EXPORT SearchResult {
 public:
  // How the result should be displayed. Do not change the order of these as
  // they are used for metrics.
  enum DisplayType {
    DISPLAY_NONE = 0,
    DISPLAY_LIST,
    DISPLAY_TILE,
    DISPLAY_RECOMMENDATION,
    // Add new values here.

    DISPLAY_TYPE_LAST,
  };

  // A tagged range in search result text.
  struct APP_LIST_EXPORT Tag {
    // Similar to ACMatchClassification::Style, the style values are not
    // mutually exclusive.
    enum Style {
      NONE  = 0,
      URL   = 1 << 0,
      MATCH = 1 << 1,
      DIM   = 1 << 2,
    };

    Tag(int styles, size_t start, size_t end)
        : styles(styles),
          range(static_cast<uint32_t>(start), static_cast<uint32_t>(end)) {}

    int styles;
    gfx::Range range;
  };
  typedef std::vector<Tag> Tags;

  // Data representing an action that can be performed on this search result.
  // An action could be represented as an icon set or as a blue button with
  // a label. Icon set is chosen if label text is empty. Otherwise, a blue
  // button with the label text will be used.
  struct APP_LIST_EXPORT Action {
    Action(const gfx::ImageSkia& base_image,
           const gfx::ImageSkia& hover_image,
           const gfx::ImageSkia& pressed_image,
           const base::string16& tooltip_text);
    Action(const base::string16& label_text,
           const base::string16& tooltip_text);
    ~Action();

    gfx::ImageSkia base_image;
    gfx::ImageSkia hover_image;
    gfx::ImageSkia pressed_image;

    base::string16 tooltip_text;
    base::string16 label_text;
  };
  typedef std::vector<Action> Actions;

  SearchResult();
  virtual ~SearchResult();

  const gfx::ImageSkia& icon() const { return icon_; }
  void SetIcon(const gfx::ImageSkia& icon);

  const gfx::ImageSkia& badge_icon() const { return badge_icon_; }
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);

  const base::string16& title() const { return title_; }
  void set_title(const base::string16& title) { title_ = title; }

  const Tags& title_tags() const { return title_tags_; }
  void set_title_tags(const Tags& tags) { title_tags_ = tags; }

  const base::string16& details() const { return details_; }
  void set_details(const base::string16& details) { details_ = details; }

  const Tags& details_tags() const { return details_tags_; }
  void set_details_tags(const Tags& tags) { details_tags_ = tags; }

  const std::string& id() const { return id_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  DisplayType display_type() const { return display_type_; }
  void set_display_type(DisplayType display_type) {
    display_type_ = display_type;
  }

  int distance_from_origin() { return distance_from_origin_; }
  void set_distance_from_origin(int distance) {
    distance_from_origin_ = distance;
  }

  const Actions& actions() const {
    return actions_;
  }
  void SetActions(const Actions& sets);

  // Whether the result can be automatically selected by a voice query.
  // (Non-voice results can still appear in the results list to be manually
  // selected.)
  bool voice_result() const { return voice_result_; }

  bool is_installing() const { return is_installing_; }
  void SetIsInstalling(bool is_installing);

  int percent_downloaded() const { return percent_downloaded_; }
  void SetPercentDownloaded(int percent_downloaded);

  // Returns the dimension at which this result's icon should be displayed.
  int GetPreferredIconDimension() const;

  void NotifyItemInstalled();

  void AddObserver(SearchResultObserver* observer);
  void RemoveObserver(SearchResultObserver* observer);

  // Updates the result's relevance score, and sets its title and title tags,
  // based on a string match result.
  void UpdateFromMatch(const TokenizedString& title,
                       const TokenizedStringMatch& match);

  // TODO(mukai): Remove this method and really simplify the ownership of
  // SearchResult. Ideally, SearchResult will be copyable.
  virtual scoped_ptr<SearchResult> Duplicate() const = 0;

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install).
  // Note the returned menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel();

  // Returns a string showing |text| marked up with brackets indicating the
  // tag positions in |tags|. Useful for debugging and testing.
  static std::string TagsDebugString(const std::string& text, const Tags& tags);

 protected:
  void set_id(const std::string& id) { id_ = id; }
  void set_voice_result(bool voice_result) { voice_result_ = voice_result; }

 private:
  friend class SearchController;

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags);

  gfx::ImageSkia icon_;
  gfx::ImageSkia badge_icon_;

  base::string16 title_;
  Tags title_tags_;

  base::string16 details_;
  Tags details_tags_;

  std::string id_;
  double relevance_;
  DisplayType display_type_;

  // The Manhattan distance from the origin of all search results to this
  // result. This is logged for UMA.
  int distance_from_origin_;

  Actions actions_;
  bool voice_result_;

  bool is_installing_;
  int percent_downloaded_;

  base::ObserverList<SearchResultObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchResult);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_RESULT_H_
