// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PermissionType} from './app_management.mojom-webui.js';

export {Permission, PermissionType, PermissionValue, TriState} from './app_management.mojom-webui.js';

export type PermissionTypeIndex = keyof typeof PermissionType;
