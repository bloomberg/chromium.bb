// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_mac.h"

#include <CoreServices/CoreServices.h>
#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/remote_cocoa/browser/window.h"
#import "ui/base/cocoa/controls/textfield_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const int kFileTypePopupTag = 1234;

CFStringRef CreateUTIFromExtension(const base::FilePath::StringType& ext) {
  base::ScopedCFTypeRef<CFStringRef> ext_cf(base::SysUTF8ToCFStringRef(ext));
  return UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassFilenameExtension, ext_cf.get(), NULL);
}

NSString* GetDescriptionFromExtension(const base::FilePath::StringType& ext) {
  base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
  base::ScopedCFTypeRef<CFStringRef> description(
      UTTypeCopyDescription(uti.get()));

  if (description && CFStringGetLength(description))
    return [[base::mac::CFToNSCast(description.get()) retain] autorelease];

  // In case no description is found, create a description based on the
  // unknown extension type (i.e. if the extension is .qqq, the we create
  // a description "QQQ File (.qqq)").
  base::string16 ext_name = base::UTF8ToUTF16(ext);
  return l10n_util::GetNSStringF(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                 base::i18n::ToUpper(ext_name), ext_name);
}

base::scoped_nsobject<NSView> CreateAccessoryView() {
  static constexpr CGFloat kControlPadding = 2;

  base::scoped_nsobject<NSView> view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 350, 60)]);

  // Create the label and center it vertically.
  NSTextField* label = [TextFieldUtils
      labelWithString:l10n_util::GetNSString(
                          IDS_SAVE_PAGE_FILE_FORMAT_PROMPT_MAC)];
  [label sizeToFit];
  NSRect label_frame = [label frame];
  label_frame.origin =
      NSMakePoint(kControlPadding, NSMidY([view frame]) - NSMidY(label_frame));
  [label setFrame:label_frame];
  [view addSubview:label];

  // Create the pop-up button, positioning it to the right of the label.
  // Its X position needs to be slightly below the label's, so that the text
  // baselines are aligned.
  base::scoped_nsobject<NSPopUpButton> pop_up_button([[NSPopUpButton alloc]
      initWithFrame:NSMakeRect(NSWidth(label_frame) + kControlPadding,
                               NSMinY(label_frame) - 5, 230, 25)
          pullsDown:NO]);
  [pop_up_button setTag:kFileTypePopupTag];
  [view addSubview:pop_up_button.get()];

  // Resize the containing view to fit the controls.
  CGFloat total_width = NSMaxX([pop_up_button frame]);
  NSRect view_frame = [view frame];
  view_frame.size.width = total_width + kControlPadding;
  [view setFrame:view_frame];

  return view;
}

}  // namespace

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogDelegate : NSObject <NSOpenSavePanelDelegate>
@end

// Target for NSPopupButton control in file dialog's accessory view.
@interface ExtensionDropdownHandler : NSObject {
 @private
  // The file dialog to which this target object corresponds. Weak reference
  // since the dialog_ will stay alive longer than this object.
  NSSavePanel* dialog_;

  // An array whose each item corresponds to an array of different extensions in
  // an extension group.
  base::scoped_nsobject<NSArray> fileTypeLists_;
}

- (id)initWithDialog:(NSSavePanel*)dialog fileTypeLists:(NSArray*)fileTypeLists;

- (void)popupAction:(id)sender;
@end

namespace ui {

using Type = SelectFileDialog::Type;
using FileTypeInfo = SelectFileDialog::FileTypeInfo;

SavePanelBridge::SavePanelBridge(PanelEndedCallback callback)
    : callback_(std::move(callback)), weak_factory_(this) {}

SavePanelBridge::~SavePanelBridge() {
  // If we never executed our callback, then the panel never closed. Cancel it
  // now.
  if (callback_)
    [panel_ cancel:panel_];

  // Balance the setDelegate called during Initialize.
  [panel_ setDelegate:nil];
}

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)), weak_factory_(this) {}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  for (const auto& dialog_data : dialog_data_list_) {
    if (dialog_data.parent_window == parent_window)
      return true;
  }
  return false;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::FileWasSelected(
    DialogData* dialog_data,
    bool is_multi,
    bool was_cancelled,
    const std::vector<base::FilePath>& files,
    int index) {
  auto it = std::find_if(
      dialog_data_list_.begin(), dialog_data_list_.end(),
      [dialog_data](const DialogData& d) { return &d == dialog_data; });
  DCHECK(it != dialog_data_list_.end());
  void* params = dialog_data->params;
  dialog_data_list_.erase(it);

  if (!listener_)
    return;

  if (was_cancelled || files.empty()) {
    listener_->FileSelectionCanceled(params);
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(files, params);
    } else {
      listener_->FileSelected(files[0], index, params);
    }
  }
}

void SelectFileDialogImpl::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_native_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER ||
         type == SELECT_EXISTING_FOLDER || type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE || type == SELECT_SAVEAS_FILE);
  NSWindow* owning_window = owning_native_window.GetNativeNSWindow();
  // TODO(https://crbug.com/913303): The select file dialog's interface to
  // Cocoa should be wrapped in a mojo interface in order to allow instantiating
  // across processes. As a temporary solution, raise the remote windows'
  // transparent in-process window to the front.
  if (remote_cocoa::IsWindowRemote(owning_window)) {
    [owning_window makeKeyAndOrderFront:nil];
    [owning_window setLevel:NSModalPanelWindowLevel];
  }

  hasMultipleFileTypeChoices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  // Add a new DialogData to the list.
  dialog_data_list_.emplace_back(owning_window, params);
  DialogData& dialog_data = dialog_data_list_.back();

  // Create a NSSavePanel for it. Note that it is safe to pass |dialog_data| by
  // a pointer because it will only be removed from the list when the callback
  // is made or after the callback has been cancelled by |weak_factory_|.
  dialog_data.save_panel_bridge =
      std::make_unique<SavePanelBridge>(base::BindOnce(
          &SelectFileDialogImpl::FileWasSelected, weak_factory_.GetWeakPtr(),
          &dialog_data, type == SELECT_OPEN_MULTI_FILE));
  dialog_data.save_panel_bridge->Initialize(type, owning_window, title,
                                            default_path, file_types,
                                            file_type_index, default_extension);
}

void SavePanelBridge::Initialize(
    Type type,
    NSWindow* owning_window,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension) {
  type_ = type;
  // Note: we need to retain the dialog as |owning_window| can be null.
  // (See http://crbug.com/29213 .)
  if (type_ == SelectFileDialog::SELECT_SAVEAS_FILE)
    panel_.reset([[NSSavePanel savePanel] retain]);
  else
    panel_.reset([[NSOpenPanel openPanel] retain]);
  NSSavePanel* dialog = panel_.get();

  if (!title.empty())
    [dialog setMessage:base::SysUTF16ToNSString(title)];

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    // The file dialog is going to do a ton of stats anyway. Not much
    // point in eliminating this one.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (base::DirectoryExists(default_path)) {
      default_dir = base::SysUTF8ToNSString(default_path.value());
    } else {
      default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
      default_filename =
          base::SysUTF8ToNSString(default_path.BaseName().value());
    }
  }

  if (type_ != SelectFileDialog::SELECT_FOLDER &&
      type_ != SelectFileDialog::SELECT_UPLOAD_FOLDER &&
      type_ != SelectFileDialog::SELECT_EXISTING_FOLDER) {
    if (file_types) {
      SetAccessoryView(file_types, file_type_index, default_extension);
    } else {
      // If no type_ info is specified, anything goes.
      [dialog setAllowsOtherFileTypes:YES];
    }
  }

  if (type_ == SelectFileDialog::SELECT_SAVEAS_FILE) {
    // When file extensions are hidden and removing the extension from
    // the default filename gives one which still has an extension
    // that OS X recognizes, it will get confused and think the user
    // is trying to override the default extension. This happens with
    // filenames like "foo.tar.gz" or "ball.of.tar.png". Work around
    // this by never hiding extensions in that case.
    base::FilePath::StringType penultimate_extension =
        default_path.RemoveFinalExtension().FinalExtension();
    if (!penultimate_extension.empty()) {
      [dialog setExtensionHidden:NO];
    } else {
      [dialog setExtensionHidden:YES];
      [dialog setCanSelectHiddenExtension:YES];
    }
  } else {
    NSOpenPanel* open_dialog = base::mac::ObjCCastStrict<NSOpenPanel>(dialog);

    if (type_ == SelectFileDialog::SELECT_OPEN_MULTI_FILE)
      [open_dialog setAllowsMultipleSelection:YES];
    else
      [open_dialog setAllowsMultipleSelection:NO];

    if (type_ == SelectFileDialog::SELECT_FOLDER ||
        type_ == SelectFileDialog::SELECT_UPLOAD_FOLDER ||
        type_ == SelectFileDialog::SELECT_EXISTING_FOLDER) {
      [open_dialog setCanChooseFiles:NO];
      [open_dialog setCanChooseDirectories:YES];

      if (type_ == SelectFileDialog::SELECT_FOLDER)
        [open_dialog setCanCreateDirectories:YES];
      else
        [open_dialog setCanCreateDirectories:NO];

      NSString* prompt =
          (type_ == SelectFileDialog::SELECT_UPLOAD_FOLDER)
              ? l10n_util::GetNSString(IDS_SELECT_UPLOAD_FOLDER_BUTTON_TITLE)
              : l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      [open_dialog setPrompt:prompt];
    } else {
      [open_dialog setCanChooseFiles:YES];
      [open_dialog setCanChooseDirectories:NO];
    }

    delegate_.reset([[SelectFileDialogDelegate alloc] init]);
    [open_dialog setDelegate:delegate_.get()];
  }
  if (default_dir)
    [dialog setDirectoryURL:[NSURL fileURLWithPath:default_dir]];
  if (default_filename)
    [dialog setNameFieldStringValue:default_filename];

  // Ensure that |callback| (rather than |this|) be retained by the block.
  auto callback = base::BindRepeating(&SavePanelBridge::OnPanelEnded,
                                      weak_factory_.GetWeakPtr());
  [dialog beginSheetModalForWindow:owning_window
                 completionHandler:^(NSInteger result) {
                   callback.Run(result != NSFileHandlingPanelOKButton);
                 }];
}

SelectFileDialogImpl::DialogData::DialogData(gfx::NativeWindow parent_window_,
                                             void* params_)
    : parent_window(parent_window_), params(params_) {}

SelectFileDialogImpl::DialogData::~DialogData() {}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Clear |weak_factory_| beforehand, to ensure that no callbacks will be made
  // when we cancel the NSSavePanels.
  weak_factory_.InvalidateWeakPtrs();

  // Walk through the open dialogs and issue the cancel callbacks that would
  // have been made.
  for (const auto& dialog_data : dialog_data_list_) {
    if (listener_)
      listener_->FileSelectionCanceled(dialog_data.params);
  }

  // Cancel the NSSavePanels be destroying their bridges.
  dialog_data_list_.clear();
}

void SavePanelBridge::SetAccessoryView(
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension) {
  DCHECK(file_types);
  base::scoped_nsobject<NSView> accessory_view = CreateAccessoryView();
  NSSavePanel* dialog = panel_.get();
  [dialog setAccessoryView:accessory_view.get()];

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  // Create an array with each item corresponding to an array of different
  // extensions in an extension group.
  NSMutableArray* file_type_lists = [NSMutableArray array];
  int default_extension_index = -1;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<base::FilePath::StringType>& ext_list =
        file_types->extensions[i];

    // Generate type description for the extension group.
    NSString* type_description = nil;
    if (i < file_types->extension_description_overrides.size() &&
        !file_types->extension_description_overrides[i].empty()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[i]);
    } else {
      // No description given for a list of extensions; pick the first one
      // from the list (arbitrarily) and use its description.
      DCHECK(!ext_list.empty());
      type_description = GetDescriptionFromExtension(ext_list[0]);
    }
    DCHECK_NE(0u, [type_description length]);
    [popup addItemWithTitle:type_description];

    // Populate file_type_lists.
    // Set to store different extensions in the current extension group.
    NSMutableSet* file_type_set = [NSMutableSet set];
    for (const base::FilePath::StringType& ext : ext_list) {
      if (ext == default_extension)
        default_extension_index = i;

      // Crash reports suggest that CreateUTIFromExtension may return nil. Hence
      // we nil check before adding to |file_type_set|. See crbug.com/630101 and
      // rdar://27490414.
      base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
      if (uti)
        [file_type_set addObject:base::mac::CFToNSCast(uti.get())];

      // Always allow the extension itself, in case the UTI doesn't map
      // back to the original extension correctly. This occurs with dynamic
      // UTIs on 10.7 and 10.8.
      // See http://crbug.com/148840, http://openradar.me/12316273
      base::ScopedCFTypeRef<CFStringRef> ext_cf(
          base::SysUTF8ToCFStringRef(ext));
      [file_type_set addObject:base::mac::CFToNSCast(ext_cf.get())];
    }
    [file_type_lists addObject:[file_type_set allObjects]];
  }

  if (file_types->include_all_files || file_types->extensions.empty()) {
    [popup addItemWithTitle:l10n_util::GetNSString(IDS_APP_SAVEAS_ALL_FILES)];
    [dialog setAllowsOtherFileTypes:YES];
  }

  extension_dropdown_handler_.reset([[ExtensionDropdownHandler alloc]
      initWithDialog:dialog
       fileTypeLists:file_type_lists]);

  // This establishes a weak reference to handler. Hence we persist it as part
  // of dialog_data_list_.
  [popup setTarget:extension_dropdown_handler_];
  [popup setAction:@selector(popupAction:)];

  // file_type_index uses 1 based indexing.
  if (file_type_index) {
    DCHECK_LE(static_cast<size_t>(file_type_index),
              file_types->extensions.size());
    DCHECK_GE(file_type_index, 1);
    [popup selectItemAtIndex:file_type_index - 1];
    [extension_dropdown_handler_ popupAction:popup];
  } else if (!default_extension.empty() && default_extension_index != -1) {
    [popup selectItemAtIndex:default_extension_index];
    [dialog
        setAllowedFileTypes:@[ base::SysUTF8ToNSString(default_extension) ]];
  } else {
    // Select the first item.
    [popup selectItemAtIndex:0];
    [extension_dropdown_handler_ popupAction:popup];
  }
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return hasMultipleFileTypeChoices_;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return new SelectFileDialogImpl(listener, std::move(policy));
}

void SavePanelBridge::OnPanelEnded(bool did_cancel) {
  if (!callback_)
    return;

  int index = 0;
  std::vector<base::FilePath> paths;
  if (!did_cancel) {
    if (type_ == SelectFileDialog::SELECT_SAVEAS_FILE) {
      if ([[panel_ URL] isFileURL]) {
        paths.push_back(base::mac::NSStringToFilePath([[panel_ URL] path]));
      }

      NSView* accessoryView = [panel_ accessoryView];
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = [popup indexOfSelectedItem] + 1;
        }
      } else {
        index = 1;
      }
    } else {
      CHECK([panel_ isKindOfClass:[NSOpenPanel class]]);
      NSArray* urls = [static_cast<NSOpenPanel*>(panel_) URLs];
      for (NSURL* url in urls)
        if ([url isFileURL])
          paths.push_back(base::FilePath(base::SysNSStringToUTF8([url path])));
    }
  }

  std::move(callback_).Run(did_cancel, paths, index);
}

}  // namespace ui

@implementation SelectFileDialogDelegate

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url {
  return [url isFileURL];
}

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at insecure content.
  // See https://crbug.com/637098.
  if ([[NSApp currentEvent] type] == NSKeyDown &&
      [[NSApp currentEvent] isARepeat]) {
    return NO;
  }

  return YES;
}

@end

@implementation ExtensionDropdownHandler

- (id)initWithDialog:(NSSavePanel*)dialog
       fileTypeLists:(NSArray*)fileTypeLists {
  if ((self = [super init])) {
    dialog_ = dialog;
    fileTypeLists_.reset([fileTypeLists retain]);
  }
  return self;
}

- (void)popupAction:(id)sender {
  NSUInteger index = [sender indexOfSelectedItem];
  if (index < [fileTypeLists_ count]) {
    // For save dialogs, this causes the first item in the allowedFileTypes
    // array to be used as the extension for the save panel.
    [dialog_ setAllowedFileTypes:[fileTypeLists_ objectAtIndex:index]];
  } else {
    // The user selected "All files" option.
    [dialog_ setAllowedFileTypes:nil];
  }
}

@end
