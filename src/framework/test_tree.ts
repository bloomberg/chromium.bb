import { ParamIterable } from "./params";

interface ITestTreeModule {
  name: string;
  description: string;
  subtrees?: Array<Promise<ITestTreeModule>>;
  add?: (tree: TestTree) => void;
}

type PTestFn = (param: object) => (Promise<void> | void);
type TestFn = () => (Promise<void> | void);
type Test = () => Promise<void>;
interface ILeaf {
  name: string;
  run: Test;
}
interface ITestWithPath {
  path: string[];
  run: Test;
}

export class TestTree {

  public static async trunk(modules: Array<Promise<ITestTreeModule>>): Promise<TestTree> {
    const trunk = new TestTree("", "");
    for (const m of modules) {
      await trunk.recurse(await m);
    }
    return trunk;
  }
  public name: string;
  public description: string;
  public subtrees: TestTree[];
  public tests: ILeaf[];

  private constructor(name: string, description: string) {
    this.name = name;
    this.description = description.trim();
    this.subtrees = [];
    this.tests = [];
  }

  public ptest(name: string, params: ParamIterable, fn: PTestFn): void {
    this.tests.push({name, run: async () => {
      for (const p of params) {
        // tslint:disable-next-line:no-console
        console.log("  * " + name + "/" + JSON.stringify(p));
        await fn(p);
      }
    }});
  }

  public test(name: string, fn: TestFn): void {
    this.tests.push({name, run: async () => {
      // tslint:disable-next-line:no-console
      fn();
    }});
  }

  public async * iterate(path: string[] = []): AsyncIterable<ITestWithPath> {
    const subtrees = await Promise.all(this.subtrees);
    for (const t of this.tests) {
      yield {
        path: path.concat([t.name]),
        run: t.run,
      };
    }
    for (const st of subtrees) {
      const subpath = path.concat([st.name]);
      // tslint:disable-next-line:no-console
      yield* st.iterate(subpath);
    }
  }

  private async recurse(m: ITestTreeModule): Promise<void> {
    const tt = new TestTree(m.name, m.description);
    if (m.subtrees) {
      for (const st of m.subtrees) {
        await tt.recurse(await st);
      }
    }
    if (m.add) {
      m.add(tt);
    }
    this.subtrees.push(tt);
  }
}
