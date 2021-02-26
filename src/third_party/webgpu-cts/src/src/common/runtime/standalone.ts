// Implements the standalone test runner (see also: /standalone/index.html).

import { DefaultTestFileLoader } from '../framework/file_loader.js';
import { Logger } from '../framework/logging/logger.js';
import { LiveTestCaseResult } from '../framework/logging/result.js';
import { parseQuery } from '../framework/query/parseQuery.js';
import { TestQueryLevel } from '../framework/query/query.js';
import { TestTreeNode, TestSubtree, TestTreeLeaf } from '../framework/tree.js';
import { assert } from '../framework/util/util.js';

import { optionEnabled } from './helper/options.js';
import { TestWorker } from './helper/test_worker.js';

window.onbeforeunload = () => {
  // Prompt user before reloading if there are any results
  return haveSomeResults ? false : undefined;
};

let haveSomeResults = false;

const runnow = optionEnabled('runnow');
const debug = optionEnabled('debug');

const logger = new Logger(debug);

const worker = optionEnabled('worker') ? new TestWorker(debug) : undefined;

const resultsVis = document.getElementById('resultsVis')!;

type SetCheckedRecursively = () => void;
type GenerateSubtreeHTML = (parent: HTMLElement) => SetCheckedRecursively;
type RunSubtree = () => Promise<void>;

interface VisualizedSubtree {
  generateSubtreeHTML: GenerateSubtreeHTML;
  runSubtree: RunSubtree;
}

// DOM generation

function memoize<T>(fn: () => T): () => T {
  let value: T | undefined;
  return () => {
    if (value === undefined) {
      value = fn();
    }
    return value;
  };
}

function makeTreeNodeHTML(tree: TestTreeNode, parentLevel: TestQueryLevel): VisualizedSubtree {
  let subtree: VisualizedSubtree;

  if ('children' in tree) {
    subtree = makeSubtreeHTML(tree, parentLevel);
  } else {
    subtree = makeCaseHTML(tree);
  }

  const generateMyHTML = (parentElement: HTMLElement) => {
    const div = $('<div>').appendTo(parentElement)[0];
    return subtree.generateSubtreeHTML(div);
  };
  return { runSubtree: subtree.runSubtree, generateSubtreeHTML: generateMyHTML };
}

function makeCaseHTML(t: TestTreeLeaf): VisualizedSubtree {
  // Becomes set once the case has been run once.
  let caseResult: LiveTestCaseResult | undefined;

  // Becomes set once the DOM for this case exists.
  let updateRenderedResult: (() => void) | undefined;

  const name = t.query.toString();
  const runSubtree = async () => {
    haveSomeResults = true;
    const [rec, res] = logger.record(name);
    caseResult = res;
    if (worker) {
      await worker.run(rec, name);
    } else {
      await t.run(rec);
    }

    if (updateRenderedResult) updateRenderedResult();
  };

  const generateSubtreeHTML = (div: HTMLElement) => {
    div.classList.add('testcase');

    const caselogs = $('<div>').addClass('testcaselogs').hide();
    const [casehead, setChecked] = makeTreeNodeHeaderHTML(t, runSubtree, 2, checked => {
      checked ? caselogs.show() : caselogs.hide();
    });
    const casetime = $('<div>').addClass('testcasetime').html('ms').appendTo(casehead);
    div.appendChild(casehead);
    div.appendChild(caselogs[0]);

    updateRenderedResult = () => {
      if (caseResult) {
        div.setAttribute('data-status', caseResult.status);

        casetime.text(caseResult.timems.toFixed(4) + ' ms');

        if (caseResult.logs) {
          caselogs.empty();
          for (const l of caseResult.logs) {
            const caselog = $('<div>').addClass('testcaselog').appendTo(caselogs);
            $('<button>')
              .addClass('testcaselogbtn')
              .attr('alt', 'Log stack to console')
              .attr('title', 'Log stack to console')
              .appendTo(caselog)
              .on('click', () => {
                /* eslint-disable-next-line no-console */
                console.log(l);
              });
            $('<pre>').addClass('testcaselogtext').appendTo(caselog).text(l.toJSON());
          }
        }
      }
    };

    updateRenderedResult();

    return setChecked;
  };

  return { runSubtree, generateSubtreeHTML };
}

function makeSubtreeHTML(n: TestSubtree, parentLevel: TestQueryLevel): VisualizedSubtree {
  const { runSubtree, generateSubtreeHTML } = makeSubtreeChildrenHTML(
    n.children.values(),
    n.query.level
  );

  const generateMyHTML = (div: HTMLElement) => {
    const subtreeHTML = $('<div>').addClass('subtreechildren');
    const generateSubtree = memoize(() => generateSubtreeHTML(subtreeHTML[0]));

    // Hide subtree - it's not generated yet.
    subtreeHTML.hide();
    const [header, setChecked] = makeTreeNodeHeaderHTML(n, runSubtree, parentLevel, checked => {
      if (checked) {
        // Make sure the subtree is generated and then show it.
        generateSubtree();
        subtreeHTML.show();
      } else {
        subtreeHTML.hide();
      }
    });

    div.classList.add('subtree');
    div.classList.add(['', 'multifile', 'multitest', 'multicase'][n.query.level]);
    div.appendChild(header);
    div.appendChild(subtreeHTML[0]);

    return () => {
      setChecked();
      const setChildrenChecked = generateSubtree();
      setChildrenChecked();
    };
  };

  return { runSubtree, generateSubtreeHTML: generateMyHTML };
}

function makeSubtreeChildrenHTML(
  children: Iterable<TestTreeNode>,
  parentLevel: TestQueryLevel
): VisualizedSubtree {
  const childFns = Array.from(children, subtree => makeTreeNodeHTML(subtree, parentLevel));

  const runMySubtree = async () => {
    for (const { runSubtree } of childFns) {
      await runSubtree();
    }
  };
  const generateMyHTML = (div: HTMLElement) => {
    const setChildrenChecked = Array.from(childFns, ({ generateSubtreeHTML }) =>
      generateSubtreeHTML(div)
    );

    return () => {
      for (const setChildChecked of setChildrenChecked) {
        setChildChecked();
      }
    };
  };

  return { runSubtree: runMySubtree, generateSubtreeHTML: generateMyHTML };
}

function makeTreeNodeHeaderHTML(
  n: TestTreeNode,
  runSubtree: RunSubtree,
  parentLevel: TestQueryLevel,
  onChange: (checked: boolean) => void
): [HTMLElement, SetCheckedRecursively] {
  const isLeaf = 'run' in n;
  const div = $('<div>').addClass('nodeheader');

  const setChecked = () => {
    if (checkbox) {
      checkbox.prop('checked', true); // (does not fire onChange)
      onChange(true);
    }
  };

  let checkbox: JQuery<HTMLElement> | undefined;
  const href = `?${worker ? 'worker&' : ''}${debug ? 'debug&' : ''}q=${n.query.toString()}`;
  if (onChange) {
    checkbox = $('<input>')
      .attr('type', 'checkbox')
      .addClass('collapsebtn')
      .on('change', function (this) {
        onChange((this as HTMLInputElement).checked);
      })
      .attr('alt', 'Expand')
      .attr('title', 'Expand')
      .appendTo(div);

    // Expand the shallower parts of the tree at load.
    // Also expand completely within subtrees that are at the same query level
    // (e.g. s:f:t,* and s:f:t,t,*).
    if (n.query.level <= lastQueryLevelToExpand || n.query.level === parentLevel) {
      setChecked();
    }
  }
  const runtext = isLeaf ? 'Run case' : 'Run subtree';
  $('<button>')
    .addClass(isLeaf ? 'leafrun' : 'subtreerun')
    .attr('alt', runtext)
    .attr('title', runtext)
    .on('click', async () => {
      await runSubtree();
    })
    .appendTo(div);
  $('<a>')
    .addClass('nodelink')
    .attr('href', href)
    .attr('alt', 'Open')
    .attr('title', 'Open')
    .appendTo(div);
  const nodetitle = $('<div>').addClass('nodetitle').appendTo(div);
  $('<input>')
    .attr('type', 'text')
    .prop('readonly', true)
    .addClass('nodequery')
    .val(n.query.toString())
    .appendTo(nodetitle);
  if ('description' in n && n.description) {
    nodetitle.append('&nbsp;');
    $('<pre>') //
      .addClass('nodedescription')
      .text(n.description)
      .appendTo(nodetitle);
  }
  return [div[0], setChecked];
}

// Collapse s:f:t:* or s:f:t:c by default.
let lastQueryLevelToExpand: TestQueryLevel = 2;

(async () => {
  const loader = new DefaultTestFileLoader();

  // TODO: start populating page before waiting for everything to load?
  const qs = new URLSearchParams(window.location.search).getAll('q');
  if (qs.length === 0) {
    qs.push('webgpu:*');
  }

  // Update the URL bar to match the exact current options.
  {
    let url = window.location.protocol + '//' + window.location.host + window.location.pathname;
    url +=
      '?' +
      new URLSearchParams([
        ['runnow', runnow ? '1' : '0'],
        ['worker', worker ? '1' : '0'],
        ['debug', debug ? '1' : '0'],
      ]).toString() +
      '&' +
      qs.map(q => 'q=' + q).join('&');
    window.history.replaceState(null, '', url);
  }

  assert(qs.length === 1, 'currently, there must be exactly one ?q=');
  const rootQuery = parseQuery(qs[0]);
  if (rootQuery.level > lastQueryLevelToExpand) {
    lastQueryLevelToExpand = rootQuery.level;
  }
  const tree = await loader.loadTree(rootQuery);

  tree.dissolveLevelBoundaries();

  const { runSubtree, generateSubtreeHTML } = makeSubtreeHTML(tree.root, 1);
  const setTreeCheckedRecursively = generateSubtreeHTML(resultsVis);

  document.getElementById('expandall')!.addEventListener('click', () => {
    setTreeCheckedRecursively();
  });

  document.getElementById('copyResultsJSON')!.addEventListener('click', () => {
    navigator.clipboard.writeText(logger.asJSON(2));
  });

  if (runnow) {
    runSubtree();
  }
})();
