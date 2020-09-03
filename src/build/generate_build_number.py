#!/usr/bin/env python2

# Copyright (C) 2019 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import bbutil
import blpwtk2
import re
import sys

def generateBuildNumber(contentVersion):
  # Get list of all tags
  cmd = 'git tag --sort=-v:refname'
  allTags = bbutil.getSplitShellOutput(cmd)

  # Get the release branch number
  currentBranchName = bbutil.getStrippedShellOutput(
          "git rev-parse --abbrev-ref HEAD")

  match = re.match('release/trml/(\\d*)', currentBranchName)

  if not match:
    raise Exception("You must be in a release branch to start a devkit build")

  releaseBranchNumber = int(match.group(1))

  # Iterate through all tags and find the latest build number with a matching
  # version of Content and a matching release branch number.  If we don't find
  # a tag, search by the previous release branch number.
  latestBuildNumber = 0
  latestBranchNumber = releaseBranchNumber

  while latestBranchNumber > 0:
    for tag in allTags:
      if contentVersion in tag:
        matches = re.match('^devkit/stable/{0}/bb(\\d*)(\\d\\d)$'.format(
                    contentVersion), tag)
        if int(matches.group(1)) == latestBranchNumber:
          latestBuildNumber = int(matches.group(2))
          break

    if 0 != latestBuildNumber:
      break

    latestBranchNumber -= 1

  if latestBuildNumber > 0:
    # Check to see if there are any new commits since the last build
    previousTag = 'devkit/stable/{0}/bb{1}{2:02d}'.format(
            contentVersion, latestBranchNumber, latestBuildNumber)

    commonAncestor = bbutil.getStrippedShellOutput(
            "git merge-base {0} HEAD".format(previousTag))
    if bbutil.getHEADSha() == commonAncestor:
      raise Exception("No new changes since " + previousTag)

  # If the last release branch was older than the current one, reset the
  # build counter
  if latestBranchNumber != releaseBranchNumber:
    latestBuildNumber = 0

  return "bb{0}{1:02d}".format(releaseBranchNumber, latestBuildNumber+1)

def main():
  print(generateBuildNumber(blpwtk2.content_version))

if __name__ == '__main__':
  sys.exit(main())

