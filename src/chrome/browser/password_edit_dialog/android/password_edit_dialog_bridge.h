// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"

namespace content {
class WebContents;
}  // namespace content

// The bridge to invoke password edit dialog in Java from native password
// manager code.
// The native code owns the lifetime of the bridge.
// PasswordEditDialogBridge::Create() creates an instance of the bridge. It can
// return nullptr when WebContents is not attached to any window. Show()
// displays the dialog on the screen.
//
// OnDialogAccepted callback is called when the user accepts saving the
// presented password (by tapping Update button). The callback parameter denotes
// the index of selected username in the list of usernames. The dialog will be
// dismissed after this callback, feature code shouldn't call Dismiss from
// callback implementation to dismiss the dialog.
//
// OnDialogDismissed callback is called when the dialog is dismissed either from
// user interactions or from the call to Dismiss() method. The implementation of
// this callback should destroy the dialog bridge instance.
//
// Here is how typically dialog bridge is created:
//   m_dialog_bridge = PasswordEditDialogBridge::Create(web_contents,
//       base::BindOnce(&OnDialogAccepted),base::BindOnce(&OnDialogDismissed));
//   if (m_dialog_bridge) m_dialog_bridge->Show(...);
//
// The owning class should dismiss displayed dialog during its own destruction:
//   if (m_dialog_bridge) m_dialog_bridge->Dismiss();
//
// Dialog bridge instance should be destroyed in OnDialogDismissed callback:
//   void OnDialogDismissed() {
//     m_dialog_bridge.reset();
//   }
//
// The PasswordEditDialog is the interface created to facilitate mocking in
// tests. PasswordEditDialogBridge contains the implementation.
class PasswordEditDialog {
 public:
  using DialogAcceptedCallback = base::OnceCallback<void(int)>;
  using DialogDismissedCallback = base::OnceCallback<void(bool)>;

  virtual ~PasswordEditDialog();

  // Calls Java side of the bridge to display password edit modal dialog.
  virtual void Show(const std::vector<std::u16string>& usernames,
                    int selected_username_index,
                    const std::u16string& password,
                    const std::u16string& origin,
                    const std::string& account_email) = 0;

  // Dismisses displayed dialog. The owner of PassworDeidtDialogBridge should
  // call this function to correctly dismiss and destroy the dialog. The object
  // can be safely destroyed after dismiss callback is executed.
  virtual void Dismiss() = 0;
};

class PasswordEditDialogBridge : public PasswordEditDialog {
 public:
  ~PasswordEditDialogBridge() override;

  // Creates and returns an instance of PasswordEditDialogBridge and
  // corresponding Java counterpart.
  // Returns nullptr if |web_contents| is not attached to a window.
  static std::unique_ptr<PasswordEditDialog> Create(
      content::WebContents* web_contents,
      DialogAcceptedCallback dialog_accepted_callback,
      DialogDismissedCallback dialog_dismissed_callback);

  // Disallow copy and assign.
  PasswordEditDialogBridge(const PasswordEditDialogBridge&) = delete;
  PasswordEditDialogBridge& operator=(const PasswordEditDialogBridge&) = delete;

  // Calls Java side of the bridge to display password edit modal dialog.
  void Show(const std::vector<std::u16string>& usernames,
            int selected_username_index,
            const std::u16string& password,
            const std::u16string& origin,
            const std::string& account_email) override;

  // Dismisses displayed dialog. The owner of PassworDeidtDialogBridge should
  // call this function to correctly dismiss and destroy the dialog. The object
  // can be safely destroyed after dismiss callback is executed.
  void Dismiss() override;

  // Called from Java to indicate that the user tapped the positive button with
  // |selected_username| being selected from usernames list.
  void OnDialogAccepted(JNIEnv* env, jint selected_username_index);

  // Called from Java when the modal dialog is dismissed.
  void OnDialogDismissed(JNIEnv* env, jboolean dialogAccepted);

 private:
  PasswordEditDialogBridge(
      base::android::ScopedJavaLocalRef<jobject> jwindow_android,
      DialogAcceptedCallback dialog_accepted_callback,
      DialogDismissedCallback dialog_dismissed_callback);

  base::android::ScopedJavaGlobalRef<jobject> java_password_dialog_;
  DialogAcceptedCallback dialog_accepted_callback_;
  DialogDismissedCallback dialog_dismissed_callback_;
};

#endif  // CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_H_
