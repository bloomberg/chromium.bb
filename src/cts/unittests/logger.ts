export const description = `
Unit tests for namespaced logging system.

Also serves as a larger test of async test functions, and of the logging system.
`;

import { StringLogger, TestTree } from "../../framework/index.js";

export function add(tree: TestTree) {
  tree.test("namespace_path", (log) => {
    const mylog = new StringLogger();

    log.expect(mylog.path().length === 0);
    {
      mylog.push("hello");
      log.expect(mylog.path().join("/") === "hello");
      {
        mylog.push("world");
        log.expect(mylog.path().join("/") === "hello/world");
        mylog.pop();
      }
      log.expect(mylog.path().join("/") === "hello");
      mylog.pop();
    }
    log.expect(mylog.path().length === 0);

    log.log(mylog.getContents());
    log.expect(mylog.getContents() === `\
* hello
  * world`);
  });

  tree.test("log_multiline", (log) => {
    const mylog = new StringLogger();

    {
      mylog.push("hello");
      mylog.log("a\nb");
      mylog.pop();
    }
    mylog.log("c");

    log.log(mylog.getContents());
    log.expect(mylog.getContents() === `\
* hello
  | a
  | b
| c`);
  });

  tree.test("push_multiline", (log) => {
    const mylog = new StringLogger();

    try {
      mylog.push("hello\nworld");
      log.expect(false);
    } catch (e) {
    }
  });
}
