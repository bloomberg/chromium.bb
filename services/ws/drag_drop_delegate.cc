// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/drag_drop_delegate.h"

#include "base/bind.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/window_tree.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace ws {

namespace {

// Converts OSExchangeData into mime type data.
using DragDataType = aura::OSExchangeDataProviderMus::Data;
DragDataType GetDragData(const ui::OSExchangeData& data) {
  aura::OSExchangeDataProviderMus mus_provider;

  base::string16 string;
  if (data.GetString(&string))
    mus_provider.SetString(string);

  GURL url;
  base::string16 title;
  if (data.GetURLAndTitle(ui::OSExchangeData::DO_NOT_CONVERT_FILENAMES, &url,
                          &title)) {
    mus_provider.SetURL(url, title);
  }

  std::vector<ui::FileInfo> file_names;
  if (data.GetFilenames(&file_names))
    mus_provider.SetFilenames(file_names);

  base::string16 html;
  GURL base_url;
  if (data.GetHtml(&html, &base_url))
    mus_provider.SetHtml(html, base_url);

  return mus_provider.GetData();
}

}  // namespace

DragDropDelegate::DragDropDelegate(WindowTree* window_tree,
                                   mojom::WindowTreeClient* window_tree_client,
                                   aura::Window* window,
                                   Id transport_window_id)
    : window_tree_(window_tree),
      tree_client_(window_tree_client),
      window_(window),
      transport_window_id_(transport_window_id) {}

DragDropDelegate::~DragDropDelegate() {
  if (in_drag_)
    EndDrag();
}

void DragDropDelegate::OnDragEntered(const ui::DropTargetEvent& event) {
  StartDrag(event);

  tree_client_->OnDragEnter(
      transport_window_id_, event.flags(), GetRootLocation(event),
      event.location_f(), event.source_operations(),
      base::BindOnce(&DragDropDelegate::UpdateDragOperations,
                     weak_ptr_factory_.GetWeakPtr()));
}

int DragDropDelegate::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(in_drag_);

  tree_client_->OnDragOver(
      transport_window_id_, event.flags(), GetRootLocation(event),
      event.location_f(), event.source_operations(),
      base::BindOnce(&DragDropDelegate::UpdateDragOperations,
                     weak_ptr_factory_.GetWeakPtr()));
  return last_drag_operations_;
}

void DragDropDelegate::OnDragExited() {
  DCHECK(in_drag_);

  tree_client_->OnDragLeave(transport_window_id_);
  EndDrag();
}

int DragDropDelegate::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(in_drag_);

  tree_client_->OnCompleteDrop(transport_window_id_, event.flags(),
                               GetRootLocation(event), event.location_f(),
                               event.source_operations(), base::DoNothing());

  EndDrag();

  // Returns a drop action derived from |last_drag_operations_| because it is
  // not safe to hold the stack and wait for the mojo to return the actual one.
  if (last_drag_operations_ == ui::DragDropTypes::DRAG_NONE)
    return ui::DragDropTypes::DRAG_NONE;

  return (last_drag_operations_ & ui::DragDropTypes::DRAG_MOVE)
             ? ui::DragDropTypes::DRAG_MOVE
             : ui::DragDropTypes::DRAG_COPY;
}

void DragDropDelegate::StartDrag(const ui::DropTargetEvent& event) {
  DCHECK(!in_drag_);

  in_drag_ = true;
  tree_client_->OnDragDropStart(mojo::MapToFlatMap(GetDragData(event.data())));
}

void DragDropDelegate::EndDrag() {
  DCHECK(in_drag_);

  in_drag_ = false;
  tree_client_->OnDragDropDone();
}

void DragDropDelegate::UpdateDragOperations(uint32_t drag_operations) {
  last_drag_operations_ = drag_operations;
}

gfx::PointF DragDropDelegate::GetRootLocation(
    const ui::DropTargetEvent& event) {
  return window_tree_->ConvertRootLocationForClient(window_,
                                                    event.root_location_f());
}

}  // namespace ws
