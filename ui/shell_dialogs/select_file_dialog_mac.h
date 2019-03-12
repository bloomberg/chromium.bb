// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <memory>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

@class ExtensionDropdownHandler;
@class SelectFileDialogDelegate;

namespace ui {

namespace test {
class SelectFileDialogMacTest;
}  // namespace test

// Implementation of SelectFileDialog that shows Cocoa dialogs for choosing a
// file or folder.
// Exported for unit tests.
class SHELL_DIALOGS_EXPORT SelectFileDialogImpl : public ui::SelectFileDialog {
 public:
  SelectFileDialogImpl(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy);

  // BaseShellDialog implementation.
  bool IsRunning(gfx::NativeWindow parent_window) const override;
  void ListenerDestroyed() override;

 protected:
  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 private:
  friend class test::SelectFileDialogMacTest;

  // Callback made when a panel is closed.
  void FileWasSelected(NSSavePanel* dialog,
                       bool is_multi,
                       bool was_cancelled,
                       const std::vector<base::FilePath>& files,
                       int index);

  // Struct to store data associated with a file dialog while it is showing.
  struct DialogData {
    DialogData(gfx::NativeWindow parent_window_, void* params_);
    ~DialogData();

    // The parent window for the panel. Weak, used only for comparisons.
    const gfx::NativeWindow parent_window;

    // |params| user data associated with this file dialog.
    void* const params;

    // The NSSavePanel that |this| tracks.
    base::scoped_nsobject<NSSavePanel> panel;

    // The delegate for |panel|.
    base::scoped_nsobject<SelectFileDialogDelegate> delegate;

    // Extension dropdown handler corresponding to this file dialog.
    base::scoped_nsobject<ExtensionDropdownHandler> extension_dropdown_handler;

   private:
    DISALLOW_COPY_AND_ASSIGN(DialogData);
  };

  ~SelectFileDialogImpl() override;

  // Sets the accessory view for the |dialog| and returns the associated
  // ExtensionDropdownHandler.
  static base::scoped_nsobject<ExtensionDropdownHandler> SetAccessoryView(
      NSSavePanel* dialog,
      const FileTypeInfo* file_types,
      int file_type_index,
      const base::FilePath::StringType& default_extension);

  bool HasMultipleFileTypeChoicesImpl() override;

  // A map from file dialogs to the DialogData associated with them.
  std::map<NSSavePanel*, std::unique_ptr<DialogData>> dialog_data_map_;

  bool hasMultipleFileTypeChoices_;

  base::WeakPtrFactory<SelectFileDialogImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

}  // namespace ui

#endif  //  UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_

