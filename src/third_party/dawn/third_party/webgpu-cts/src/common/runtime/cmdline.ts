/* eslint no-console: "off" */

import { DefaultTestFileLoader } from '../internal/file_loader.js';
import { prettyPrintLog } from '../internal/logging/log_message.js';
import { Logger } from '../internal/logging/logger.js';
import { LiveTestCaseResult } from '../internal/logging/result.js';
import { parseQuery } from '../internal/query/parseQuery.js';
import { parseExpectationsForTestQuery } from '../internal/query/query.js';
import { Colors } from '../util/colors.js';
import { setGPUProvider } from '../util/navigator_gpu.js';
import { assert, unreachable } from '../util/util.js';

import sys from './helper/sys.js';

function usage(rc: number): never {
  console.log('Usage:');
  console.log(`  tools/run_${sys.type} [OPTIONS...] QUERIES...`);
  console.log(`  tools/run_${sys.type} 'unittests:*' 'webgpu:buffers,*'`);
  console.log('Options:');
  console.log('  --colors             Enable ANSI colors in output.');
  console.log('  --verbose            Print result/log of every test as it runs.');
  console.log(
    '  --list               Print all testcase names that match the given query and exit.'
  );
  console.log('  --debug              Include debug messages in logging.');
  console.log('  --print-json         Print the complete result JSON in the output.');
  console.log('  --expectations       Path to expectations file.');
  console.log('  --gpu-provider       Path to node module that provides the GPU implementation.');
  console.log('  --gpu-provider-flag  Flag to set on the gpu-provider as <flag>=<value>');
  console.log('  --quiet              Suppress summary information in output');
  return sys.exit(rc);
}

interface GPUProviderModule {
  create(flags: string[]): GPU;
}

type listModes = 'none' | 'cases' | 'unimplemented';

Colors.enabled = false;

let verbose = false;
let listMode: listModes = 'none';
let debug = false;
let printJSON = false;
let quiet = false;
let loadWebGPUExpectations: Promise<unknown> | undefined = undefined;
let gpuProviderModule: GPUProviderModule | undefined = undefined;

const queries: string[] = [];
const gpuProviderFlags: string[] = [];
for (let i = 0; i < sys.args.length; ++i) {
  const a = sys.args[i];
  if (a.startsWith('-')) {
    if (a === '--colors') {
      Colors.enabled = true;
    } else if (a === '--verbose') {
      verbose = true;
    } else if (a === '--list') {
      listMode = 'cases';
    } else if (a === '--list-unimplemented') {
      listMode = 'unimplemented';
    } else if (a === '--debug') {
      debug = true;
    } else if (a === '--print-json') {
      printJSON = true;
    } else if (a === '--expectations') {
      const expectationsFile = new URL(sys.args[++i], `file://${sys.cwd()}`).pathname;
      loadWebGPUExpectations = import(expectationsFile).then(m => m.expectations);
    } else if (a === '--gpu-provider') {
      const modulePath = sys.args[++i];
      gpuProviderModule = require(modulePath);
    } else if (a === '--gpu-provider-flag') {
      gpuProviderFlags.push(sys.args[++i]);
    } else if (a === '--quiet') {
      quiet = true;
    } else {
      console.log('unrecognized flag: ', a);
      usage(1);
    }
  } else {
    queries.push(a);
  }
}

if (gpuProviderModule) {
  setGPUProvider(() => gpuProviderModule!.create(gpuProviderFlags));
}

if (queries.length === 0) {
  console.log('no queries specified');
  usage(0);
}

(async () => {
  const loader = new DefaultTestFileLoader();
  assert(queries.length === 1, 'currently, there must be exactly one query on the cmd line');
  const filterQuery = parseQuery(queries[0]);
  const testcases = await loader.loadCases(filterQuery);
  const expectations = parseExpectationsForTestQuery(
    await (loadWebGPUExpectations ?? []),
    filterQuery
  );

  Logger.globalDebugMode = debug;
  const log = new Logger();

  const failed: Array<[string, LiveTestCaseResult]> = [];
  const warned: Array<[string, LiveTestCaseResult]> = [];
  const skipped: Array<[string, LiveTestCaseResult]> = [];

  let total = 0;

  for (const testcase of testcases) {
    const name = testcase.query.toString();
    switch (listMode) {
      case 'cases':
        console.log(name);
        continue;
      case 'unimplemented':
        if (testcase.isUnimplemented) {
          console.log(name);
        }
        continue;
      default:
        break;
    }

    const [rec, res] = log.record(name);
    await testcase.run(rec, expectations);

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

  if (listMode !== 'none') {
    return;
  }

  assert(total > 0, 'found no tests!');

  // MAINTENANCE_TODO: write results out somewhere (a file?)
  if (printJSON) {
    console.log(log.asJSON(2));
  }

  if (!quiet) {
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
  }

  if (failed.length || warned.length) {
    sys.exit(1);
  }
})().catch(ex => {
  console.log(ex.stack ?? ex.toString());
  sys.exit(1);
});

function printResults(results: Array<[string, LiveTestCaseResult]>): void {
  for (const [name, r] of results) {
    console.log(`[${r.status}] ${name} (${r.timems}ms). Log:`);
    if (r.logs) {
      for (const l of r.logs) {
        console.log(prettyPrintLog(l));
      }
    }
  }
}
