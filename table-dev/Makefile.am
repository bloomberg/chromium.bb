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

HYPH_LEVELS       =  1    2    3
LEFT_HYPHEN_MIN   =  1    1    1
RIGHT_HYPHEN_MIN  =  1    1    1
PAT_START         =  1    1    1
PAT_FINISH        =  15   15   15
GOOD_WEIGHT       =  1    1    1
BAD_WEIGHT        =  100  100  100
THRESHOLD         =  1    1    1

MAX_HYPH_LEVEL = $(word $(words $(HYPH_LEVELS)),$(HYPH_LEVELS))

.PHONY : patterns
patterns : $(PATTERNS_TABLE)

.PHONY : tables
tables : $(CONTRACTIONS_TABLE) $(PATTERNS_TABLE)

.PHONY : dictionary
dictionary : $(DICTIONARY)

.PHONY : suggestions
suggestions : $(CONTRACTIONS_TABLE) $(PATTERNS_TABLE)

suggestions :
	bash wrap_python.sh make_suggestions.py -d $(DICTIONARY) -t $(BASE_TABLE),$(CONTRACTIONS_TABLE),$(PATTERNS_TABLE) --auto-chunk true >$(WORKING_FILE)
	$(EDITOR) $(WORKING_FILE)

.INTERMEDIATE : $(WORKING_FILE)

$(CONTRACTIONS_TABLE) : $(WORKING_FILE)
	bash submit_rules.sh $< $@ $(BASE_TABLE)

$(DICTIONARY) : $(WORKING_FILE)
	bash submit_rows.sh $< $@ $(BASE_TABLE)

$(PATTERNS_TABLE) : patterns.$(MAX_HYPH_LEVEL).dic check-patterns
	cp $< $@

# dictionary file must not contain bad (.) or missed (-) hyphens"
.PHONY : check-patterns
check-patterns : dictionary.$(MAX_HYPH_LEVEL)
	if cat $< | grep '\.[^0]\|-' >/dev/null; then false; else true; fi

-include make-patterns.mk
make-patterns.mk : Makefile
	@while true; do \
		echo "patterns.0 :" && \
		echo "	touch \$$@" && \
		echo "" && \
		echo "patterns.0.dic :" && \
		echo "	echo \"UTF-8\" >\$$@" && \
		echo "" && \
		echo "dictionary.0 : \$$(DICTIONARY)" && \
		echo "	bash wrap_python.sh export_chunked_words.py -d \$$< >\$$@" && \
		echo "" && \
		prev_level=0 && \
		for level in $(HYPH_LEVELS); do \
			echo "patterns.$$level : dictionary.$$prev_level patterns.$$prev_level alphabet" && \
			echo "	if cat $$< | grep '\.[^0]\|-' >/dev/null; then \\" && \
			echo "		bash wrap_patgen.sh \$$< \$$(word 2,\$$^) \$$@ \$$(word 3,\$$^) \\" && \
			echo "			\$$(word $$level,\$$(LEFT_HYPHEN_MIN)) \\" && \
			echo "			\$$(word $$level,\$$(RIGHT_HYPHEN_MIN)) \\" && \
			echo "			$$level \\" && \
			echo "			\$$(word $$level,\$$(PAT_START)) \\" && \
			echo "			\$$(word $$level,\$$(PAT_FINISH)) \\" && \
			echo "			\$$(word $$level,\$$(GOOD_WEIGHT)) \\" && \
			echo "			\$$(word $$level,\$$(BAD_WEIGHT)) \\" && \
			echo "			\$$(word $$level,\$$(THRESHOLD)) \\" && \
			echo "			>patgen.log && \\" && \
			echo "		rm patgen.log; \\" && \
			echo "	else \\" && \
			echo "		cp \$$(word 2,\$$^) \$$@ && \\" && \
			echo "		rm -f pattmp.$$level; \\" && \
			echo "	fi" && \
			echo "" && \
			echo "dictionary.$$level : \$$(DICTIONARY) patterns.$$level.dic" && \
			echo "	if [ -e pattmp.$$level ]; then \\" && \
			echo "		bash wrap_python.sh export_chunked_words.py -d \$$< -t \$$(BASE_TABLE),\$$(word 2,\$$^) >\$$@ && \\" && \
			echo "		diff \$$@ pattmp.$$level >/dev/null; \\" && \
			echo "	else \\" && \
			echo "		cp dictionary.$$prev_level \$$@; \\" && \
			echo "	fi" && \
			echo "" && \
			echo "patterns.$$level.dic : %.dic : % patterns.$$prev_level.dic" && \
			echo "	if [ -e pattmp.$$level ]; then \\" && \
			echo "		perl substrings.pl \$$< tmp >substrings.log && \\" && \
			echo "		rm substrings.log && \\" && \
			echo "		echo \"UTF-8\\\n\\" && \
			echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\\\n\\" && \
			echo "% auto-generated file, don't edit! %\\\n\\" && \
			echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\" | cat - tmp >\$$@; \\" && \
			echo "	else \\" && \
			echo "		cp \$$(word 2,\$$^) \$$@; \\" && \
			echo "	fi" && \
			echo "" && \
			prev_level=$$level; \
		done && \
		echo ".PHONY : clean-patterns" && \
		echo "clean-patterns :" && \
		echo "	rm -f patterns.0 $(patsubst %,patterns.%,$(HYPH_LEVELS))" && \
		echo "	rm -f patterns.0.dic $(patsubst %,patterns.%.dic,$(HYPH_LEVELS))" && \
		echo "	rm -f dictionary.0 $(patsubst %,dictionary.%,$(HYPH_LEVELS))" && \
		echo "	rm -f $(patsubst %,pattmp.%,$(HYPH_LEVELS))" && \
		echo "" && \
		break; \
	done >$@

alphabet : $(DICTIONARY)
	bash wrap_python.sh generate_alphabet.py -d $< -t $(BASE_TABLE) >$@

clean : clean-table-dev

.PHONY : clean-table-dev
clean-table-dev : clean-patterns
	rm -f alphabet $(WORKING_FILE) make-patterns.mk substrings.log patgen.log
