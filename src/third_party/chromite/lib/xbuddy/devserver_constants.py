# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module contains constants shared across all other devserver modules."""

#### Google Storage locations and names. ####
# TODO (joyc) move the google storage filenames of artfacts here
CHANNELS = 'canary', 'dev', 'beta', 'stable'
GS_IMAGE_DIR = 'gs://chromeos-image-archive'
GS_LATEST_MASTER = '%(image_dir)s/%(board)s%(suffix)s/LATEST-master'
GS_LATEST_BASE_VERSION = (
    '%(image_dir)s/%(board)s%(suffix)s/LATEST-%(base_version)s')
IMAGE_DIR = '%(board)s%(suffix)s/%(version)s'

GS_RELEASES_DIR = 'gs://chromeos-releases'
GS_CHANNEL_DIR = GS_RELEASES_DIR + '/%(channel)s-channel/%(board)s/'

VERSION = r'[-0-9\.]+'
VERSION_RE = 'R%s' % VERSION

STAGED_BUILD_REGEX = '/static/(?P<build>.*-.*/%s)/.*' % VERSION_RE


#### Local storage locations and names. ####
AUTOTEST_DIR = 'autotest'
BASE_IMAGE_FILE = 'chromiumos_base_image.bin'
IMAGE_FILE = 'chromiumos_image.bin'
FACTORY_IMAGE_FILE = 'factory_test/chromiumos_factory_image.bin'
FACTORY_SHIM_IMAGE_FILE = 'factory_shim/factory_install_shim.bin'
RECOVERY_IMAGE_FILE = 'recovery_image.bin'
SIGNED_IMAGE_FILE = 'signed_image.bin'
TEST_IMAGE_FILE = 'chromiumos_test_image.bin'

ALL_IMAGES = (
    BASE_IMAGE_FILE,
    IMAGE_FILE,
    RECOVERY_IMAGE_FILE,
    TEST_IMAGE_FILE,
)

#### Update files
CACHE_DIR = 'cache'
STATEFUL_FILE = 'stateful.tgz'
UPDATE_FILE = 'update.gz'
UPDATE_METADATA_FILE = 'update.gz.json'

#### Android files
ANDROID_BOOT_IMAGE_FILE = 'boot.img'
ANDROID_SYSTEM_IMAGE_FILE = 'system.img'
ANDROID_FASTBOOT = 'fastboot'
