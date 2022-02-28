#!/bin/bash
# Copyright 2016 The TensorFlow Authors. All Rights Reserved.
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
# Pip install TensorFlow and run basic test on the pip package.

set -e
set -x

# Get size check function
source tensorflow/tools/ci_build/release/common.sh

function run_smoke_test() {

  if [[ -z "${WHL_NAME}" ]]; then
    echo "TF WHL path not given, unable to install and test."
    exit 1
  fi

  # Upload the PIP package if whl test passes.
  if [ ${IN_VENV} -eq 0 ]; then
    VENV_TMP_DIR=$(mktemp -d)

    ${PYTHON_BIN_PATH} -m pip install virtualenv

    ${PYTHON_BIN_PATH} -m virtualenv -p ${PYTHON_BIN_PATH} "${VENV_TMP_DIR}" || \
        die "FAILED: Unable to create virtualenv"

    source "${VENV_TMP_DIR}/bin/activate" || \
        die "FAILED: Unable to activate virtualenv "
  fi

  # install tensorflow
  python -m pip install ${WHL_NAME} || \
      die "pip install (forcing to reinstall tensorflow) FAILED"
      echo "Successfully installed pip package ${WHL_NAME}"

  # Test TensorflowFlow imports
  test_tf_imports

  # Test TensorFlow whl file size
  test_tf_whl_size ${WHL_NAME}

  RESULT=$?

  # Upload the PIP package if whl test passes.
  if [ ${IN_VENV} -eq 0 ]; then
    # Deactivate from virtualenv.
    deactivate || source deactivate || die "FAILED: Unable to deactivate from existing virtualenv."
    sudo rm -rf "${KOKORO_GFILE_DIR}/venv"
  fi

  return $RESULT
}

function test_tf_imports() {
  TMP_DIR=$(mktemp -d)
  pushd "${TMP_DIR}"

  # test for basic import and perform tf.add operation.
  RET_VAL=$(python -c "import tensorflow as tf; t1=tf.constant([1,2,3,4]); t2=tf.constant([5,6,7,8]); print(tf.add(t1,t2).shape)")
  if ! [[ ${RET_VAL} == *'(4,)'* ]]; then
    echo "Unexpected return value: ${RET_VALUE}"
    echo "PIP test on virtualenv FAILED, will not upload ${WHL_NAME} package."
     return 1
  fi

  # test basic keras is available
  RET_VAL=$(python -c "import tensorflow as tf; print(tf.keras.__name__)")
  if ! [[ ${RET_VAL} == *'keras.api'* ]]; then
    echo "Unexpected return value: ${RET_VALUE}"
    echo "PIP test on virtualenv FAILED, will not upload ${WHL_NAME} package."
    return 1
  fi

  # similar test for estimator
  RET_VAL=$(python -c "import tensorflow as tf; print(tf.estimator.__name__)")
  if ! [[ ${RET_VAL} == *'tensorflow_estimator.python.estimator.api._v2.estimator'* ]]; then
    echo "Unexpected return value: ${RET_VALUE}"
    echo "PIP test on virtualenv FAILED, will not upload ${WHL_NAME} package."
    return 1
  fi

  RESULT=$?

  popd
  return $RESULT
}

###########################################################################
# Main
###########################################################################

IN_VENV=$(python -c 'import sys; print("1" if sys.version_info.major == 3 and sys.prefix != sys.base_prefix else "0")')
WHL_NAME=${1}
run_smoke_test
