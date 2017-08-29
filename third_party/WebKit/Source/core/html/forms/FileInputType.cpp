/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/FileInputType.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/Event.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormData.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"
#include "core/layout/LayoutFileUploadControl.h"
#include "core/page/ChromeClient.h"
#include "core/page/DragData.h"
#include "platform/FileMetadata.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/PlatformLocale.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

namespace {

WebVector<WebString> CollectAcceptTypes(const HTMLInputElement& input) {
  Vector<String> mime_types = input.AcceptMIMETypes();
  Vector<String> extensions = input.AcceptFileExtensions();

  Vector<String> accept_types;
  accept_types.ReserveCapacity(mime_types.size() + extensions.size());
  accept_types.AppendVector(mime_types);
  accept_types.AppendVector(extensions);
  return accept_types;
}

}  // namespace

inline FileInputType::FileInputType(HTMLInputElement& element)
    : InputType(element),
      KeyboardClickableInputTypeView(element),
      file_list_(FileList::Create()) {}

InputType* FileInputType::Create(HTMLInputElement& element) {
  return new FileInputType(element);
}

DEFINE_TRACE(FileInputType) {
  visitor->Trace(file_list_);
  KeyboardClickableInputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* FileInputType::CreateView() {
  return this;
}

Vector<FileChooserFileInfo> FileInputType::FilesFromFormControlState(
    const FormControlState& state) {
  Vector<FileChooserFileInfo> files;
  for (size_t i = 0; i < state.ValueSize(); i += 2) {
    if (!state[i + 1].IsEmpty())
      files.push_back(FileChooserFileInfo(state[i], state[i + 1]));
    else
      files.push_back(FileChooserFileInfo(state[i]));
  }
  return files;
}

const AtomicString& FileInputType::FormControlType() const {
  return InputTypeNames::file;
}

FormControlState FileInputType::SaveFormControlState() const {
  if (file_list_->IsEmpty())
    return FormControlState();
  FormControlState state;
  unsigned num_files = file_list_->length();
  for (unsigned i = 0; i < num_files; ++i) {
    if (file_list_->item(i)->HasBackingFile()) {
      state.Append(file_list_->item(i)->GetPath());
      state.Append(file_list_->item(i)->name());
    }
    // FIXME: handle Blob-backed File instances, see http://crbug.com/394948
  }
  return state;
}

void FileInputType::RestoreFormControlState(const FormControlState& state) {
  if (state.ValueSize() % 2)
    return;
  FilesChosen(FilesFromFormControlState(state));
}

void FileInputType::AppendToFormData(FormData& form_data) const {
  FileList* file_list = GetElement().files();
  unsigned num_files = file_list->length();
  if (num_files == 0) {
    form_data.append(GetElement().GetName(), File::Create(""));
    return;
  }

  for (unsigned i = 0; i < num_files; ++i)
    form_data.append(GetElement().GetName(), file_list->item(i));
}

bool FileInputType::ValueMissing(const String& value) const {
  return GetElement().IsRequired() && value.IsEmpty();
}

String FileInputType::ValueMissingText() const {
  return GetLocale().QueryString(
      GetElement().Multiple()
          ? WebLocalizedString::kValidationValueMissingForMultipleFile
          : WebLocalizedString::kValidationValueMissingForFile);
}

void FileInputType::HandleDOMActivateEvent(Event* event) {
  if (GetElement().IsDisabledFormControl())
    return;

  if (!UserGestureIndicator::ProcessingUserGesture())
    return;

  if (ChromeClient* chrome_client = this->GetChromeClient()) {
    WebFileChooserParams params;
    HTMLInputElement& input = GetElement();
    bool is_directory = input.FastHasAttribute(webkitdirectoryAttr);
    params.directory = is_directory;
    params.need_local_path = is_directory;
    params.multi_select = is_directory || input.FastHasAttribute(multipleAttr);
    params.accept_types = CollectAcceptTypes(input);
    params.selected_files = file_list_->PathsForUserVisibleFiles();
    params.use_media_capture = RuntimeEnabledFeatures::MediaCaptureEnabled() &&
                               input.FastHasAttribute(captureAttr);
    params.requestor = input.GetDocument().Url();

    chrome_client->OpenFileChooser(input.GetDocument().GetFrame(),
                                   NewFileChooser(params));
  }
  event->SetDefaultHandled();
}

LayoutObject* FileInputType::CreateLayoutObject(const ComputedStyle&) const {
  return new LayoutFileUploadControl(&GetElement());
}

InputType::ValueMode FileInputType::GetValueMode() const {
  return ValueMode::kFilename;
}

bool FileInputType::CanSetStringValue() const {
  return false;
}

FileList* FileInputType::Files() {
  return file_list_.Get();
}

bool FileInputType::CanSetValue(const String& value) {
  // For security reasons, we don't allow setting the filename, but we do allow
  // clearing it.  The HTML5 spec (as of the 10/24/08 working draft) says that
  // the value attribute isn't applicable to the file upload control at all, but
  // for now we are keeping this behavior to avoid breaking existing websites
  // that may be relying on this.
  return value.IsEmpty();
}

String FileInputType::ValueInFilenameValueMode() const {
  if (file_list_->IsEmpty())
    return String();

  // HTML5 tells us that we're supposed to use this goofy value for
  // file input controls. Historically, browsers revealed the real
  // file path, but that's a privacy problem. Code on the web
  // decided to try to parse the value by looking for backslashes
  // (because that's what Windows file paths use). To be compatible
  // with that code, we make up a fake path for the file.
  return "C:\\fakepath\\" + file_list_->item(0)->name();
}

void FileInputType::SetValue(const String&,
                             bool value_changed,
                             TextFieldEventBehavior,
                             TextControlSetValueSelection) {
  if (!value_changed)
    return;

  file_list_->clear();
  GetElement().SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kControlValue));
  GetElement().SetNeedsValidityCheck();
}

FileList* FileInputType::CreateFileList(
    const Vector<FileChooserFileInfo>& files,
    bool has_webkit_directory_attr) {
  FileList* file_list(FileList::Create());
  size_t size = files.size();

  // If a directory is being selected, the UI allows a directory to be chosen
  // and the paths provided here share a root directory somewhere up the tree;
  // we want to store only the relative paths from that point.
  if (size && has_webkit_directory_attr) {
    // Find the common root path.
    String root_path = DirectoryName(files[0].path);
    for (size_t i = 1; i < size; ++i) {
      while (!files[i].path.StartsWith(root_path))
        root_path = DirectoryName(root_path);
    }
    root_path = DirectoryName(root_path);
    DCHECK(root_path.length());
    int root_length = root_path.length();
    if (root_path[root_length - 1] != '\\' && root_path[root_length - 1] != '/')
      root_length += 1;
    for (const auto& file : files) {
      // Normalize backslashes to slashes before exposing the relative path to
      // script.
      String relative_path =
          file.path.Substring(root_length).Replace('\\', '/');
      file_list->Append(File::CreateWithRelativePath(file.path, relative_path));
    }
    return file_list;
  }

  for (const auto& file : files) {
    if (file.file_system_url.IsEmpty()) {
      file_list->Append(
          File::CreateForUserProvidedFile(file.path, file.display_name));
    } else {
      file_list->Append(File::CreateForFileSystemFile(
          file.file_system_url, file.metadata, File::kIsUserVisible));
    }
  }
  return file_list;
}

void FileInputType::CountUsage() {
  Document* document = &GetElement().GetDocument();
  if (document->IsSecureContext())
    UseCounter::Count(*document, WebFeature::kInputTypeFileInsecureOrigin);
  else
    UseCounter::Count(*document, WebFeature::kInputTypeFileSecureOrigin);
}

void FileInputType::CreateShadowSubtree() {
  DCHECK(GetElement().Shadow());
  HTMLInputElement* button =
      HTMLInputElement::Create(GetElement().GetDocument(), false);
  button->setType(InputTypeNames::button);
  button->setAttribute(
      valueAttr,
      AtomicString(GetLocale().QueryString(
          GetElement().Multiple()
              ? WebLocalizedString::kFileButtonChooseMultipleFilesLabel
              : WebLocalizedString::kFileButtonChooseFileLabel)));
  button->SetShadowPseudoId(AtomicString("-webkit-file-upload-button"));
  GetElement().UserAgentShadowRoot()->AppendChild(button);
}

void FileInputType::DisabledAttributeChanged() {
  DCHECK(GetElement().Shadow());
  if (Element* button =
          ToElementOrDie(GetElement().UserAgentShadowRoot()->firstChild()))
    button->SetBooleanAttribute(disabledAttr,
                                GetElement().IsDisabledFormControl());
}

void FileInputType::MultipleAttributeChanged() {
  DCHECK(GetElement().Shadow());
  if (Element* button =
          ToElementOrDie(GetElement().UserAgentShadowRoot()->firstChild()))
    button->setAttribute(
        valueAttr,
        AtomicString(GetLocale().QueryString(
            GetElement().Multiple()
                ? WebLocalizedString::kFileButtonChooseMultipleFilesLabel
                : WebLocalizedString::kFileButtonChooseFileLabel)));
}

void FileInputType::SetFiles(FileList* files) {
  if (!files)
    return;

  bool files_changed = false;
  if (files->length() != file_list_->length()) {
    files_changed = true;
  } else {
    for (unsigned i = 0; i < files->length(); ++i) {
      if (!files->item(i)->HasSameSource(*file_list_->item(i))) {
        files_changed = true;
        break;
      }
    }
  }

  file_list_ = files;

  GetElement().NotifyFormStateChanged();
  GetElement().SetNeedsValidityCheck();

  if (GetElement().GetLayoutObject())
    GetElement().GetLayoutObject()->SetShouldDoFullPaintInvalidation();

  if (files_changed) {
    // This call may cause destruction of this instance.
    // input instance is safe since it is ref-counted.
    GetElement().DispatchChangeEvent();
  }
}

void FileInputType::FilesChosen(const Vector<FileChooserFileInfo>& files) {
  SetFiles(CreateFileList(files,
                          GetElement().FastHasAttribute(webkitdirectoryAttr)));
}

void FileInputType::SetFilesFromDirectory(const String& path) {
  if (ChromeClient* chrome_client = this->GetChromeClient()) {
    Vector<String> files;
    files.push_back(path);
    WebFileChooserParams params;
    params.directory = true;
    params.multi_select = true;
    params.selected_files = files;
    params.accept_types = CollectAcceptTypes(GetElement());
    params.requestor = GetElement().GetDocument().Url();
    chrome_client->EnumerateChosenDirectory(NewFileChooser(params));
  }
}

void FileInputType::SetFilesFromPaths(const Vector<String>& paths) {
  if (paths.IsEmpty())
    return;

  HTMLInputElement& input = GetElement();
  if (input.FastHasAttribute(webkitdirectoryAttr)) {
    SetFilesFromDirectory(paths[0]);
    return;
  }

  Vector<FileChooserFileInfo> files;
  for (const auto& path : paths)
    files.push_back(FileChooserFileInfo(path));

  if (input.FastHasAttribute(multipleAttr)) {
    FilesChosen(files);
  } else {
    Vector<FileChooserFileInfo> first_file_only;
    first_file_only.push_back(files[0]);
    FilesChosen(first_file_only);
  }
}

bool FileInputType::ReceiveDroppedFiles(const DragData* drag_data) {
  Vector<String> paths;
  drag_data->AsFilePaths(paths);
  if (paths.IsEmpty())
    return false;

  if (!GetElement().FastHasAttribute(webkitdirectoryAttr)) {
    dropped_file_system_id_ = drag_data->DroppedFileSystemId();
  }
  SetFilesFromPaths(paths);
  return true;
}

String FileInputType::DroppedFileSystemId() {
  return dropped_file_system_id_;
}

String FileInputType::DefaultToolTip(const InputTypeView&) const {
  FileList* file_list = file_list_.Get();
  unsigned list_size = file_list->length();
  if (!list_size) {
    return GetLocale().QueryString(
        WebLocalizedString::kFileButtonNoFileSelectedLabel);
  }

  StringBuilder names;
  for (size_t i = 0; i < list_size; ++i) {
    names.Append(file_list->item(i)->name());
    if (i != list_size - 1)
      names.Append('\n');
  }
  return names.ToString();
}

void FileInputType::CopyNonAttributeProperties(const HTMLInputElement& source) {
  DCHECK(file_list_->IsEmpty());
  const FileList* source_list = source.files();
  for (unsigned i = 0; i < source_list->length(); ++i)
    file_list_->Append(source_list->item(i)->Clone());
}

}  // namespace blink
