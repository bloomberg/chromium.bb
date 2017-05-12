noinst_LTLIBRARIES = liblouis-table-dev.la

liblouis_table_dev_la_SOURCES = table-dev.c

liblouis_table_dev_la_LDFLAGS = -rpath /nowhere

liblouis_table_dev_la_LIBADD = $(top_builddir)/liblouis/liblouis.la

check_PROGRAMS = test-table-dev

test_table_dev_SOURCES = test-table-dev.c

test_table_dev_LDADD = $(top_builddir)/table-dev/liblouis-table-dev.la

TESTS =	$(check_PROGRAMS)

TESTS_ENVIRONMENT =	LD_LIBRARY_PATH=$(top_srcdir)/table-dev/.libs

#################################################################

BASE_TABLE         = base.cti
PATTERNS_TABLE     = patterns.dic
CONTRACTIONS_TABLE = contractions.ctb
DICTIONARY         = dictionary.sqlite
WORKING_FILE       = working_file.txt

LEFT_HYPHEN_MIN    = 1
RIGHT_HYPHEN_MIN   = 1
HYPH_LEVEL         = 1
PAT_START          = 1
PAT_FINISH         = 15
GOOD_WEIGHT        = 1
BAD_WEIGHT         = 100
THRESHOLD          = 1

.PHONY : patterns
patterns : $(PATTERNS_TABLE)

.PHONY : tables
tables : $(CONTRACTIONS_TABLE) $(PATTERNS_TABLE)

.PHONY : dictionary
dictionary : $(DICTIONARY)

.PHONY : suggestions
suggestions : $(CONTRACTIONS_TABLE) $(PATTERNS_TABLE)

suggestions :
	bash wrap_python.sh make_suggestions.py -d $(DICTIONARY) -t $(BASE_TABLE),$(CONTRACTIONS_TABLE),$(PATTERNS_TABLE) -x true >$(WORKING_FILE)
	$(EDITOR) $(WORKING_FILE)

.INTERMEDIATE : $(WORKING_FILE)

$(CONTRACTIONS_TABLE) : $(WORKING_FILE)
	bash submit_rules.sh $< $@ $(BASE_TABLE)

$(DICTIONARY) : $(WORKING_FILE)
	bash submit_rows.sh $< $@ $(BASE_TABLE)

$(PATTERNS_TABLE) : patterns.$(HYPH_LEVEL).dic check-patterns
	cp $< $@

# dictionary file must not contain bad (.) or missed (-) hyphens"
.PHONY : check-patterns
check-patterns : dictionary.$(HYPH_LEVEL)
	if cat $< | grep '\.[^0]\|-' >/dev/null; then false; else true; fi

$(patsubst %,patterns.%.dic,1 2 3 4 5 6 7 8 9) : %.dic : %
	perl substrings.pl $< tmp
	@echo "UTF-8\n\
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\
% auto-generated file, don't edit! %\n\
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" | cat - tmp >$@
	@rm tmp

-include make-patterns.mk
make-patterns.mk : Makefile
	rm -f $@
	@for level in $(HYPH_LEVEL); do \
		echo "patterns.$$(($$level-1)) :" && \
		echo "	touch \$$@" && \
		echo "" && \
		echo "dictionary.$$(($$level-1)) : \$$(DICTIONARY)" && \
		echo "	bash wrap_python.sh export_chunked_words.py -d \$$< >\$$@" && \
		echo "" && \
		echo "patterns.$$level : dictionary.$$(($$level-1)) patterns.$$(($$level-1)) alphabet" && \
		echo "	bash wrap_patgen.sh \$$< \$$(word 2,\$$^) \$$@ \$$(word 3,\$$^) \\" && \
		echo "		\$$(LEFT_HYPHEN_MIN) \$$(RIGHT_HYPHEN_MIN) $$level \$$(PAT_START) \$$(PAT_FINISH) \$$(GOOD_WEIGHT) \$$(BAD_WEIGHT) \$$(THRESHOLD)" && \
		echo "" && \
		echo "dictionary.$$level : \$$(DICTIONARY) patterns.$$level.dic pattmp.$$level" && \
		echo "	bash wrap_python.sh export_chunked_words.py -d \$$< -t \$$(BASE_TABLE),\$$(word 2,\$$^) >\$$@" && \
		echo "	# dictionary file that we produced must be identical to the one that patgen produced" && \
		echo "	diff \$$@ \$$(word 3,\$$^) >/dev/null" && \
		echo "" && \
		echo ".PHONY : clean-patterns" && \
		echo "clean-patterns :" && \
		echo "	rm -f patterns.$$level patterns.$$(($$level-1))" && \
		echo "	rm -f dictionary.$$level dictionary.$$(($$level-1))" && \
		echo "	rm -f pattmp.$$level" && \
		echo "	" && \
		echo ""; \
	done >$@

alphabet : $(DICTIONARY)
	bash wrap_python.sh generate_alphabet.py -d $< -t $(BASE_TABLE) >$@

clean : clean-table-dev

.PHONY : clean-table-dev
clean-table-dev : clean-patterns
	rm -f patterns.$(HYPH_LEVEL).dic alphabet $(WORKING_FILE) make-patterns.mk
