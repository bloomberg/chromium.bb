// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kVerticalPadding = 16;

// Compares two UserNoteInstances by their rect's origin, which represents their
// position in a web page. If the UserNoteInstances have the same position,
// compare them by their modification date.
bool UserNoteComparator(const user_notes::UserNoteInstance* first,
                        user_notes::UserNoteInstance* second) {
  if (first->rect() == second->rect()) {
    return first->model().metadata().modification_date() <
           second->model().metadata().modification_date();
  }
  return first->rect() < second->rect();
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(UserNoteUICoordinator,
                                      kScrollViewElementIdForTesting);

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : BrowserUserData<UserNoteUICoordinator>(*browser), browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
}

UserNoteUICoordinator::~UserNoteUICoordinator() = default;

void UserNoteUICoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kUserNote,
      l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
      ui::ImageModel::FromVectorIcon(kInkHighlighterIcon, ui::kColorIcon),
      base::BindRepeating(&UserNoteUICoordinator::CreateUserNotesView,
                          base::Unretained(this))));
}

void UserNoteUICoordinator::OnNoteDeleted(const base::UnguessableToken& id,
                                          UserNoteView* user_note_view) {
  scroll_view_->contents()->RemoveChildView(user_note_view);
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteDeleted(id);
}

void UserNoteUICoordinator::OnNoteCreationDone(
    const base::UnguessableToken& id,
    const std::string& note_content) {
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteCreationDone(id, note_content);
}

void UserNoteUICoordinator::OnNoteCreationCancelled(
    const base::UnguessableToken& id,
    UserNoteView* user_note_view) {
  scroll_view_->contents()->RemoveChildView(user_note_view);
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteCreationCancelled(id);
}

void UserNoteUICoordinator::OnNoteUpdated(const base::UnguessableToken& id,
                                          const std::string& note_content) {
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteUpdated(id, note_content);
}

void UserNoteUICoordinator::FocusNote(const std::string& guid) {
  // TODO(cheickcisse): Implement FocusNote, which will be called by
  // UserNoteService to inform, inform the user note side panel to scroll the
  // corresponding note into view in the side panel.
}

void UserNoteUICoordinator::StartNoteCreation(
    user_notes::UserNoteInstance* instance) {
  scoped_view_observer_.Observe(scroll_view_);

  auto* scroll_contents_view = scroll_view_->contents();
  scroll_to_note_id_ = instance->model().id();

  int index = 0;
  for (views::View* child_view : scroll_contents_view->children()) {
    UserNoteView* user_note_view = static_cast<UserNoteView*>(child_view);
    if (user_note_view->user_note_rect() < instance->rect()) {
      index++;
      continue;
    }
    break;
  }

  scroll_contents_view->AddChildViewAt(
      std::make_unique<UserNoteView>(this, instance,
                                     UserNoteView::State::kCreating),
      index);
}

void UserNoteUICoordinator::OnViewBoundsChanged(views::View* observed_view) {
  // Scrolling to note can only be done after the view is drawn
  // (bounds has changed), otherwise we cannot get the bounds of each view.
  // After the view is drawn, we don't need to observe it anymore.
  scoped_view_observer_.Reset();
  ScrollToNote();
}

void UserNoteUICoordinator::ScrollToNote() {
  if (scroll_to_note_id_ == base::UnguessableToken::Null())
    return;

  for (views::View* child_content_view : scroll_view_->contents()->children()) {
    UserNoteView* user_note_view =
        static_cast<UserNoteView*>(child_content_view);
    if (user_note_view->UserNoteId() == scroll_to_note_id_) {
      child_content_view->ScrollViewToVisible();
      break;
    }
  }

  scroll_to_note_id_ = base::UnguessableToken::Null();
}

void UserNoteUICoordinator::Invalidate() {
  if (!scroll_view_)
    return;

  if (!browser_->tab_strip_model()->GetActiveWebContents()) {
    if (scroll_view_->contents() &&
        scroll_view_->contents()->children().size() > 0) {
      scroll_view_->contents()->RemoveAllChildViews();
    }
    return;
  }

  auto* user_notes_manager = user_notes::UserNoteManager::GetForPage(
      browser_->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage());

  std::vector<user_notes::UserNoteInstance*> user_note_instances =
      user_notes_manager ? user_notes_manager->GetAllNoteInstances()
                         : std::vector<user_notes::UserNoteInstance*>();
  std::sort(user_note_instances.begin(), user_note_instances.end(),
            UserNoteComparator);

  uint32_t instances_index = 0;
  uint32_t views_index = 0;
  auto* scroll_contents_view = scroll_view_->contents();

  while (instances_index < user_note_instances.size() ||
         views_index < scroll_contents_view->children().size()) {
    // If we've reached the end of the UserNoteInstance vector but not the end
    // of the scroll_contents_view children's vector, we should remove the
    // remaining child views from the scroll_contents_view.
    if (instances_index >= user_note_instances.size()) {
      views::View* user_note_view =
          scroll_contents_view->children().at(views_index);
      scroll_contents_view->RemoveChildView(user_note_view);
      continue;
    }

    user_notes::UserNoteInstance* const user_note_instance =
        user_note_instances.at(instances_index);
    DCHECK(user_note_instance);

    // If we've reached the end of the scroll_contents_view child's vector but
    // not the end of the UserNoteInstance vector, we should create new
    // UserNoteViews from the remaining notes in the UserNoteInstance
    // vector.
    if (views_index >= scroll_contents_view->children().size()) {
      scroll_contents_view->AddChildViewAt(
          std::make_unique<UserNoteView>(this, user_note_instance,
                                         UserNoteView::State::kDefault),
          views_index);
      instances_index++;
      views_index++;
      continue;
    }

    UserNoteView* user_note_view = static_cast<UserNoteView*>(
        scroll_contents_view->children().at(views_index));

    if (user_note_view->UserNoteId() == base::UnguessableToken::Null()) {
      // Remove the current UserNoteView from scroll_contents_view if its Id is
      // null.
      scroll_contents_view->RemoveChildView(user_note_view);
      continue;
    }

    if (user_note_view->UserNoteId() == user_note_instance->model().id()) {
      instances_index++;
      views_index++;
    } else if (user_note_view->user_note_rect() < user_note_instance->rect()) {
      // Remove the current UserNoteView because the note is no longer available
      // in the UserNoteInstance vector.
      scroll_contents_view->RemoveChildView(user_note_view);
    } else {
      // Add a new UserNoteView because the current UserNoteInstance note is
      // missing from scroll_contents_view's children.
      scroll_contents_view->AddChildViewAt(
          std::make_unique<UserNoteView>(this, user_note_instance,
                                         UserNoteView::State::kDefault),
          views_index);
      instances_index++;
      views_index++;
    }
  }
}

void UserNoteUICoordinator::Show() {
  auto* side_panel_coordinator =
      BrowserView::GetBrowserViewForBrowser(browser_)->side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kUserNote);
}

void UserNoteUICoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // TODO(cheickcisse): Call invalidate on active tab changed. It seems to
  // fail when the tab is deleted.
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesView() {
  // Layout structure:
  //
  // [| [NoteView]              | <--- scroll content view ] <--- scroll view
  // [| ...                     |]
  // [| ...                     |]

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = root_view->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetProperty(views::kElementIdentifierKey,
                            kScrollViewElementIdForTesting);
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroll_view_->ClipHeightTo(0, 0);

  // TODO(cheickcisse): Populate scroll content view.
  views::View* scroll_contents_view =
      scroll_view_->SetContents(std::make_unique<views::View>());

  constexpr int edge_margin = 16;
  auto* layout =
      scroll_contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(kVerticalPadding, edge_margin), kVerticalPadding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  Invalidate();
  return root_view;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNoteUICoordinator);
