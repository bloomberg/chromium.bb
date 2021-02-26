// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_

#include <memory>

#include "ash/app_list/model/app_list_model_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/selection_model.h"

namespace ash {

class SearchBoxModelObserver;

// SearchBoxModel provides the user entered text, and the system state that
// influences the search box behavior.
class APP_LIST_MODEL_EXPORT SearchBoxModel {
 public:
  SearchBoxModel();
  ~SearchBoxModel();

  void SetTabletMode(bool is_tablet_mode);
  bool is_tablet_mode() const { return is_tablet_mode_; }

  void SetShowAssistantButton(bool show);
  bool show_assistant_button() const { return show_assistant_button_; }

  void SetSearchEngineIsGoogle(bool is_google);
  bool search_engine_is_google() const { return search_engine_is_google_; }

  // Sets/gets the text for the search box's Textfield and the voice search
  // flag.
  void Update(const base::string16& text,
              bool initiated_by_user);
  const base::string16& text() const { return text_; }

  void AddObserver(SearchBoxModelObserver* observer);
  void RemoveObserver(SearchBoxModelObserver* observer);

 private:
  base::string16 text_;
  bool search_engine_is_google_ = false;
  bool is_tablet_mode_ = false;
  bool show_assistant_button_ = false;

  base::ObserverList<SearchBoxModelObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxModel);
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_BOX_MODEL_H_
