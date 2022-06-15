// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/user_notes/interfaces/user_note_service_delegate.h"
#include "components/user_notes/interfaces/user_notes_ui_delegate.h"
#include "components/user_notes/model/user_note.h"

class UserNoteUICoordinatorTest;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace user_notes {

class UserNoteManager;

// Keyed service coordinating the different parts (Renderer, UI layer, storage
// layer) of the User Notes feature for the current user profile.
class UserNoteService : public KeyedService, public UserNotesUIDelegate {
 public:
  explicit UserNoteService(std::unique_ptr<UserNoteServiceDelegate> delegate);
  ~UserNoteService() override;
  UserNoteService(const UserNoteService&) = delete;
  UserNoteService& operator=(const UserNoteService&) = delete;

  base::SafeRef<UserNoteService> GetSafeRef() const;

  // Returns a pointer to the note model associated with the given ID, or
  // `nullptr` if none exists.
  const UserNote* GetNoteModel(const base::UnguessableToken& id) const;

  // Returns true if the provided ID is found in the creation map, indicating
  // the note is still in progress, and false otherwise.
  bool IsNoteInProgress(const base::UnguessableToken& id) const;

  // Called by the embedder when a frame navigates to a new URL. Queries the
  // storage to find notes associated with that URL, and if there are any, kicks
  // off the logic to display them in the page.
  virtual void OnFrameNavigated(content::RenderFrameHost* rfh);

  // Called by `UserNoteManager` objects when a `UserNoteInstance` is added to
  // the page they're attached to. Updates the model map to add a ref to the
  // given `UserNoteManager` for the note with the specified ID.
  void OnNoteInstanceAddedToPage(const base::UnguessableToken& id,
                                 UserNoteManager* manager);

  // Same as `OnNoteInstanceAddedToPage`, except for when a note is removed from
  // a page. Updates the model map to remove the ref to the given
  // `UserNoteManager`. If this is the last page where the note was displayed,
  // also deletes the model from the model map.
  void OnNoteInstanceRemovedFromPage(const base::UnguessableToken& id,
                                     UserNoteManager* manager);

  // Called by a note manager when the user selects "Add a note" from the
  // associated page's context menu. Kicks off the note creation process.
  // TODO(gujen) and TODO(bokan): Make the renderer notify the manager and the
  // manager call this method. Also consider pre-creating the agent on the
  // renderer side so it is immediately ready when the browser side receives the
  // request.
  void OnAddNoteRequested(content::RenderFrameHost* frame,
                          std::string original_text,
                          std::string selector,
                          gfx::Rect rect);

  // UserNotesUIDelegate implementation.
  void OnNoteFocused(const base::UnguessableToken& id) override;
  void OnNoteDeleted(const base::UnguessableToken& id) override;
  void OnNoteCreationDone(const base::UnguessableToken& id,
                          const std::string& note_content) override;
  void OnNoteCreationCancelled(const base::UnguessableToken& id) override;
  void OnNoteUpdated(const base::UnguessableToken& id,
                     const std::string& note_content) override;

 private:
  struct ModelMapEntry {
    explicit ModelMapEntry(std::unique_ptr<UserNote> m);
    ~ModelMapEntry();
    ModelMapEntry(ModelMapEntry&& other);
    ModelMapEntry(const ModelMapEntry&) = delete;
    ModelMapEntry& operator=(const ModelMapEntry&) = delete;

    std::unique_ptr<UserNote> model;
    std::unordered_set<UserNoteManager*> managers;
  };

  friend class UserNoteBaseTest;
  friend class UserNoteInstanceTest;
  friend class UserNoteUtilsTest;
  friend class ::UserNoteUICoordinatorTest;

  // Source of truth for the in-memory note models. Any note currently being
  // displayed in a tab is stored in this data structure. Each entry also
  // contains a set of pointers to all `UserNoteManager` objects holding an
  // instance of that note, which is necessary to clean up the models when
  // they're no longer in use and to remove notes from affected web pages when
  // they're deleted by the user.
  std::unordered_map<base::UnguessableToken,
                     ModelMapEntry,
                     base::UnguessableTokenHash>
      model_map_;

  // A map to store in-progress notes during the note creation process. This is
  // needed in order to separate incomplete notes from the rest of the real
  // note models. Completed notes are eventually moved to the model map.
  std::unordered_map<base::UnguessableToken,
                     ModelMapEntry,
                     base::UnguessableTokenHash>
      creation_map_;

  std::unique_ptr<UserNoteServiceDelegate> delegate_;
  base::WeakPtrFactory<UserNoteService> weak_ptr_factory_{this};
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
