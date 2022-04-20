// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_

#include "base/memory/safe_ref.h"
#include "components/user_notes/model/user_note.h"

namespace user_notes {

// A class that represents the manifestation of a note within a specific web
// page.
class UserNoteInstance {
 public:
  explicit UserNoteInstance(base::SafeRef<UserNote> model);
  ~UserNoteInstance();
  UserNoteInstance(const UserNoteInstance&) = delete;
  UserNoteInstance& operator=(const UserNoteInstance&) = delete;

  const UserNote& model() { return *model_; }

 private:
  // A ref to the backing model of this note instance. The model is owned by
  // |UserNoteService|. The model is expected to outlive this class.
  base::SafeRef<UserNote> model_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
