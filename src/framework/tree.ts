import { Logger } from './logger.js';
import { TestFilterResult } from './test_filter/index.js';
import { RunCase } from './test_group.js';

export interface FilterResultTreeNode {
  //description?: string;
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
  }
  yield path + terminator;
}

export function treeFromFilterResults(
  log: Logger,
  listing: IterableIterator<TestFilterResult>
): FilterResultTreeNode {
  function insertOrNew(n: FilterResultTreeNode, k: string): FilterResultTreeNode {
    const children = n.children!;
    if (children.has(k)) {
      return children.get(k) as FilterResultTreeNode;
    }
    const v = { children: new Map() };
    children.set(k, v);
    return v;
  }

  const tree = { children: new Map() };
  for (const f of listing) {
    const files = insertOrNew(tree, f.id.suite + ':');
    let tests = files;
    for (const path of iteratePath(f.id.path, ':')) {
      tests = insertOrNew(tests, f.id.suite + ':' + path);
    }

    if (!('g' in f.spec)) continue;

    const [tRec] = log.record(f.id);
    const fId = f.id.suite + ':' + f.id.path;
    for (const t of f.spec.g.iterate(tRec)) {
      let cases = tests;
      for (const path of iteratePath(t.id.test, '~')) {
        cases = insertOrNew(cases, fId + ':' + path);
      }

      const p = t.id.params ? JSON.stringify(t.id.params) : '';
      cases.children!.set(fId + ':' + t.id.test + '=' + p, {
        runCase: t,
      });
    }
  }
  return tree;
}
