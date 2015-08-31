#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "liblouis.h"
#include "louis.h"
#include "brl_checks.h"

int check_with_mode(
    const char *tableList, const char *str, const char *typeform,
    const char *expected, int mode, int direction);

void
print_int_array(const char *prefix, int *pos_list, int len)
{
  int i;
  printf("%s ", prefix);
  for (i = 0; i < len; i++)
    printf("%d ", pos_list[i]);
  printf("\n");
}

#define UTF8_BUFSIZE 32
#define UNICODE_SURROGATE_PAIR -1
#define UNICODE_BAD_INPUT -1

/* Input: a Unicode code point, "ucs".

   Output: UTF-8 characters in buffer "utf8".

   Return value: the number of bytes written into "utf8", or -1 if
   there was an error.

   This adds a zero byte to the end of the string. It assumes that the
   buffer "utf8" has at least seven bytes of space to write to. */

/* from http://std.dkuug.dk/jtc1/sc2/WG2/docs/n1335.html

R.4 Mapping from UCS-4 form to UTF-8 form

Table 4 defines in mathematical notation the mapping from the UCS-4
coded representation form to the UTF-8 coded representation form.

In the left column (UCS-4) the notation x indicates the four-octet
coded representation of a single character of the UCS. In the right
column (UTF-8) x indicates the corresponding integer value.

NOTE 3 - Values of x in the range 0000 D800 .. 0000 DFFF are reserved
for the UTF-16 form and do not occur in UCS-4. The values 0000 FFFE
and 0000 FFFF also do not occur (see clause 8). The mappings of these
code positions in UTF-8 are undefined.

NOTE 4 - The algorithm for converting from UCS-4 to UTF-8 can be
summarised as follows.

For each coded character in UCS-4 the length of octet sequence in
UTF-8 is determined by the entry in the right column of Table 1. The
bits in the UCS-4 coded representation, starting from the least
significant bit, are then distributed across the free bit positions in
order of increasing significance until no more free bit positions are
available.

Table 4 - Mapping from UCS-4 to UTF-8

Range of values			Sequence of
in UCS-4  			octets in UTF-8

x = 0000 0000 .. 0000 007F;	x;
x = 0000 0080 .. 0000 07FF;	C0 + x/2**6;
				80 + x%2**6;
x = 0000 0800 .. 0000 FFFF;	E0 + x/2**12;
	(see Note 3)		80 + x/2**6%2**6;
				80 + x%2**6;
x = 0001 0000 .. 001F FFFF;	F0 + x/2**18;
				80 + x/2**12%2**6;
				80 + x/2**6%2**6;
				80 + x%2**6;
x = 0020 0000 .. 03FF FFFF;	F8 + x/2**24;
				80 + x/2**18%2**6;
				80 + x/2**12%2**6;
				80 + x/2**6%2**6;
				80 + x%2**6;
x = 0400 0000 .. 7FFF FFFF;	FC + x/2**30;
				80 + x/2**24%2**6;
				80 + x/2**18%2**6;
				80 + x/2**12%2**6;
				80 + x/2**6%2**6;
				80 + x%2**6; */

int ucs_to_utf8 (widechar ucs, unsigned char * utf8)
{
    if (ucs < 0x80) {
      utf8[0] = ucs;
      utf8[1] = '\0';
      return 1;
    } else if (ucs < 0x800) {
      utf8[0] = (ucs >> 6)   | 0xC0;
      utf8[1] = (ucs & 0x3F) | 0x80;
      utf8[2] = '\0';
      return 2;
    } else if (ucs < 0xFFFF) {
      if (ucs >= 0xD800 && ucs <= 0xDFFF) {
	/* Ill-formed. */
	return UNICODE_SURROGATE_PAIR;
      }
      utf8[0] = ((ucs >> 12)       ) | 0xE0;
      utf8[1] = ((ucs >> 6 ) & 0x3F) | 0x80;
      utf8[2] = ((ucs      ) & 0x3F) | 0x80;
      utf8[3] = '\0';
      return 3;
    } else if (ucs < 0x1FFFFF) {
      utf8[0] = 0xF0 | ((ucs >> 18));
      utf8[1] = 0x80 | ((ucs >> 12) & 0x3F);
      utf8[2] = 0x80 | ((ucs >> 6) & 0x3F);
      utf8[3] = 0x80 | ((ucs & 0x3F));
        utf8[4] = '\0';
        return 4;
    } else if (ucs < 0x3FFFFFF) {
      utf8[0] = 0xF0 | ((ucs >> 24));
      utf8[1] = 0x80 | ((ucs >> 18) & 0x3F);
      utf8[2] = 0x80 | ((ucs >> 12) & 0x3F);
      utf8[3] = 0x80 | ((ucs >> 6) & 0x3F);
      utf8[4] = 0x80 | ((ucs & 0x3F));
      utf8[5] = '\0';
      return 5;
    } else if (ucs < 0x7FFFFFFF) {
      utf8[0] = 0xF0 | ((ucs >> 30));
      utf8[1] = 0x80 | ((ucs >> 24) & 0x3F);
      utf8[2] = 0x80 | ((ucs >> 18) & 0x3F);
      utf8[3] = 0x80 | ((ucs >> 12) & 0x3F);
      utf8[4] = 0x80 | ((ucs >> 6) & 0x3F);
      utf8[5] = 0x80 | ((ucs & 0x3F));
      utf8[6] = '\0';
      return 6;
    }
    return UNICODE_BAD_INPUT;
}

void
print_widechars(widechar * buf, int len)
{
  int i;
  unsigned char utf8[UTF8_BUFSIZE];

  for (i = 0; i < len; i++) {
    ucs_to_utf8(buf[i], utf8);
    printf("%s", utf8);
  }
}

/* Helper function to convert a typeform string of '0's, '1's, '2's etc.
   to the required format, which is an array of 0s, 1s, 2s, etc.
   For example, "0000011111000" is converted to {0,0,0,0,0,1,1,1,1,1,0,0,0}
   The caller is responsible for freeing the returned array. */
char *
convert_typeform(const char* typeform_string)
{
  int len = strlen(typeform_string);
  char *typeform = malloc(len * sizeof(char));
  int i;
  for (i=0; i<len; i++)
    typeform[i] = typeform_string[i] - '0';
  return typeform;
}

/* Check if a string is translated as expected. Return 0 if the
   translation is as expected and 1 otherwise. */
int
check_translation(const char *tableList, const char *str,
		  const char *typeform, const char *expected)
{
  return check_translation_with_mode(tableList, str, typeform, expected, 0);
}

/* Check if a string is translated as expected. Return 0 if the
   translation is as expected and 1 otherwise. */
int
check_translation_with_mode(const char *tableList, const char *str,
			    const char *typeform, const char *expected, 
			    int mode)
{
    return check_with_mode(tableList, str, typeform, expected, mode, 0);
}

/* Check if a string is backtranslated as expected. Return 0 if the
   backtranslation is as expected and 1 otherwise. */
int
check_backtranslation(const char *tableList, const char *str,
		  const char *typeform, const char *expected)
{
  return check_backtranslation_with_mode(tableList, str, typeform, expected, 0);
}

/* Check if a string is backtranslated as expected. Return 0 if the
   backtranslation is as expected and 1 otherwise. */
int
check_backtranslation_with_mode(const char *tableList, const char *str,
			    const char *typeform, const char *expected, 
			    int mode)
{
    return check_with_mode(tableList, str, typeform, expected, mode, 1);
}

/* direction, 0=forward, otherwise backwards */
int check_with_mode(
    const char *tableList, const char *str, const char *typeform,
    const char *expected, int mode, int direction)
{
  widechar *inbuf, *outbuf, *expectedbuf;
  int inlen;
  int outlen;
  int i, rv = 0;
  int funcStatus = 0;

  char *typeformbuf = NULL;

  int expectedlen = strlen(expected);

  inlen = strlen(str);
  outlen = inlen * 10;
  inbuf = malloc(sizeof(widechar) * inlen);
  outbuf = malloc(sizeof(widechar) * outlen);
  expectedbuf = malloc(sizeof(widechar) * expectedlen);
  if (typeform != NULL)
    {
      typeformbuf = malloc(outlen);
      memcpy(typeformbuf, typeform, outlen);
    }
  inlen = extParseChars(str, inbuf);
  if (!inlen)
    {
      printf("Cannot parse input string.\n");
      return 1;
    }
  if (direction == 0)
    {
      funcStatus = lou_translate(tableList, inbuf, &inlen, outbuf, &outlen,
		     typeformbuf, NULL, NULL, NULL, NULL, mode);
    } else {
      funcStatus = lou_backTranslate(tableList, inbuf, &inlen, outbuf, &outlen,
		     typeformbuf, NULL, NULL, NULL, NULL, mode);
    }
  if (!funcStatus)
    {
      printf("Translation failed.\n");
      return 1;
    }

  expectedlen = extParseChars(expected, expectedbuf);
  for (i = 0; i < outlen && i < expectedlen && expectedbuf[i] == outbuf[i]; i++);
  if (i < outlen || i < expectedlen)
    {
      rv = 1;
      outbuf[outlen] = 0;
      printf("Input: '%s'\n", str);
      printf("Expected: '%s' (length %d)\n", expected, expectedlen);
      printf("Received: '");
      print_widechars(outbuf, outlen);
      printf("' (length %d)\n", outlen);

      if (i < outlen && i < expectedlen) 
	{
	  unsigned char expected_utf8[UTF8_BUFSIZE];
	  unsigned char out_utf8[UTF8_BUFSIZE];

	  ucs_to_utf8(expectedbuf[i], expected_utf8);
	  ucs_to_utf8(outbuf[i], out_utf8);
	  printf("Diff: Expected '%s' but received '%s' in index %d\n",
		 expected_utf8, out_utf8, i);
	}
      else if (i < expectedlen)
	{
	  unsigned char expected_utf8[UTF8_BUFSIZE];
	  ucs_to_utf8(expectedbuf[i], expected_utf8);
	  printf("Diff: Expected '%s' but received nothing in index %d\n",
		 expected_utf8, i);
	}
      else 
	{
	  unsigned char out_utf8[UTF8_BUFSIZE];
	  ucs_to_utf8(outbuf[i], out_utf8);
	  printf("Diff: Expected nothing but received '%s' in index %d\n",
		 out_utf8, i);
	}
    }

  free(inbuf);
  free(outbuf);
  free(expectedbuf);
  free(typeformbuf);
  return rv;
}

int
check_inpos(const char *tableList, const char *str, const int *expected_poslist)
{
  widechar *inbuf;
  widechar *outbuf;
  int *inpos;
  int inlen;
  int outlen;
  int i, rv = 0;

  inlen = strlen(str) * 2;
  outlen = inlen;
  inbuf = malloc(sizeof(widechar) * inlen);
  outbuf = malloc(sizeof(widechar) * outlen);
  inpos = malloc(sizeof(int) * inlen);
  for (i = 0; i < inlen; i++)
    {
      inbuf[i] = str[i];
    }
  lou_translate(tableList, inbuf, &inlen, outbuf, &outlen,
		NULL, NULL, NULL, inpos, NULL, 0);
  for (i = 0; i < outlen; i++)
    {
      if (expected_poslist[i] != inpos[i])
	{
	  rv = 1;
	  printf("Expected %d, recieved %d in index %d\n",
		 expected_poslist[i], inpos[i], i);
	}
    }

  free(inbuf);
  free(outbuf);
  free(inpos);
  return rv;

}

int
check_outpos(const char *tableList, const char *str, const int *expected_poslist)
{
  widechar *inbuf;
  widechar *outbuf;
  int *inpos, *outpos;
  int origInlen, inlen;
  int outlen;
  int i, rv = 0;

  origInlen = inlen = strlen(str);
  outlen = inlen * 2;
  inbuf = malloc(sizeof(widechar) * inlen);
  outbuf = malloc(sizeof(widechar) * outlen);
  /* outputPos can be affected by inputPos, so pass inputPos as well. */
  inpos = malloc(sizeof(int) * outlen);
  outpos = malloc(sizeof(int) * inlen);
  for (i = 0; i < inlen; i++)
    {
      inbuf[i] = str[i];
    }
  lou_translate(tableList, inbuf, &inlen, outbuf, &outlen,
		NULL, NULL, outpos, inpos, NULL, 0);
  if (inlen != origInlen)
    {
      printf("original inlen %d and returned inlen %d differ\n",
        origInlen, inlen);
    }

  for (i = 0; i < inlen; i++)
    {
      if (expected_poslist[i] != outpos[i])
	{
	  rv = 1;
	  printf("Expected %d, recieved %d in index %d\n",
		 expected_poslist[i], outpos[i], i);
	}
    }

  free(inbuf);
  free(outbuf);
  free(inpos);
  free(outpos);
  return rv;

}

int
check_cursor_pos(const char *tableList, const char *str, const int *expected_pos)
{
  widechar *inbuf;
  widechar *outbuf;
  int *inpos, *outpos;
  int inlen;
  int outlen;
  int cursor_pos;
  int i, rv = 0;

  inlen = strlen(str);
  outlen = inlen;
  inbuf = malloc(sizeof(widechar) * inlen);
  outbuf = malloc(sizeof(widechar) * inlen);
  inpos = malloc(sizeof(int) * inlen);
  outpos = malloc(sizeof(int) * inlen);

  inlen = extParseChars(str, inbuf);

  for (i = 0; i < inlen; i++)
    {
      cursor_pos = i;
      lou_translate(tableList, inbuf, &inlen, outbuf, &outlen,
		    NULL, NULL, NULL, NULL, &cursor_pos, compbrlAtCursor);
      if (expected_pos[i] != cursor_pos)
	{
	  rv = 1;
	  printf("string='%s' cursor=%d ('%c') expected=%d \
recieved=%d ('%c')\n", str, i, str[i], expected_pos[i], cursor_pos, (char) outbuf[cursor_pos]);
	}
    }

  free(inbuf);
  free(outbuf);
  free(inpos);
  free(outpos);

  return rv;
}

/* Check if a string is hyphenated as expected. Return 0 if the
   hyphenation is as expected and 1 otherwise. */
int
check_hyphenation(const char *tableList, const char *str, const char *expected)
{
  widechar *inbuf;
  char *hyphens;
  int inlen;
  int rv = 0;

  inlen = strlen(str);
  inbuf = malloc(sizeof(widechar) * inlen);
  inlen = extParseChars(str, inbuf);
  if (!inlen)
    {
      printf("Cannot parse input string.\n");
      return 1;
    }
  hyphens = calloc(inlen+1, sizeof(char));

  if (!lou_hyphenate(tableList, inbuf, inlen, hyphens, 0))
    {
      printf("Hyphenation failed.\n");
      return 1;
    }

  if (strcmp(expected, hyphens))
    {
      printf("Input:    '%s'\n", str);
      printf("Expected: '%s'\n", expected);
      printf("Received: '%s'\n", hyphens);
      rv = 1;
    }

  free(inbuf);
  free(hyphens);
  lou_free();
  return rv;

}
