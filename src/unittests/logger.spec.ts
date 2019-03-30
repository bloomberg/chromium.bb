export const description = `
Unit tests for namespaced logging system.

Also serves as a larger test of async test functions, and of the logging system.
`;

import { Logger, TestGroup } from "../framework/index.js";
import { DefaultFixture } from "../framework/default_fixture.js";

export const group = new TestGroup();

group.test("construct", DefaultFixture, (t) => {
  const mylog = new Logger();
  const [testres, testrec] = mylog.record("foo/bar");
  const [res1] = testrec.record("baz");
  const params2 = {};
  const [res2] = testrec.record("qux", params2);

  t.expect(testres.path === "foo/bar");
  t.expect(testres.cases.length === 2);
  t.expect(testres.cases[0] === res1);
  t.expect(testres.cases[1] === res2);
  t.expect(res1.name === "baz");
  t.expect(res1.params === undefined);
  t.expect(res1.logs === undefined);
  t.expect(res1.status === "running");
  t.expect(res1.timems < 0);
  t.expect(res2.name === "qux");
  t.expect(res2.params === params2);
  t.expect(res2.logs === undefined);
  t.expect(res2.status === "running");
  t.expect(res2.timems < 0);
});

group.test("empty", DefaultFixture, (t) => {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  t.expect(res.status === "running");
  rec.finish();
  t.expect(res.status === "pass");
  t.expect(res.timems >= 0);
});

group.test("pass", DefaultFixture, (t) => {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.log("hello");
  t.expect(res.status === "running");
  rec.finish();
  t.expect(res.status === "pass");
  t.expect(res.timems >= 0);
});

group.test("warn", DefaultFixture, (t) => {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.warn();
  t.expect(res.status === "running");
  rec.finish();
  t.expect(res.status === "warn");
  t.expect(res.timems >= 0);
});

group.test("fail", DefaultFixture, (t) => {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.fail("bye");
  t.expect(res.status === "running");
  rec.finish();
  t.expect(res.status === "fail");
  t.expect(res.timems >= 0);
});
