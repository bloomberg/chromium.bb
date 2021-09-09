#!/bin/bash
# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
set -e
set -x

source tensorflow/tools/ci_build/release/common.sh
install_bazelisk

# Pick a more recent version of xcode
sudo xcode-select --switch /Applications/Xcode_9.2.app/Contents/Developer

# Install pip dependencies
python -m virtualenv tf_build_env --system-site-packages
source tf_build_env/bin/activate
install_macos_pip_deps sudo

# Run configure.
export TF_NEED_CUDA=0
export CC_OPT_FLAGS='-mavx'
export PYTHON_BIN_PATH=$(which python2)
yes "" | "$PYTHON_BIN_PATH" configure.py

# TODO(b/122370901): Fix nomac, no_mac inconsistency.
tag_filters="-no_oss,-oss_serial,-nomac,-no_mac,-no_oss_py2"

# Get the default test targets for bazel.
source tensorflow/tools/ci_build/build_scripts/PRESUBMIT_BUILD_TARGETS.sh

# Run tests
bazel test --test_output=errors --config=opt \
  --incompatible_depset_union=false \
  --build_tag_filters="${tag_filters}" \
  --test_tag_filters="${tag_filters}" -- \
  ${DEFAULT_BAZEL_TARGETS} \
  -//tensorflow/lite/...
