class Repeat {
  async process(i) { return i; }
}
registerTask("repeat", Repeat);

class Sum {
  async process(i, j) {
    return await Promise.resolve(i + j);
  }
}
registerTask("sum", Sum);
