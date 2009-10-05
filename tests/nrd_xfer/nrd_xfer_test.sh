#!/bin/bash

# linux specific test for nrd_xfer
# derived from: src/trusted/desc/nrd_xfer_test.sh

set -o nounset
set -o errexit

readonly EMULATOR=${EMULATOR:-}
readonly SEL_LDR=$1
readonly TEST_EXE=$2
# pass remaining flags to sel_ldr
shift 2
readonly SEL_FLAGS=$*

readonly TMP_FILE="/tmp/sel_ldr_nrd_xfer_test.$$"
readonly MESSAGE="TEST_STRING_PAYLOAD"

# NOTE: This script is timing sensitive as it waits for the server to initialize
#       it self and to receive data from the client.
#       The variable below can be increased until a better mechanism is in place
readonly WAIT_TIME=1

# register cleanup. exit code sematics are unclear - so we play it safe.
exit_code=-1
trap 'rm -f ${TMP_FILE}; exit ${exit_code}' EXIT SIGHUP SIGINT SIGQUIT SIGTERM

echo
echo "starting server in background"
echo
${EMULATOR} ${SEL_LDR} ${SEL_FLAGS} -X -1 -D 1 -- ${TEST_EXE} -s > ${TMP_FILE} &

sleep ${WAIT_TIME}

# capability is emited by sel_ldr before module output
ADDR=$(head -1 ${TMP_FILE})
# 27 == sizeof cap.path in struct NaClDescConnCap
ADDR=${ADDR:0:27}
echo "capability is ${ADDR}"

echo
echo "running client"
echo
${EMULATOR} ${SEL_LDR} ${SEL_FLAGS} -X -1 -a 6:${ADDR} -- ${TEST_EXE} -c 6 -m ${MESSAGE}

sleep ${WAIT_TIME}

echo
echo "dump output"
echo
cat ${TMP_FILE}

echo
echo "check whether test message was received"
echo

if grep ${MESSAGE} ${TMP_FILE} ; then
  echo "Success"
  exit_code=0
  exit 0
else
  echo "Error: message was not received properly"
  exit_code=1
  exit 1
fi
