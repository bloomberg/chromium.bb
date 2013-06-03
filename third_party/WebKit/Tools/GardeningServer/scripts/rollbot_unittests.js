/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


(function () {

module("rollbot");

var kSearchResults = {
  "cursor": "long_string_we_call_cursor",
  "results": [
    {
      "description": "Blink roll 151668:151677\n\nhttp:\/\/build.chromium.org\/f\/chromium\/perf\/dashboard\/ui\/changelog_blink.html?url=\/trunk&range=151669:151677&mode=html\nTBR=\nBUG=",
      "cc": [
        "chromium-reviews@chromium.org",
      ],
      "reviewers": [
      ],
      "messages": [
        {
          "sender": "eseidel@chromium.org",
          "recipients": [
            "eseidel@chromium.org",
            "chromium-reviews@chromium.org",
          ],
          "text": "This roll was automatically created by the Blink AutoRollBot (crbug.com\/242461).\n",
          "disapproval": false,
          "date": "2013-06-03 18:14:34.033780",
          "approval": false
        },
      ],
      "owner_email": "eseidel@chromium.org",
      "private": false,
      "base_url": "https:\/\/chromium.googlesource.com\/chromium\/src.git@master",
      "owner": "eseidel",
      "subject": "Blink roll 151668:151677",
      "created": "2013-06-03 18:14:28.926040",
      "patchsets": [
        1
      ],
      "modified": "2013-06-03 18:14:46.869990",
      "closed": false,
      "commit": true,
      "issue": 16337011
    },
    {
      "description": "Add --json-output option to layout_test_wrapper.py\n\nBUG=238381",
      "cc": [
        "chromium-reviews@chromium.org",
      ],
      "reviewers": [
        "iannucci@chromium.org"
      ],
      "messages": [
        {
          "sender": "eseidel@chromium.org",
          "recipients": [
            "eseidel@chromium.org",
            "chromium-reviews@chromium.org",
          ],
          "text": "I'm not quite sure how to test this code.\n\nI'm also ",
          "disapproval": false,
          "date": "2013-05-30 23:42:39.309160",
          "approval": false
        },
      ]
    }
  ]
};

test("fetchCurrentRoll", 6, function() {
    var simulator = new NetworkSimulator();
    simulator.get = function(url, callback)
    {
        simulator.scheduleCallback(function() {
            callback(kSearchResults);
        });
    };

    simulator.runTest(function() {
        rollbot.fetchCurrentRoll(function(roll) {
            equals(roll.issue, 16337011);
            equals(roll.url, "https://codereview.chromium.org/16337011");
            equals(roll.isStopped, false);
            equals(roll.fromRevision, "151668");
            equals(roll.toRevision, "151677");
        });
    });
});

})();
