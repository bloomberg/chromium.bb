// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InstanceChecker} from '../common/instance_checker.js';

import {SelectToSpeak} from './select_to_speak.js';

InstanceChecker.closeExtraInstances();
export const selectToSpeak = new SelectToSpeak();
