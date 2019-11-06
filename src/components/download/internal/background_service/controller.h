// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/task/download_task_types.h"

namespace download {

struct DownloadParams;
struct SchedulingParams;

// The type of completion when the download entry transits to complete state.
// TODO(xingliu): Implement timeout and unknown failure types.
enum class CompletionType {
  // The download is successfully finished.
  SUCCEED = 0,
  // The download is interrupted and failed.
  FAIL = 1,
  // The download is aborted by the client.
  ABORT = 2,
  // The download is timed out and the connection is closed.
  TIMEOUT = 3,
  // The download is failed for unknown reasons.
  UNKNOWN = 4,
  // The download is cancelled by the client.
  CANCEL = 5,
  // The download expended it's number of expensive retries.
  OUT_OF_RETRIES = 6,
  // The download expended it's number of 'free' retries.
  OUT_OF_RESUMPTIONS = 7,
  // The upload was timed out due to unresponsive client.
  UPLOAD_TIMEOUT = 8,
  // The count of entries for the enum.
  COUNT = 9,
};

// The core Controller responsible for gluing various DownloadService components
// together to manage the active downloads.
class Controller {
 public:
  enum class State {
    // The Controller has been created but has not been initialized yet.  It
    // cannot be used.
    CREATED = 1,

    // The Controller has been created and Initialize() has been called but has
    // not yet finished.  It cannot be used.
    INITIALIZING = 2,

    // The Controller has been created and initialized.  It can be used.
    READY = 3,

    // The Controller failed to initialize and is in the process of recovering.
    // It cannot be used.
    RECOVERING = 4,

    // The Controller was unable to recover and is unusable this session.
    UNAVAILABLE = 5,
  };

  Controller() = default;
  virtual ~Controller() = default;

  // Initializes the controller. Initialization may be asynchronous.
  virtual void Initialize(const base::Closure& callback) = 0;

  // Returns the status of Controller.
  virtual State GetState() = 0;

  // Starts a download with |params|.  See DownloadParams::StartCallback and
  // DownloadParams::StartResponse for information on how a caller can determine
  // whether or not the download was successfully accepted and queued.
  virtual void StartDownload(const DownloadParams& params) = 0;

  // Pauses a download request associated with |guid| if one exists.
  virtual void PauseDownload(const std::string& guid) = 0;

  // Resumes a download request associated with |guid| if one exists.  The
  // download request may or may not start downloading at this time, but it will
  // no longer be blocked by any prior PauseDownload() actions.
  virtual void ResumeDownload(const std::string& guid) = 0;

  // Cancels a download request associated with |guid| if one exists.
  virtual void CancelDownload(const std::string& guid) = 0;

  // Changes the SchedulingParams of a download request associated with |guid|
  // to |params|.
  virtual void ChangeDownloadCriteria(const std::string& guid,
                                      const SchedulingParams& params) = 0;

  // Exposes the owner of the download request for |guid| if one exists.
  // Otherwise returns DownloadClient::INVALID for an unowned entry.
  virtual DownloadClient GetOwnerOfDownload(const std::string& guid) = 0;

  // See DownloadService::OnStartScheduledTask.
  virtual void OnStartScheduledTask(DownloadTaskType task_type,
                                    TaskFinishedCallback callback) = 0;

  // See DownloadService::OnStopScheduledTask.
  virtual bool OnStopScheduledTask(DownloadTaskType task_type) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_H_
