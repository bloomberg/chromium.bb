// tslint:disable: no-console

import * as fs from 'fs';
import * as process from 'process';

import { TestSpecID } from '../framework/id.js';
import { assert, unreachable } from '../framework/index.js';
import { TestLoader } from '../framework/loader.js';
import { LiveTestCaseResult, Logger } from '../framework/logger.js';
import { makeQueryString } from '../framework/url_query.js';

function usage(rc: number): never {
  console.log('Usage:');
  console.log('  tools/run [OPTIONS...] QUERIES...');
  console.log('  tools/run unittests: cts:buffers/');
  console.log('Options:');
  console.log('  --verbose     Print result/log of every test as it runs.');
  console.log('  --debug       Include debug messages in logging.');
  console.log('  --print-json  Print the complete result JSON in the output.');
  return process.exit(rc);
}

if (!fs.existsSync('src/runtime/cmdline.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

let verbose = false;
let debug = false;
let printJSON = false;
const filterArgs = [];
for (const a of process.argv.slice(2)) {
  if (a.startsWith('-')) {
    if (a === '--verbose') {
      verbose = true;
    } else if (a === '--debug') {
      debug = true;
    } else if (a === '--print-json') {
      printJSON = true;
    } else {
      usage(1);
    }
  } else {
    filterArgs.push(a);
  }
}

if (filterArgs.length === 0) {
  usage(0);
}

(async () => {
  try {
    const loader = new TestLoader();
    const files = await loader.loadTestsFromCmdLine(filterArgs);

    const log = new Logger();

    const failed: Array<[TestSpecID, LiveTestCaseResult]> = [];
    const warned: Array<[TestSpecID, LiveTestCaseResult]> = [];
    const skipped: Array<[TestSpecID, LiveTestCaseResult]> = [];

    let total = 0;
    for (const f of files) {
      if (!('g' in f.spec)) {
        continue;
      }

      const [rec] = log.record(f.id);
      for (const t of f.spec.g.iterate(rec)) {
        const res = await t.run(debug);
        if (verbose) {
          printResults([[f.id, res]]);
        }

        total++;
        if (res.status === 'pass') {
        } else if (res.status === 'fail') {
          failed.push([f.id, res]);
        } else if (res.status === 'warn') {
          warned.push([f.id, res]);
        } else if (res.status === 'skip') {
          skipped.push([f.id, res]);
        } else {
          unreachable('unrecognized status');
        }
      }
    }

    assert(total > 0, 'found no tests!');

    // TODO: write results out somewhere (a file?)
    if (printJSON) {
      console.log(log.asJSON(2));
    }

    if (skipped.length) {
      console.log('');
      console.log('** Skipped **');
      printResults(skipped);
    }
    if (warned.length) {
      console.log('');
      console.log('** Warnings **');
      printResults(warned);
    }
    if (failed.length) {
      console.log('');
      console.log('** Failures **');
      printResults(failed);
    }

    const passed = total - warned.length - failed.length - skipped.length;
    function pct(x: number): string {
      return ((100 * x) / total).toFixed(2);
    }
    function rpt(x: number): string {
      const xs = x.toString().padStart(1 + Math.log10(total), ' ');
      return `${xs} / ${total} = ${pct(x).padStart(6, ' ')}%`;
    }
    console.log('');
    console.log(`** Summary **
Passed  w/o warnings = ${rpt(passed)}
Passed with warnings = ${rpt(warned.length)}
Skipped              = ${rpt(skipped.length)}
Failed               = ${rpt(failed.length)}`);

    if (failed.length || warned.length) {
      process.exit(1);
    }
  } catch (ex) {
    console.log(ex);
    process.exit(1);
  }
})();

function printResults(results: Array<[TestSpecID, LiveTestCaseResult]>): void {
  for (const [id, r] of results) {
    console.log(`[${r.status}] ${makeQueryString(id, r)} (${r.timems}ms). Log:`);
    if (r.logs) {
      for (const l of r.logs) {
        console.log('  - ' + l.toJSON().replace(/\n/g, '\n    '));
      }
    }
  }
}
