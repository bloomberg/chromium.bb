harness_TESTS =

EXTRA_DIST = $(harness_TESTS)			\
	     runHarness.py

HARNESS_DIR = $(top_srcdir)/tests/harness

harness_ENVIRONMENT =												\
	HARNESS_DIR=$(HARNESS_DIR)										\
	LOUIS_TABLEPATH=$(top_srcdir)/tables,$(top_srcdir)/tests/tables,$(top_srcdir)/tests/tables/moreTables	\
	PYTHONPATH=$(HARNESS_DIR):$(top_builddir)/python:$$PYTHONPATH						\
	LD_LIBRARY_PATH=$(top_builddir)/liblouis/.libs:$$LD_LIBRARY_PATH					\
	PATH=$(top_builddir)/tools:$$PATH

if HAVE_PYTHON
if HAVE_UCS4
runall runAll:
	@$(harness_ENVIRONMENT) $(HARNESS_DIR)/runHarness.py

$(harness_TESTS):
	@$(harness_ENVIRONMENT) $(HARNESS_DIR)/runHarness.py --compact_output $@
else
runall runAll:
	@echo Please configure with --enable-ucs4
endif
else
runall runAll:
	@echo Python is required to run the harness tests.
endif

check:
	@true

.PHONY: runall runAll $(harness_TESTS)
