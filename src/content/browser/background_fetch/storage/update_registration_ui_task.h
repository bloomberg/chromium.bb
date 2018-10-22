// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace background_fetch {

// Updates Background Fetch UI options. Accepts a new title.
class UpdateRegistrationUITask : public DatabaseTask {
 public:
  using UpdateRegistrationUICallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;

  UpdateRegistrationUITask(DatabaseTaskHost* host,
                           const BackgroundFetchRegistrationId& registration_id,
                           const base::Optional<std::string>& title,
                           const base::Optional<SkBitmap>& icon,
                           UpdateRegistrationUICallback callback);

  ~UpdateRegistrationUITask() override;

  void Start() override;

 private:
  void DidGetUIOptions(const std::vector<std::string>& data,
                       blink::ServiceWorkerStatusCode status);

  void DidSerializeIcon(std::string serialized_icon);

  void StoreUIOptions();

  void DidUpdateUIOptions(blink::ServiceWorkerStatusCode status);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  std::string HistogramName() const override;

  BackgroundFetchRegistrationId registration_id_;
  base::Optional<std::string> title_;
  base::Optional<SkBitmap> icon_;

  proto::BackgroundFetchUIOptions ui_options_;

  UpdateRegistrationUICallback callback_;

  base::WeakPtrFactory<UpdateRegistrationUITask>
      weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(UpdateRegistrationUITask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_
