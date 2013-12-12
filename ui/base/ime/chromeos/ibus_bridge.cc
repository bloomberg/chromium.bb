// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ibus_bridge.h"

#include <map>
#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

static IBusBridge* g_ibus_bridge = NULL;

// An implementation of IBusBridge.
class IBusBridgeImpl : public IBusBridge {
 public:
  IBusBridgeImpl()
    : input_context_handler_(NULL),
      engine_handler_(NULL),
      candidate_window_handler_(NULL) {
  }

  virtual ~IBusBridgeImpl() {
  }

  // IBusBridge override.
  virtual IBusInputContextHandlerInterface*
      GetInputContextHandler() const OVERRIDE {
    return input_context_handler_;
  }

  // IBusBridge override.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) OVERRIDE {
    input_context_handler_ = handler;
  }

  // IBusBridge override.
  virtual void SetEngineHandler(
      const std::string& engine_id,
      IBusEngineHandlerInterface* handler) OVERRIDE {
    DCHECK(!engine_id.empty());
    DCHECK(handler);
    engine_handler_map_[engine_id] = handler;
  }

  // IBusBridge override.
  virtual IBusEngineHandlerInterface* GetEngineHandler(
      const std::string& engine_id) OVERRIDE {
    if (engine_id.empty() ||
        engine_handler_map_.find(engine_id) == engine_handler_map_.end()) {
      return NULL;
    }
    return engine_handler_map_[engine_id];
  }

  // IBusBridge override.
  virtual void SetCurrentEngineHandler(
      IBusEngineHandlerInterface* handler) OVERRIDE {
    engine_handler_ = handler;
  }

  // IBusBridge override.
  virtual IBusEngineHandlerInterface* SetCurrentEngineHandlerById(
      const std::string& engine_id) OVERRIDE {
    if (engine_id.empty()) {
      engine_handler_ = NULL;
      return NULL;
    }

    DCHECK(engine_handler_map_.find(engine_id) != engine_handler_map_.end());
    engine_handler_ = engine_handler_map_[engine_id];
    return engine_handler_;
  }

  // IBusBridge override.
  virtual IBusEngineHandlerInterface* GetCurrentEngineHandler() const OVERRIDE {
    return engine_handler_;
  }

  // IBusBridge override.
  virtual IBusPanelCandidateWindowHandlerInterface*
  GetCandidateWindowHandler() const OVERRIDE {
    return candidate_window_handler_;
  }

  // IBusBridge override.
  virtual void SetCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) OVERRIDE {
    candidate_window_handler_ = handler;
  }

 private:
  IBusInputContextHandlerInterface* input_context_handler_;
  IBusEngineHandlerInterface* engine_handler_;
  IBusPanelCandidateWindowHandlerInterface* candidate_window_handler_;
  std::map<std::string, IBusEngineHandlerInterface*> engine_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(IBusBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IBusBridge
IBusBridge::IBusBridge() {
}

IBusBridge::~IBusBridge() {
}

// static.
void IBusBridge::Initialize() {
  if (!g_ibus_bridge)
    g_ibus_bridge = new IBusBridgeImpl();
}

// static.
void IBusBridge::Shutdown() {
  delete g_ibus_bridge;
  g_ibus_bridge = NULL;
}

// static.
IBusBridge* IBusBridge::Get() {
  return g_ibus_bridge;
}

}  // namespace chromeos
