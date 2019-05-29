import { Fixture } from './test_group.js';

export class DefaultFixture extends Fixture {
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
