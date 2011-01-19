// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_source.h"

namespace ui {

DragSource::DragSource() : cancel_drag_(false) {
}

HRESULT DragSource::QueryContinueDrag(BOOL escape_pressed, DWORD key_state) {
  if (cancel_drag_)
    return DRAGDROP_S_CANCEL;

  if (escape_pressed) {
    OnDragSourceCancel();
    return DRAGDROP_S_CANCEL;
  }

  if (!(key_state & MK_LBUTTON)) {
    OnDragSourceDrop();
    return DRAGDROP_S_DROP;
  }

  OnDragSourceMove();
  return S_OK;
}

HRESULT DragSource::GiveFeedback(DWORD effect) {
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

HRESULT DragSource::QueryInterface(const IID& iid, void** object) {
  *object = NULL;
  if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDropSource)) {
    *object = this;
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG DragSource::AddRef() {
  base::RefCountedThreadSafe<DragSource>::AddRef();
  return 0;
}

ULONG DragSource::Release() {
  base::RefCountedThreadSafe<DragSource>::Release();
  return 0;
}

}  // namespace ui
