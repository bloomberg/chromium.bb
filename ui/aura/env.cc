// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env.h"

#include "ui/aura/env_observer.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_target_iterator.h"

namespace aura {

// static
Env* Env::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Env, public:

Env::Env()
    : mouse_button_flags_(0),
      is_touch_down_(false),
      input_state_lookup_(InputStateLookup::Create().Pass()) {
}

Env::~Env() {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnWillDestroyEnv());

  ui::Compositor::Terminate();
}

//static
void Env::CreateInstance() {
  if (!instance_) {
    instance_ = new Env;
    instance_->Init();
  }
}

// static
Env* Env::GetInstance() {
  DCHECK(instance_) << "Env::CreateInstance must be called before getting "
                       "the instance of Env.";
  return instance_;
}

// static
void Env::DeleteInstance() {
  delete instance_;
  instance_ = NULL;
}

void Env::AddObserver(EnvObserver* observer) {
  observers_.AddObserver(observer);
}

void Env::RemoveObserver(EnvObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Env::IsMouseButtonDown() const {
  return input_state_lookup_.get() ? input_state_lookup_->IsMouseButtonDown() :
      mouse_button_flags_ != 0;
}

////////////////////////////////////////////////////////////////////////////////
// Env, private:

void Env::Init() {
  ui::Compositor::Initialize();
}

void Env::NotifyWindowInitialized(Window* window) {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnWindowInitialized(window));
}

void Env::NotifyHostInitialized(WindowTreeHost* host) {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnHostInitialized(host));
}

void Env::NotifyHostActivated(WindowTreeHost* host) {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnHostActivated(host));
}

////////////////////////////////////////////////////////////////////////////////
// Env, ui::EventTarget implementation:

bool Env::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Env::GetParentTarget() {
  return NULL;
}

scoped_ptr<ui::EventTargetIterator> Env::GetChildIterator() const {
  return scoped_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* Env::GetEventTargeter() {
  NOTREACHED();
  return NULL;
}

}  // namespace aura
