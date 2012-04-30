#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "liblouis.h"

int main(int argc, char **argv) {
  wchar_t *text = L"⠍⠎";
  wchar_t *expected = L"⠎⠍";
  const char* tableList = "./tables/letterDefTest.ctb";
  setlocale(LC_ALL, "en_GB.UTF-8");

  wchar_t *inbuf;
  wchar_t *outbuf;
  int inlen;
  int outlen;
  int i = 0;
  int expectedlen = strlen(expected);
  inlen = strlen(text) * 2;
  outlen = inlen;
  inbuf = malloc(sizeof(wchar_t) * inlen);
  outbuf = malloc(sizeof(wchar_t) * outlen);
  printf("%ls\n", text);
  for (i = 0; i < inlen; i++) {
      inbuf[i] = text[i];
  }
  if (!lou_translate(tableList, inbuf, &inlen, outbuf, &outlen,
                     NULL, NULL, NULL, NULL, NULL, 0)) {
      printf("Translation failed.\n");
      return 1;
    }
  printf("%ls\n", outbuf);
  free(inbuf);
  free(outbuf);
  lou_free();
  return 0;
}
