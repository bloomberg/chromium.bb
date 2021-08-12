// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"

class Profile;

namespace sharesheet {

// The SharesheetController allows ShareActions to request changes to the state
// of the sharesheet.
class SharesheetController {
 public:
  virtual ~SharesheetController() = default;

  virtual Profile* GetProfile() = 0;

  // When called will set the bubble size to |width| and |height|.
  // |width| and |height| must be set to a positive int.
  virtual void SetSharesheetSize(int width, int height) = 0;

  // Called by ShareAction to notify SharesheetBubbleView via result that
  // ShareAction has closed with |result| (i.e. success, cancel, or error).
  virtual void CloseSharesheet(SharesheetResult result) = 0;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
