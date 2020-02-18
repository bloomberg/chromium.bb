// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"

namespace chromeos {

const char kEasyUnlockKeyMetaNameBluetoothAddress[] = "eu.btaddr";
const char kEasyUnlockKeyMetaNamePsk[] = "eu.psk";
const char kEasyUnlockKeyMetaNamePubKey[] = "eu.pubkey";
const char kEasyUnlockKeyMetaNameChallenge[] = "eu.C";
const char kEasyUnlockKeyMetaNameWrappedSecret[] = "eu.WUK";
const char kEasyUnlockKeyMetaNameSerializedBeaconSeeds[] = "eu.BS";
const char kEasyUnlockKeyMetaNameUnlockKey[] = "eu.unlock_key";

EasyUnlockDeviceKeyData::EasyUnlockDeviceKeyData() = default;

EasyUnlockDeviceKeyData::EasyUnlockDeviceKeyData(
    const EasyUnlockDeviceKeyData&) = default;

EasyUnlockDeviceKeyData::~EasyUnlockDeviceKeyData() = default;

}  // namespace chromeos
