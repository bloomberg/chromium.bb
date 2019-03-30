export const description = `
Unit tests for parameterization system.
`;

import {
  IParamsSpec,
  objectEquals,
  ParamSpecIterable,
  pcombine,
  pexclude,
  pfilter,
  poptions,
  TestGroup,
  FixtureCreate,
} from "../framework/index.js";
import { DefaultFixture, makeDefaultFixtureCreate } from "../framework/default_fixture.js";

export const group = new TestGroup();

class ParamsTest extends DefaultFixture {
  public static create: FixtureCreate<ParamsTest> = makeDefaultFixtureCreate(ParamsTest);

  public test(act: ParamSpecIterable, exp: IParamsSpec[]) {
    this.expect(objectEquals(Array.from(act), exp));
  }
}

group.test("options", ParamsTest, (t) => {
  t.test(
    poptions("hello", [1, 2, 3]),
    [{ hello: 1 }, { hello: 2 }, { hello: 3 }]);
});

group.test("combine/none", ParamsTest, (t) => {
  t.test(
    pcombine([]),
    []);
});

group.test("combine/zeroes_and_ones", ParamsTest, (t) => {
  t.test(pcombine([[], []]), []);
  t.test(pcombine([[], [{}]]), []);
  t.test(pcombine([[{}], []]), []);
  t.test(pcombine([[{}], [{}]]), [{}]);
});

group.test("combine/mixed", ParamsTest, (t) => {
  t.test(
    pcombine([ poptions("x", [1, 2]), poptions("y", ["a", "b"]), [{p: 4}, {q: 5}], [{}] ]),
    [
      { p: 4, x: 1, y: "a" }, { q: 5, x: 1, y: "a" },
      { p: 4, x: 1, y: "b" }, { q: 5, x: 1, y: "b" },
      { p: 4, x: 2, y: "a" }, { q: 5, x: 2, y: "a" },
      { p: 4, x: 2, y: "b" }, { q: 5, x: 2, y: "b" },
    ]);
});

group.test("filter", ParamsTest, (t) => {
  t.test(
    pfilter(
      [{ a: true, x: 1 }, { a: false, y: 2 }],
      (p) => p.a),
    [{ a: true, x: 1 }]);
});

group.test("exclude", ParamsTest, (t) => {
  t.test(
    pexclude(
      [{ a: true, x: 1 }, { a: false, y: 2 }],
      [{ a: true }, { a: false, y: 2 }]),
    [{ a: true, x: 1 }]);
});
