import { RunCase, RunFn } from '../internal/test_group.js';
import { assert } from '../util/util.js';

import { TestFileLoader } from './file_loader.js';
import { TestParamsRW } from './params_utils.js';
import { compareQueries, Ordering } from './query/compare.js';
import {
  TestQuery,
  TestQueryMultiCase,
  TestQuerySingleCase,
  TestQueryMultiFile,
  TestQueryMultiTest,
} from './query/query.js';
import { kBigSeparator, kWildcard, kPathSeparator, kParamSeparator } from './query/separators.js';
import { stringifySingleParam } from './query/stringify_params.js';
import { StacklessError } from './util.js';

// `loadTreeForQuery()` loads a TestTree for a given queryToLoad.
// The resulting tree is a linked-list all the way from `suite:*` to queryToLoad,
// and under queryToLoad is a tree containing every case matched by queryToLoad.
//
// `subqueriesToExpand` influences the `collapsible` flag on nodes in the resulting tree.
// A node is considered "collapsible" if none of the subqueriesToExpand is a StrictSubset
// of that node.
//
// In WebKit/Blink-style web_tests, an expectation file marks individual cts.https.html "variants
// as "Failure", "Crash", etc. By passing in the list of expectations as the subqueriesToExpand,
// we can programmatically subdivide the cts.https.html "variants" list to be able to implement
// arbitrarily-fine suppressions (instead of having to suppress entire test files, which would
// lose a lot of coverage).
//
// `iterateCollapsedNodes()` produces the list of queries for the variants list.
//
// Though somewhat complicated, this system has important benefits:
//   - Avoids having to suppress entire test files, which would cause large test coverage loss.
//   - Minimizes the number of page loads needed for fine-grained suppressions.
//     (In the naive case, we could do one page load per test case - but the test suite would
//     take impossibly long to run.)
//   - Enables developers to put any number of tests in one file as appropriate, without worrying
//     about expectation granularity.

interface TestTreeNodeBase<T extends TestQuery> {
  readonly query: T;
  /**
   * Readable "relative" name for display in standalone runner.
   * Not always the exact relative name, because sometimes there isn't
   * one (e.g. s:f:* relative to s:f,*), but something that is readable.
   */
  readonly readableRelativeName: string;
  subtreeCounts?: { tests: number; nodesWithTODO: number };
}

export interface TestSubtree<T extends TestQuery = TestQuery> extends TestTreeNodeBase<T> {
  readonly children: Map<string, TestTreeNode>;
  readonly collapsible: boolean;
  description?: string;
  readonly testCreationStack?: Error;
}

export interface TestTreeLeaf extends TestTreeNodeBase<TestQuerySingleCase> {
  readonly run: RunFn;
  readonly isUnimplemented?: boolean;
  subtreeCounts?: undefined;
}

export type TestTreeNode = TestSubtree | TestTreeLeaf;

/**
 * When iterating through "collapsed" tree nodes, indicates how many "query levels" to traverse
 * through before starting to collapse nodes.
 *
 * Corresponds with TestQueryLevel, but excludes 4 (SingleCase):
 * - 1 = MultiFile. Expands so every file is in the collapsed tree.
 * - 2 = MultiTest. Expands so every test is in the collapsed tree.
 * - 3 = MultiCase. Expands so every case is in the collapsed tree (i.e. collapsing disabled).
 */
export type ExpandThroughLevel = 1 | 2 | 3;

export class TestTree {
  /**
   * The `queryToLoad` that this test tree was created for.
   * Test trees are always rooted at `suite:*`, but they only contain nodes that fit
   * within `forQuery`.
   *
   * This is used for `iterateCollapsedNodes` which only starts collapsing at the next
   * `TestQueryLevel` after `forQuery`.
   */
  readonly forQuery: TestQuery;
  readonly root: TestSubtree;

  constructor(forQuery: TestQuery, root: TestSubtree) {
    this.forQuery = forQuery;
    TestTree.propagateCounts(root);
    this.root = root;
    assert(
      root.query.level === 1 && root.query.depthInLevel === 0,
      'TestTree root must be the root (suite:*)'
    );
  }

  /**
   * Iterate through the leaves of a version of the tree which has been pruned to exclude
   * subtrees which:
   * - are at a deeper `TestQueryLevel` than `this.forQuery`, and
   * - were not a `Ordering.StrictSubset` of any of the `subqueriesToExpand` during tree creation.
   */
  iterateCollapsedNodes({
    includeIntermediateNodes = false,
    includeEmptySubtrees = false,
    alwaysExpandThroughLevel,
  }: {
    /** Whether to include intermediate tree nodes or only collapsed-leaves. */
    includeIntermediateNodes?: boolean;
    /** Whether to include collapsed-leaves with no children. */
    includeEmptySubtrees?: boolean;
    /** Never collapse nodes up through this level. */
    alwaysExpandThroughLevel: ExpandThroughLevel;
  }): IterableIterator<Readonly<TestTreeNode>> {
    const expandThroughLevel = Math.max(this.forQuery.level, alwaysExpandThroughLevel);
    return TestTree.iterateSubtreeNodes(this.root, {
      includeIntermediateNodes,
      includeEmptySubtrees,
      expandThroughLevel,
    });
  }

  iterateLeaves(): IterableIterator<Readonly<TestTreeLeaf>> {
    return TestTree.iterateSubtreeLeaves(this.root);
  }

  /**
   * Dissolve nodes which have only one child, e.g.:
   *   a,* { a,b,* { a,b:* { ... } } }
   * collapses down into:
   *   a,* { a,b:* { ... } }
   * which is less needlessly verbose when displaying the tree in the standalone runner.
   */
  dissolveSingleChildTrees(): void {
    const newRoot = dissolveSingleChildTrees(this.root);
    assert(newRoot === this.root);
  }

  toString(): string {
    return TestTree.subtreeToString('(root)', this.root, '');
  }

  static *iterateSubtreeNodes(
    subtree: TestSubtree,
    opts: {
      includeIntermediateNodes: boolean;
      includeEmptySubtrees: boolean;
      expandThroughLevel: number;
    }
  ): IterableIterator<TestTreeNode> {
    if (opts.includeIntermediateNodes) {
      yield subtree;
    }

    for (const [, child] of subtree.children) {
      if ('children' in child) {
        // Is a subtree
        const collapsible = child.collapsible && child.query.level > opts.expandThroughLevel;
        if (child.children.size > 0 && !collapsible) {
          yield* TestTree.iterateSubtreeNodes(child, opts);
        } else if (child.children.size > 0 || opts.includeEmptySubtrees) {
          // Don't yield empty subtrees (e.g. files with no tests) unless includeEmptySubtrees
          yield child;
        }
      } else {
        // Is a leaf
        yield child;
      }
    }
  }

  static *iterateSubtreeLeaves(subtree: TestSubtree): IterableIterator<TestTreeLeaf> {
    for (const [, child] of subtree.children) {
      if ('children' in child) {
        yield* TestTree.iterateSubtreeLeaves(child);
      } else {
        yield child;
      }
    }
  }

  /** Propagate the subtreeTODOs/subtreeTests state upward from leaves to parent nodes. */
  static propagateCounts(subtree: TestSubtree): { tests: number; nodesWithTODO: number } {
    subtree.subtreeCounts ??= { tests: 0, nodesWithTODO: 0 };
    for (const [, child] of subtree.children) {
      if ('children' in child) {
        const counts = TestTree.propagateCounts(child);
        subtree.subtreeCounts.tests += counts.tests;
        subtree.subtreeCounts.nodesWithTODO += counts.nodesWithTODO;
      }
    }
    return subtree.subtreeCounts;
  }

  /** Displays counts in the format `(Nodes with TODOs) / (Total test count)`. */
  static countsToString(tree: TestTreeNode): string {
    if (tree.subtreeCounts) {
      return `${tree.subtreeCounts.nodesWithTODO} / ${tree.subtreeCounts.tests}`;
    } else {
      return '';
    }
  }

  static subtreeToString(name: string, tree: TestTreeNode, indent: string): string {
    const collapsible = 'run' in tree ? '>' : tree.collapsible ? '+' : '-';
    let s =
      indent +
      `${collapsible} ${TestTree.countsToString(tree)} ${JSON.stringify(name)} => ${tree.query}`;
    if ('children' in tree) {
      if (tree.description !== undefined) {
        s += `\n${indent}  | ${JSON.stringify(tree.description)}`;
      }

      for (const [name, child] of tree.children) {
        s += '\n' + TestTree.subtreeToString(name, child, indent + '  ');
      }
    }
    return s;
  }
}

// MAINTENANCE_TODO: Consider having subqueriesToExpand actually impact the depth-order of params
// in the tree.
export async function loadTreeForQuery(
  loader: TestFileLoader,
  queryToLoad: TestQuery,
  subqueriesToExpand: TestQuery[]
): Promise<TestTree> {
  const suite = queryToLoad.suite;
  const specs = await loader.listing(suite);

  const subqueriesToExpandEntries = Array.from(subqueriesToExpand.entries());
  const seenSubqueriesToExpand: boolean[] = new Array(subqueriesToExpand.length);
  seenSubqueriesToExpand.fill(false);

  const isCollapsible = (subquery: TestQuery) =>
    subqueriesToExpandEntries.every(([i, toExpand]) => {
      const ordering = compareQueries(toExpand, subquery);

      // If toExpand == subquery, no expansion is needed (but it's still "seen").
      if (ordering === Ordering.Equal) seenSubqueriesToExpand[i] = true;
      return ordering !== Ordering.StrictSubset;
    });

  // L0 = suite-level, e.g. suite:*
  // L1 =  file-level, e.g. suite:a,b:*
  // L2 =  test-level, e.g. suite:a,b:c,d:*
  // L3 =  case-level, e.g. suite:a,b:c,d:
  let foundCase = false;
  // L0 is suite:*
  const subtreeL0 = makeTreeForSuite(suite, isCollapsible);
  for (const entry of specs) {
    if (entry.file.length === 0 && 'readme' in entry) {
      // Suite-level readme.
      setSubtreeDescriptionAndCountTODOs(subtreeL0, entry.readme);
      continue;
    }

    {
      const queryL1 = new TestQueryMultiFile(suite, entry.file);
      const orderingL1 = compareQueries(queryL1, queryToLoad);
      if (orderingL1 === Ordering.Unordered) {
        // File path is not matched by this query.
        continue;
      }
    }

    if ('readme' in entry) {
      // Entry is a README that is an ancestor or descendant of the query.
      // (It's included for display in the standalone runner.)

      // readmeSubtree is suite:a,b,*
      // (This is always going to dedup with a file path, if there are any test spec files under
      // the directory that has the README).
      const readmeSubtree: TestSubtree<TestQueryMultiFile> = addSubtreeForDirPath(
        subtreeL0,
        entry.file,
        isCollapsible
      );
      setSubtreeDescriptionAndCountTODOs(readmeSubtree, entry.readme);
      continue;
    }
    // Entry is a spec file.

    const spec = await loader.importSpecFile(queryToLoad.suite, entry.file);
    // subtreeL1 is suite:a,b:*
    const subtreeL1: TestSubtree<TestQueryMultiTest> = addSubtreeForFilePath(
      subtreeL0,
      entry.file,
      isCollapsible
    );
    setSubtreeDescriptionAndCountTODOs(subtreeL1, spec.description);

    let groupHasTests = false;
    for (const t of spec.g.iterate()) {
      groupHasTests = true;
      {
        const queryL2 = new TestQueryMultiCase(suite, entry.file, t.testPath, {});
        const orderingL2 = compareQueries(queryL2, queryToLoad);
        if (orderingL2 === Ordering.Unordered) {
          // Test path is not matched by this query.
          continue;
        }
      }

      // subtreeL2 is suite:a,b:c,d:*
      const subtreeL2: TestSubtree<TestQueryMultiCase> = addSubtreeForTestPath(
        subtreeL1,
        t.testPath,
        t.testCreationStack,
        isCollapsible
      );
      // This is 1 test. Set tests=1 then count TODOs.
      subtreeL2.subtreeCounts ??= { tests: 1, nodesWithTODO: 0 };
      if (t.description) setSubtreeDescriptionAndCountTODOs(subtreeL2, t.description);

      // MAINTENANCE_TODO: If tree generation gets too slow, avoid actually iterating the cases in a
      // file if there's no need to (based on the subqueriesToExpand).
      for (const c of t.iterate()) {
        {
          const queryL3 = new TestQuerySingleCase(suite, entry.file, c.id.test, c.id.params);
          const orderingL3 = compareQueries(queryL3, queryToLoad);
          if (orderingL3 === Ordering.Unordered || orderingL3 === Ordering.StrictSuperset) {
            // Case is not matched by this query.
            continue;
          }
        }

        // Leaf for case is suite:a,b:c,d:x=1;y=2
        addLeafForCase(subtreeL2, c, isCollapsible);

        foundCase = true;
      }
    }
    if (!groupHasTests && !subtreeL1.subtreeCounts) {
      throw new StacklessError(
        `${subtreeL1.query} has no tests - it must have "TODO" in its description`
      );
    }
  }

  for (const [i, sq] of subqueriesToExpandEntries) {
    const subquerySeen = seenSubqueriesToExpand[i];
    if (!subquerySeen) {
      throw new StacklessError(
        `subqueriesToExpand entry did not match anything \
(could be wrong, or could be redundant with a previous subquery):\n  ${sq.toString()}`
      );
    }
  }
  assert(foundCase, `Query \`${queryToLoad.toString()}\` does not match any cases`);

  return new TestTree(queryToLoad, subtreeL0);
}

function setSubtreeDescriptionAndCountTODOs(
  subtree: TestSubtree<TestQueryMultiFile>,
  description: string
) {
  assert(subtree.description === undefined);
  subtree.description = description.trim();
  subtree.subtreeCounts ??= { tests: 0, nodesWithTODO: 0 };
  if (subtree.description.indexOf('TODO') !== -1) {
    subtree.subtreeCounts.nodesWithTODO++;
  }
}

function makeTreeForSuite(
  suite: string,
  isCollapsible: (sq: TestQuery) => boolean
): TestSubtree<TestQueryMultiFile> {
  const query = new TestQueryMultiFile(suite, []);
  return {
    readableRelativeName: suite + kBigSeparator,
    query,
    children: new Map(),
    collapsible: isCollapsible(query),
  };
}

function addSubtreeForDirPath(
  tree: TestSubtree<TestQueryMultiFile>,
  file: string[],
  isCollapsible: (sq: TestQuery) => boolean
): TestSubtree<TestQueryMultiFile> {
  const subqueryFile: string[] = [];
  // To start, tree is suite:*
  // This loop goes from that -> suite:a,* -> suite:a,b,*
  for (const part of file) {
    subqueryFile.push(part);
    tree = getOrInsertSubtree(part, tree, () => {
      const query = new TestQueryMultiFile(tree.query.suite, subqueryFile);
      return {
        readableRelativeName: part + kPathSeparator + kWildcard,
        query,
        collapsible: isCollapsible(query),
      };
    });
  }
  return tree;
}

function addSubtreeForFilePath(
  tree: TestSubtree<TestQueryMultiFile>,
  file: string[],
  isCollapsible: (sq: TestQuery) => boolean
): TestSubtree<TestQueryMultiTest> {
  // To start, tree is suite:*
  // This goes from that -> suite:a,* -> suite:a,b,*
  tree = addSubtreeForDirPath(tree, file, isCollapsible);
  // This goes from that -> suite:a,b:*
  const subtree = getOrInsertSubtree('', tree, () => {
    const query = new TestQueryMultiTest(tree.query.suite, tree.query.filePathParts, []);
    assert(file.length > 0, 'file path is empty');
    return {
      readableRelativeName: file[file.length - 1] + kBigSeparator + kWildcard,
      query,
      collapsible: isCollapsible(query),
    };
  });
  return subtree;
}

function addSubtreeForTestPath(
  tree: TestSubtree<TestQueryMultiTest>,
  test: readonly string[],
  testCreationStack: Error,
  isCollapsible: (sq: TestQuery) => boolean
): TestSubtree<TestQueryMultiCase> {
  const subqueryTest: string[] = [];
  // To start, tree is suite:a,b:*
  // This loop goes from that -> suite:a,b:c,* -> suite:a,b:c,d,*
  for (const part of test) {
    subqueryTest.push(part);
    tree = getOrInsertSubtree(part, tree, () => {
      const query = new TestQueryMultiTest(
        tree.query.suite,
        tree.query.filePathParts,
        subqueryTest
      );
      return {
        readableRelativeName: part + kPathSeparator + kWildcard,
        query,
        collapsible: isCollapsible(query),
      };
    });
  }
  // This goes from that -> suite:a,b:c,d:*
  return getOrInsertSubtree('', tree, () => {
    const query = new TestQueryMultiCase(
      tree.query.suite,
      tree.query.filePathParts,
      subqueryTest,
      {}
    );
    assert(subqueryTest.length > 0, 'subqueryTest is empty');
    return {
      readableRelativeName: subqueryTest[subqueryTest.length - 1] + kBigSeparator + kWildcard,
      kWildcard,
      query,
      testCreationStack,
      collapsible: isCollapsible(query),
    };
  });
}

function addLeafForCase(
  tree: TestSubtree<TestQueryMultiTest>,
  t: RunCase,
  checkCollapsible: (sq: TestQuery) => boolean
): void {
  const query = tree.query;
  let name: string = '';
  const subqueryParams: TestParamsRW = {};

  // To start, tree is suite:a,b:c,d:*
  // This loop goes from that -> suite:a,b:c,d:x=1;* -> suite:a,b:c,d:x=1;y=2;*
  for (const [k, v] of Object.entries(t.id.params)) {
    name = stringifySingleParam(k, v);
    subqueryParams[k] = v;

    tree = getOrInsertSubtree(name, tree, () => {
      const subquery = new TestQueryMultiCase(
        query.suite,
        query.filePathParts,
        query.testPathParts,
        subqueryParams
      );
      return {
        readableRelativeName: name + kParamSeparator + kWildcard,
        query: subquery,
        collapsible: checkCollapsible(subquery),
      };
    });
  }

  // This goes from that -> suite:a,b:c,d:x=1;y=2
  const subquery = new TestQuerySingleCase(
    query.suite,
    query.filePathParts,
    query.testPathParts,
    subqueryParams
  );
  checkCollapsible(subquery); // mark seenSubqueriesToExpand
  insertLeaf(tree, subquery, t);
}

function getOrInsertSubtree<T extends TestQuery>(
  key: string,
  parent: TestSubtree,
  createSubtree: () => Omit<TestSubtree<T>, 'children'>
): TestSubtree<T> {
  let v: TestSubtree<T>;
  const child = parent.children.get(key);
  if (child !== undefined) {
    assert('children' in child); // Make sure cached subtree is not actually a leaf
    v = child as TestSubtree<T>;
  } else {
    v = { ...createSubtree(), children: new Map() };
    parent.children.set(key, v);
  }
  return v;
}

function insertLeaf(parent: TestSubtree, query: TestQuerySingleCase, t: RunCase) {
  const leaf: TestTreeLeaf = {
    readableRelativeName: readableNameForCase(query),
    query,
    run: (rec, expectations) => t.run(rec, query, expectations || []),
    isUnimplemented: t.isUnimplemented,
  };

  // This is a leaf (e.g. s:f:t:x=1;* -> s:f:t:x=1). The key is always ''.
  const key = '';
  assert(!parent.children.has(key), `Duplicate testcase: ${query}`);
  parent.children.set(key, leaf);
}

function dissolveSingleChildTrees(tree: TestTreeNode): TestTreeNode {
  if ('children' in tree) {
    const shouldDissolveThisTree =
      tree.children.size === 1 && tree.query.depthInLevel !== 0 && tree.description === undefined;
    if (shouldDissolveThisTree) {
      // Loops exactly once
      for (const [, child] of tree.children) {
        // Recurse on child
        return dissolveSingleChildTrees(child);
      }
    }

    for (const [k, child] of tree.children) {
      // Recurse on each child
      const newChild = dissolveSingleChildTrees(child);
      if (newChild !== child) {
        tree.children.set(k, newChild);
      }
    }
  }
  return tree;
}

/** Generate a readable relative name for a case (used in standalone). */
function readableNameForCase(query: TestQuerySingleCase): string {
  const paramsKeys = Object.keys(query.params);
  if (paramsKeys.length === 0) {
    return query.testPathParts[query.testPathParts.length - 1] + kBigSeparator;
  } else {
    const lastKey = paramsKeys[paramsKeys.length - 1];
    return stringifySingleParam(lastKey, query.params[lastKey]);
  }
}
