static uint var_1 = 0u;

void main_1() {
  while (true) {
    var_1 = 1u;
    if (false) {
    } else {
      break;
    }
    var_1 = 3u;
    {
      var_1 = 2u;
    }
  }
  return;
}

void main() {
  main_1();
  return;
}
