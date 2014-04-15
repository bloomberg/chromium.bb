// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include "base/run_loop.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

const char* kAtomsToCache[] = {
  kChromeSelection,
  NULL
};

}  // namespace

SelectionRequestor::SelectionRequestor(Display* x_display,
                                       Window x_window,
                                       Atom selection_name)
    : x_display_(x_display),
      x_window_(x_window),
      selection_name_(selection_name),
      atom_cache_(x_display_, kAtomsToCache) {
}

SelectionRequestor::~SelectionRequestor() {}

bool SelectionRequestor::PerformBlockingConvertSelection(
    Atom target,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_bytes,
    size_t* out_data_items,
    Atom* out_type) {
  // The name of the property we're asking to be set on |x_window_|.
  Atom property_to_set = atom_cache_.GetAtom(kChromeSelection);

  XConvertSelection(x_display_,
                    selection_name_,
                    target,
                    property_to_set,
                    x_window_,
                    CurrentTime);

  // Now that we've thrown our message off to the X11 server, we block waiting
  // for a response.
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;

  // Stop waiting for a response after a certain amount of time.
  const int kMaxWaitTimeForClipboardResponse = 300;
  loop->PostDelayedTask(
      FROM_HERE,
      run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(kMaxWaitTimeForClipboardResponse));

  PendingRequest pending_request(target, run_loop.QuitClosure());
  pending_requests_.push_back(&pending_request);
  run_loop.Run();
  DCHECK(!pending_requests_.empty());
  DCHECK_EQ(&pending_request, pending_requests_.back());
  pending_requests_.pop_back();

  if (pending_request.returned_property != property_to_set)
    return false;

  return ui::GetRawBytesOfProperty(x_window_, pending_request.returned_property,
                                   out_data, out_data_bytes, out_data_items,
                                   out_type);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    const std::vector< ::Atom>& types) {
  for (std::vector< ::Atom>::const_iterator it = types.begin();
       it != types.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> data;
    size_t data_bytes = 0;
    ::Atom type = None;
    if (PerformBlockingConvertSelection(*it,
                                        &data,
                                        &data_bytes,
                                        NULL,
                                        &type) &&
        type == *it) {
      return SelectionData(type, data);
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(const XSelectionEvent& event) {
  // Find the PendingRequest for the corresponding XConvertSelection call. If
  // there are multiple pending requests on the same target, satisfy them in
  // FIFO order.
  PendingRequest* request_notified = NULL;
  if (selection_name_ == event.selection) {
    for (std::list<PendingRequest*>::iterator iter = pending_requests_.begin();
         iter != pending_requests_.end(); ++iter) {
      PendingRequest* request = *iter;
      if (request->returned)
        continue;
      if (request->target != event.target)
        continue;
      request_notified = request;
      break;
    }
  }

  // This event doesn't correspond to any XConvertSelection calls that we
  // issued in PerformBlockingConvertSelection. This shouldn't happen, but any
  // client can send any message, so it can happen.
  if (!request_notified)
    return;

  request_notified->returned_property = event.property;
  request_notified->returned = true;
  request_notified->quit_closure.Run();
}

SelectionRequestor::PendingRequest::PendingRequest(Atom target,
                                                   base::Closure quit_closure)
    : target(target),
      quit_closure(quit_closure),
      returned_property(None),
      returned(false) {
}

SelectionRequestor::PendingRequest::~PendingRequest() {
}

}  // namespace ui
