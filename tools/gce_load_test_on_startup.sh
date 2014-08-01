#!/bin/bash
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# Script to be used on GCE slave startup to initiate a load test. Each VM will
# fire an equivalent number of bots and clients. Fine tune the value depending
# on what kind of load test is desired.
#
# Please see https://developers.google.com/compute/docs/howtos/startupscript for
# more details on how to use this script.
#
# The script may run as root, which is counter intuitive. We don't mind much
# because is deleted right after the load test, but still keep this in mind!

set -e

## Settings

# Server to load test against. Please set to your server.
SERVER=https://CHANGE-ME-TO-PROPER-VALUE.appspot.com

# Source code to use.
REPO=https://code.google.com/p/swarming.client.git

# Once the tasks are completed, one can harvest the logs back:
#   scp "slave:/var/log/swarming/*.*" .
LOG=/var/log/swarming


## Actual work starts here.
echo 'dirname $0'
echo $PWD
mkdir -p $LOG

# Installs python and git.
apt-get install -y git-core python

# It will end up in /client.
git clone $REPO client
cd client

# This is assuming 8 cores system, so it's worth having 8 different python
# processes, 4 fake bots load processes, 4 fake clients load processes. Each
# load test process creates 250 instances. This gives us a nice 1k bots, 1k
# clients per VM which makes it simple to size the load test, want 20k bots and
# 20k clients? Fire 20 VMs. It assumes a high network throughput per host since
# 250 bots + 250 clients generates a fair amount of HTTPS requests. This is not
# a problem on GCE, these VMs have pretty high network I/O. This may not hold
# true on other Cloud Hosting provider. Tune accordingly!

# Start the bots first, 250 fake bots per process.
for i in {1..4}; do
  ./tools/swarming_load_test_bot.py -S $SERVER --slaves 250 --suffix $i \
      --dump=$LOG/bot$i.json > $LOG/bot$i.log &
  echo "Bot $i pid: $!"
done

# Each client will send by default 16 tasks/sec * 60 sec, so 16*60*4 = 3840
# tasks per VM.
for i in {1..4}; do
  ./tools/swarming_load_test_client.py -S $SERVER --concurrent 250 \
      --dump=$LOG/client$i.json > $LOG/client$i.log &
  echo "Client $i pid: $!"
done
