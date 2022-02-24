// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_throttler/frame_throttling_controller.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

void CollectFrameSinkIds(const aura::Window* window,
                         base::flat_set<viz::FrameSinkId>* frame_sink_ids) {
  if (window->GetFrameSinkId().is_valid()) {
    frame_sink_ids->insert(window->GetFrameSinkId());
    return;
  }
  for (auto* child : window->children()) {
    CollectFrameSinkIds(child, frame_sink_ids);
  }
}

// Recursively walks through all descendents of |window| and collects those
// belonging to a browser and with their frame sink ids being a member of |ids|.
// |inside_browser| indicates if the |window| already belongs to a browser.
// |frame_sink_ids| is the output containing the result frame sinks ids.
void CollectBrowserFrameSinkIdsInWindow(
    const aura::Window* window,
    bool inside_browser,
    const base::flat_set<viz::FrameSinkId>& ids,
    base::flat_set<viz::FrameSinkId>* frame_sink_ids) {
  if (inside_browser || ash::AppType::BROWSER ==
                            static_cast<ash::AppType>(
                                window->GetProperty(aura::client::kAppType))) {
    const auto& id = window->GetFrameSinkId();
    if (id.is_valid() && ids.contains(id))
      frame_sink_ids->insert(id);
    inside_browser = true;
  }

  for (auto* child : window->children()) {
    CollectBrowserFrameSinkIdsInWindow(child, inside_browser, ids,
                                       frame_sink_ids);
  }
}

}  // namespace

ThrottleCandidates::ThrottleCandidates() = default;

ThrottleCandidates::~ThrottleCandidates() = default;

ThrottleCandidates::ThrottleCandidates(const ThrottleCandidates&) = default;

ThrottleCandidates& ThrottleCandidates::operator=(const ThrottleCandidates&) =
    default;

bool ThrottleCandidates::IsEmpty() const {
  return browser_frame_sink_ids.empty() && lacros_candidates.empty();
}

void FrameThrottlingController::ResetThrottleCandidates(
    ThrottleCandidates* candidates) {
  candidates->browser_frame_sink_ids.clear();
  for (auto& lacros_candidate : candidates->lacros_candidates) {
    // Reset the window property for frame rate throttling.
    lacros_candidate.first->SetProperty(ash::kFrameRateThrottleKey, false);
  }
  candidates->lacros_candidates.clear();
}

FrameThrottlingController::FrameThrottlingController(
    ui::ContextFactory* context_factory)
    : context_factory_(context_factory) {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kFrameThrottleFps)) {
    int value;
    if (base::StringToInt(cl->GetSwitchValueASCII(switches::kFrameThrottleFps),
                          &value)) {
      throttled_fps_ = value;
    }
  }
}

FrameThrottlingController::~FrameThrottlingController() {
  EndThrottling();
}

void FrameThrottlingController::StartThrottling(
    const std::vector<aura::Window*>& windows) {
  if (windows_manually_throttled_)
    EndThrottling();

  if (windows.empty())
    return;

  auto all_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  std::vector<aura::Window*> all_arc_windows;
  std::copy_if(all_windows.begin(), all_windows.end(),
               std::back_inserter(all_arc_windows), [](aura::Window* window) {
                 return ash::AppType::ARC_APP ==
                        static_cast<ash::AppType>(
                            window->GetProperty(aura::client::kAppType));
               });

  std::vector<aura::Window*> arc_windows;
  arc_windows.reserve(windows.size());
  for (auto* window : windows) {
    ash::AppType type =
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
    switch (type) {
      case ash::AppType::BROWSER:
        CollectFrameSinkIds(
            window, &manually_throttled_candidates_.browser_frame_sink_ids);
        break;
      case ash::AppType::ARC_APP:
        arc_windows.push_back(window);
        break;
      case ash::AppType::LACROS:
        CollectLacrosCandidates(
            window, &manually_throttled_candidates_.lacros_candidates, window);
        break;
      default:
        break;
    }
  }

  // Throttle browser and lacros windows.
  if (!manually_throttled_candidates_.IsEmpty()) {
    UpdateThrottlingOnFrameSinks();
    for (const auto& lacros_candidate :
         manually_throttled_candidates_.lacros_candidates) {
      lacros_candidate.first->SetProperty(ash::kFrameRateThrottleKey, true);
    }
    windows_manually_throttled_ = true;
  }

  // Do not throttle arc if at least one arc window should not be throttled.
  if (!arc_windows.empty() && (arc_windows.size() == all_arc_windows.size())) {
    StartThrottlingArc(arc_windows);
    windows_manually_throttled_ = true;
  }
}

void FrameThrottlingController::StartThrottlingArc(
    const std::vector<aura::Window*>& arc_windows) {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingStarted(arc_windows, throttled_fps_);
  }
}

void FrameThrottlingController::EndThrottlingArc() {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingEnded();
  }
}

void FrameThrottlingController::EndThrottling() {
  if (windows_manually_throttled_) {
    ResetThrottleCandidates(&manually_throttled_candidates_);
    UpdateThrottlingOnFrameSinks();
    EndThrottlingArc();

    windows_manually_throttled_ = false;
  }
}

void FrameThrottlingController::OnCompositingFrameSinksToThrottleUpdated(
    const aura::WindowTreeHost* window_tree_host,
    const base::flat_set<viz::FrameSinkId>& ids) {
  ThrottleCandidates& candidates = host_to_candidates_map_[window_tree_host];
  ResetThrottleCandidates(&candidates);

  const aura::Window* window = window_tree_host->window();
  CollectBrowserFrameSinkIdsInWindow(window, false, ids,
                                     &candidates.browser_frame_sink_ids);
  CollectLacrosWindowsInWindow(const_cast<aura::Window*>(window), false, ids,
                               &candidates.lacros_candidates);
  UpdateThrottlingOnFrameSinks();
  for (auto& lacros_candidate : candidates.lacros_candidates)
    lacros_candidate.first->SetProperty(ash::kFrameRateThrottleKey, true);
}

void FrameThrottlingController::OnWindowDestroying(aura::Window* window) {
  if (window->IsRootWindow()) {
    auto it = host_to_candidates_map_.find(window->GetHost());
    if (it != host_to_candidates_map_.end()) {
      for (auto& lacros_candidate : it->second.lacros_candidates) {
        lacros_candidate.first->SetProperty(ash::kFrameRateThrottleKey, false);
        lacros_candidate.first->RemoveObserver(this);
      }
      host_to_candidates_map_.erase(it);
      UpdateThrottlingOnFrameSinks();
    }
  } else {
    bool window_removed = false;
    for (auto& host_to_candidates : host_to_candidates_map_) {
      auto& lacros_candidates = host_to_candidates.second.lacros_candidates;
      auto it = lacros_candidates.find(window);
      if (it != lacros_candidates.end()) {
        window_removed = true;
        lacros_candidates.erase(it);
      }
    }
    auto& manually_throttled_lacros_candidates =
        manually_throttled_candidates_.lacros_candidates;
    auto it = manually_throttled_lacros_candidates.find(window);
    if (it != manually_throttled_lacros_candidates.end()) {
      window_removed = true;
      manually_throttled_lacros_candidates.erase(it);
    }

    if (window_removed)
      UpdateThrottlingOnFrameSinks();
  }
  window->RemoveObserver(this);
}

void FrameThrottlingController::OnWindowTreeHostCreated(
    aura::WindowTreeHost* host) {
  host->AddObserver(this);
  host->window()->AddObserver(this);
}

std::vector<viz::FrameSinkId>
FrameThrottlingController::GetFrameSinkIdsToThrottle() const {
  std::vector<viz::FrameSinkId> ids_to_throttle;
  ids_to_throttle.reserve(host_to_candidates_map_.size() * 2);
  // Add frame sink ids gathered from compositing.
  for (const auto& pair : host_to_candidates_map_) {
    ids_to_throttle.insert(ids_to_throttle.end(),
                           pair.second.browser_frame_sink_ids.begin(),
                           pair.second.browser_frame_sink_ids.end());
    // insert the frame sink ids for lacros windows.
    for (const auto& candidate : pair.second.lacros_candidates) {
      ids_to_throttle.push_back(candidate.second);
    }
  }
  // Add frame sink ids from special ui modes.
  if (!manually_throttled_candidates_.IsEmpty()) {
    ids_to_throttle.insert(
        ids_to_throttle.end(),
        manually_throttled_candidates_.browser_frame_sink_ids.begin(),
        manually_throttled_candidates_.browser_frame_sink_ids.end());
    for (const auto& lacros_candidate :
         manually_throttled_candidates_.lacros_candidates) {
      ids_to_throttle.push_back(lacros_candidate.second);
    }
  }
  return ids_to_throttle;
}

void FrameThrottlingController::UpdateThrottlingOnFrameSinks() {
  context_factory_->GetHostFrameSinkManager()->Throttle(
      GetFrameSinkIdsToThrottle(), base::Hertz(throttled_fps_));
}

void FrameThrottlingController::AddArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.AddObserver(observer);
}

void FrameThrottlingController::RemoveArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.RemoveObserver(observer);
}

bool FrameThrottlingController::HasArcObserver(
    FrameThrottlingObserver* observer) {
  return arc_observers_.HasObserver(observer);
}

void FrameThrottlingController::CollectLacrosWindowsInWindow(
    aura::Window* window,
    bool inside_lacros,
    const base::flat_set<viz::FrameSinkId>& ids,
    base::flat_map<aura::Window*, viz::FrameSinkId>* candidates,
    aura::Window* lacros_window) {
  if (ash::AppType::LACROS ==
      static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType))) {
    DCHECK(!lacros_window);
    lacros_window = window;
    inside_lacros = true;
  }

  if (inside_lacros) {
    const auto& id = window->GetFrameSinkId();
    if (id.is_valid() && ids.contains(id)) {
      DCHECK(lacros_window);
      candidates->insert(std::make_pair(lacros_window, id));
      if (!lacros_window->HasObserver(this))
        lacros_window->AddObserver(this);
      return;
    }
  }

  for (auto* child : window->children()) {
    CollectLacrosWindowsInWindow(child, inside_lacros, ids, candidates,
                                 lacros_window);
  }
}

void FrameThrottlingController::CollectLacrosCandidates(
    aura::Window* window,
    base::flat_map<aura::Window*, viz::FrameSinkId>* candidates,
    aura::Window* lacros_window) {
  const auto& id = window->GetFrameSinkId();
  if (id.is_valid()) {
    DCHECK(lacros_window);
    candidates->insert(std::make_pair(lacros_window, id));
    if (!lacros_window->HasObserver(this))
      lacros_window->AddObserver(this);
    return;
  }
  for (auto* child : window->children())
    CollectLacrosCandidates(child, candidates, lacros_window);
}

}  // namespace ash
