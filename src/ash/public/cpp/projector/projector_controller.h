// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace ash {

struct AnnotatorTool;

// File extension of Projector metadata file. It is used to identify Projector
// screencasts at processing pending screencasts and fetching screencast list.
constexpr char kProjectorMetadataFileExtension[] = "projector";

class ProjectorClient;

// Enum class used to notify the ProjectorController on the availability of
// speech recognition.
enum class ASH_PUBLIC_EXPORT SpeechRecognitionAvailability {
  // Device does not support SODA (Speech on Device API)
  kOnDeviceSpeechRecognitionNotSupported,
  // User's language is not supported by SODA.
  kUserLanguageNotSupported,
  // SODA binary is not yet installed.
  kSodaNotInstalled,
  // SODA binary and language packs are downloading.
  kSodaInstalling,
  // SODA is available to be used.
  kAvailable
};

// Interface to control projector in ash.
class ASH_PUBLIC_EXPORT ProjectorController {
 public:
  ProjectorController();
  ProjectorController(const ProjectorController&) = delete;
  ProjectorController& operator=(const ProjectorController&) = delete;
  virtual ~ProjectorController();

  static ProjectorController* Get();

  // Returns whether the extended features for projector are enabled. This is
  // decided based on a command line switch.
  static bool AreExtendedProjectorFeaturesDisabled();

  // Starts a capture mode session for the projector workflow if no video
  // recording is currently in progress. `storage_dir` is the container
  // directory name for screencasts and will be used to create the storage path.
  virtual void StartProjectorSession(const std::string& storage_dir) = 0;

  // Make sure the client is set before attempting to use to the
  // ProjectorController.
  virtual void SetClient(ProjectorClient* client) = 0;

  // Called when speech recognition using SODA is available.
  virtual void OnSpeechRecognitionAvailabilityChanged(
      SpeechRecognitionAvailability availability) = 0;

  // Called when transcription result from mic input is ready.
  virtual void OnTranscription(
      const media::SpeechRecognitionResult& result) = 0;

  // Called when there is an error in transcription.
  virtual void OnTranscriptionError() = 0;

  // Returns true if Projector screen recording feature is available on the
  // device. If on device speech recognition is not available on device, then
  // Projector is not eligible.
  virtual bool IsEligible() const = 0;

  // Returns true if we can start a new Projector session.
  virtual bool CanStartNewSession() const = 0;

  // The following functions are callbacks from the annotator back to the
  // ProjectorController.

  // Callback indicating that the annotator tool has changed.
  virtual void OnToolSet(const AnnotatorTool& tool) = 0;
  // Callback indicating availability of undo and redo functionalities.
  virtual void OnUndoRedoAvailabilityChanged(bool undo_available,
                                             bool redo_available) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
