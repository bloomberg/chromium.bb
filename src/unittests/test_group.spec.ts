
export const description = `
Unit tests for parameterization system.
`;

import {
  Fixture,
  TestGroup,
  DefaultFixture,
  FixtureCreate,
  makeDefaultFixtureCreate,
} from "../framework/index.js";

export const group = new TestGroup();

// TODO: these tests should really create their own TestGroup and assert things about it

group.test("default_fixture", DefaultFixture, (t) => {
  function print(t: Fixture) {
    t.log(JSON.stringify(t.params));
  }

  const g = new TestGroup();

  g.test("test", DefaultFixture, print);
  g.testp("testp", {a: 1}, DefaultFixture, print);

  // TODO: check output
});

group.test("custom_fixture", DefaultFixture, (t) => {
  class Printer extends DefaultFixture {
    public static create: FixtureCreate<Printer> = makeDefaultFixtureCreate(Printer);

    public print() {
      this.log(JSON.stringify(this.params));
    }
  }

  const g = new TestGroup();

  g.test("test", Printer, (t) => { t.print(); });
  g.testp("testp", {a: 1}, Printer, (t) => { t.print(); });

  // TODO: check output
});
