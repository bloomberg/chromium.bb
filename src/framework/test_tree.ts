import { ParamIterable } from "./params";

interface ITestTreeModule {
  name: string;
  description: string;
  subtrees?: Array<Promise<ITestTreeModule>>;
  add?: any;
}

type PTestFn = (param: object) => (Promise<void> | void);
type TestFn = () => (Promise<void> | void);
type Test = () => Promise<void>;

export class TestTree {

  public static async trunk(modules: Array<Promise<ITestTreeModule>>): Promise<TestTree> {
    const trunk = new TestTree("", "");
    for (const m of modules) {
      trunk.recurse(await m);
    }
    return trunk;
  }
  public name: string;
  public description: string;
  public subtrees: TestTree[];
  public tests: Test[];

  private constructor(name: string, description: string) {
    this.name = name;
    this.description = description;
    this.subtrees = [];
    this.tests = [];
  }

  public ptest(name: string, params: ParamIterable, fn: PTestFn): void {
    this.tests.push(async () => {
      for (const p of params) {
        // tslint:disable-next-line:no-console
        console.log("  * " + name + "/" + JSON.stringify(p));
        await fn(p);
      }
    });
  }

  public test(name: string, fn: TestFn): void {
    this.tests.push(async () => {
      // tslint:disable-next-line:no-console
      console.log("  * " + name);
      fn();
    });
  }

  public async * run(path: string[] = []): AsyncIterable<void> {
    const subtrees = await Promise.all(this.subtrees);
    for (const t of this.tests) {
      await t();
    }
    for (const st of subtrees) {
      const subpath = path.concat([st.name]);
      // tslint:disable-next-line:no-console
      console.log("* " + subpath.join("/"));
      yield* st.run(subpath);
    }
  }

  private async recurse(m: ITestTreeModule): Promise<void> {
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
}
