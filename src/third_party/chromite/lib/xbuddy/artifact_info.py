# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module contains a list of artifact name related constants and methods."""

############ Artifact Names ############

# Note these are the available 'artifacts' that are known to the devserver. If
# you just need to stage a file from gs directly, use the files api. See the
# stage documentation for more info.

#### Update payload names. ####

# The name of artifact to stage a full update payload.
FULL_PAYLOAD = 'full_payload'

# The name of the artifact to stage N-to-N payloads for a build.
DELTA_PAYLOAD = 'delta_payload'

# The payload containing stateful data not stored on the rootfs of the image.
STATEFUL_PAYLOAD = 'stateful'

#### The following are the names of images to stages. ####

# The base image i.e. the image without any test/developer enhancements.
BASE_IMAGE = 'base_image'

# The recovery image - the image used to recover a chromiumos device.
RECOVERY_IMAGE = 'recovery_image'

# The image that has been signed and can be pushed to regular Chrome OS users.
SIGNED_IMAGE = 'signed_image'

# The test image - the base image with both develolper and test enhancements.
TEST_IMAGE = 'test_image'

# The developer image - the base image with developer enhancements.
DEV_IMAGE = 'dev_image'

# The name of artifact to stage a quick provision payload.
QUICK_PROVISION = 'quick_provision'

#### Autotest related packages. ####

# Autotest -- the main autotest directory without the test_suites subdir.
AUTOTEST = 'autotest'

# Control Files -- the autotest control files without the test_suites subdir.
CONTROL_FILES = 'control_files'

# Autotest Packages-- the autotest packages subdirectory.
AUTOTEST_PACKAGES = 'autotest_packages'

# Autotest Server Package.
AUTOTEST_SERVER_PACKAGE = 'autotest_server_package'

# Test Suites - just the test suites control files from the autotest directory.
TEST_SUITES = 'test_suites'

# AU Suite - The control files for the autotest autoupdate suite.
AU_SUITE = 'au_suite'

# AU Suite - The control files for the paygen autoupdate suite (depends
# on channel defined in devserver_constants).
PAYGEN_AU_SUITE_TEMPLATE = 'paygen_au_%(channel)s_suite'

#### Miscellaneous artifacts. ####

# Firmware tarball.
FIRMWARE = 'firmware'

# Tarball containing debug symbols for the given build.
SYMBOLS = 'symbols'

# A compressed tarball containing only sym files of debug symbols for the
# given build.
SYMBOLS_ONLY = 'symbols_only'

# The factory test image.
FACTORY_IMAGE = 'factory_image'

# The factory shim image.
FACTORY_SHIM_IMAGE = 'factory_shim_image'

#### Libiota Artifacts. These are in the same namespace as the above. ####

# Archive with test binaries for flashing to the DUT.
LIBIOTA_TEST_BINARIES = 'libiota_test_binaries'

# Utilities required for managing the DUT.
LIBIOTA_BOARD_UTILS = 'libiota_board_utils'

#### Android artifacts. These are in a different namespace from the above. ####

# Various android images stored in a zip file (including boot and system).
# For example, shamu-img-2284311.zip contains boot.img, cache.img, recovery.img,
# system.img and userdata.img. fastboot can use the zip file to update the dut
# in a single command. Therefore, devserver does not unzip the zip file to avoid
# unnecessary load on the devserver.
ANDROID_ZIP_IMAGES = 'zip_images'

# Radio image.
ANDROID_RADIO_IMAGE = 'radio_image'

# Bootloader image.
ANDROID_BOOTLOADER_IMAGE = 'bootloader_image'

# fastboot, utility to flash image to Android device.
ANDROID_FASTBOOT = 'fastboot'

# Test zip file for Android build, e.g., shamu-tests-2284311.zip
ANDROID_TEST_ZIP = 'test_zip'

# Zip file of vendor partitions used by Brillo device.
ANDROID_VENDOR_PARTITION_ZIP = 'vendor_partitions'

# Zip file of native tests used by Android devices.
ANDROID_NATIVETESTS_ZIP = 'nativetests'
ANDROID_CONTINUOUS_NATIVE_TESTS_ZIP = 'continuous_native_tests'
ANDROID_CONTINUOUS_INSTRUMENTATION_TESTS_ZIP = (
    'continuous_instrumentation_tests')
ANDROID_CTS_ZIP = 'android-cts'

# Zip file including all target files and images
ANDROID_TARGET_FILES_ZIP = 'target_files'

# Zip file containing DTB for emulators
ANDROID_DTB_ZIP = 'dtb'

# Zip file containing push_to_device.py and tools it depends on
ANDROID_PUSH_TO_DEVICE_ZIP = 'push_to_device_zip'

# Zip file containing selinux policy files.
ANDROID_SEPOLICY_ZIP = 'sepolicy.zip'

# In general, downloading one artifact usually indicates that the caller will
# want to download other artifacts later. The following map explicitly defines
# this relationship. Specifically:
# If X is requested, all items in Y should also get triggered for download.
CROS_REQUESTED_TO_OPTIONAL_MAP = {
    TEST_SUITES: [CONTROL_FILES, AUTOTEST_PACKAGES],
}
# Launch Control builds do not support autotest package.
ANDROID_REQUESTED_TO_OPTIONAL_MAP = {
    TEST_SUITES: [CONTROL_FILES],
}

# Map between the artifact name and the folder after it's unzipped.
ARTIFACT_UNZIP_FOLDER_MAP = {
    ANDROID_TEST_ZIP: 'DATA',
}
