export const description = `
Unit tests for parameterization system.
`;

import {
  DefaultFixture,
  IParamsSpec,
  objectEquals,
  ParamSpecIterable,
  pcombine,
  pexclude,
  pfilter,
  poptions,
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

class ParamsTest extends DefaultFixture {
  public test(act: ParamSpecIterable, exp: IParamsSpec[]) {
    this.expect(objectEquals(Array.from(act), exp));
  }
}

group.testf("options", ParamsTest, function() {
  this.test(
    poptions("hello", [1, 2, 3]),
    [{ hello: 1 }, { hello: 2 }, { hello: 3 }]);
});

group.testf("combine/none", ParamsTest, function() {
  this.test(
    pcombine([]),
    []);
});

group.testf("combine/zeroes_and_ones", ParamsTest, function() {
  this.test(pcombine([[], []]), []);
  this.test(pcombine([[], [{}]]), []);
  this.test(pcombine([[{}], []]), []);
  this.test(pcombine([[{}], [{}]]), [{}]);
});

group.testf("combine/mixed", ParamsTest, function() {
  this.test(
    pcombine([ poptions("x", [1, 2]), poptions("y", ["a", "b"]), [{p: 4}, {q: 5}], [{}] ]),
    [
      { p: 4, x: 1, y: "a" }, { q: 5, x: 1, y: "a" },
      { p: 4, x: 1, y: "b" }, { q: 5, x: 1, y: "b" },
      { p: 4, x: 2, y: "a" }, { q: 5, x: 2, y: "a" },
      { p: 4, x: 2, y: "b" }, { q: 5, x: 2, y: "b" },
    ]);
});

group.testf("filter", ParamsTest, function() {
  this.test(
    pfilter(
      [{ a: true, x: 1 }, { a: false, y: 2 }],
      (p) => p.a),
    [{ a: true, x: 1 }]);
});

group.testf("exclude", ParamsTest, function() {
  this.test(
    pexclude(
      [{ a: true, x: 1 }, { a: false, y: 2 }],
      [{ a: true }, { a: false, y: 2 }]),
    [{ a: true, x: 1 }]);
});
