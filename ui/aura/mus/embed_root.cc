// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/embed_root.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/mus/embed_root_delegate.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/focus_synchronizer_observer.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"

namespace aura {
namespace {

// FocusClient implementation used for embedded windows. This has minimal
// checks as to what can get focus.
class EmbeddedFocusClient : public client::FocusClient,
                            public WindowObserver,
                            public FocusSynchronizerObserver {
 public:
  EmbeddedFocusClient(FocusSynchronizer* focus_synchronizer, Window* root)
      : focus_synchronizer_(focus_synchronizer), root_(root) {
    client::SetFocusClient(root, this);
  }

  ~EmbeddedFocusClient() override {
    client::SetFocusClient(root_, nullptr);
    if (focused_window_)
      focused_window_->RemoveObserver(this);
  }

  // client::FocusClient:
  void AddObserver(client::FocusChangeObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(client::FocusChangeObserver* observer) override {
    observers_.RemoveObserver(observer);
  }
  void FocusWindow(Window* window) override {
    if (IsValidWindowForFocus(window) && window != GetFocusedWindow())
      FocusWindowImpl(window);

    if (GetFocusedWindow() &&
        focus_synchronizer_->active_focus_client() != this) {
      focus_synchronizer_->SetActiveFocusClient(this, root_);
      scoped_focus_synchronizer_observer_.Add(focus_synchronizer_);
    } else if (!GetFocusedWindow() &&
               focus_synchronizer_->active_focus_client() == this) {
      scoped_focus_synchronizer_observer_.RemoveAll();
      focus_synchronizer_->SetActiveFocusClient(nullptr, nullptr);
    }
  }
  void ResetFocusWithinActiveWindow(Window* window) override {
    // This is never called in the embedding case.
    NOTREACHED();
  }
  Window* GetFocusedWindow() override { return focused_window_; }

 private:
  bool IsValidWindowForFocus(Window* window) const {
    return !window || (root_->Contains(window) && window->CanFocus());
  }

  void FocusWindowImpl(Window* window) {
    Window* lost_focus = focused_window_;

    if (lost_focus)
      lost_focus->RemoveObserver(this);
    focused_window_ = window;
    if (focused_window_)
      focused_window_->AddObserver(this);

    WindowTracker window_tracker;
    if (lost_focus)
      window_tracker.Add(lost_focus);

    for (auto& observer : observers_) {
      observer.OnWindowFocused(
          focused_window_,
          window_tracker.Contains(lost_focus) ? lost_focus : nullptr);
    }
    if (window_tracker.Contains(lost_focus)) {
      client::FocusChangeObserver* observer =
          client::GetFocusChangeObserver(lost_focus);
      if (observer)
        observer->OnWindowFocused(focused_window_, lost_focus);
    }
    client::FocusChangeObserver* observer =
        client::GetFocusChangeObserver(focused_window_);
    if (observer) {
      observer->OnWindowFocused(
          focused_window_,
          window_tracker.Contains(lost_focus) ? lost_focus : nullptr);
    }
  }

  // WindowObserver:
  void OnWindowDestroying(Window* window) override {
    DCHECK_EQ(window, focused_window_);
  }

  // FocusSynchronizerObserver:
  void OnActiveFocusClientChanged(client::FocusClient* focus_client,
                                  Window* focus_client_root) override {
    DCHECK_NE(this, focus_client);
    scoped_focus_synchronizer_observer_.RemoveAll();
    FocusWindowImpl(nullptr);
  }

  // FocusSynchronizer instance of our WindowTreeClient. Not owned.
  FocusSynchronizer* const focus_synchronizer_;

  // Root of the hierarchy this is the FocusClient for.
  Window* const root_;

  ScopedObserver<FocusSynchronizer, FocusSynchronizerObserver>
      scoped_focus_synchronizer_observer_{this};

  Window* focused_window_ = nullptr;

  base::ObserverList<client::FocusChangeObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedFocusClient);
};

}  // namespace

EmbedRoot::~EmbedRoot() {
  window_tree_client_->OnEmbedRootDestroyed(this);
  // Makes use of window_tree_host_->window(), so needs to be destroyed before
  // |window_tree_host_|.
  focus_client_.reset();
}

Window* EmbedRoot::window() {
  return window_tree_host_ ? window_tree_host_->window() : nullptr;
}

EmbedRoot::EmbedRoot(WindowTreeClient* window_tree_client,
                     EmbedRootDelegate* delegate,
                     ws::ClientSpecificId window_id)
    : window_tree_client_(window_tree_client),
      delegate_(delegate),
      weak_factory_(this) {
  window_tree_client_->tree_->ScheduleEmbedForExistingClient(
      window_id, base::BindOnce(&EmbedRoot::OnScheduledEmbedForExistingClient,
                                weak_factory_.GetWeakPtr()));
}

void EmbedRoot::OnScheduledEmbedForExistingClient(
    const base::UnguessableToken& token) {
  token_ = token;
  delegate_->OnEmbedTokenAvailable(token);
}

void EmbedRoot::OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) {
  focus_client_ = std::make_unique<EmbeddedFocusClient>(
      window_tree_client_->focus_synchronizer(), window_tree_host->window());
  window_tree_host_ = std::move(window_tree_host);
  delegate_->OnEmbed(window());
}

void EmbedRoot::OnUnembed() {
  window_tree_host_.reset();
  focus_client_.reset();

  // |delegate_| may delete us.
  delegate_->OnUnembed();
}

}  // namespace aura
