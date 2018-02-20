// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_role_properties.h"
#include "build/build_config.h"

namespace ui {

namespace {
#if defined(OS_WIN)
static bool kExposeLayoutTableAsDataTable = true;
#else
static bool kExposeLayoutTableAsDataTable = false;
#endif
}  // namespace

bool IsRoleClickable(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton:
      return true;
    default:
      return false;
  }
}

bool IsDocument(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return true;
    default:
      return false;
  }
}

bool IsCellOrTableHeaderRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
      return true;
    case ax::mojom::Role::kLayoutTableCell:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsTableLikeRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTreeGrid:
      return true;
    case ax::mojom::Role::kLayoutTable:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsContainerWithSelectableChildrenRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
      return true;
    default:
      return false;
  }
}

bool IsRowContainer(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTable:
      return true;
    default:
      return false;
  }
}

bool IsControl(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kTree:
      return true;
    default:
      return false;
  }
}

bool IsMenuRelated(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
      return true;
    default:
      return false;
  }
}

bool IsImage(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kVideo:
      return true;
    default:
      return false;
  }
}

}  // namespace ui
