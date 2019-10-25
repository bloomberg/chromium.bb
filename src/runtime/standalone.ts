// Implements the standalone test runner (see also: index.html).

import { RunCase } from '../framework/index.js';
import { TestLoader } from '../framework/loader.js';
import { Logger, LiveTestCaseResult } from '../framework/logger.js';
import { FilterResultTreeNode, treeFromFilterResults } from '../framework/tree.js';
import { encodeSelectively } from '../framework/url_query.js';

import { TestWorker } from './helper/test_worker.js';

window.onbeforeunload = () => {
  // Prompt user before reloading if there are any results
  return haveSomeResults ? false : undefined;
};

let haveSomeResults = false;
const log = new Logger();

const url = new URL(window.location.toString());
const runnow = url.searchParams.get('runnow') === '1';
const debug = url.searchParams.get('debug') === '1';

const worker = url.searchParams.get('worker') === '1' ? new TestWorker() : undefined;

const resultsVis = document.getElementById('resultsVis')!;
const resultsJSON = document.getElementById('resultsJSON')!;

type RunSubtree = () => Promise<void>;

// DOM generation

function makeTreeNodeHTML(name: string, tree: FilterResultTreeNode): [HTMLElement, RunSubtree] {
  if (tree.children) {
    return makeSubtreeHTML(name, tree);
  } else {
    return makeCaseHTML(name, tree.runCase!);
  }
}

function makeCaseHTML(name: string, t: RunCase): [HTMLElement, RunSubtree] {
  const div = $('<div>').addClass('testcase');

  const runSubtree = async () => {
    haveSomeResults = true;
    let res: LiveTestCaseResult;
    if (worker) {
      res = await worker.run(name, debug);
      t.injectResult(res);
    } else {
      res = await t.run(debug);
    }

    casetime.text(res.timems.toFixed(4) + ' ms');

    div
      .removeClass('pass')
      .removeClass('warn')
      .removeClass('fail')
      .addClass(res.status);

    if (res.logs) {
      caselogs.empty();
      for (const l of res.logs) {
        $('<div>')
          .addClass('testcaselog')
          .appendTo(caselogs)
          .text(l);
      }
    }
  };

  const casehead = makeTreeNodeHeaderHTML(name, undefined, runSubtree);
  div.append(casehead);
  const casetime = $('<div>')
    .addClass('testcasetime')
    .html('ms')
    .appendTo(casehead);
  const caselogs = $('<div>')
    .addClass('testcaselogs')
    .appendTo(div);

  return [div[0], runSubtree];
}

function makeSubtreeHTML(name: string, subtree: FilterResultTreeNode): [HTMLElement, RunSubtree] {
  const div = $('<div>').addClass('subtree');

  const header = makeTreeNodeHeaderHTML(name, subtree.description, () => {
    return runSubtree();
  });
  div.append(header);

  const subtreeHTML = $('<div>')
    .addClass('subtreechildren')
    .appendTo(div);
  const runSubtree = makeSubtreeChildrenHTML(subtreeHTML[0], subtree.children!);

  return [div[0], runSubtree];
}

function makeSubtreeChildrenHTML(
  div: HTMLElement,
  children: Map<string, FilterResultTreeNode>
): RunSubtree {
  const runSubtreeFns: RunSubtree[] = [];
  for (const [name, subtree] of children) {
    const [subtreeHTML, runSubtree] = makeTreeNodeHTML(name, subtree);
    div.append(subtreeHTML);
    runSubtreeFns.push(runSubtree);
  }

  return async () => {
    for (const runSubtree of runSubtreeFns) {
      await runSubtree();
    }
  };
}

function makeTreeNodeHeaderHTML(
  name: string,
  description: string | undefined,
  runSubtree: RunSubtree
): HTMLElement {
  const div = $('<div>').addClass('nodeheader');

  const nameEncoded = encodeSelectively(name);
  let nameHTML;
  {
    const i = nameEncoded.indexOf('{');
    const n1 = i === -1 ? nameEncoded : nameEncoded.slice(0, i + 1);
    const n2 = i === -1 ? '' : nameEncoded.slice(i + 1);
    nameHTML = n1.replace(/:/g, ':<wbr>') + '<wbr>' + n2.replace(/,/g, ',<wbr>');
  }

  $('<div>')
    .addClass('noderunbox')
    .appendTo(div)
    .append(
      $('<button>')
        .addClass('noderun')
        .attr('alt', 'run')
        .html('&#x25b6;')
        .on('click', async () => {
          await runSubtree();
          updateJSON();
        })
    );
  const nodetitle = $('<div>')
    .addClass('nodetitle')
    .appendTo(div);
  $('<span>')
    .addClass('nodename')
    .html(nameHTML)
    .appendTo(nodetitle);
  $('<a>')
    .addClass('nodelink')
    .attr('href', '?q=' + nameEncoded)
    .attr('alt', 'open')
    .html('&#x2b08;')
    .appendTo(nodetitle);
  if (description) {
    $('<div>')
      .addClass('nodedescription')
      .html(description.replace(/\n/g, '<br>'))
      .appendTo(nodetitle);
  }
  return div[0];
}

function updateJSON(): void {
  resultsJSON.textContent = log.asJSON(2);
}

(async () => {
  const loader = new TestLoader();

  // TODO: everything after this point is very similar across the three runtimes.
  // TODO: start populating page before waiting for everything to load?
  const files = await loader.loadTestsFromQuery(window.location.search);
  const tree = treeFromFilterResults(log, files);

  const runSubtree = makeSubtreeChildrenHTML(resultsVis, tree.children!);

  if (runnow) {
    runSubtree();
  }
})();
