#include <yaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

yaml_parser_t parser;
yaml_event_t event;

int
error(char *msg, yaml_event_t *event) {
  printf("%s (%zu:%zu)\n", msg, event->start_mark.line, event->start_mark.column);
  exit (EXIT_FAILURE);
}

int
read_tables(yaml_parser_t *parser) {
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SCALAR_EVENT) ||
      !strcmp(event.data.scalar.value, "tables"))
    error("Tables expected", &event);
  if (!yaml_parser_parse(parser, &event) ||
      (event.type != YAML_SEQUENCE_START_EVENT))
    error("Tables sequence expected", &event);

  int done = 0;
  char *tables_list = malloc (512);
  while (!done) {
    if (yaml_parser_parse(parser, &event) &&
	(event.type == YAML_SEQUENCE_START_EVENT)) {
      strcat(tables_list, event.data.scalar.value);
    }
  }
}

void
read_flags(yaml_parser_t *parser) {
}

void
read_tests(yaml_parser_t *parser) {
}

void
print_event_type(int event_type) {
  char* event_name;

  switch (event_type) {
  case YAML_STREAM_START_EVENT:
    event_name = "YAML_STREAM_START_EVENT";
    break;
  case YAML_STREAM_END_EVENT:
    event_name = "YAML_STREAM_END_EVENT";
    break;
  case YAML_DOCUMENT_START_EVENT:
    event_name = "YAML_DOCUMENT_START_EVENT";
    break;
  case YAML_DOCUMENT_END_EVENT:
    event_name = "YAML_DOCUMENT_END_EVENT";
    break;
  case YAML_SCALAR_EVENT:
    event_name = "YAML_SCALAR_EVENT";
    break;
  case YAML_SEQUENCE_START_EVENT:
    event_name = "YAML_SEQUENCE_START_EVENT";
    break;
  case YAML_SEQUENCE_END_EVENT:
    event_name = "YAML_SEQUENCE_END_EVENT";
    break;
  case YAML_MAPPING_START_EVENT:
    event_name = "YAML_MAPPING_START_EVENT";
    break;
  case YAML_MAPPING_END_EVENT:
    event_name = "YAML_MAPPING_END_EVENT";
    break;
  default :
    sprintf(event_name, "unknown event %i", event_type);
  }
  printf("Event %s\n", event_name);
}

int
main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s file1.yaml\n", argv[0]);
    return 0;
  }

  FILE *file;
  yaml_parser_t parser;
  yaml_event_t event;
  int done = 0;
  int count = 0;
  int error = 0;

  file = fopen(argv[1], "rb");
  assert(file);
  
  assert(yaml_parser_initialize(&parser));
  
  yaml_parser_set_input_file(&parser, file);
  
  while (!done)
    {
      if (!yaml_parser_parse(&parser, &event)) {
	error = 1;
	break;
      }
      
      print_event_type(event.type);

      if (event.type == YAML_SCALAR_EVENT) {
	printf("Scalar event: %s\n", event.data.scalar.value);
      }

      done = (event.type == YAML_STREAM_END_EVENT);
      
      yaml_event_delete(&event);
      
      count ++;
    }
  
  yaml_parser_delete(&parser);
  
  assert(!fclose(file));
  
  printf("%s (%d events)\n", (error ? "FAILURE" : "SUCCESS"), count);
  
  return error ? 1 : 0;
}
