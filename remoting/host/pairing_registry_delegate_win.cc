// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_win.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"

namespace remoting {

namespace {

const wchar_t kClientNameKey[] = L"clientName";
const wchar_t kCreatedTimeKey[] = L"createdTime";
const wchar_t kSharedSecretKey[] = L"sharedSecret";

// Duplicates a registry key handle (returned by RegCreateXxx/RegOpenXxx).
// The returned handle cannot be inherited and has the same permissions as
// the source one.
bool DuplicateKeyHandle(HKEY source, ScopedRegKey* dest) {
  HANDLE handle;
  if (!DuplicateHandle(GetCurrentProcess(),
                       source,
                       GetCurrentProcess(),
                       &handle,
                       0,
                       FALSE,
                       DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate a registry key handle";
    return false;
  }

  dest->Set(reinterpret_cast<HKEY>(handle));
  return true;
}

}  // namespace

using protocol::PairingRegistry;

PairingRegistryDelegateWin::PairingRegistryDelegateWin() {
}

PairingRegistryDelegateWin::~PairingRegistryDelegateWin() {
}

bool PairingRegistryDelegateWin::SetRootKeys(HKEY privileged,
                                             HKEY unprivileged) {
  DCHECK(!privileged_);
  DCHECK(!unprivileged_);
  DCHECK(unprivileged);

  if (!DuplicateKeyHandle(unprivileged, &unprivileged_))
    return false;

  if (privileged) {
    if (!DuplicateKeyHandle(privileged, &privileged_))
      return false;
  }

  return true;
}

scoped_ptr<base::ListValue> PairingRegistryDelegateWin::LoadAll() {
  scoped_ptr<base::ListValue> pairings(new base::ListValue());

  DWORD index = 0;
  for (LONG result = ERROR_SUCCESS; result == ERROR_SUCCESS; ++index) {
    wchar_t name[MAX_PATH];
    result = RegEnumKey(unprivileged_, index, name, arraysize(name));
    if (result == ERROR_SUCCESS) {
      PairingRegistry::Pairing pairing = Load(WideToUTF8(name));
      if (pairing.is_valid())
        pairings->Append(pairing.ToValue().release());
    }
  }

  return pairings.Pass();
}

bool PairingRegistryDelegateWin::DeleteAll() {
  LONG result = ERROR_SUCCESS;
  bool success = true;
  while (result == ERROR_SUCCESS) {
    wchar_t name[MAX_PATH];
    result = RegEnumKey(unprivileged_, 0, name, arraysize(name));
    if (result == ERROR_SUCCESS)
      success = success && Delete(WideToUTF8(name));
  }

  success = success && result == ERROR_NO_MORE_ITEMS;
  return success;
}

PairingRegistry::Pairing PairingRegistryDelegateWin::Load(
    const std::string& client_id) {
  string16 key_name = UTF8ToUTF16(client_id);

  FILETIME created_time;
  string16 client_name;
  string16 shared_secret;

  // Read unprivileged fields first.
  base::win::RegKey pairing_key;
  LONG result = pairing_key.Open(unprivileged_, key_name.c_str(), KEY_READ);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot open pairing entry '" << client_id << "'";
    return PairingRegistry::Pairing();
  }
  result = pairing_key.ReadValue(kClientNameKey, &client_name);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot read '" << client_id << "/" << kClientNameKey << "'";
    return PairingRegistry::Pairing();
  }
  DWORD size = sizeof(created_time);
  DWORD type;
  result = pairing_key.ReadValue(kCreatedTimeKey,
                                 &created_time,
                                 &size,
                                 &type);
  if (result != ERROR_SUCCESS ||
      size != sizeof(created_time) ||
      type != REG_QWORD) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot read '" << client_id << "/" << kCreatedTimeKey
                << "'";
    return PairingRegistry::Pairing();
  }

  // Read the shared secret.
  if (privileged_.IsValid()) {
    result = pairing_key.Open(privileged_, key_name.c_str(), KEY_READ);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Cannot open pairing entry '" << client_id << "'";
      return PairingRegistry::Pairing();
    }
    result = pairing_key.ReadValue(kSharedSecretKey, &shared_secret);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Cannot read '" << client_id << "/" << kSharedSecretKey
                  << "'";
      return PairingRegistry::Pairing();
    }
  }

  return PairingRegistry::Pairing(base::Time::FromFileTime(created_time),
                                  UTF16ToUTF8(client_name),
                                  client_id,
                                  UTF16ToUTF8(shared_secret));
}

bool PairingRegistryDelegateWin::Save(const PairingRegistry::Pairing& pairing) {
  string16 key_name = UTF8ToUTF16(pairing.client_id());

  if (!privileged_.IsValid()) {
    LOG(ERROR) << "Cannot save pairing entry '" << pairing.client_id()
                << "': the delegate is read-only.";
    return false;
  }

  // Store the shared secret first.
  base::win::RegKey pairing_key;
  LONG result = pairing_key.Create(privileged_, key_name.c_str(), KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot save pairing entry '" << pairing.client_id()
                << "'";
    return false;
  }
  result = pairing_key.WriteValue(kSharedSecretKey,
                                  UTF8ToUTF16(pairing.shared_secret()).c_str());
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot write '" << pairing.client_id() << "/"
                << kSharedSecretKey << "'";
    return false;
  }

  // Store the rest of the fields.
  result = pairing_key.Create(unprivileged_, key_name.c_str(), KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot save pairing entry '" << pairing.client_id()
                << "'";
    return false;
  }
  result = pairing_key.WriteValue(kClientNameKey,
                                  UTF8ToUTF16(pairing.client_name()).c_str());
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot write '" << pairing.client_id() << "/"
                << kClientNameKey << "'";
    return false;
  }
  FILETIME created_time = pairing.created_time().ToFileTime();
  result = pairing_key.WriteValue(kCreatedTimeKey,
                                  &created_time,
                                  sizeof(created_time),
                                  REG_QWORD);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot write '" << pairing.client_id() << "/"
                << kCreatedTimeKey << "'";
    return false;
  }

  return true;
}

bool PairingRegistryDelegateWin::Delete(const std::string& client_id) {
  string16 key_name = UTF8ToUTF16(client_id);

  if (!privileged_.IsValid()) {
    LOG(ERROR) << "Cannot delete pairing entry '" << client_id
                << "': the delegate is read-only.";
    return false;
  }

  LONG result = RegDeleteKey(privileged_, key_name.c_str());
  if (result != ERROR_SUCCESS &&
      result != ERROR_FILE_NOT_FOUND &&
      result != ERROR_PATH_NOT_FOUND) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot delete pairing entry '" << client_id << "'";
    return false;
  }

  result = RegDeleteKey(unprivileged_, key_name.c_str());
  if (result != ERROR_SUCCESS &&
      result != ERROR_FILE_NOT_FOUND &&
      result != ERROR_PATH_NOT_FOUND) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot delete pairing entry '" << client_id << "'";
    return false;
  }

  return true;
}

scoped_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate() {
  return scoped_ptr<PairingRegistry::Delegate>();
}

}  // namespace remoting
