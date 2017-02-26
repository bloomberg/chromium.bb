function outer(index) {

  function inner1(a) {
    return a + 1;
  }

  function inner2(a) {
    return a + 2;
  }

  function inner3(a) { return a + 3; } function inner4(a) { return a + 4; } function inner5(a) { return a + 5; }

  return [inner1, inner2, inner3, inner4, inner5][index];
}

function performActions() {
  return outer(1)(0) + outer(3)(0);
} function outer2() {
  return outer(0)(0);
}
