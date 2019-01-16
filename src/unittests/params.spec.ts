export const description = `
Unit tests for parameterization system.
`;

import {
  DefaultFixture,
  pcombine,
  poptions,
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

function print(this: DefaultFixture) {
  this.log(JSON.stringify(this.params));
}

for (const params of [ {hello: 1}, {hello: 2} ]) {
  group.testp("literal", params, print);
}

for (const params of poptions("hello", [1, 2, 3])) {
  group.testp("options", params, print);
}

for (const params of pcombine([])) {
  group.testp("combine/none", params, function() { this.fail(); });
}

for (const params of pcombine([[{}], [{}]])) {
  group.testp("combine/unit_unit", params, print);
}

for (const params of pcombine([ poptions("x", [1, 2]), poptions("y", ["a", "b"]), [{}] ])) {
  group.testp("combine/lists", params, print);
}

for (const params of pcombine([
    [{x: 1, y: 2}, {x: 10, y: 20}],
    [{z: "z"}, {w: "w"}],
  ])) {
  group.testp("combine/arrays", params, print);
}

for (const params of pcombine([
    poptions("x", [1, 2]),
    [{z: "z"}, {w: "w"}],
  ])) {
  group.testp("combine/mixed", params, print);
}
