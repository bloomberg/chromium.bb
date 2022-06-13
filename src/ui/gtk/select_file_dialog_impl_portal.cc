// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/select_file_dialog_impl_portal.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/url_util.h"

namespace gtk {

namespace {

constexpr char kDBusMethodNameHasOwner[] = "NameHasOwner";
constexpr char kDBusMethodListActivatableNames[] = "ListActivatableNames";

constexpr char kXdgPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kXdgPortalObject[] = "/org/freedesktop/portal/desktop";

constexpr int kXdgPortalRequiredVersion = 3;

constexpr char kXdgPortalRequestInterfaceName[] =
    "org.freedesktop.portal.Request";
constexpr char kXdgPortalResponseSignal[] = "Response";

constexpr char kFileChooserInterfaceName[] =
    "org.freedesktop.portal.FileChooser";

constexpr char kFileChooserMethodOpenFile[] = "OpenFile";
constexpr char kFileChooserMethodSaveFile[] = "SaveFile";

constexpr char kFileChooserOptionHandleToken[] = "handle_token";
constexpr char kFileChooserOptionAcceptLabel[] = "accept_label";
constexpr char kFileChooserOptionMultiple[] = "multiple";
constexpr char kFileChooserOptionDirectory[] = "directory";
constexpr char kFileChooserOptionFilters[] = "filters";
constexpr char kFileChooserOptionCurrentFilter[] = "current_filter";
constexpr char kFileChooserOptionCurrentFolder[] = "current_folder";
constexpr char kFileChooserOptionCurrentName[] = "current_name";

constexpr int kFileChooserFilterKindGlob = 0;

constexpr char kFileUriPrefix[] = "file://";

struct FileChooserProperties : dbus::PropertySet {
  dbus::Property<uint32_t> version;

  explicit FileChooserProperties(dbus::ObjectProxy* object_proxy)
      : dbus::PropertySet(object_proxy, kFileChooserInterfaceName, {}) {
    RegisterProperty("version", &version);
  }

  ~FileChooserProperties() override = default;
};

void AppendStringOption(dbus::MessageWriter* writer,
                        const std::string& name,
                        const std::string& value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);
  option_writer.AppendVariantOfString(value);

  writer->CloseContainer(&option_writer);
}

void AppendByteStringOption(dbus::MessageWriter* writer,
                            const std::string& name,
                            const std::string& value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);

  dbus::MessageWriter value_writer(nullptr);
  option_writer.OpenVariant("ay", &value_writer);

  value_writer.AppendArrayOfBytes(
      reinterpret_cast<const std::uint8_t*>(value.c_str()),
      // size + 1 will include the null terminator.
      value.size() + 1);

  option_writer.CloseContainer(&value_writer);
  writer->CloseContainer(&option_writer);
}

void AppendBoolOption(dbus::MessageWriter* writer,
                      const std::string& name,
                      bool value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);
  option_writer.AppendVariantOfBool(value);

  writer->CloseContainer(&option_writer);
}

}  // namespace

// static
SelectFileDialogImpl* SelectFileDialogImpl::NewSelectFileDialogImplPortal(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectFileDialogImplPortal(listener, std::move(policy));
}

SelectFileDialogImplPortal::SelectFileDialogImplPortal(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialogImpl(listener, std::move(policy)) {}

SelectFileDialogImplPortal::~SelectFileDialogImplPortal() = default;

// static
void SelectFileDialogImplPortal::StartAvailabilityTestInBackground() {
  if (GetAvailabilityTestCompletionFlag()->IsSet())
    return;

  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogImplPortal::CheckPortalAvailabilityOnBusThread));
}

// static
bool SelectFileDialogImplPortal::IsPortalAvailable() {
  if (!GetAvailabilityTestCompletionFlag()->IsSet())
    LOG(WARNING) << "Portal availiability checked before test was complete";

  return is_portal_available_;
}

// static
void SelectFileDialogImplPortal::DestroyPortalConnection() {
  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogImplPortal::DestroyBusOnBusThread));
}

bool SelectFileDialogImplPortal::IsRunning(
    gfx::NativeWindow parent_window) const {
  if (parent_window && parent_window->GetHost()) {
    auto window = parent_window->GetHost()->GetAcceleratedWidget();
    return parents_.find(window) != parents_.end();
  }

  return false;
}

void SelectFileDialogImplPortal::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  auto info = base::MakeRefCounted<DialogInfo>();
  info->type = type;
  info->main_task_runner = base::SequencedTaskRunnerHandle::Get();
  info->listener_params = params;

  if (owning_window && owning_window->GetHost()) {
    info->parent = owning_window->GetHost()->GetAcceleratedWidget();
    parents_.insert(*info->parent);
  }

  if (file_types)
    file_types_ = *file_types;

  file_type_index_ = file_type_index;

  PortalFilterSet filter_set = BuildFilterSet();

  // Keep a copy of the filters so the index of the chosen one can be identified
  // and returned to listeners later.
  filters_ = filter_set.filters;

  if (info->parent) {
    if (GtkUi::GetPlatform()->ExportWindowHandle(
            *info->parent,
            base::BindOnce(
                &SelectFileDialogImplPortal::SelectFileImplWithParentHandle,
                // Note that we can't move any of the parameters, as the
                // fallback case below requires them to all still be available.
                this, info, title, default_path, filter_set,
                default_extension))) {
      // Return early to skip the fallback below.
      return;
    } else {
      LOG(WARNING) << "Failed to export window handle for portal select dialog";
    }
  }

  // No parent, so just use a blank parent handle.
  SelectFileImplWithParentHandle(std::move(info), std::move(title),
                                 std::move(default_path), std::move(filter_set),
                                 std::move(default_extension), "");
}

bool SelectFileDialogImplPortal::HasMultipleFileTypeChoicesImpl() {
  return file_types_.extensions.size() > 1;
}

// static
void SelectFileDialogImplPortal::CheckPortalAvailabilityOnBusThread() {
  base::AtomicFlag* availability_test_complete =
      GetAvailabilityTestCompletionFlag();
  if (availability_test_complete->IsSet())
    return;

  dbus::Bus* bus = AcquireBusOnBusThread();

  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  if (IsPortalRunningOnBusThread(dbus_proxy) ||
      IsPortalActivatableOnBusThread(dbus_proxy)) {
    dbus::ObjectPath portal_path(kXdgPortalObject);
    dbus::ObjectProxy* portal =
        bus->GetObjectProxy(kXdgPortalService, portal_path);

    FileChooserProperties properties(portal);
    if (!properties.GetAndBlock(&properties.version)) {
      LOG(ERROR) << "Failed to read portal version property";
    } else if (properties.version.value() >= kXdgPortalRequiredVersion) {
      is_portal_available_ = true;
    }
  }

  VLOG(1) << "File chooser portal available: "
          << (is_portal_available_ ? "yes" : "no");
  availability_test_complete->Set();
}

// static
bool SelectFileDialogImplPortal::IsPortalRunningOnBusThread(
    dbus::ObjectProxy* dbus_proxy) {
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kDBusMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kXdgPortalService);

  std::unique_ptr<dbus::Response> response = dbus_proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response)
    return false;

  dbus::MessageReader reader(response.get());
  bool owned = false;
  if (!reader.PopBool(&owned)) {
    LOG(ERROR) << "Failed to read response";
    return false;
  }

  return owned;
}

// static
bool SelectFileDialogImplPortal::IsPortalActivatableOnBusThread(
    dbus::ObjectProxy* dbus_proxy) {
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS,
                               kDBusMethodListActivatableNames);

  std::unique_ptr<dbus::Response> response = dbus_proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response)
    return false;

  dbus::MessageReader reader(response.get());
  std::vector<std::string> names;
  if (!reader.PopArrayOfStrings(&names)) {
    LOG(ERROR) << "Failed to read response";
    return false;
  }

  return base::Contains(names, kXdgPortalService);
}

SelectFileDialogImplPortal::PortalFilter::PortalFilter() = default;
SelectFileDialogImplPortal::PortalFilter::PortalFilter(
    const PortalFilter& other) = default;
SelectFileDialogImplPortal::PortalFilter::PortalFilter(PortalFilter&& other) =
    default;
SelectFileDialogImplPortal::PortalFilter::~PortalFilter() = default;

SelectFileDialogImplPortal::PortalFilterSet::PortalFilterSet() = default;
SelectFileDialogImplPortal::PortalFilterSet::PortalFilterSet(
    const PortalFilterSet& other) = default;
SelectFileDialogImplPortal::PortalFilterSet::PortalFilterSet(
    PortalFilterSet&& other) = default;
SelectFileDialogImplPortal::PortalFilterSet::~PortalFilterSet() = default;

SelectFileDialogImplPortal::DialogInfo::DialogInfo() = default;
SelectFileDialogImplPortal::DialogInfo::~DialogInfo() = default;

// static
scoped_refptr<dbus::Bus>*
SelectFileDialogImplPortal::AcquireBusStorageOnBusThread() {
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus(nullptr);
  if (!*bus) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();

    *bus = base::MakeRefCounted<dbus::Bus>(options);
  }

  return bus.get();
}

// static
dbus::Bus* SelectFileDialogImplPortal::AcquireBusOnBusThread() {
  return AcquireBusStorageOnBusThread()->get();
}

void SelectFileDialogImplPortal::DestroyBusOnBusThread() {
  scoped_refptr<dbus::Bus>* bus_storage = AcquireBusStorageOnBusThread();
  (*bus_storage)->ShutdownAndBlock();

  // If the connection is restarted later on, we need to make sure the entire
  // bus is newly created. Otherwise, references to an old, invalid task runner
  // may persist.
  bus_storage->reset();
}

// static
base::AtomicFlag*
SelectFileDialogImplPortal::GetAvailabilityTestCompletionFlag() {
  static base::NoDestructor<base::AtomicFlag> flag;
  return flag.get();
}

SelectFileDialogImplPortal::PortalFilterSet
SelectFileDialogImplPortal::BuildFilterSet() {
  PortalFilterSet filter_set;

  for (size_t i = 0; i < file_types_.extensions.size(); ++i) {
    PortalFilter filter;

    for (const std::string& extension : file_types_.extensions[i]) {
      if (extension.empty())
        continue;

      filter.patterns.insert(base::StringPrintf("*.%s", extension.c_str()));
    }

    if (filter.patterns.empty())
      continue;

    // If there is no matching description, use a default description based on
    // the filter.
    if (i < file_types_.extension_description_overrides.size()) {
      filter.name =
          base::UTF16ToUTF8(file_types_.extension_description_overrides[i]);
    } else {
      std::vector<std::string> patterns_vector(filter.patterns.begin(),
                                               filter.patterns.end());
      filter.name = base::JoinString(patterns_vector, ",");
    }

    // The -1 is required to match against the right filter because
    // |file_type_index_| is 1-indexed.
    if (i == file_type_index_ - 1)
      filter_set.default_filter = filter;

    filter_set.filters.push_back(std::move(filter));
  }

  if (file_types_.include_all_files && !filter_set.filters.empty()) {
    // Add the *.* filter, but only if we have added other filters (otherwise it
    // is implied).
    PortalFilter filter;
    filter.name = l10n_util::GetStringUTF8(IDS_SAVEAS_ALL_FILES);
    filter.patterns.insert("*.*");

    filter_set.filters.push_back(std::move(filter));
  }

  return filter_set;
}

void SelectFileDialogImplPortal::SelectFileImplWithParentHandle(
    scoped_refptr<DialogInfo> info,
    std::u16string title,
    base::FilePath default_path,
    PortalFilterSet filter_set,
    base::FilePath::StringType default_extension,
    std::string parent_handle) {
  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogImplPortal::SelectFileImplOnBusThread,
                     this, std::move(info), std::move(title),
                     std::move(default_path), std::move(filter_set),
                     std::move(default_extension), std::move(parent_handle)));
}

void SelectFileDialogImplPortal::SelectFileImplOnBusThread(
    scoped_refptr<DialogInfo> info,
    std::u16string title,
    base::FilePath default_path,
    PortalFilterSet filter_set,
    base::FilePath::StringType default_extension,
    std::string parent_handle) {
  dbus::Bus* bus = AcquireBusOnBusThread();
  if (!bus->Connect())
    LOG(ERROR) << "Could not connect to bus for XDG portal";

  std::string method;
  switch (info->type) {
    case SELECT_FOLDER:
    case SELECT_UPLOAD_FOLDER:
    case SELECT_EXISTING_FOLDER:
    case SELECT_OPEN_FILE:
    case SELECT_OPEN_MULTI_FILE:
      method = kFileChooserMethodOpenFile;
      break;
    case SELECT_SAVEAS_FILE:
      method = kFileChooserMethodSaveFile;
      break;
    case SELECT_NONE:
      NOTREACHED();
      break;
  }

  dbus::MethodCall method_call(kFileChooserInterfaceName, method);
  dbus::MessageWriter writer(&method_call);

  writer.AppendString(parent_handle);

  if (!title.empty()) {
    writer.AppendString(base::UTF16ToUTF8(title));
  } else {
    int message_id = 0;
    if (info->type == SELECT_SAVEAS_FILE) {
      message_id = IDS_SAVEAS_ALL_FILES;
    } else if (info->type == SELECT_OPEN_MULTI_FILE) {
      message_id = IDS_OPEN_FILES_DIALOG_TITLE;
    } else {
      message_id = IDS_OPEN_FILE_DIALOG_TITLE;
    }
    writer.AppendString(l10n_util::GetStringUTF8(message_id));
  }

  std::string response_handle_token =
      base::StringPrintf("handle_%d", handle_token_counter_++);

  AppendOptions(&writer, info->type, response_handle_token, default_path,
                filter_set);

  // The sender part of the handle object contains the D-Bus connection name
  // without the prefix colon and with all dots replaced with underscores.
  std::string sender_part;
  base::ReplaceChars(bus->GetConnectionName().substr(1), ".", "_",
                     &sender_part);

  dbus::ObjectPath expected_handle_path(
      base::StringPrintf("/org/freedesktop/portal/desktop/request/%s/%s",
                         sender_part.c_str(), response_handle_token.c_str()));

  info->response_handle =
      bus->GetObjectProxy(kXdgPortalService, expected_handle_path);
  ConnectToHandle(info);

  dbus::ObjectPath portal_path(kXdgPortalObject);
  dbus::ObjectProxy* portal =
      bus->GetObjectProxy(kXdgPortalService, portal_path);
  portal->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&SelectFileDialogImplPortal::OnCallResponse, this,
                     base::Unretained(bus), info));
}

void SelectFileDialogImplPortal::AppendOptions(
    dbus::MessageWriter* writer,
    Type type,
    const std::string& response_handle_token,
    const base::FilePath& default_path,
    const PortalFilterSet& filter_set) {
  dbus::MessageWriter options_writer(nullptr);
  writer->OpenArray("{sv}", &options_writer);

  AppendStringOption(&options_writer, kFileChooserOptionHandleToken,
                     response_handle_token);

  if (type == SELECT_UPLOAD_FOLDER) {
    AppendStringOption(&options_writer, kFileChooserOptionAcceptLabel,
                       l10n_util::GetStringUTF8(
                           IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON));
  }

  if (type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER ||
      type == SELECT_EXISTING_FOLDER) {
    AppendBoolOption(&options_writer, kFileChooserOptionDirectory, true);
  } else if (type == SELECT_OPEN_MULTI_FILE) {
    AppendBoolOption(&options_writer, kFileChooserOptionMultiple, true);
  }

  if (type == SELECT_SAVEAS_FILE && !default_path.empty()) {
    if (CallDirectoryExistsOnUIThread(default_path)) {
      // If this is an existing directory, navigate to that directory, with no
      // filename.
      AppendByteStringOption(&options_writer, kFileChooserOptionCurrentFolder,
                             default_path.value());
    } else {
      // The default path does not exist, or is an existing file. We use
      // current_folder followed by current_name, as per the recommendation of
      // the GTK docs and the pattern followed by SelectFileDialogImplGTK.
      AppendByteStringOption(&options_writer, kFileChooserOptionCurrentFolder,
                             default_path.DirName().value());
      AppendStringOption(&options_writer, kFileChooserOptionCurrentName,
                         default_path.BaseName().value());
    }
  }

  AppendFiltersOption(&options_writer, filter_set.filters);
  if (filter_set.default_filter) {
    dbus::MessageWriter option_writer(nullptr);
    options_writer.OpenDictEntry(&option_writer);

    option_writer.AppendString(kFileChooserOptionCurrentFilter);

    dbus::MessageWriter value_writer(nullptr);
    option_writer.OpenVariant("(sa(us))", &value_writer);

    AppendFilterStruct(&value_writer, *filter_set.default_filter);

    option_writer.CloseContainer(&value_writer);
    options_writer.CloseContainer(&option_writer);
  }

  writer->CloseContainer(&options_writer);
}

void SelectFileDialogImplPortal::AppendFiltersOption(
    dbus::MessageWriter* writer,
    const std::vector<PortalFilter>& filters) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(kFileChooserOptionFilters);

  dbus::MessageWriter variant_writer(nullptr);
  option_writer.OpenVariant("a(sa(us))", &variant_writer);

  dbus::MessageWriter filters_writer(nullptr);
  variant_writer.OpenArray("(sa(us))", &filters_writer);

  for (const PortalFilter& filter : filters) {
    AppendFilterStruct(&filters_writer, filter);
  }

  variant_writer.CloseContainer(&filters_writer);
  option_writer.CloseContainer(&variant_writer);
  writer->CloseContainer(&option_writer);
}

void SelectFileDialogImplPortal::AppendFilterStruct(
    dbus::MessageWriter* writer,
    const PortalFilter& filter) {
  dbus::MessageWriter filter_writer(nullptr);
  writer->OpenStruct(&filter_writer);

  filter_writer.AppendString(filter.name);

  dbus::MessageWriter patterns_writer(nullptr);
  filter_writer.OpenArray("(us)", &patterns_writer);

  for (const std::string& pattern : filter.patterns) {
    dbus::MessageWriter pattern_writer(nullptr);
    patterns_writer.OpenStruct(&pattern_writer);

    pattern_writer.AppendUint32(kFileChooserFilterKindGlob);
    pattern_writer.AppendString(pattern);

    patterns_writer.CloseContainer(&pattern_writer);
  }

  filter_writer.CloseContainer(&patterns_writer);
  writer->CloseContainer(&filter_writer);
}

void SelectFileDialogImplPortal::ConnectToHandle(
    scoped_refptr<DialogInfo> info) {
  info->response_handle->ConnectToSignal(
      kXdgPortalRequestInterfaceName, kXdgPortalResponseSignal,
      base::BindRepeating(&SelectFileDialogImplPortal::OnResponseSignalEmitted,
                          this, info),
      base::BindOnce(&SelectFileDialogImplPortal::OnResponseSignalConnected,
                     this, info));
}

void SelectFileDialogImplPortal::CompleteOpen(scoped_refptr<DialogInfo> info,
                                              std::vector<base::FilePath> paths,
                                              std::string current_filter) {
  info->response_handle->Detach();
  info->main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogImplPortal::CompleteOpenOnMainThread,
                     this, info, std::move(paths), std::move(current_filter)));
}

void SelectFileDialogImplPortal::CancelOpen(scoped_refptr<DialogInfo> info) {
  info->response_handle->Detach();
  info->main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogImplPortal::CancelOpenOnMainThread, this,
                     info));
}

void SelectFileDialogImplPortal::CompleteOpenOnMainThread(
    scoped_refptr<DialogInfo> info,
    std::vector<base::FilePath> paths,
    std::string current_filter) {
  UnparentOnMainThread(info.get());

  if (listener_) {
    if (info->type == SELECT_OPEN_MULTI_FILE) {
      listener_->MultiFilesSelected(paths, info->listener_params);
    } else if (paths.size() > 1) {
      LOG(ERROR) << "Got >1 file URI from a single-file chooser";
    } else {
      int index = 1;
      for (size_t i = 0; i < filters_.size(); ++i) {
        if (filters_[i].name == current_filter) {
          index = 1 + i;
          break;
        }
      }
      listener_->FileSelected(paths.front(), index, info->listener_params);
    }
  }
}

void SelectFileDialogImplPortal::CancelOpenOnMainThread(
    scoped_refptr<DialogInfo> info) {
  UnparentOnMainThread(info.get());

  if (listener_)
    listener_->FileSelectionCanceled(info->listener_params);
}

void SelectFileDialogImplPortal::UnparentOnMainThread(DialogInfo* info) {
  if (info->parent) {
    parents_.erase(*info->parent);
    info->parent.reset();
  }
}

void SelectFileDialogImplPortal::OnCallResponse(
    dbus::Bus* bus,
    scoped_refptr<DialogInfo> info,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (response) {
    dbus::MessageReader reader(response);
    dbus::ObjectPath actual_handle_path;
    if (!reader.PopObjectPath(&actual_handle_path)) {
      LOG(ERROR) << "Invalid portal response";
    } else {
      if (info->response_handle->object_path() != actual_handle_path) {
        VLOG(1) << "Re-attaching response handle to "
                << actual_handle_path.value();

        info->response_handle->Detach();
        info->response_handle =
            bus->GetObjectProxy(kXdgPortalService, actual_handle_path);
        ConnectToHandle(info);
      }

      // Return before the operation is cancelled.
      return;
    }
  } else if (error_response) {
    std::string error_name = error_response->GetErrorName();
    std::string error_message;
    dbus::MessageReader reader(error_response);
    reader.PopString(&error_message);

    LOG(ERROR) << "Portal returned error: " << error_name << ": "
               << error_message;
  } else {
    NOTREACHED();
  }

  // All error paths end up here.
  CancelOpen(std::move(info));
}

void SelectFileDialogImplPortal::OnResponseSignalConnected(
    scoped_refptr<DialogInfo> info,
    const std::string& interface,
    const std::string& signal,
    bool connected) {
  if (!connected) {
    LOG(ERROR) << "Could not connect to Response signal";
    CancelOpen(std::move(info));
  }
}

void SelectFileDialogImplPortal::OnResponseSignalEmitted(
    scoped_refptr<DialogInfo> info,
    dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  std::vector<std::string> uris;
  std::string current_filter;
  if (!CheckResponseCode(&reader) ||
      !ReadResponseResults(&reader, &uris, &current_filter)) {
    CancelOpen(std::move(info));
    return;
  }

  std::vector<base::FilePath> paths = ConvertUrisToPaths(uris);
  if (!paths.empty())
    CompleteOpen(std::move(info), std::move(paths), std::move(current_filter));
  else
    CancelOpen(std::move(info));
}

bool SelectFileDialogImplPortal::CheckResponseCode(
    dbus::MessageReader* reader) {
  std::uint32_t response = 0;
  if (!reader->PopUint32(&response)) {
    LOG(ERROR) << "Failed to read response ID";
    return false;
  } else if (response != 0) {
    return false;
  }

  return true;
}

bool SelectFileDialogImplPortal::ReadResponseResults(
    dbus::MessageReader* reader,
    std::vector<std::string>* uris,
    std::string* current_filter) {
  dbus::MessageReader results_reader(nullptr);
  if (!reader->PopArray(&results_reader)) {
    LOG(ERROR) << "Failed to read file chooser variant";
    return false;
  }

  while (results_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(nullptr);
    std::string key;
    if (!results_reader.PopDictEntry(&entry_reader) ||
        !entry_reader.PopString(&key)) {
      LOG(ERROR) << "Failed to read response entry";
      return false;
    }

    if (key == "uris") {
      dbus::MessageReader uris_reader(nullptr);
      if (!entry_reader.PopVariant(&uris_reader) ||
          !uris_reader.PopArrayOfStrings(uris)) {
        LOG(ERROR) << "Failed to read <uris> response entry value";
        return false;
      }
    }
    if (key == "current_filter") {
      dbus::MessageReader current_filter_reader(nullptr);
      dbus::MessageReader current_filter_struct_reader(nullptr);
      if (!entry_reader.PopVariant(&current_filter_reader) ||
          !current_filter_reader.PopStruct(&current_filter_struct_reader) ||
          !current_filter_struct_reader.PopString(current_filter)) {
        LOG(ERROR) << "Failed to read <current_filter> response entry value";
      }
    }
  }

  return true;
}

std::vector<base::FilePath> SelectFileDialogImplPortal::ConvertUrisToPaths(
    const std::vector<std::string>& uris) {
  std::vector<base::FilePath> paths;
  for (const std::string& uri : uris) {
    if (!base::StartsWith(uri, kFileUriPrefix, base::CompareCase::SENSITIVE)) {
      LOG(WARNING) << "Ignoring unknown file chooser URI: " << uri;
      continue;
    }

    base::StringPiece encoded_path(uri);
    encoded_path.remove_prefix(strlen(kFileUriPrefix));

    url::RawCanonOutputT<char16_t> decoded_path;
    url::DecodeURLEscapeSequences(encoded_path.data(), encoded_path.size(),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &decoded_path);
    paths.emplace_back(base::UTF16ToUTF8(
        base::StringPiece16(decoded_path.data(), decoded_path.length())));
  }

  return paths;
}

bool SelectFileDialogImplPortal::is_portal_available_ = false;
int SelectFileDialogImplPortal::handle_token_counter_ = 0;

}  // namespace gtk
