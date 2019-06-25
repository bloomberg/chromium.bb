// tslint:disable: no-console

import * as fs from 'fs';
import * as process from 'process';

import { ITestNode } from '../framework/loader';
import { Logger, IResult } from '../framework/logger';
import { parseFilters } from '../framework/url_query';
import { TestLoaderNode } from './loader_node';

function usage(rc: number) {
  console.log('Usage:');
  console.log('  node tools/run [FILTERS...]');
  console.log('  node tools/run unittests: demos:params:');
  process.exit(rc);
}

if (process.argv.length <= 2) {
  usage(0);
}

if (!fs.existsSync('src/tools/run.ts')) {
  console.log('Must be run from repository root');
  usage(1);
}

(async () => {
  const filters = parseFilters(process.argv.slice(2));
  const loader = new TestLoaderNode();
  const listing = await loader.loadTests('./out/suites', filters);

  const log = new Logger();
  const entries = await Promise.all(
    Array.from(listing, ({ suite, path, node }) =>
      node.then((n: ITestNode) => ({ suite, path, node: n }))
    )
  );

  const failed: IResult[] = [];
  const warned: IResult[] = [];

  // TODO: don't run all tests all at once
  const running = [];
  for (const entry of entries) {
    const {
      path,
      node: { g: group },
    } = entry;
    if (!group) {
      continue;
    }

    const [, rec] = log.record(path);
    for (const t of group.iterate(rec)) {
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

  await Promise.all(running);

  // TODO: write results out somewhere (a file?)
  // const resultsJSON = JSON.stringify(log.results, undefined, 2);

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
})();
