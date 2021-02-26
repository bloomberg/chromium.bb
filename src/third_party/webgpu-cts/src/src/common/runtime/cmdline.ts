/* eslint no-console: "off" */
/* eslint no-process-exit: "off" */

import * as fs from 'fs';
import * as process from 'process';

import { DefaultTestFileLoader } from '../framework/file_loader.js';
import { Logger } from '../framework/logging/logger.js';
import { LiveTestCaseResult } from '../framework/logging/result.js';
import { parseQuery } from '../framework/query/parseQuery.js';
import { assert, unreachable } from '../framework/util/util.js';

function usage(rc: number): never {
  console.log('Usage:');
  console.log('  tools/run [OPTIONS...] QUERIES...');
  console.log("  tools/run 'unittests:*' 'webgpu:buffers,*'");
  console.log('Options:');
  console.log('  --verbose     Print result/log of every test as it runs.');
  console.log('  --debug       Include debug messages in logging.');
  console.log('  --print-json  Print the complete result JSON in the output.');
  return process.exit(rc);
}

if (!fs.existsSync('src/common/runtime/cmdline.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

let verbose = false;
let debug = false;
let printJSON = false;
const queries: string[] = [];
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
    queries.push(a);
  }
}

if (queries.length === 0) {
  usage(0);
}

(async () => {
  try {
    const loader = new DefaultTestFileLoader();
    assert(queries.length === 1, 'currently, there must be exactly one query on the cmd line');
    const testcases = await loader.loadCases(parseQuery(queries[0]));

    const log = new Logger(debug);

    const failed: Array<[string, LiveTestCaseResult]> = [];
    const warned: Array<[string, LiveTestCaseResult]> = [];
    const skipped: Array<[string, LiveTestCaseResult]> = [];

    let total = 0;

    for (const testcase of testcases) {
      const name = testcase.query.toString();
      const [rec, res] = log.record(name);
      await testcase.run(rec);

      if (verbose) {
        printResults([[name, res]]);
      }

      total++;
      switch (res.status) {
        case 'pass':
          break;
        case 'fail':
          failed.push([name, res]);
          break;
        case 'warn':
          warned.push([name, res]);
          break;
        case 'skip':
          skipped.push([name, res]);
          break;
        default:
          unreachable('unrecognized status');
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
    const pct = (x: number) => ((100 * x) / total).toFixed(2);
    const rpt = (x: number) => {
      const xs = x.toString().padStart(1 + Math.log10(total), ' ');
      return `${xs} / ${total} = ${pct(x).padStart(6, ' ')}%`;
    };
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

function printResults(results: Array<[string, LiveTestCaseResult]>): void {
  for (const [name, r] of results) {
    console.log(`[${r.status}] ${name} (${r.timems}ms). Log:`);
    if (r.logs) {
      for (const l of r.logs) {
        console.log('  - ' + l.toJSON().replace(/\n/g, '\n    '));
      }
    }
  }
}
