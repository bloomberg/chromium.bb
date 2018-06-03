// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ui {

class ACCELERATED_WIDGET_MAC_EXPORT CATransactionCoordinator {
 public:
  class PreCommitObserver {
   public:
    virtual bool ShouldWaitInPreCommit() = 0;
    virtual base::TimeDelta PreCommitTimeout() = 0;
  };

  class PostCommitObserver {
   public:
    virtual void OnActivateForTransaction() = 0;
    virtual void OnEnterPostCommit() = 0;
    virtual bool ShouldWaitInPostCommit() = 0;
  };

  static CATransactionCoordinator& Get();

  void Synchronize();

  void AddPreCommitObserver(PreCommitObserver*);
  void RemovePreCommitObserver(PreCommitObserver*);

  void AddPostCommitObserver(PostCommitObserver*);
  void RemovePostCommitObserver(PostCommitObserver*);

 private:
  friend class base::NoDestructor<CATransactionCoordinator>;
  CATransactionCoordinator();
  ~CATransactionCoordinator();

  bool active_ = false;
  base::ObserverList<PreCommitObserver> pre_commit_observers_;
  base::ObserverList<PostCommitObserver> post_commit_observers_;

  DISALLOW_COPY_AND_ASSIGN(CATransactionCoordinator);
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
