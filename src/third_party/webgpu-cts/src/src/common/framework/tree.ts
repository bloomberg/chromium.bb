import { Logger } from './logger.js';
import { stringifyPublicParams } from './params_utils.js';
import { TestFilterResult } from './test_filter/test_filter_result.js';
import { RunCase } from './test_group.js';

export interface FilterResultTreeNode {
  description?: string;
  runCase?: RunCase;
  children?: Map<string, FilterResultTreeNode>;
}

// e.g. iteratePath('a/b/c/d', ':') yields ['a/', 'a/b/', 'a/b/c/', 'a/b/c/d:']
function* iteratePath(path: string, terminator: string): IterableIterator<string> {
  const parts = path.split('/');
  if (parts.length > 1) {
    let partial = parts[0] + '/';
    yield partial;
    for (let i = 1; i < parts.length - 1; ++i) {
      partial += parts[i] + '/';
      yield partial;
    }
    // Path ends in '/' (so is a README).
    if (parts[parts.length - 1] === '') {
      return;
    }
  }
  yield path + terminator;
}

export function treeFromFilterResults(
  log: Logger,
  listing: IterableIterator<TestFilterResult>
): FilterResultTreeNode {
  function getOrInsert(n: FilterResultTreeNode, k: string): FilterResultTreeNode {
    const children = n.children!;
    if (children.has(k)) {
      return children.get(k)!;
    }
    const v = { children: new Map() };
    children.set(k, v);
    return v;
  }

  const tree = { children: new Map() };
  for (const f of listing) {
    const files = getOrInsert(tree, f.id.suite + ':');
    if (f.id.path === '') {
      // This is a suite README.
      files.description = f.spec.description.trim();
      continue;
    }

    let tests = files;
    for (const path of iteratePath(f.id.path, ':')) {
      tests = getOrInsert(tests, f.id.suite + ':' + path);
    }
    if (f.spec.description) {
      // This is a directory README or spec file.
      tests.description = f.spec.description.trim();
    }

    if (!('g' in f.spec)) {
      // This is a directory README.
      continue;
    }

    const [tRec] = log.record(f.id);
    const fId = f.id.suite + ':' + f.id.path;
    for (const t of f.spec.g.iterate(tRec)) {
      let cases = tests;
      for (const path of iteratePath(t.id.test, '~')) {
        cases = getOrInsert(cases, fId + ':' + path);
      }

      const p = stringifyPublicParams(t.id.params);
      cases.children!.set(fId + ':' + t.id.test + '=' + p, {
        runCase: t,
      });
    }
  }
  return tree;
}
