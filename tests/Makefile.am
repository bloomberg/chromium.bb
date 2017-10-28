SUBDIRS = tables tablesWithMetadata yaml braille-specs ueb_test_data

AM_CPPFLAGS =					\
	-I$(top_srcdir)/liblouis		\
	-I$(top_srcdir)/gnulib			\
	-I$(top_srcdir)/tools			\
	-I$(top_builddir)/gnulib		\
	-I$(top_builddir)/tools

LDADD =							\
	$(top_builddir)/liblouis/liblouis.la		\
	$(top_builddir)/gnulib/libgnu.la		\
	$(top_builddir)/tools/libbrlcheck.la		\
	$(top_builddir)/tools/gnulib/libgnutools.la	\
	$(LTLIBINTL)

backtranslate_SOURCES = backtranslate.c
backtranslate_noUndefinedDots_SOURCES = backtranslate_noUndefinedDots.c
backtranslate_partialTrans_SOURCES = backtranslate_noUndefinedDots.c
backtranslate_with_letsign_SOURCES = backtranslate_with_letsign.c
capitalization_SOURCES = capitalization.c
capitalized_word_SOURCES = default_table.h capitalized_word.c
checkTable_SOURCES = checkTable.c
compbrlAtCursor_SOURCES = compbrlAtCursor.c
emphclass_SOURCES = emphclass.c
en_gb_g1_italics_SOURCES = en_gb_g1_italics.c
findTable_SOURCES = findTable.c
getTable_SOURCES = getTable.c
hash_collision_SOURCES = hash_collision.c
hyphenate_achena_SOURCES = hyphenate_achena.c
hyphenate_alderen_SOURCES = hyphenate_alderen.c
hyphenate_straightforward_SOURCES = hyphenate_straightforward.c
hyphenate_xxx_SOURCES = hyphenate_xxx.c
inpos_SOURCES = inpos.c
inpos_compbrl_SOURCES = default_table.h inpos_compbrl.c
inpos_match_replace_SOURCES = inpos_match_replace.c
lastworditalafter_SOURCES = default_table.h lastworditalafter.c
letterDefTest_SOURCES = letterDefTest.c
logging_SOURCES = logging.c
outpos_SOURCES = outpos.c
pass0_typebuf_SOURCES = pass0_typebuf.c
pass1Only_SOURCES = pass1Only.c
pass2_SOURCES = pass2.c
pass2_inpos_SOURCES = pass2_inpos.c
present_progressive_SOURCES = default_table.h present_progressive.c
resolve_table_SOURCES = resolve_table.c
squash_space_SOURCES = squash_space.c
typeform_SOURCES = typeform.c
typeform_for_emphclass_SOURCES = typeform_for_emphclass.c
uplow_with_unicode_SOURCES = uplow_with_unicode.c
check_metadata_SOURCES = check_metadata.c
suggestChunks_SOURCES = suggestChunks.c

# Note that this is not currently included in check_programs below
# otherwise make check would never complete.
infiniteTranslationLoop_SOURCES = infiniteTranslationLoop.c
noinst_PROGRAMS = infiniteTranslationLoop

program_TESTS =					\
	backtranslate				\
	backtranslate_noUndefinedDots		\
	backtranslate_partialTrans		\
	backtranslate_with_letsign		\
	capitalization				\
	capitalized_with_sentance		\
	capitalized_word			\
	checkTable				\
	check_metadata				\
	compbrlAtCursor				\
	emphclass				\
	en_gb_g1_italics			\
	findTable				\
	getTable				\
	hash_collision				\
	hyphenate_achena			\
	hyphenate_alderen			\
	hyphenate_straightforward		\
	hyphenate_xxx				\
	inpos					\
	inpos_compbrl				\
	inpos_match_replace			\
	lastworditalafter			\
	letterDefTest				\
	logging					\
	outpos					\
	pass0_typebuf				\
	pass1Only				\
	pass2					\
	pass2_inpos				\
	present_progressive			\
	resolve_table				\
	squash_space				\
	typeform				\
	typeform_for_emphclass			\
	uplow_with_unicode				\
	suggestChunks

check_PROGRAMS = $(program_TESTS)

dist_check_SCRIPTS =

# The test scripts aren't really set up to work in a cross compilation
if !CROSS_COMPILING
dist_check_SCRIPTS +=	 			\
	check_all_tables.pl			\
	check_endless_loop.pl			\
	check_included_tables.sh		\
	multiple_table_path.pl			\
	ueb_test_data.pl
endif

# The present_progressive should not really fail but for the moment
# the issue doesn't seem to be critical enough that anybody has enough
# interest to fix it.
XFAIL_TESTS =					\
	present_progressive			\
	lastworditalafter			\
	en_gb_g1_italics			\
	squash_space				\
	compbrlAtCursor

dist_yaml_TESTS =			  \
	yaml/back_cont_then_punc.yaml	  \
	yaml/begcaps_endcaps.yaml	  \
	yaml/before_begmidword.yaml	  \
	yaml/capsnocont.yaml		  \
	yaml/capsword_for_back.yaml	  \
	yaml/emphasis-full.yaml		  \
	yaml/emphasis-no-context.yaml	  \
	yaml/example_test.yaml		  \
	yaml/foo.yaml			  \
	yaml/issue-238.yaml		  \
	yaml/issue-401.yaml		  \
	yaml/issue-35.yaml		  \
	yaml/issue-384.yaml		  \
	yaml/issue-400.yaml		  \
	yaml/letterDefTest_harness.yaml	  \
	yaml/multipass-backward.yaml	  \
	yaml/multipass-forward.yaml	  \
	yaml/multipass.yaml		  \
	yaml/new_emph.yaml		  \
	yaml/nocross_harness.yaml	  \
	yaml/noletsignafter.yaml	  \
	yaml/syllable.yaml

TEST_EXTENSIONS = .yaml
YAML_LOG_COMPILER =  $(WINE) $(top_builddir)/tools/lou_checkyaml$(EXEEXT)
LOG_COMPILE = $(WINE)

EXTRA_DIST = $(dist_yaml_TESTS)

XFAIL_TESTS += ueb_test_data.pl

TESTS =				     \
	$(program_TESTS)	     \
	$(dist_check_SCRIPTS)	     \
	$(dist_yaml_TESTS)

AM_TESTS_ENVIRONMENT =								\
	LOUIS_TABLEPATH=$(abs_top_srcdir)					\
	UEB_TEST_DATA_PATH=$(top_srcdir)/tests/ueb_test_data			\
	LD_LIBRARY_PATH=$(top_builddir)/liblouis/.libs:$$LD_LIBRARY_PATH	\
	PATH=$(top_builddir)/tools:$$PATH

