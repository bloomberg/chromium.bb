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

source tensorflow/tools/ci_build/release/common.sh
install_bazelisk

# For python3 path on Mac
export PATH=$PATH:/usr/local/bin

sudo pip install twine

./tensorflow/tools/ci_build/update_version.py --nightly

# Run configure.
export TF_NEED_CUDA=0
export CC_OPT_FLAGS='-mavx'
export PYTHON_BIN_PATH=$(which python3.5)
yes "" | "$PYTHON_BIN_PATH" configure.py

# Build the pip package
bazel build --config=opt tensorflow/tools/pip_package:build_pip_package
mkdir pip_pkg
./bazel-bin/tensorflow/tools/pip_package/build_pip_package pip_pkg --nightly_flag

# Also upload the python 3.5 package as python 3.3 and 3.4 packages.
FILENAME="$(ls pip_pkg/tf_nightly-*dev*-macosx_*.whl)"
tensorflow/tools/ci_build/copy_binary.py --filename "${FILENAME}" --new_py_ver 33
tensorflow/tools/ci_build/copy_binary.py --filename "${FILENAME}" --new_py_ver 34

for f in $(ls pip_pkg/tf_nightly-*dev*macosx*.whl); do
  echo "Uploading package: ${f}"
  twine upload -r pypi-warehouse "${f}" || echo
done
