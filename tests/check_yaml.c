#include <yaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "liblouis.h"
#include "brl_checks.h"

const char* event_names[] = {"YAML_NO_EVENT", "YAML_STREAM_START_EVENT", "YAML_STREAM_END_EVENT",
			     "YAML_DOCUMENT_START_EVENT", "YAML_DOCUMENT_END_EVENT",
			     "YAML_ALIAS_EVENT", "YAML_SCALAR_EVENT",
			     "YAML_SEQUENCE_START_EVENT", "YAML_SEQUENCE_END_EVENT",
			     "YAML_MAPPING_START_EVENT", "YAML_MAPPING_END_EVENT"};
const char* encoding_names[] = {"YAML_ANY_ENCODING", "YAML_UTF8_ENCODING",
				"YAML_UTF16LE_ENCODING", "YAML_UTF16BE_ENCODING"};

yaml_parser_t parser;
yaml_event_t event;

char *file_name;
int translation_mode = 0;

int errors = 0;
int count = 0;

int
error(char *msg, yaml_event_t *event) {
  printf("%s:%zu: error: %s\n", file_name, event->start_mark.line, msg);
  exit (EXIT_FAILURE);
}

int
yaml_error(yaml_event_type_t expected, yaml_event_t *event) {
  printf("%s:%zu: error: expected %s (actual %s)\n",
	 file_name, event->start_mark.line,
	 event_names[expected], event_names[event->type]);
  exit (EXIT_FAILURE);
}

void
read_tables(yaml_parser_t *parser, char *tables_list) {
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SCALAR_EVENT) ||
      strcmp(event.data.scalar.value, "tables")) {
    error("tables expected", &event);
  }

  yaml_event_delete(&event);

  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SEQUENCE_START_EVENT))
    yaml_error(YAML_SEQUENCE_START_EVENT, &event);

  yaml_event_delete(&event);

  int done = 0;
  char *p = tables_list;
  while (!done) {
    if (!yaml_parser_parse(parser, &event)) {
      error("Error in YAML", &event);
    }
    if (event.type == YAML_SEQUENCE_END_EVENT) {
      done = 1;
    } else if (event.type == YAML_SCALAR_EVENT ) {
      if (tables_list != p) strcat(p++, ",");
      strcat(p, event.data.scalar.value);
      p += event.data.scalar.length;
    }
    yaml_event_delete(&event);
  }
}

void
read_flags(yaml_parser_t *parser) {
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_MAPPING_START_EVENT))
    yaml_error(YAML_MAPPING_START_EVENT, &event);

  yaml_event_delete(&event);

  int done = 0;
  while (!done) {
    if (!yaml_parser_parse(parser, &event)) {
      error("Error in YAML", &event);
    }
    if (event.type == YAML_MAPPING_END_EVENT) {
      done = 1;
    } else if (event.type == YAML_SCALAR_EVENT ) {
      printf("Flag %s\n", event.data.scalar.value);
    }
    yaml_event_delete(&event);
  }
}

void
read_test(yaml_parser_t *parser, char *tables_list) {
  yaml_event_t event;
  char *word;
  char *translation;

  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SCALAR_EVENT))
    error("Word expected", &event);

  word = strndup(event.data.scalar.value, event.data.scalar.length);
  yaml_event_delete(&event);

  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SCALAR_EVENT))
    error("Translation expected", &event);

  translation = strndup(event.data.scalar.value, event.data.scalar.length);
  yaml_event_delete(&event);

  if (check_translation_with_mode(tables_list, word, NULL, translation, translation_mode))
    errors++;

  count++;
  free(word);
  free(translation);

  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SEQUENCE_END_EVENT))
    yaml_error(YAML_SEQUENCE_END_EVENT, &event);

  yaml_event_delete(&event);
}

void
read_tests(yaml_parser_t *parser, char *tables_list) {
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SEQUENCE_START_EVENT))
    yaml_error(YAML_SEQUENCE_START_EVENT, &event);

  yaml_event_delete(&event);

  int done = 0;
  while (!done) {
    if (!yaml_parser_parse(parser, &event)) {
      error("Error in YAML", &event);
    }
    if (event.type == YAML_SEQUENCE_END_EVENT) {
      done = 1;
      yaml_event_delete(&event);
    } else if (event.type == YAML_SEQUENCE_START_EVENT ) {
      yaml_event_delete(&event);
      read_test(parser, tables_list);
    } else {
      error("Unexpected event", &event);
    }
  }
}

int
main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s file.yaml\n", argv[0]);
    return 0;
  }

  FILE *file;
  yaml_parser_t parser;
  yaml_event_t event;

  file = fopen(argv[1], "rb");
  assert(file);

  file_name = argv[1];

  assert(yaml_parser_initialize(&parser));

  yaml_parser_set_input_file(&parser, file);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_STREAM_START_EVENT)) {
    yaml_error(YAML_STREAM_START_EVENT, &event);
  }
  printf("Encoding %s\n", encoding_names[event.data.stream_start.encoding]);
  yaml_event_delete(&event);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_DOCUMENT_START_EVENT)) {
    yaml_error(YAML_DOCUMENT_START_EVENT, &event);
  }
  yaml_event_delete(&event);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_MAPPING_START_EVENT)) {
    yaml_error(YAML_MAPPING_START_EVENT, &event);
  }
  yaml_event_delete(&event);

  char *tables_list = malloc(sizeof(char) * 512);
  read_tables(&parser, tables_list);
  printf("Tables: %s\n", tables_list);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_SCALAR_EVENT)) {
    yaml_error(YAML_SCALAR_EVENT, &event);
  }

  if (!strcmp(event.data.scalar.value, "flags")) {
    yaml_event_delete(&event);
    read_flags(&parser);

    if (!yaml_parser_parse(&parser, &event) ||
	(event.type != YAML_SCALAR_EVENT) ||
	strcmp(event.data.scalar.value, "tests")) {
      error("tests expected", &event);
    }
    yaml_event_delete(&event);
    read_tests(&parser, tables_list);

  } else if (!strcmp(event.data.scalar.value, "tests")) {
    yaml_event_delete(&event);
    read_tests(&parser, tables_list);
  } else {
    error("flags or tests expected", &event);
  }

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_MAPPING_END_EVENT)) {
    yaml_error(YAML_MAPPING_END_EVENT, &event);
  }
  yaml_event_delete(&event);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_DOCUMENT_END_EVENT)) {
    yaml_error(YAML_DOCUMENT_END_EVENT, &event);
  }
  yaml_event_delete(&event);

  if (!yaml_parser_parse(&parser, &event) ||
      (event.type != YAML_STREAM_END_EVENT)) {
    yaml_error(YAML_STREAM_END_EVENT, &event);
  }
  yaml_event_delete(&event);

  yaml_parser_delete(&parser);

  assert(!fclose(file));

  printf("%s (%d tests)\n", (errors ? "FAILURE" : "SUCCESS"), count);

  return errors ? 1 : 0;
}
