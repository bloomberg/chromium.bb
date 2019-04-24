// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

WebUIGraphDumpImpl::WebUIGraphDumpImpl(Graph* graph)
    : graph_(graph), binding_(this) {
  DCHECK(graph);
}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {}

void WebUIGraphDumpImpl::Bind(
    resource_coordinator::mojom::WebUIGraphDumpRequest request,
    base::OnceClosure error_handler) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(std::move(error_handler));
}

void WebUIGraphDumpImpl::GetCurrentGraph(GetCurrentGraphCallback callback) {
  resource_coordinator::mojom::WebUIGraphPtr graph =
      resource_coordinator::mojom::WebUIGraph::New();

  {
    auto processes = graph_->GetAllProcessNodes();
    graph->processes.reserve(processes.size());
    for (auto* process : processes) {
      resource_coordinator::mojom::WebUIProcessInfoPtr process_info =
          resource_coordinator::mojom::WebUIProcessInfo::New();

      process_info->id = process->id().id;
      process_info->pid = process->process_id();
      process_info->cumulative_cpu_usage = process->cumulative_cpu_usage();
      process_info->private_footprint_kb = process->private_footprint_kb();

      graph->processes.push_back(std::move(process_info));
    }
  }

  {
    auto frames = graph_->GetAllFrameNodes();
    graph->frames.reserve(frames.size());
    for (auto* frame : frames) {
      resource_coordinator::mojom::WebUIFrameInfoPtr frame_info =
          resource_coordinator::mojom::WebUIFrameInfo::New();

      frame_info->id = frame->id().id;

      auto* parent_frame = frame->GetParentFrameNode();
      frame_info->parent_frame_id = parent_frame ? parent_frame->id().id : 0;

      auto* process = frame->GetProcessNode();
      frame_info->process_id = process ? process->id().id : 0;

      graph->frames.push_back(std::move(frame_info));
    }
  }

  {
    auto pages = graph_->GetAllPageNodes();
    graph->pages.reserve(pages.size());
    for (auto* page : pages) {
      resource_coordinator::mojom::WebUIPageInfoPtr page_info =
          resource_coordinator::mojom::WebUIPageInfo::New();

      page_info->id = page->id().id;
      page_info->main_frame_url = page->main_frame_url();

      auto* main_frame = page->GetMainFrameNode();
      page_info->main_frame_id = main_frame ? main_frame->id().id : 0;

      graph->pages.push_back(std::move(page_info));
    }
  }
  std::move(callback).Run(std::move(graph));
}

}  // namespace performance_manager
