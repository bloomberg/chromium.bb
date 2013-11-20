// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_
#define UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_

#include <Windows.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_export.h"

namespace ui {
namespace internal {
class InputMethodDelegate;
class RemoteInputMethodDelegateWin;
}  // namespace internal

class InputMethod;

// RemoteInputMethodWin is a special implementation of ui::InputMethod that
// works as a proxy of an IME handler running in the metro_driver process.
// RemoteInputMethodWin works as follows.
// - Any action to RemoteInputMethodPrivateWin should be delegated to the
//   metro_driver process via RemoteInputMethodDelegateWin.
// - Data retrieval from RemoteInputMethodPrivateWin is implemented with
//   data cache. Whenever the IME state in the metro_driver process is changed,
//   RemoteRootWindowHostWin, which receives IPCs from metro_driver process,
//   will call RemoteInputMethodPrivateWin::OnCandidatePopupChanged and/or
//   RemoteInputMethodPrivateWin::OnInputSourceChanged accordingly so that
//   the state cache should be updated.
// Caveats: RemoteInputMethodWin does not support InputMethodObserver yet.

// Returns the public interface of RemoteInputMethodWin.
// Caveats: Currently only one instance of RemoteInputMethodWin is able to run
// at the same time.
UI_EXPORT scoped_ptr<InputMethod> CreateRemoteInputMethodWin(
    internal::InputMethodDelegate* delegate);

// Private interface of RemoteInputMethodWin.
class UI_EXPORT RemoteInputMethodPrivateWin {
 public:
  RemoteInputMethodPrivateWin();

  // Returns the private interface of RemoteInputMethodWin when and only when
  // |input_method| is instanciated via CreateRemoteInputMethodWin. Caller does
  // not take the ownership of the returned object.
  // As you might notice, this is yet another reinplementation of dynamic_cast
  // or IUnknown::QueryInterface.
  static RemoteInputMethodPrivateWin* Get(InputMethod* input_method);

  // Installs RemoteInputMethodDelegateWin delegate. Set NULL to |delegate| to
  // unregister.
  virtual void SetRemoteDelegate(
      internal::RemoteInputMethodDelegateWin* delegate) = 0;

  // Updates internal cache so that subsequent calls of
  // RemoteInputMethodWin::IsCandidatePopupOpen can return the correct value
  // based on remote IME activities in the metro_driver process.
  virtual void OnCandidatePopupChanged(bool visible) = 0;

  // Updates internal cache so that subsequent calls of
  // RemoteInputMethodWin::GetInputLocale and
  // RemoteInputMethodWin::GetInputTextDirection can return the correct
  // values based on remote IME activities in the metro_driver process.
  virtual void OnInputSourceChanged(LANGID langid, bool is_ime) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteInputMethodPrivateWin);
};

}  // namespace ui

#endif  // UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_
