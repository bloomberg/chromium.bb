// tslint:disable: no-console

import * as fs from 'fs';
import * as process from 'process';

import { TestSpecFile, TestLoader } from '../framework/loader.js';
import { Logger, TestCaseLiveResult } from '../framework/logger.js';

function usage(rc: number) {
  console.log('Usage:');
  console.log('  tools/run [FILTERS...]');
  console.log('  tools/run unittests: cts:buffers/');
  process.exit(rc);
}

if (process.argv.length <= 2) {
  usage(0);
}

if (!fs.existsSync('src/tools/run.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

let verbose = false;
const filterArgs = [];
for (const a of process.argv.slice(2)) {
  if (a.startsWith('-')) {
    if (a === '--verbose' || a === '-v') {
      verbose = true;
    } else {
      usage(1);
    }
  } else {
    filterArgs.push(a);
  }
}

(async () => {
  try {
    const loader = new TestLoader();
    const listing = await loader.loadTestsFromCmdLine(filterArgs);

    const log = new Logger();
    const entries = await Promise.all(
      Array.from(listing, ({ suite, path, spec }) =>
        spec.then((s: TestSpecFile) => ({ suite, path, g: s.g }))
      )
    );

    const failed: TestCaseLiveResult[] = [];
    const warned: TestCaseLiveResult[] = [];

    // TODO: don't run all tests all at once
    const running = [];
    for (const entry of entries) {
      const { path, g } = entry;
      if (!g) {
        continue;
      }

      const [rec] = log.record(path);
      for (const t of g.iterate(rec)) {
        running.push(
          (async () => {
            const res = await t.run();
            if (res.status === 'fail') {
              failed.push(res);
            }
            if (res.status === 'warn') {
              warned.push(res);
            }
          })()
        );
      }
    }

    if (running.length === 0) {
      throw new Error('found no tests!');
    }

    await Promise.all(running);

    // TODO: write results out somewhere (a file?)
    if (verbose) {
      console.log(log.asJSON(2));
    }

    if (warned.length) {
      console.log('');
      console.log('** Warnings **');
      for (const r of warned) {
        console.log(r);
      }
    }
    if (failed.length) {
      console.log('');
      console.log('** Failures **');
      for (const r of failed) {
        console.log(r);
      }
    }

    const total = running.length;
    const passed = total - warned.length - failed.length;
    function pct(x: number) {
      return ((100 * x) / total).toFixed(2);
    }
    function rpt(x: number) {
      const xs = x.toString().padStart(1 + Math.log10(total), ' ');
      return `${xs} / ${total} = ${pct(x).padStart(6, ' ')}%`;
    }
    console.log('');
    console.log(`** Summary **
Passed  w/o warnings = ${rpt(passed)}
Passed with warnings = ${rpt(warned.length)}
Failed               = ${rpt(failed.length)}`);

    if (failed.length || warned.length) {
      process.exit(1);
    }
  } catch (ex) {
    console.log(ex);
  }
})();
