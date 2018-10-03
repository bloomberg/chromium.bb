// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

namespace {

// Generate a unique audio focus request ID for the audio focus request. The IDs
// are only handed out by the audio focus manager.
int GenerateAudioFocusRequestId() {
  static base::AtomicSequenceNumber request_id;
  return request_id.GetNext();
}

}  // namespace

// static
AudioFocusManager* AudioFocusManager::GetInstance() {
  return base::Singleton<AudioFocusManager>::get();
}

AudioFocusManager::RequestResponse AudioFocusManager::RequestAudioFocus(
    mojom::MediaSessionPtr media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    base::Optional<RequestId> previous_id) {
  DCHECK(!session_info.is_null());
  DCHECK(media_session.is_bound());

  if (!audio_focus_stack_.empty() &&
      previous_id == audio_focus_stack_.back()->id() &&
      audio_focus_stack_.back()->audio_focus_type() == type &&
      audio_focus_stack_.back()->IsActive()) {
    // Early returning if |media_session| is already on top (has focus) and is
    // active.
    return AudioFocusManager::RequestResponse(previous_id.value(), true);
  }

  // If we have a previous ID then we should remove it from the stack.
  if (previous_id.has_value())
    RemoveFocusEntryIfPresent(previous_id.value());

  if (type == mojom::AudioFocusType::kGainTransientMayDuck) {
    for (auto& old_session : audio_focus_stack_)
      old_session->session()->StartDucking();
  } else {
    for (auto& old_session : audio_focus_stack_) {
      // If the session has the force duck bit set then we should duck the
      // session instead of suspending it.
      if (old_session->info()->force_duck) {
        old_session->session()->StartDucking();
      } else {
        old_session->session()->Suspend(
            mojom::MediaSession::SuspendType::kSystem);
      }
    }
  }

  audio_focus_stack_.push_back(std::make_unique<AudioFocusManager::StackRow>(
      std::move(media_session), std::move(session_info), type,
      previous_id ? *previous_id : GenerateAudioFocusRequestId()));

  AudioFocusManager::StackRow* row = audio_focus_stack_.back().get();
  row->session()->StopDucking();

  // Notify observers that we were gained audio focus.
  observers_.ForAllPtrs([&row, type](mojom::AudioFocusObserver* observer) {
    observer->OnFocusGained(row->info().Clone(), type);
  });

  // We always grant the audio focus request but this may not always be the case
  // in the future.
  return AudioFocusManager::RequestResponse(row->id(), true);
}

void AudioFocusManager::AbandonAudioFocus(RequestId id) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back()->id() != id) {
    RemoveFocusEntryIfPresent(id);
    return;
  }

  auto row = std::move(audio_focus_stack_.back());
  audio_focus_stack_.pop_back();

  if (audio_focus_stack_.empty()) {
    // Notify observers that we lost audio focus.
    observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
      observer->OnFocusLost(row->info().Clone());
    });
    return;
  }

  // Allow the top-most MediaSession having force duck to unduck even if
  // it is not active.
  for (auto iter = audio_focus_stack_.rbegin();
       iter != audio_focus_stack_.rend(); ++iter) {
    if (!(*iter)->info()->force_duck)
      continue;

    auto duck_row = std::move(*iter);
    duck_row->session()->StopDucking();
    audio_focus_stack_.erase(std::next(iter).base());
    audio_focus_stack_.push_back(std::move(duck_row));
    return;
  }

  // Only try to unduck the new MediaSession on top. The session might be
  // still inactive but it will not be resumed (so it doesn't surprise the
  // user).
  audio_focus_stack_.back()->session()->StopDucking();

  // Notify observers that we lost audio focus.
  observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
    observer->OnFocusLost(row->info().Clone());
  });
}

mojo::InterfacePtrSetElementId AudioFocusManager::AddObserver(
    mojom::AudioFocusObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return observers_.AddPtr(std::move(observer));
}

mojom::AudioFocusType AudioFocusManager::GetFocusTypeForSession(RequestId id) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() == id)
      return row->audio_focus_type();
  }

  NOTREACHED();
  return mojom::AudioFocusType::kGain;
}

void AudioFocusManager::RemoveObserver(mojo::InterfacePtrSetElementId id) {
  observers_.RemovePtr(id);
}

void AudioFocusManager::ResetForTesting() {
  audio_focus_stack_.clear();
  observers_.CloseAll();
}

void AudioFocusManager::FlushForTesting() {
  observers_.FlushForTesting();

  for (auto& session : audio_focus_stack_)
    session->FlushForTesting();
}

AudioFocusManager::AudioFocusManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AudioFocusManager::~AudioFocusManager() = default;

void AudioFocusManager::RemoveFocusEntryIfPresent(
    AudioFocusManager::RequestId id) {
  for (auto iter = audio_focus_stack_.begin(); iter != audio_focus_stack_.end();
       ++iter) {
    if ((*iter)->id() == id) {
      audio_focus_stack_.erase(iter);
      break;
    }
  }
}

AudioFocusManager::StackRow::StackRow(mojom::MediaSessionPtr session,
                                      mojom::MediaSessionInfoPtr current_info,
                                      mojom::AudioFocusType audio_focus_type,
                                      AudioFocusManager::RequestId id)
    : id_(id),
      session_(std::move(session)),
      current_info_(std::move(current_info)),
      audio_focus_type_(audio_focus_type),
      binding_(this) {
  mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));

  // Listen for mojo errors.
  binding_.set_connection_error_handler(base::BindOnce(
      &AudioFocusManager::StackRow::OnConnectionError, base::Unretained(this)));
  session_.set_connection_error_handler(base::BindOnce(
      &AudioFocusManager::StackRow::OnConnectionError, base::Unretained(this)));

  // Listen to info changes on the MediaSession.
  session_->AddObserver(std::move(observer));
};

AudioFocusManager::StackRow::~StackRow() = default;

void AudioFocusManager::StackRow::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr info) {
  current_info_ = std::move(info);
}

mojom::MediaSession* AudioFocusManager::StackRow::session() {
  return session_.get();
}

const mojom::MediaSessionInfoPtr& AudioFocusManager::StackRow::info() const {
  return current_info_;
}

bool AudioFocusManager::StackRow::IsActive() const {
  return current_info_->state == mojom::MediaSessionInfo::SessionState::kActive;
}

void AudioFocusManager::StackRow::FlushForTesting() {
  session_.FlushForTesting();
  binding_.FlushForTesting();
}

void AudioFocusManager::StackRow::OnConnectionError() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioFocusManager::AbandonAudioFocus,
                     base::Unretained(AudioFocusManager::GetInstance()), id()));
}

}  // namespace media_session
