export const description = `
Unit tests for namespaced logging system.

Also serves as a larger test of async test functions, and of the logging system.
`;

import { Logger, TestGroup } from "../framework/index.js";

export const group = new TestGroup();

group.test("construct", function() {
  const mylog = new Logger();
  const [testres, testrec] = mylog.record("foo/bar");
  const [res1, ] = testrec.record("baz");
  const params2 = {};
  const [res2, ] = testrec.record("qux", params2);

  this.expect(testres.path === "foo/bar");
  this.expect(testres.cases.length === 2);
  this.expect(testres.cases[0] === res1);
  this.expect(testres.cases[1] === res2);
  this.expect(res1.name === "baz");
  this.expect(res1.params === undefined);
  this.expect(res1.logs === undefined);
  this.expect(res1.status === "running");
  this.expect(res1.timems < 0);
  this.expect(res2.name === "qux");
  this.expect(res2.params === params2);
  this.expect(res2.logs === undefined);
  this.expect(res2.status === "running");
  this.expect(res2.timems < 0);
});

group.test("empty", function() {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  this.expect(res.status === "running");
  rec.finish();
  this.expect(res.status === "pass");
  this.expect(res.timems >= 0);
});

group.test("pass", function() {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.log("hello");
  this.expect(res.status === "running");
  rec.finish();
  this.expect(res.status === "pass");
  this.expect(res.timems >= 0);
});

group.test("warn", function() {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.warn();
  this.expect(res.status === "running");
  rec.finish();
  this.expect(res.status === "warn");
  this.expect(res.timems >= 0);
});

group.test("fail", function() {
  const mylog = new Logger();
  const [, testrec] = mylog.record("");
  const [res, rec] = testrec.record("baz");

  rec.start();
  rec.fail("bye");
  this.expect(res.status === "running");
  rec.finish();
  this.expect(res.status === "fail");
  this.expect(res.timems >= 0);
});
