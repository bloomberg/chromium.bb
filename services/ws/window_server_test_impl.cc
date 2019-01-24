// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_server_test_impl.h"

#include <set>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "services/ws/client_root.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window.h"

namespace ws {

namespace {

// Returns true if bitmap contains more than 5 different colors.
bool SanityCheckOutputSkBitmap(const SkBitmap& bitmap) {
  DCHECK_EQ(bitmap.info().colorType(), kN32_SkColorType);

  const uint32_t* packed_pixels =
      reinterpret_cast<uint32_t*>(bitmap.getPixels());
  if (!packed_pixels)
    return false;

  constexpr size_t kMaxColors = 5;
  std::set<uint32_t> colors;
  for (size_t i = 0; i < bitmap.computeByteSize(); i += 4) {
    // Get the color ignoring alpha.
    uint32_t c = SkColorSetA(packed_pixels[i >> 2], 0);
    if (colors.find(c) != colors.end())
      continue;

    colors.insert(c);
    if (colors.size() >= kMaxColors)
      return true;
  }

  return false;
}

}  // namespace

WindowServerTestImpl::WindowServerTestImpl(WindowService* window_service)
    : window_service_(window_service) {}

WindowServerTestImpl::~WindowServerTestImpl() = default;

WindowTree* WindowServerTestImpl::GetWindowTreeWithClientName(
    const std::string& name) {
  for (WindowTree* window_tree : window_service_->window_trees()) {
    if (window_tree->client_name() == name)
      return window_tree;
  }
  return nullptr;
}

void WindowServerTestImpl::OnSurfaceActivated(
    const std::string& desired_client_name,
    EnsureClientHasDrawnWindowCallback cb,
    const std::string& actual_client_name) {
  if (desired_client_name == actual_client_name) {
    RequestWindowContents(desired_client_name, /*retry_count=*/0,
                          std::move(cb));
  } else {
    // No tree with the given name, or it hasn't painted yet. Install a callback
    // for the next time a client creates a CompositorFramesink.
    InstallCallback(desired_client_name, std::move(cb));
  }
}

void WindowServerTestImpl::InstallCallback(
    const std::string& client_name,
    EnsureClientHasDrawnWindowCallback cb) {
  window_service_->SetSurfaceActivationCallback(
      base::BindOnce(&WindowServerTestImpl::OnSurfaceActivated,
                     base::Unretained(this), client_name, std::move(cb)));
}

void WindowServerTestImpl::RequestWindowContents(
    const std::string& client_name,
    int retry_count,
    EnsureClientHasDrawnWindowCallback cb) {
  ClientRoot* client_root = GetWindowTreeWithClientName(client_name)
                                ->GetFirstRootWithCompositorFrameSink();
  if (!client_root || client_root->window()->bounds().IsEmpty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WindowServerTestImpl::RequestWindowContents,
                                  base::Unretained(this), client_name,
                                  retry_count, std::move(cb)));
    return;
  }

  auto copy_output_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&WindowServerTestImpl::OnWindowContentsCaptured,
                     base::Unretained(this), client_name, retry_count,
                     std::move(cb)));
  aura::Window* window = client_root->window();
  copy_output_request->set_area(gfx::Rect(window->bounds().size()));
  window->layer()->RequestCopyOfOutput(std::move(copy_output_request));
}

void WindowServerTestImpl::OnWindowContentsCaptured(
    const std::string& client_name,
    int retry_count,
    EnsureClientHasDrawnWindowCallback cb,
    std::unique_ptr<viz::CopyOutputResult> result) {
  if (!result->IsEmpty() && SanityCheckOutputSkBitmap(result->AsSkBitmap())) {
    std::move(cb).Run(/*success=*/true);
    return;
  }

  // Arbitrary retry params.
  constexpr int kMaxRetry = 3;
  constexpr base::TimeDelta kRetryDelay = base::TimeDelta::FromSeconds(1);

  ++retry_count;
  if (retry_count == kMaxRetry) {
    std::move(cb).Run(/*success=*/false);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WindowServerTestImpl::RequestWindowContents,
                     base::Unretained(this), client_name, retry_count,
                     std::move(cb)),
      kRetryDelay);
}

void WindowServerTestImpl::EnsureClientHasDrawnWindow(
    const std::string& client_name,
    EnsureClientHasDrawnWindowCallback callback) {
  WindowTree* tree = GetWindowTreeWithClientName(client_name);
  if (tree && tree->GetFirstRootWithCompositorFrameSink()) {
    RequestWindowContents(client_name, /*retry_count=*/0, std::move(callback));
    return;
  }
  InstallCallback(client_name, std::move(callback));
}

}  // namespace ws
