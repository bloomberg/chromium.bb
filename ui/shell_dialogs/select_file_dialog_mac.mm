// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#import <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>
#include <stddef.h>

#include <map>
#include <set>
#include <vector>

#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#import "ui/base/cocoa/nib_loading.h"
#include "ui/base/l10n/l10n_util_mac.h"
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

}  // namespace

class SelectFileDialogImpl;

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogBridge : NSObject<NSOpenSavePanelDelegate> {
 @private
  SelectFileDialogImpl* selectFileDialogImpl_;  // WEAK; owns us
}

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s;
- (void)endedPanel:(NSSavePanel*)panel
         didCancel:(bool)did_cancel
              type:(ui::SelectFileDialog::Type)type
      parentWindow:(NSWindow*)parentWindow;

// NSSavePanel delegate method
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url;

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

// Implementation of SelectFileDialog that shows Cocoa dialogs for choosing a
// file or folder.
class SelectFileDialogImpl : public ui::SelectFileDialog {
 public:
  explicit SelectFileDialogImpl(Listener* listener,
                                ui::SelectFilePolicy* policy);

  // BaseShellDialog implementation.
  bool IsRunning(gfx::NativeWindow parent_window) const override;
  void ListenerDestroyed() override;

  // Callback from ObjC bridge.
  void FileWasSelected(NSSavePanel* dialog,
                       NSWindow* parent_window,
                       bool was_cancelled,
                       bool is_multi,
                       const std::vector<base::FilePath>& files,
                       int index);

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
  // Struct to store data associated with a file dialog while it is showing.
  struct DialogData {
    DialogData(void* params_,
               base::scoped_nsobject<ExtensionDropdownHandler> handler)
        : params(params_), extension_dropdown_handler(handler) {}

    // |params| user data associated with this file dialog.
    void* params;

    // Extension dropdown handler corresponding to this file dialog.
    base::scoped_nsobject<ExtensionDropdownHandler> extension_dropdown_handler;
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

  // The bridge for results from Cocoa to return to us.
  base::scoped_nsobject<SelectFileDialogBridge> bridge_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<NSWindow*> parents_;

  // A map from file dialogs to the DialogData associated with them.
  std::map<NSSavePanel*, DialogData> dialog_data_map_;

  bool hasMultipleFileTypeChoices_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener,
                                           ui::SelectFilePolicy* policy)
    : SelectFileDialog(listener, policy),
      bridge_([[SelectFileDialogBridge alloc]
               initWithSelectFileDialogImpl:this]) {
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::FileWasSelected(
    NSSavePanel* dialog,
    NSWindow* parent_window,
    bool was_cancelled,
    bool is_multi,
    const std::vector<base::FilePath>& files,
    int index) {
  parents_.erase(parent_window);

  auto it = dialog_data_map_.find(dialog);
  DCHECK(it != dialog_data_map_.end());
  void* params = it->second.params;
  dialog_data_map_.erase(it);

  [dialog setDelegate:nil];

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
    gfx::NativeWindow owning_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER ||
         type == SELECT_UPLOAD_FOLDER ||
         type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE ||
         type == SELECT_SAVEAS_FILE);
  parents_.insert(owning_window);

  // Note: we need to retain the dialog as owning_window can be null.
  // (See http://crbug.com/29213 .)
  NSSavePanel* dialog;
  if (type == SELECT_SAVEAS_FILE)
    dialog = [[NSSavePanel savePanel] retain];
  else
    dialog = [[NSOpenPanel openPanel] retain];

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

  base::scoped_nsobject<ExtensionDropdownHandler> handler;
  if (type != SELECT_FOLDER && type != SELECT_UPLOAD_FOLDER) {
    if (file_types) {
      handler = SelectFileDialogImpl::SetAccessoryView(
          dialog, file_types, file_type_index, default_extension);
    } else {
      // If no type info is specified, anything goes.
      [dialog setAllowsOtherFileTypes:YES];
    }
  }

  auto inserted = dialog_data_map_.insert(
      std::make_pair(dialog, DialogData(params, handler)));
  DCHECK(inserted.second);  // Dialog should never exist already.

  hasMultipleFileTypeChoices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  if (type == SELECT_SAVEAS_FILE) {
    // When file extensions are hidden and removing the extension from
    // the default filename gives one which still has an extension
    // that OS X recognizes, it will get confused and think the user
    // is trying to override the default extension. This happens with
    // filenames like "foo.tar.gz" or "ball.of.tar.png". Work around
    // this by never hiding extensions in that case.
    base::FilePath::StringType penultimate_extension =
        default_path.RemoveFinalExtension().FinalExtension();
    if (!penultimate_extension.empty() &&
        penultimate_extension.length() <= 5U) {
      [dialog setExtensionHidden:NO];
    } else {
      [dialog setCanSelectHiddenExtension:YES];
    }
  } else {
    NSOpenPanel* open_dialog = (NSOpenPanel*)dialog;

    if (type == SELECT_OPEN_MULTI_FILE)
      [open_dialog setAllowsMultipleSelection:YES];
    else
      [open_dialog setAllowsMultipleSelection:NO];

    if (type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER) {
      [open_dialog setCanChooseFiles:NO];
      [open_dialog setCanChooseDirectories:YES];
      [open_dialog setCanCreateDirectories:YES];
      NSString *prompt = (type == SELECT_UPLOAD_FOLDER)
          ? l10n_util::GetNSString(IDS_SELECT_UPLOAD_FOLDER_BUTTON_TITLE)
          : l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      [open_dialog setPrompt:prompt];
    } else {
      [open_dialog setCanChooseFiles:YES];
      [open_dialog setCanChooseDirectories:NO];
    }

    [open_dialog setDelegate:bridge_.get()];
  }
  if (default_dir)
    [dialog setDirectoryURL:[NSURL fileURLWithPath:default_dir]];
  if (default_filename)
    [dialog setNameFieldStringValue:default_filename];
  [dialog beginSheetModalForWindow:owning_window
                 completionHandler:^(NSInteger result) {
    [bridge_.get() endedPanel:dialog
                    didCancel:result != NSFileHandlingPanelOKButton
                         type:type
                 parentWindow:owning_window];
  }];
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Walk through the open dialogs and close them all.  Use a temporary vector
  // to hold the pointers, since we can't delete from the map as we're iterating
  // through it.
  std::vector<NSSavePanel*> panels;
  for (const auto& value : dialog_data_map_)
    panels.push_back(value.first);

  for (const auto& panel : panels)
    [panel cancel:panel];
}

// static
base::scoped_nsobject<ExtensionDropdownHandler>
SelectFileDialogImpl::SetAccessoryView(
    NSSavePanel* dialog,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension) {
  DCHECK(file_types);
  NSView* accessory_view = ui::GetViewFromNib(@"SaveAccessoryView");
  if (!accessory_view)
    return base::scoped_nsobject<ExtensionDropdownHandler>();
  [dialog setAccessoryView:accessory_view];

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  // Create an array with each item corresponding to an array of different
  // extensions in an extension group.
  NSMutableArray* file_type_lists = [NSMutableArray array];
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
      base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
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

  base::scoped_nsobject<ExtensionDropdownHandler> handler(
      [[ExtensionDropdownHandler alloc] initWithDialog:dialog
                                         fileTypeLists:file_type_lists]);

  // This establishes a weak reference to handler. Hence we persist it as part
  // of dialog_data_map_.
  [popup setTarget:handler];
  [popup setAction:@selector(popupAction:)];

  if (default_extension.empty()) {
    // Select the first item.
    [popup selectItemAtIndex:0];
    [handler popupAction:popup];
  } else {
    [popup selectItemAtIndex:-1];
    [dialog
        setAllowedFileTypes:@[ base::SysUTF8ToNSString(default_extension) ]];
  }

  return handler;
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return hasMultipleFileTypeChoices_;
}

@implementation SelectFileDialogBridge

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s {
  if ((self = [super init])) {
    selectFileDialogImpl_ = s;
  }
  return self;
}

- (void)endedPanel:(NSSavePanel*)panel
         didCancel:(bool)did_cancel
              type:(ui::SelectFileDialog::Type)type
      parentWindow:(NSWindow*)parentWindow {
  int index = 0;
  std::vector<base::FilePath> paths;
  if (!did_cancel) {
    if (type == ui::SelectFileDialog::SELECT_SAVEAS_FILE) {
      if ([[panel URL] isFileURL]) {
        paths.push_back(base::mac::NSStringToFilePath([[panel URL] path]));
      }

      NSView* accessoryView = [panel accessoryView];
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
      CHECK([panel isKindOfClass:[NSOpenPanel class]]);
      NSArray* urls = [static_cast<NSOpenPanel*>(panel) URLs];
      for (NSURL* url in urls)
        if ([url isFileURL])
          paths.push_back(base::FilePath(base::SysNSStringToUTF8([url path])));
    }
  }

  bool isMulti = type == ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
  selectFileDialogImpl_->FileWasSelected(panel,
                                         parentWindow,
                                         did_cancel,
                                         isMulti,
                                         paths,
                                         index);
  [panel release];
}

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url {
  return [url isFileURL];
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

namespace ui {

SelectFileDialog* CreateMacSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy) {
  return new SelectFileDialogImpl(listener, policy);
}

}  // namespace ui
