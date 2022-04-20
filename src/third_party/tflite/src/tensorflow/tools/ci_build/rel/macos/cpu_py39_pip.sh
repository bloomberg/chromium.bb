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
source tensorflow/tools/ci_build/release/mac_build_utils.sh
install_bazelisk

# Selects a version of Xcode.
export DEVELOPER_DIR=/Applications/Xcode_11.3.app/Contents/Developer
sudo xcode-select -s "${DEVELOPER_DIR}"

# Set up python version via pyenv
export PYENV_VERSION=3.9.9
setup_python_from_pyenv_macos "${PYENV_VERSION}"

PIP_WHL_DIR="${KOKORO_ARTIFACTS_DIR}/tensorflow/pip-whl"
bazel_build_wheel ${PIP_WHL_DIR}

for WHL_PATH in $(ls "${PIP_WHL_DIR}"/tensorflow*.whl); do
  bazel_test_wheel ${WHL_PATH}
done
