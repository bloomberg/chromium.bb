import { getStackTrace, now } from "./util.js";

type Status = "running" | "pass" | "warn" | "fail";
interface ITestLog {
  path: string,
  cases: IResult[],
}
export interface IResult {
  name: string,
  params?: object,
  status: Status,
  logs?: ILogEntry[],
  timems: number,
}
interface ILogEntry {
}

export class Logger {
  public readonly results: ITestLog[] = [];

  constructor() {
  }

  public record(path: string): [ITestLog, GroupRecorder] {
    const cases: IResult[] = [];
    const test: ITestLog = { path, cases };
    this.results.push(test);
    return [test, new GroupRecorder(test)];
  }
}

export class GroupRecorder {
  private test: ITestLog;

  constructor(test: ITestLog) {
    this.test = test;
  }

  public record(name: string, params?: object): [IResult, CaseRecorder] {
    const result: IResult = { name, status: "running", timems: -1 };
    if (params) {
      result.params = params;
    }
    this.test.cases.push(result);
    return [result, new CaseRecorder(result)];
  }
}

export class CaseRecorder {
  private result: IResult;
  private failed: boolean = false;
  private warned: boolean = false;
  private startTime: number = -1;
  private logs: ILogEntry[] = [];

  constructor(result: IResult) {
    this.result = result;
  }

  public start() {
    this.startTime = now();
    this.logs = [];
    this.failed = false;
    this.warned = false;
  }

  public finish() {
    if (this.startTime < 0) {
      throw new Error("finish() before start()");
    }
    const endTime = now();
    this.result.timems = endTime - this.startTime;
    this.result.status = this.failed ? "fail" :
        this.warned ? "warn" : "pass";

    if (this.failed || this.warned) {
      this.result.logs = this.logs;
    }
  }

  public log(msg: string) {
    this.logs.push(msg);
  }

  public fail(msg?: string) {
    this.failImpl(msg);
  }

  public warn(msg?: string) {
    this.warned = true;
    if (msg) {
      this.log("WARN: " + msg);
    } else {
      this.log("WARN");
    }
    this.log(getStackTrace());
  }

  public ok(msg?: string) {
    if (msg) {
      this.log("PASS: " + msg);
    } else {
      this.log("PASS");
    }
  }

  public expect(cond: boolean, msg?: string) {
    if (cond) {
      this.ok(msg)
    } else {
      this.failImpl(msg);
    }
  }

  private failImpl(msg?: string) {
    this.failed = true;
    let m = "FAIL";
    if (msg) {
      m += ": " + msg;
    }
    m += " " + getStackTrace();
    this.log(m);
  }
}
