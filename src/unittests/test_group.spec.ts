
export const description = `
Unit tests for parameterization system.
`;

import {
  CaseRecorder,
  Fixture,
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

function print(this: Fixture) {
  this.log(JSON.stringify(this.params));
}

group.test("test", print);

group.testp("testp", {a: 1}, print);

class Printer extends Fixture {
  print() {
    this.log(JSON.stringify(this.params));
  }
}

group.testf("testf", Printer, function() {
  this.print();
});

group.testpf("testpf", {a: 1}, Printer, function() {
  this.print();
});
