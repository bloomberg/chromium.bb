#!/bin/bash

# linux specific test for nrd_xfer

set -o nounset
set -o errexit

readonly EMULATOR=${EMULATOR:-}
readonly TEST_EXE=$1
readonly TMP_FILE="/tmp/nrd_xfer_test.$$"
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
${EMULATOR} ${TEST_EXE} -s  > ${TMP_FILE} &

sleep ${WAIT_TIME}

readonly ADDR=$(tail -1 ${TMP_FILE})
echo "socket address is ${ADDR}"

echo
echo "running client"
echo
${EMULATOR} ${TEST_EXE} -c ${ADDR} -m ${MESSAGE}

sleep ${WAIT_TIME}

echo
echo "dump output"
echo
cat ${TMP_FILE}

echo
echo "check"
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
