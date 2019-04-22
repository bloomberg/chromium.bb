// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {

BatchElementChecker::BatchElementChecker() : weak_ptr_factory_(this) {}

BatchElementChecker::~BatchElementChecker() {}

void BatchElementChecker::AddElementCheck(const Selector& selector,
                                          ElementCheckCallback callback) {
  DCHECK(!started_);

  element_check_callbacks_[selector].emplace_back(std::move(callback));
}

void BatchElementChecker::AddFieldValueCheck(const Selector& selector,
                                             GetFieldValueCallback callback) {
  DCHECK(!started_);

  get_field_value_callbacks_[selector].emplace_back(std::move(callback));
}

bool BatchElementChecker::empty() const {
  return element_check_callbacks_.empty() && get_field_value_callbacks_.empty();
}

void BatchElementChecker::Run(WebController* web_controller,
                              base::OnceCallback<void()> all_done) {
  DCHECK(web_controller);
  DCHECK(!started_);
  started_ = true;

  all_done_ = std::move(all_done);
  pending_checks_count_ =
      element_check_callbacks_.size() + get_field_value_callbacks_.size() + 1;

  for (auto& entry : element_check_callbacks_) {
    web_controller->ElementCheck(
        entry.first, /* strict= */ false,
        base::BindOnce(
            &BatchElementChecker::OnElementChecked,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
  }

  for (auto& entry : get_field_value_callbacks_) {
    web_controller->GetFieldValue(
        entry.first,
        base::BindOnce(
            &BatchElementChecker::OnGetFieldValue,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
  }

  // The extra +1 of pending_check_count and this check happening last
  // guarantees that all_done cannot be called before the end of this function.
  // Without this, callbacks could be called synchronously by the web
  // controller, the call all_done, which could delete this instance and all its
  // datastructures while the function is still going through them.
  //
  // TODO(crbug.com/806868): make sure 'all_done' callback is called
  // asynchronously and fix unit tests accordingly.
  CheckDone();
}

void BatchElementChecker::OnElementChecked(
    std::vector<ElementCheckCallback>* callbacks,
    bool exists) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(exists);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::OnGetFieldValue(
    std::vector<GetFieldValueCallback>* callbacks,
    bool exists,
    const std::string& value) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(exists, value);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::CheckDone() {
  pending_checks_count_--;
  DCHECK_GE(pending_checks_count_, 0);
  if (pending_checks_count_ <= 0) {
    DCHECK(all_done_);
    std::move(all_done_).Run();
    // Don't do anything after calling all_done, since this could have been
    // deleted.
  }
}

}  // namespace autofill_assistant
