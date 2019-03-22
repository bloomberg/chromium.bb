// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_STYLE_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_STYLE_H_

// Defines the style of a collection view cell. Individual cells may choose to
// expose and respect this setting.
// TODO(crbug.com/894800): Remove this.
enum class CollectionViewCellStyle {
  // A cell style that conforms to Material Design guidelines.
  kMaterial = 0,

  // A cell style that conforms to stock UIKit guidelines.
  kUIKit,
};

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_STYLE_H_
