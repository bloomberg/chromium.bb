// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/audio_focus_manager_metrics_helper.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

namespace {

mojom::EnforcementMode GetDefaultEnforcementMode() {
  if (base::FeatureList::IsEnabled(features::kAudioFocusEnforcement)) {
    if (base::FeatureList::IsEnabled(features::kAudioFocusSessionGrouping))
      return mojom::EnforcementMode::kSingleGroup;
    return mojom::EnforcementMode::kSingleSession;
  }

  return mojom::EnforcementMode::kNone;
}

}  // namespace

class AudioFocusManager::StackRow : public mojom::AudioFocusRequestClient {
 public:
  StackRow(AudioFocusManager* owner,
           mojom::AudioFocusRequestClientRequest request,
           mojom::MediaSessionPtr session,
           mojom::MediaSessionInfoPtr session_info,
           mojom::AudioFocusType audio_focus_type,
           RequestId id,
           const std::string& source_name,
           const base::UnguessableToken& group_id)
      : id_(id),
        source_name_(source_name),
        group_id_(group_id),
        metrics_helper_(source_name),
        session_(std::move(session)),
        session_info_(std::move(session_info)),
        audio_focus_type_(audio_focus_type),
        binding_(this, std::move(request)),
        owner_(owner) {
    // Listen for mojo errors.
    binding_.set_connection_error_handler(
        base::BindOnce(&AudioFocusManager::StackRow::OnConnectionError,
                       base::Unretained(this)));
    session_.set_connection_error_handler(
        base::BindOnce(&AudioFocusManager::StackRow::OnConnectionError,
                       base::Unretained(this)));

    metrics_helper_.OnRequestAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kInitial,
        audio_focus_type);
  }

  ~StackRow() override = default;

  // mojom::AudioFocusRequestClient.
  void RequestAudioFocus(mojom::MediaSessionInfoPtr session_info,
                         mojom::AudioFocusType type,
                         RequestAudioFocusCallback callback) override {
    SetSessionInfo(std::move(session_info));

    if (IsActive() && owner_->IsSessionOnTopOfAudioFocusStack(id(), type)) {
      // Early returning if |media_session| is already on top (has focus) and is
      // active.
      std::move(callback).Run();
      return;
    }

    // Remove this StackRow for the audio focus stack.
    std::unique_ptr<StackRow> row = owner_->RemoveFocusEntryIfPresent(id());
    DCHECK(row);

    owner_->RequestAudioFocusInternal(std::move(row), type);

    std::move(callback).Run();

    metrics_helper_.OnRequestAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kUpdate,
        audio_focus_type_);
  }

  void AbandonAudioFocus() override {
    metrics_helper_.OnAbandonAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::kAPI);

    owner_->AbandonAudioFocusInternal(id_);
  }

  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) override {
    bool suspended_change =
        (info->state == mojom::MediaSessionInfo::SessionState::kSuspended ||
         IsSuspended()) &&
        info->state != session_info_->state;

    SetSessionInfo(std::move(info));

    // If we have transitioned to/from a suspended state then we should
    // re-enforce audio focus.
    if (suspended_change)
      owner_->EnforceAudioFocus();
  }

  mojom::MediaSession* session() { return session_.get(); }
  const mojom::MediaSessionInfoPtr& info() const { return session_info_; }
  mojom::AudioFocusType audio_focus_type() const { return audio_focus_type_; }

  void SetAudioFocusType(mojom::AudioFocusType type) {
    audio_focus_type_ = type;
  }

  bool IsActive() const {
    return session_info_->state ==
           mojom::MediaSessionInfo::SessionState::kActive;
  }

  bool IsSuspended() const {
    return session_info_->state ==
           mojom::MediaSessionInfo::SessionState::kSuspended;
  }

  RequestId id() const { return id_; }

  const std::string& source_name() const { return source_name_; }

  const base::UnguessableToken& group_id() const { return group_id_; }

  mojom::AudioFocusRequestStatePtr ToAudioFocusRequestState() const {
    auto request = mojom::AudioFocusRequestState::New();
    request->session_info = session_info_.Clone();
    request->audio_focus_type = audio_focus_type_;
    request->request_id = id_;
    request->source_name = source_name_;
    return request;
  }

  void BindToController(mojom::MediaControllerRequest request) {
    if (!controller_) {
      controller_ = std::make_unique<MediaController>();
      controller_->SetMediaSession(session_.get());
    }

    controller_->BindToInterface(std::move(request));
  }

  void Suspend(const EnforcementState& state) {
    DCHECK(!session_info_->force_duck);

    // In most cases if we stop or suspend we should call the ::Suspend method
    // on the media session. The only exception is if the session has the
    // |prefer_stop_for_gain_focus_loss| bit set in which case we should use
    // ::Stop and ::Suspend respectively.
    if (state.should_stop && session_info_->prefer_stop_for_gain_focus_loss) {
      session_->Stop(mojom::MediaSession::SuspendType::kSystem);
    } else {
      was_suspended_ = was_suspended_ || state.should_suspend;
      session_->Suspend(mojom::MediaSession::SuspendType::kSystem);
    }
  }

  void MaybeResume() {
    DCHECK(!session_info_->force_duck);

    if (!was_suspended_)
      return;

    was_suspended_ = false;
    session_->Resume(mojom::MediaSession::SuspendType::kSystem);
  }

 private:
  void SetSessionInfo(mojom::MediaSessionInfoPtr session_info) {
    bool is_controllable_changed =
        session_info_->is_controllable != session_info->is_controllable;

    session_info_ = std::move(session_info);

    if (is_controllable_changed)
      owner_->MaybeUpdateActiveSession();
  }

  void OnConnectionError() {
    // Since we have multiple pathways that can call |OnConnectionError| we
    // should use the |encountered_error_| bit to make sure we abandon focus
    // just the first time.
    if (encountered_error_)
      return;
    encountered_error_ = true;

    metrics_helper_.OnAbandonAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::
            kConnectionError);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AudioFocusManager::AbandonAudioFocusInternal,
                                  base::Unretained(owner_), id_));
  }

  const RequestId id_;
  const std::string source_name_;
  const base::UnguessableToken group_id_;

  AudioFocusManagerMetricsHelper metrics_helper_;
  bool encountered_error_ = false;
  bool was_suspended_ = false;

  std::unique_ptr<MediaController> controller_;

  mojom::MediaSessionPtr session_;
  mojom::MediaSessionInfoPtr session_info_;
  mojom::AudioFocusType audio_focus_type_;
  mojo::Binding<mojom::AudioFocusRequestClient> binding_;

  // Weak pointer to the owning |AudioFocusManager| instance.
  AudioFocusManager* owner_;

  DISALLOW_COPY_AND_ASSIGN(StackRow);
};

void AudioFocusManager::RequestAudioFocus(
    mojom::AudioFocusRequestClientRequest request,
    mojom::MediaSessionPtr media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    RequestAudioFocusCallback callback) {
  RequestGroupedAudioFocus(
      std::move(request), std::move(media_session), std::move(session_info),
      type, base::UnguessableToken::Create(), std::move(callback));
}

void AudioFocusManager::RequestGroupedAudioFocus(
    mojom::AudioFocusRequestClientRequest request,
    mojom::MediaSessionPtr media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    const base::UnguessableToken& group_id,
    RequestGroupedAudioFocusCallback callback) {
  base::UnguessableToken request_id = base::UnguessableToken::Create();

  RequestAudioFocusInternal(
      std::make_unique<StackRow>(this, std::move(request),
                                 std::move(media_session),
                                 std::move(session_info), type, request_id,
                                 GetBindingSourceName(), group_id),
      type);

  std::move(callback).Run(request_id);
}

void AudioFocusManager::GetFocusRequests(GetFocusRequestsCallback callback) {
  std::vector<mojom::AudioFocusRequestStatePtr> requests;

  for (const auto& row : audio_focus_stack_)
    requests.push_back(row->ToAudioFocusRequestState());

  std::move(callback).Run(std::move(requests));
}

void AudioFocusManager::GetDebugInfoForRequest(
    const RequestId& request_id,
    GetDebugInfoForRequestCallback callback) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() != request_id)
      continue;

    row->session()->GetDebugInfo(base::BindOnce(
        [](const base::UnguessableToken& group_id,
           GetDebugInfoForRequestCallback callback,
           mojom::MediaSessionDebugInfoPtr info) {
          // Inject the |group_id| into the state string. This is because in
          // some cases the group id is automatically generated by the media
          // session service so the session is unaware of it.
          if (!info->state.empty())
            info->state += " ";
          info->state += "GroupId=" + group_id.ToString();

          std::move(callback).Run(std::move(info));
        },
        row->group_id(), std::move(callback)));
    return;
  }

  std::move(callback).Run(mojom::MediaSessionDebugInfo::New());
}

void AudioFocusManager::AbandonAudioFocusInternal(RequestId id) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back()->id() != id) {
    RemoveFocusEntryIfPresent(id);
    MaybeUpdateActiveSession();
    return;
  }

  auto row = std::move(audio_focus_stack_.back());
  audio_focus_stack_.pop_back();

  if (audio_focus_stack_.empty()) {
    // Notify observers that we lost audio focus.
    observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
      observer->OnFocusLost(row->ToAudioFocusRequestState());
    });

    MaybeUpdateActiveSession();
    return;
  }

  EnforceAudioFocus();
  MaybeUpdateActiveSession();

  // Notify observers that we lost audio focus.
  observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
    observer->OnFocusLost(row->ToAudioFocusRequestState());
  });

  // Notify observers that the session on top gained focus.
  StackRow* new_session = audio_focus_stack_.back().get();
  observers_.ForAllPtrs([&new_session](mojom::AudioFocusObserver* observer) {
    observer->OnFocusGained(new_session->ToAudioFocusRequestState());
  });
}

void AudioFocusManager::AddObserver(mojom::AudioFocusObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddPtr(std::move(observer));
}

void AudioFocusManager::SetSourceName(const std::string& name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.dispatch_context()->source_name = name;
}

void AudioFocusManager::SetEnforcementMode(mojom::EnforcementMode mode) {
  if (mode == mojom::EnforcementMode::kDefault)
    mode = GetDefaultEnforcementMode();

  if (mode == enforcement_mode_)
    return;

  enforcement_mode_ = mode;

  if (audio_focus_stack_.empty())
    return;

  EnforceAudioFocus();
}

void AudioFocusManager::CreateActiveMediaController(
    mojom::MediaControllerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  active_media_controller_.BindToInterface(std::move(request));
}

void AudioFocusManager::CreateMediaControllerForSession(
    mojom::MediaControllerRequest request,
    const base::UnguessableToken& request_id) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() != request_id)
      continue;

    row->BindToController(std::move(request));
    break;
  }
}

void AudioFocusManager::BindToInterface(
    mojom::AudioFocusManagerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.AddBinding(this, std::move(request),
                       std::make_unique<BindingContext>());
}

void AudioFocusManager::BindToDebugInterface(
    mojom::AudioFocusManagerDebugRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  debug_bindings_.AddBinding(this, std::move(request));
}

void AudioFocusManager::BindToControllerManagerInterface(
    mojom::MediaControllerManagerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  controller_bindings_.AddBinding(this, std::move(request));
}

void AudioFocusManager::RequestAudioFocusInternal(std::unique_ptr<StackRow> row,
                                                  mojom::AudioFocusType type) {
  row->SetAudioFocusType(type);
  audio_focus_stack_.push_back(std::move(row));

  EnforceAudioFocus();
  MaybeUpdateActiveSession();

  // Notify observers that we were gained audio focus.
  mojom::AudioFocusRequestStatePtr session_state =
      audio_focus_stack_.back()->ToAudioFocusRequestState();
  observers_.ForAllPtrs([&session_state](mojom::AudioFocusObserver* observer) {
    observer->OnFocusGained(session_state.Clone());
  });
}

void AudioFocusManager::EnforceAudioFocus() {
  DCHECK_NE(mojom::EnforcementMode::kDefault, enforcement_mode_);
  if (audio_focus_stack_.empty())
    return;

  EnforcementState state;

  for (auto& session : base::Reversed(audio_focus_stack_)) {
    EnforceSingleSession(session.get(), state);

    // Update the flags based on the audio focus type of this session. If the
    // session is suspended then any transient audio focus type should not have
    // an effect.
    switch (session->audio_focus_type()) {
      case mojom::AudioFocusType::kGain:
        state.should_stop = true;
        break;
      case mojom::AudioFocusType::kGainTransient:
        if (!session->IsSuspended())
          state.should_suspend = true;
        break;
      case mojom::AudioFocusType::kGainTransientMayDuck:
        if (!session->IsSuspended())
          state.should_duck = true;
        break;
      case mojom::AudioFocusType::kAmbient:
        break;
    }
  }
}

void AudioFocusManager::MaybeUpdateActiveSession() {
  StackRow* active = nullptr;

  for (auto& row : base::Reversed(audio_focus_stack_)) {
    if (!row->info()->is_controllable)
      continue;

    active = row.get();
    break;
  }

  if (!active_media_controller_.SetMediaSession(active ? active->session()
                                                       : nullptr)) {
    return;
  }

  mojom::AudioFocusRequestStatePtr state =
      active ? active->ToAudioFocusRequestState() : nullptr;

  // Notify observers that the active media session changed.
  observers_.ForAllPtrs([&state](mojom::AudioFocusObserver* observer) {
    observer->OnActiveSessionChanged(state.Clone());
  });
}

AudioFocusManager::AudioFocusManager()
    : enforcement_mode_(GetDefaultEnforcementMode()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AudioFocusManager::~AudioFocusManager() = default;

std::unique_ptr<AudioFocusManager::StackRow>
AudioFocusManager::RemoveFocusEntryIfPresent(RequestId id) {
  std::unique_ptr<StackRow> row;

  for (auto iter = audio_focus_stack_.begin(); iter != audio_focus_stack_.end();
       ++iter) {
    if ((*iter)->id() == id) {
      row.swap((*iter));
      audio_focus_stack_.erase(iter);
      break;
    }
  }

  return row;
}

const std::string& AudioFocusManager::GetBindingSourceName() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return bindings_.dispatch_context()->source_name;
}

bool AudioFocusManager::IsSessionOnTopOfAudioFocusStack(
    RequestId id,
    mojom::AudioFocusType type) const {
  return !audio_focus_stack_.empty() && audio_focus_stack_.back()->id() == id &&
         audio_focus_stack_.back()->audio_focus_type() == type;
}

bool AudioFocusManager::ShouldSessionBeSuspended(
    const StackRow* session,
    const EnforcementState& state) const {
  bool should_suspend_any = state.should_stop || state.should_suspend;

  switch (enforcement_mode_) {
    case mojom::EnforcementMode::kSingleSession:
      return should_suspend_any;
    case mojom::EnforcementMode::kSingleGroup:
      return should_suspend_any &&
             session->group_id() != audio_focus_stack_.back()->group_id();
    case mojom::EnforcementMode::kNone:
      return false;
    case mojom::EnforcementMode::kDefault:
      NOTIMPLEMENTED();
      return false;
  }
}

bool AudioFocusManager::ShouldSessionBeDucked(
    const StackRow* session,
    const EnforcementState& state) const {
  switch (enforcement_mode_) {
    case mojom::EnforcementMode::kSingleSession:
    case mojom::EnforcementMode::kSingleGroup:
      if (session->info()->force_duck)
        return state.should_duck || ShouldSessionBeSuspended(session, state);
      return state.should_duck;
    case mojom::EnforcementMode::kNone:
      return false;
    case mojom::EnforcementMode::kDefault:
      NOTIMPLEMENTED();
      return false;
  }
}

void AudioFocusManager::EnforceSingleSession(StackRow* session,
                                             const EnforcementState& state) {
  if (ShouldSessionBeDucked(session, state)) {
    session->session()->StartDucking();
  } else {
    session->session()->StopDucking();
  }

  // If the session wants to be ducked instead of suspended we should stop now.
  if (session->info()->force_duck)
    return;

  if (ShouldSessionBeSuspended(session, state)) {
    session->Suspend(state);
  } else {
    session->MaybeResume();
  }
}

}  // namespace media_session
