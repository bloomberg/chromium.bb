// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace ash {

// Implementation of the holding space browser client.
class HoldingSpaceClientImpl : public HoldingSpaceClient {
 public:
  explicit HoldingSpaceClientImpl(Profile* profile);
  HoldingSpaceClientImpl(const HoldingSpaceClientImpl& other) = delete;
  HoldingSpaceClientImpl& operator=(const HoldingSpaceClientImpl& other) =
      delete;
  ~HoldingSpaceClientImpl() override;

  // HoldingSpaceClient:
  void AddDiagnosticsLog(const base::FilePath& file_path) override;
  void AddScreenRecording(const base::FilePath& file_path) override;
  void AddScreenshot(const base::FilePath& file_path) override;
  void CancelItems(const std::vector<const HoldingSpaceItem*>& items) override;
  void CopyImageToClipboard(const HoldingSpaceItem&, SuccessCallback) override;
  base::FilePath CrackFileSystemUrl(const GURL& file_system_url) const override;
  void OpenDownloads(SuccessCallback callback) override;
  void OpenItems(const std::vector<const HoldingSpaceItem*>& items,
                 SuccessCallback callback) override;
  void OpenMyFiles(SuccessCallback callback) override;
  void PauseItems(const std::vector<const HoldingSpaceItem*>& items) override;
  void PinFiles(const std::vector<base::FilePath>& file_paths) override;
  void PinItems(const std::vector<const HoldingSpaceItem*>& items) override;
  void ResumeItems(const std::vector<const HoldingSpaceItem*>& items) override;
  void ShowItemInFolder(const HoldingSpaceItem&, SuccessCallback) override;
  void UnpinItems(const std::vector<const HoldingSpaceItem*>& items) override;

 private:
  Profile* const profile_;

  base::WeakPtrFactory<HoldingSpaceClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
