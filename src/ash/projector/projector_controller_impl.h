// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
#define ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/components/audio/cras_audio_handler.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class ProjectorClient;
class ProjectorUiController;
class ProjectorMetadataController;

// A controller to handle projector functionalities.
class ASH_EXPORT ProjectorControllerImpl
    : public ProjectorController,
      public ProjectorSessionObserver,
      public CrasAudioHandler::AudioObserver {
 public:
  // Callback that should be executed when the screencast container directory is
  // created. `screencast_file_path_no_extension` is the path of screencast file
  // without extension. `screencast_file_path_no_extension` will be empty if
  // fail in creating the directory. The path will be used for generating the
  // screencast media file by appending the media file extension.
  using CreateScreencastContainerFolderCallback = base::OnceCallback<void(
      const base::FilePath& screencast_file_path_no_extension)>;

  ProjectorControllerImpl();
  ProjectorControllerImpl(const ProjectorControllerImpl&) = delete;
  ProjectorControllerImpl& operator=(const ProjectorControllerImpl&) = delete;
  ~ProjectorControllerImpl() override;

  static ProjectorControllerImpl* Get();

  // ProjectorController:
  void StartProjectorSession(const std::string& storage_dir) override;
  void SetClient(ProjectorClient* client) override;
  void OnSpeechRecognitionAvailabilityChanged(
      SpeechRecognitionAvailability availability) override;
  void OnTranscription(const media::SpeechRecognitionResult& result) override;
  void OnTranscriptionError() override;
  void OnSpeechRecognitionStopped() override;
  bool IsEligible() const override;
  NewScreencastPrecondition GetNewScreencastPrecondition() const override;
  void OnToolSet(const AnnotatorTool& tool) override;
  void OnUndoRedoAvailabilityChanged(bool undo_available,
                                     bool redo_available) override;

  // Create the screencast container directory. If there is an error, the
  // callback will be triggered with an empty FilePath.
  //
  // For now, Projector Screencasts are all uploaded to Drive. This method will
  // create the folder in DriveFS mounted path. Files saved in this path will
  // then be synced to Drive by DriveFS. DriveFS only supports primary account.
  void CreateScreencastContainerFolder(
      CreateScreencastContainerFolderCallback callback);

  // Called by Capture Mode to notify with the state of a video recording.
  // `is_in_projector_mode` indicates whether it's a projector-initiated video
  // recording.
  void OnRecordingStarted(bool is_in_projector_mode);
  void OnRecordingEnded(bool is_in_projector_mode);

  // Called by Capture Mode to notify us that a Projector-initiated recording
  // session was aborted (i.e. recording was never started) due to e.g. user
  // cancellation, an error, or a DLP/HDCP restriction.
  void OnRecordingStartAborted();

  // Invoked when laser pointer button is pressed.
  void OnLaserPointerPressed();
  // Invoked when marker button is pressed.
  void OnMarkerPressed();
  // Reset and disable the laser pointer and the annotator tools.
  void ResetTools();
  // Returns true if laser pointer is active.
  bool IsLaserPointerEnabled();
  // Returns true if annotator is active.
  bool IsAnnotatorEnabled();

  // Notifies the ProjectorClient if the Projector SWA can trigger a
  // new Projector session. The preconditions are calculated in
  // ProjectorControllerImpl::CanStartNewSession. The following are
  // preconditions that are checked:
  // 1. On device speech recognition availability changes.
  // 2. Screen recording state changed( whether an active recording is already
  // taking place or not).
  // 3. Whether DriveFS is mounted or not.
  void OnNewScreencastPreconditionChanged();

  void SetProjectorUiControllerForTest(
      std::unique_ptr<ProjectorUiController> ui_controller);
  void SetProjectorMetadataControllerForTest(
      std::unique_ptr<ProjectorMetadataController> metadata_controller);

  ProjectorUiController* ui_controller() { return ui_controller_.get(); }
  ProjectorSessionImpl* projector_session() { return projector_session_.get(); }

  // CrasAudioHandler::AudioObserver:
  void OnAudioNodesChanged() override;

 private:
  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  bool IsInputDeviceAvailable() const;

  // Starts or stops the speech recognition session.
  void StartSpeechRecognition();
  void MaybeStopSpeechRecognition();

  // Triggered when finish creating the screencast container folder. This method
  // caches the the container folder path in `ProjectorSession` and triggers the
  // `CreateScreencastContainerFolderCallback' with the screencast file path
  // without file extension. This path will be used by screen capture to save
  // screencast media file after appending the media file extension.
  void OnContainerFolderCreated(
      const base::FilePath& path,
      CreateScreencastContainerFolderCallback callback,
      bool success);

  // Saves the screencast including metadata.
  void SaveScreencast();

  // Get the screencast file path without file extension. This will be used
  // to construct media and metadata file path.
  base::FilePath GetScreencastFilePathNoExtension() const;

  ProjectorClient* client_ = nullptr;
  std::unique_ptr<ProjectorSessionImpl> projector_session_;
  std::unique_ptr<ProjectorUiController> ui_controller_;
  std::unique_ptr<ProjectorMetadataController> metadata_controller_;

  // Whether SODA is available on the device.
  SpeechRecognitionAvailability speech_recognition_availability_ =
      SpeechRecognitionAvailability::kOnDeviceSpeechRecognitionNotSupported;

  // Whether speech recognition is taking place or not.
  bool is_speech_recognition_on_ = false;

  base::WeakPtrFactory<ProjectorControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
