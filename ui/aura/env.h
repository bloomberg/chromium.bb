// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_H_
#define UI_AURA_ENV_H_
#pragma once

#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace aura {

class EnvObserver;
class Window;

// A singleton object that tracks general state within Aura.
// TODO(beng): manage RootWindows.
class AURA_EXPORT Env {
 public:
  Env();
  ~Env();

  static Env* GetInstance();
  static void DeleteInstance();

  void AddObserver(EnvObserver* observer);
  void RemoveObserver(EnvObserver* observer);

 private:
  friend class Window;

  // Called by the Window when it is initialized. Notifies observers.
  void NotifyWindowInitialized(Window* window);

  ObserverList<EnvObserver> observers_;

  static Env* instance_;

  DISALLOW_COPY_AND_ASSIGN(Env);
};

}  // namespace aura

#endif  // UI_AURA_ENV_H_
