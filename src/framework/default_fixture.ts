import { Fixture } from './test_group.js';

export class DefaultFixture extends Fixture {
  private outstanding: number = 0;

  public finalize() {
    if (this.outstanding !== 0) {
      throw new Error('there were outstanding asynchronous expectations (e.g. shouldReject) at the end of the test');
    }
  }

  public warn(msg?: string) {
    this.rec.warn(msg);
  }

  public fail(msg?: string) {
    this.rec.fail(msg);
  }

  public ok(msg?: string) {
    const m = msg ? (': ' + msg) : '';
    this.log('OK' + m);
  }

  public async shouldReject(p: Promise<any>, msg?: string): Promise<void> {
    this.outstanding++;
    const m = msg ? (': ' + msg) : '';
    try {
      await p;
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      this.ok('threw' + m);
    }
    this.outstanding--;
  }

  public shouldThrow(fn: () => void, msg?: string) {
    const m = msg ? (': ' + msg) : '';
    try {
      fn();
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      this.ok('threw' + m);
    }
  }

  public expect(cond: boolean, msg?: string) {
    if (cond) {
      this.ok(msg);
    } else {
      this.rec.fail(msg);
    }
  }
}
