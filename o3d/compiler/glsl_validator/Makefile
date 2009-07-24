# Makefile for the Flex/Bison GLSL ES grammar and simple test harness.
# "make" to build everything.
# "make clean" to clean the build.
# "make test" to run tests.

CC=gcc
BUILD=build
SRC=glsl_es

LEXER_INPUT=$(SRC)/GLSL_ES.l
GRAMMAR_INPUT=$(SRC)/GLSL_ES.y
GRAMMAR_INPUTS=$(LEXER_INPUT) $(GRAMMAR_INPUT)
LEXER_OUTPUT=$(BUILD)/GLSL_ES.lex.c
GRAMMAR_OUTPUT=$(BUILD)/GLSL_ES.tab.c
GRAMMAR_OUTPUTS=$(LEXER_OUTPUT) $(GRAMMAR_OUTPUT)

$(BUILD)/GLSLESParser: $(GRAMMAR_OUTPUTS)
	$(CC) -I$(BUILD) $(GRAMMAR_OUTPUTS) -o $(BUILD)/GLSLESParser

$(GRAMMAR_OUTPUTS): $(GRAMMAR_INPUTS)
	mkdir -p $(BUILD)
	lex -o $(LEXER_OUTPUT) $(LEXER_INPUT)
	bison -o $(GRAMMAR_OUTPUT) -v -d --debug $(GRAMMAR_INPUT)

.PHONY: test

test:
	@for i in $(shell ls shaders/*.vert) $(shell ls shaders/*.frag) ; do \
		echo Parsing $$i ; \
		time $(BUILD)/GLSLESParser < $$i ; \
	done

clean:
	rm $(BUILD)/GLSLESParser $(GRAMMAR_OUTPUTS) $(BUILD)/GLSL_ES.output $(BUILD)/GLSL_ES.tab.h
