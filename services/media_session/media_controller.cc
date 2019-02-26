// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <set>

#include "services/media_session/public/cpp/media_image_manager.h"

namespace media_session {

// ImageObserverHolder will hold each mojo image observer with the image
// size and type preferences it specified when the observer was added.
class MediaController::ImageObserverHolder {
 public:
  ImageObserverHolder(MediaController* owner,
                      mojom::MediaSessionImageType type,
                      int minimum_size_px,
                      int desired_size_px,
                      mojom::MediaControllerImageObserverPtr observer,
                      const std::vector<MediaImage>& current_images)
      : manager_(minimum_size_px, desired_size_px),
        owner_(owner),
        type_(type),
        minimum_size_px_(minimum_size_px),
        desired_size_px_(desired_size_px),
        observer_(std::move(observer)) {
    // Set a connection error handler so that we will remove observers that have
    // had an error / been closed.
    observer_.set_connection_error_handler(base::BindOnce(
        &MediaController::CleanupImageObservers, base::Unretained(owner_)));

    // Flush the observer with the latest state.
    ImagesChanged(current_images);
  }

  ~ImageObserverHolder() = default;

  bool is_valid() const { return !observer_.encountered_error(); }

  mojom::MediaSessionImageType type() const { return type_; }

  void ImagesChanged(const std::vector<MediaImage>& images) {
    base::Optional<MediaImage> image = manager_.SelectImage(images);

    // If we could not find an image then we should call with an empty image to
    // flush the observer.
    if (!image) {
      ClearImage();
      return;
    }

    DCHECK(owner_->session_);
    owner_->session_->GetMediaImageBitmap(
        *image, minimum_size_px_, desired_size_px_,
        base::BindOnce(&MediaController::ImageObserverHolder::OnImage,
                       base::Unretained(this)));
  }

  void ClearImage() {
    observer_->MediaControllerImageChanged(type_, SkBitmap());
  }

 private:
  void OnImage(const SkBitmap& image) {
    observer_->MediaControllerImageChanged(type_, image);
  }

  media_session::MediaImageManager manager_;

  MediaController* const owner_;

  mojom::MediaSessionImageType const type_;

  int const minimum_size_px_;

  int const desired_size_px_;

  mojom::MediaControllerImageObserverPtr observer_;

  DISALLOW_COPY_AND_ASSIGN(ImageObserverHolder);
};

MediaController::MediaController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MediaController::~MediaController() = default;

void MediaController::Suspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Suspend(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Resume(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Stop(mojom::MediaSession::SuspendType::kUI);
}

void MediaController::ToggleSuspendResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_info_.is_null())
    return;

  switch (session_info_->playback_state) {
    case mojom::MediaPlaybackState::kPlaying:
      Suspend();
      break;
    case mojom::MediaPlaybackState::kPaused:
      Resume();
      break;
  }
}

void MediaController::AddObserver(mojom::MediaControllerObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Flush the new observer with the current state.
  observer->MediaSessionInfoChanged(session_info_.Clone());
  observer->MediaSessionMetadataChanged(session_metadata_);
  observer->MediaSessionActionsChanged(session_actions_);

  observers_.AddPtr(std::move(observer));
}

void MediaController::MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&info](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionInfoChanged(info.Clone());
  });

  session_info_ = std::move(info);
}

void MediaController::MediaSessionMetadataChanged(
    const base::Optional<MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&metadata](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionMetadataChanged(metadata);
  });

  session_metadata_ = metadata;
}

void MediaController::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.ForAllPtrs([&actions](mojom::MediaControllerObserver* observer) {
    observer->MediaSessionActionsChanged(actions);
  });

  session_actions_ = actions;
}

void MediaController::MediaSessionImagesChanged(
    const base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>&
        images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Work out which image types have changed.
  std::set<mojom::MediaSessionImageType> types_changed;
  for (const auto& entry : images) {
    auto it = session_images_.find(entry.first);
    if (it != session_images_.end() && entry.second == it->second)
      continue;

    types_changed.insert(entry.first);
  }

  session_images_ = images;

  for (auto& holder : image_observers_) {
    auto it = session_images_.find(holder->type());

    if (it == session_images_.end()) {
      // No image of this type is available from the session so we should clear
      // any image the observers might have.
      holder->ClearImage();
    } else if (base::ContainsKey(types_changed, holder->type())) {
      holder->ImagesChanged(it->second);
    }
  }
}

void MediaController::PreviousTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->PreviousTrack();
}

void MediaController::NextTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->NextTrack();
}

void MediaController::Seek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->Seek(seek_time);
}

void MediaController::ObserveImages(
    mojom::MediaSessionImageType type,
    int minimum_size_px,
    int desired_size_px,
    mojom::MediaControllerImageObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = session_images_.find(type);

  image_observers_.push_back(std::make_unique<ImageObserverHolder>(
      this, type, minimum_size_px, desired_size_px, std::move(observer),
      it == session_images_.end() ? std::vector<MediaImage>() : it->second));
}

bool MediaController::SetMediaSession(mojom::MediaSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool changed = session != session_;

  if (changed) {
    session_binding_.Close();
    session_info_.reset();
    session_metadata_.reset();
    session_actions_.clear();
    session_images_.clear();

    if (session) {
      // Add |this| as an observer for |session|.
      mojom::MediaSessionObserverPtr observer;
      session_binding_.Bind(mojo::MakeRequest(&observer));
      session->AddObserver(std::move(observer));
    } else {
      // If we are no longer bound to a session we should flush the observers
      // with empty data.
      observers_.ForAllPtrs([](mojom::MediaControllerObserver* observer) {
        observer->MediaSessionInfoChanged(nullptr);
        observer->MediaSessionMetadataChanged(base::nullopt);
        observer->MediaSessionActionsChanged(
            std::vector<mojom::MediaSessionAction>());
      });

      for (auto& holder : image_observers_)
        holder->ClearImage();
    }
  }

  session_ = session;
  return changed;
}

void MediaController::BindToInterface(mojom::MediaControllerRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void MediaController::FlushForTesting() {
  bindings_.FlushForTesting();
}

void MediaController::CleanupImageObservers() {
  base::EraseIf(image_observers_,
                [](const auto& holder) { return !holder->is_valid(); });
}

}  // namespace media_session
