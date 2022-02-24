// Copyright 2022 The Chromium Authors.All rights reserved.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

// Note: Prefer to run this file via generate_telemetry_expecations.py which
// parses arguments and forwards them to this .js script.
const ctsRoot = process.argv[2];
const { expectations } = require(process.argv[3]);
const expectationsOut = process.argv[4] // Optional

const fs = require('fs');

const { DefaultTestFileLoader } = require(`${ctsRoot}/common/internal/file_loader`);
const { parseQuery } = require(`${ctsRoot}/common/internal/query/parseQuery`);

(async () => {
  const outStream = expectationsOut ? fs.createWriteStream(expectationsOut)
                                    : process.stdout;

  try {
    const loader = new DefaultTestFileLoader();
    for (const entry of expectations) {
      for (const testcase of await loader.loadCases(parseQuery(entry.q))) {
        const name = testcase.query.toString();
        if (entry.b) {
          outStream.write(entry.b);
          outStream.write(' ');
        }

        if (entry.t) {
          outStream.write('[')
          for (const tag of entry.t) {
            outStream.write(' ');
            outStream.write(tag);
          }
          outStream.write(' ] ')
        }

        outStream.write(name);

        if (entry.e) {
          outStream.write(' [')
          for (const exp of entry.e) {
            outStream.write(' ');
            outStream.write(exp);
          }
          outStream.write(' ]')
        }

        outStream.write('\n');
      }
    }
  } finally {
    // An error may have happened. Wait for the stream to finish writing
    // before exiting in case seeing the intermediate results is helpful.
    await new Promise(resolve => outStream.once('finish', resolve));
  }
})();
