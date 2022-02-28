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
export DEVELOPER_DIR=/Applications/Xcode_10.3.app/Contents/Developer
sudo xcode-select -s "${DEVELOPER_DIR}"

# Set up and install MacOS pip dependencies.
install_macos_pip_deps_no_venv python3.7

# Export required variables for running pip_new.sh
export OS_TYPE="MACOS"
export CONTAINER_TYPE="CPU"
export TF_PYTHON_VERSION='python3.7'
export PYTHON_BIN_PATH="$(which ${TF_PYTHON_VERSION})"
export TF_BUILD_BOTH_CPU_PACKAGES=1

# Export optional variables for running pip.sh
export TF_BUILD_FLAGS="--config=release_cpu_macos"
export TF_TEST_FLAGS="--define=no_tensorflow_py_deps=true --test_lang_filters=py --test_output=errors --verbose_failures=true --keep_going --test_env=TF2_BEHAVIOR=1"
export TF_TEST_TARGETS="//tensorflow/python/..."
export TF_PIP_TESTS="test_pip_virtualenv_non_clean test_pip_virtualenv_clean"
export TF_TEST_FILTER_TAGS='-nomac,-no_mac,-no_oss,-oss_serial,-no_oss_py37,-v1only,-gpu,-tpu,-benchmark-test'
#export IS_NIGHTLY=0 # Not nightly; uncomment if building from tf repo.
export TF_PROJECT_NAME="tensorflow"
export TF_PIP_TEST_ROOT="pip_test"

./tensorflow/tools/ci_build/builds/pip_new.sh
