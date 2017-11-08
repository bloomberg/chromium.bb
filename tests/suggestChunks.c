/* liblouis Braille Translation and Back-Translation Library

Copyright (C) 2017 Swiss Library for the Blind, Visually Impaired and Print Disabled

Copying and distribution of this file, with or without modification,
are permitted in any medium without royalty provided the copyright
notice and this notice are preserved. This file is offered as-is,
without any warranty. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "internal.h"

extern void loadTable(const char *tableList);
extern widechar toLowercase(widechar c);
extern int suggestChunks(widechar *text, widechar *braille, char *hyphen_string);
extern int find_matching_rules(widechar *text, int text_len, widechar *braille, int braille_len, char *data);

static int test_toLowercase() {
	widechar upper = 'A';
	widechar lower = 'a';
	return lower != toLowercase(upper);
}

static int test_suggestChunks() {
	widechar text[] = {'f', 'o', 'o', 'b', 'a', 'r', '\0'};
	widechar braille[] = {'f', 'u', 'b', 'r', '\0'};
	char hyphen_string[8];
	if (!suggestChunks(text, braille, hyphen_string))
		return 1;
	return strcmp("^00x00$", hyphen_string);
}

static void test_suggestChunks_longWord() {
	char* tableList =							\
		"sbs-table-dev/sbs.dis,"				\
		"sbs-table-dev/sbs-de-core6.cti,"		\
		"sbs-table-dev/sbs-apos.cti,"			\
		"sbs-table-dev/sbs-de-accents.cti,"	\
		"sbs-table-dev/sbs-contractions.ctb,"	\
		"sbs-table-dev/sbs-patterns.dic";

	char *text          = "achtunddreißigtausenddreihundertsiebzehn";
	char *braille       = "A4TUNDDR3^IGT1SENDDR3HUNDERTS0BZEHN";
	char *hyphen_string[40];

	int in_len = strlen(text);
	int out_len = in_len;

	widechar *inbuf = malloc(sizeof(widechar) * in_len);
	widechar *outbuf = malloc(sizeof(widechar) * out_len);

	in_len = _lou_extParseChars(text, inbuf);
	out_len = _lou_extParseChars(braille, outbuf);

	loadTable(tableList);
	
	find_matching_rules(inbuf, in_len, outbuf, out_len, hyphen_string);
}

int main(int argc, char **argv) {
	loadTable("tests/tables/suggestChunks.ctb");
	if (test_toLowercase())
		return 1;
	if (test_suggestChunks())
		return 1;
	test_suggestChunks_longWord();
	return 0;
}
