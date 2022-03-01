// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_
#define CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"

namespace ash {
namespace shimless_rma {

class ChromeShimlessRmaDelegate : public ShimlessRmaDelegate {
 public:
  ChromeShimlessRmaDelegate();

  ChromeShimlessRmaDelegate(const ChromeShimlessRmaDelegate&) = delete;
  ChromeShimlessRmaDelegate& operator=(const ChromeShimlessRmaDelegate&) =
      delete;

  ~ChromeShimlessRmaDelegate() override;

  // ShimlessRmaDelegate:
  void RestartChrome() override;
  void ShowDiagnosticsDialog() override;
};

}  // namespace shimless_rma
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_
