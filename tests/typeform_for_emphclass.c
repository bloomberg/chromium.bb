#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "liblouis.h"

int
main (int argc, char **argv)
{
	int result = 0;
	const char *table = "emphclass_valid.utb";
	const char *class;
	class = "italic";
	formtype typeform = italic;
	if (lou_getTypeformForEmphClass(table, class) != typeform) {
		fprintf(stderr, "%s should have typeform %x\n", class, typeform);
		result = 1;
	}
	class = "underline";
	typeform = underline;
	if (lou_getTypeformForEmphClass(table, class) != typeform) {
		fprintf(stderr, "%s should have typeform %x\n", class, typeform);
		result = 1;
	}
	class = "bold";
	typeform = bold;
	if (lou_getTypeformForEmphClass(table, class) != typeform) {
		fprintf(stderr, "%s should have typeform %x\n", class, typeform);
		result = 1;
	}
	class = "foo";
	typeform = emph_4;
	if (lou_getTypeformForEmphClass(table, class) != typeform) {
		fprintf(stderr, "%s should have typeform %x\n", class, typeform);
		result = 1;
	}
	class = "bar";
	if (lou_getTypeformForEmphClass(table, class) != 0) {
		fprintf(stderr, "%s should not be defined\n", class);
		result = 1;
	}
	lou_free();
	return result;
}
