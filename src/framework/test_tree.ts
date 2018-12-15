import { ParamIterable } from './params';

interface TestTreeModule {
  name: string;
  description: string;
  subtrees?: Promise<TestTreeModule>[];
  add?: any;
}

type PTestFn = (param: object) => (Promise<void> | void);
type TestFn = () => (Promise<void> | void);
type Test = () => Promise<void>;

export class TestTree {
  name: string;
  description: string;
  subtrees: TestTree[];
  tests: Test[];

  private constructor(name: string, description: string) {
    this.name = name;
    this.description = description;
    this.subtrees = [];
    this.tests = [];
  }

  static async trunk(modules: Promise<TestTreeModule>[]): Promise<TestTree> {
    const trunk = new TestTree('', '');
    for (const m of modules) {
      trunk.recurse(await m);
    }
    return trunk;
  }

  private async recurse(m: TestTreeModule): Promise<void> {
    const tt = new TestTree(m.name, m.description);
    if (m.subtrees) {
      for (const st of m.subtrees) {
        await this.recurse(await st);
      }
    }
    if (m.add) {
      m.add(tt);
    }
    this.subtrees.push(tt);
  }

  ptest(name: string, params: ParamIterable, fn: PTestFn): void {
    this.tests.push(async () => {
      for (const p of params) {
        console.log('  * ' + name + '/' + JSON.stringify(p));
        await fn(p);
      }
    });
  }

  test(name: string, fn: TestFn): void {
    this.tests.push(async () => {
      console.log('  * ' + name);
      fn();
    });
  }

  async * run(path: string[] = []): AsyncIterable<void> {
    const subtrees = await Promise.all(this.subtrees);
    for (const t of this.tests) {
      await t();
    }
    for (const st of subtrees) {
      const subpath = path.concat([st.name]);
      console.log('* ' + subpath.join('/'));
      yield* st.run(subpath);
    }
  }
}
