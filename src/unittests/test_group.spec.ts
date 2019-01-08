
export const description = `
Unit tests for parameterization system.
`;

import {
  CaseRecorder,
  TestClass,
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

function print(log: CaseRecorder, p: object) {
  log.log(JSON.stringify(p));
}

group.test("noclass_noparams", {
}, print);

group.test("noclass_params", {
  cases: [{a: 1}, {a: 2}],
}, print);

class Printer extends TestClass {
  print() {
    this.log.log(JSON.stringify(this.params));
  }
}

group.test("class_noparams", {
  class: Printer,
}, function(this: Printer) {
  this.print();
});

group.test("class_params", {
  class: Printer,
  cases: [{a: 1}, {a: 2}],
}, function(this: Printer) {
  this.print();
});
