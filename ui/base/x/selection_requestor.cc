// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <algorithm>
#include <X11/Xlib.h>

#include "base/run_loop.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

const char* kAtomsToCache[] = {
  kChromeSelection,
  NULL
};

// The amount of time to wait for a request to complete.
const int kRequestTimeoutMs = 300;

}  // namespace

SelectionRequestor::SelectionRequestor(XDisplay* x_display,
                                       XID x_window,
                                       PlatformEventDispatcher* dispatcher)
    : x_display_(x_display),
      x_window_(x_window),
      x_property_(None),
      dispatcher_(dispatcher),
      current_request_index_(0u),
      atom_cache_(x_display_, kAtomsToCache) {
  x_property_ = atom_cache_.GetAtom(kChromeSelection);
}

SelectionRequestor::~SelectionRequestor() {}

bool SelectionRequestor::PerformBlockingConvertSelection(
    XAtom selection,
    XAtom target,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_items,
    XAtom* out_type) {
  base::TimeTicks timeout =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kRequestTimeoutMs);
  Request request(selection, target, timeout);
  requests_.push_back(&request);
  if (requests_.size() == 1u)
    ConvertSelectionForCurrentRequest();
  BlockTillSelectionNotifyForRequest(&request);

  std::vector<Request*>::iterator request_it = std::find(
      requests_.begin(), requests_.end(), &request);
  CHECK(request_it != requests_.end());
  if (static_cast<int>(current_request_index_) >
      request_it - requests_.begin()) {
    --current_request_index_;
  }
  requests_.erase(request_it);

  if (requests_.empty())
    abort_timer_.Stop();

  if (request.success) {
    if (out_data)
      *out_data = request.out_data;
    if (out_data_items)
      *out_data_items = request.out_data_items;
    if (out_type)
      *out_type = request.out_type;
  }
  return request.success;
}

void SelectionRequestor::PerformBlockingConvertSelectionWithParameter(
    XAtom selection,
    XAtom target,
    const std::vector<XAtom>& parameter) {
  SetAtomArrayProperty(x_window_, kChromeSelection, "ATOM", parameter);
  PerformBlockingConvertSelection(selection, target, NULL, NULL, NULL);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    XAtom selection,
    const std::vector<XAtom>& types) {
  for (std::vector<XAtom>::const_iterator it = types.begin();
       it != types.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> data;
    XAtom type = None;
    if (PerformBlockingConvertSelection(selection,
                                        *it,
                                        &data,
                                        NULL,
                                        &type) &&
        type == *it) {
      return SelectionData(type, data);
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(const XEvent& event) {
  Request* request = GetCurrentRequest();
  XAtom event_property = event.xselection.property;
  if (!request ||
      request->completed ||
      request->selection != event.xselection.selection ||
      request->target != event.xselection.target) {
    // ICCCM requires us to delete the property passed into SelectionNotify.
    if (event_property != None)
      XDeleteProperty(x_display_, x_window_, event_property);
    return;
  }

  request->success = false;
  if (event_property == x_property_) {
    request->success = ui::GetRawBytesOfProperty(x_window_,
                                                 x_property_,
                                                 &request->out_data,
                                                 &request->out_data_items,
                                                 &request->out_type);
  }
  if (event_property != None)
    XDeleteProperty(x_display_, x_window_, event_property);

  CompleteRequest(current_request_index_);
}

void SelectionRequestor::AbortStaleRequests() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (size_t i = current_request_index_;
       i < requests_.size() && requests_[i]->timeout <= now;
       ++i) {
    CompleteRequest(i);
  }
}

void SelectionRequestor::CompleteRequest(size_t index) {
   if (index >= requests_.size())
     return;

  Request* request = requests_[index];
  if (request->completed)
    return;
  request->completed = true;

  if (index == current_request_index_) {
    ++current_request_index_;
    ConvertSelectionForCurrentRequest();
  }

  if (!request->quit_closure.is_null())
    request->quit_closure.Run();
}

void SelectionRequestor::ConvertSelectionForCurrentRequest() {
  Request* request = GetCurrentRequest();
  if (request) {
    XConvertSelection(x_display_,
                      request->selection,
                      request->target,
                      x_property_,
                      x_window_,
                      CurrentTime);
  }
}

void SelectionRequestor::BlockTillSelectionNotifyForRequest(Request* request) {
  if (PlatformEventSource::GetInstance()) {
    if (!abort_timer_.IsRunning()) {
      abort_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kRequestTimeoutMs),
                         this,
                         &SelectionRequestor::AbortStaleRequests);
    }

    base::MessageLoop::ScopedNestableTaskAllower allow_nested(
        base::MessageLoopForUI::current());
    base::RunLoop run_loop;
    request->quit_closure = run_loop.QuitClosure();
    run_loop.Run();

    // We cannot put logic to process the next request here because the RunLoop
    // might be nested. For instance, request 'B' may start a RunLoop while the
    // RunLoop for request 'A' is running. It is not possible to end the RunLoop
    // for request 'A' without first ending the RunLoop for request 'B'.
  } else {
    // This occurs if PerformBlockingConvertSelection() is called during
    // shutdown and the PlatformEventSource has already been destroyed.
    while (!request->completed &&
           request->timeout > base::TimeTicks::Now()) {
      if (XPending(x_display_)) {
        XEvent event;
        XNextEvent(x_display_, &event);
        dispatcher_->DispatchEvent(&event);
      }
    }
  }
}

SelectionRequestor::Request* SelectionRequestor::GetCurrentRequest() {
  return current_request_index_ == requests_.size() ?
      NULL : requests_[current_request_index_];
}

SelectionRequestor::Request::Request(XAtom selection,
                                     XAtom target,
                                     base::TimeTicks timeout)
    : selection(selection),
      target(target),
      out_data_items(0u),
      out_type(None),
      success(false),
      timeout(timeout),
      completed(false) {
}

SelectionRequestor::Request::~Request() {
}

}  // namespace ui
