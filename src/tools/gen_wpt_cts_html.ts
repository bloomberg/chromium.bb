// tslint:disable: no-console

import { promises as fs } from 'fs';

import { generateMinimalQueryList } from '../framework/generate_minimal_query_list.js';
import { TestSuiteListingEntry } from '../framework/listing.js';
import { TestLoader } from '../framework/loader.js';
import { listing } from '../suites/cts/index.js';

function printUsageAndExit(rc: number): void {
  console.error(`\
Usage:
  tools/gen_wpt_cts_html OUTPUT_FILE TEMPLATE_FILE [ARGUMENTS_PREFIXES_FILE EXPECTATIONS_FILE EXPECTATIONS_PREFIX SUITE]
  tools/gen_wpt_cts_html out-wpt/cts.html templates/cts.html
  tools/gen_wpt_cts_html my/path/to/cts.html templates/cts.html arguments.txt myexpectations.txt 'path/to/cts.html' cts

where arguments.txt is a file containing a list of arguments prefixes to both generate and expect
in the expectations. The entire variant list generation runs *once per prefix*, so this
multiplies the size of the variant list.

  ?worker=0&q=
  ?worker=1&q=

and myexpectations.txt is a file containing a list of WPT paths to suppress, e.g.:

  path/to/cts.html?worker=0&q=cts:a/foo:bar={"x":1}
  path/to/cts.html?worker=1&q=cts:a/foo:bar={"x":1}

  path/to/cts.html?worker=1&q=cts:a/foo:bar={"x":3}
`);
  process.exit(rc);
}

if (process.argv.length !== 4 && process.argv.length !== 8) {
  printUsageAndExit(0);
}

const [
  ,
  ,
  outFile,
  templateFile,
  argsPrefixesFile,
  expectationsFile,
  expectationsPrefix,
  suite,
] = process.argv;

(async () => {
  if (process.argv.length === 4) {
    const entries = (await listing) as TestSuiteListingEntry[];
    const lines = entries
      // Exclude READMEs.
      .filter(l => l.path.length !== 0 && !l.path.endsWith('/'))
      .map(l => '?q=cts:' + l.path);
    await generateFile(lines);
  } else {
    // Prefixes sorted from longest to shortest
    const argsPrefixes = (await fs.readFile(argsPrefixesFile, 'utf8'))
      .split('\n')
      .filter(a => a.length)
      .sort((a, b) => b.length - a.length);
    const expectationLines = (await fs.readFile(expectationsFile, 'utf8'))
      .split('\n')
      .filter(l => l.length);

    const expectations: Map<string, string[]> = new Map();
    for (const prefix of argsPrefixes) {
      expectations.set(prefix, []);
    }

    expLoop: for (const exp of expectationLines) {
      // Take each expectation for the longest prefix it matches.
      for (const argsPrefix of argsPrefixes) {
        const prefix = expectationsPrefix + argsPrefix;
        if (exp.startsWith(prefix)) {
          expectations.get(argsPrefix)!.push(exp.substring(prefix.length));
          continue expLoop;
        }
      }
      throw new Error('All input lines must start with one of the prefixes. ' + exp);
    }

    const loader = new TestLoader();
    const files = Array.from(await loader.loadTestsFromCmdLine([suite + ':']));

    const lines = [];
    for (const prefix of argsPrefixes) {
      lines.push(undefined);
      for (const q of await generateMinimalQueryList(files, expectations.get(prefix)!)) {
        lines.push(prefix + q);
      }
    }
    await generateFile(lines);
  }
})();

async function generateFile(lines: Array<string | undefined>): Promise<void> {
  let result = '';
  result += '<!-- AUTO-GENERATED - DO NOT EDIT. See WebGPU CTS: tools/gen_wpt_cts_html. -->\n';

  result += await fs.readFile(templateFile, 'utf8');

  for (const line of lines) {
    if (line === undefined) {
      result += '\n';
    } else {
      result += `<meta name=variant content='${line}'>\n`;
    }
  }

  await fs.writeFile(outFile, result);
}
