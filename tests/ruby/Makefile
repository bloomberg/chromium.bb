#
# simple GNU Makefile for Ruby demo
# (Linux, Mac & Cygwin)
#
# Please see googleclient/native_client/common/Makefile.mk for details

# if nacl target, copy nexe to ruby.nexe for ruby.html to launch
ifeq (nacl,$(filter nacl,$(MAKECMDGOALS)))
POST_BUILD = cp $(EXE_NAME) ruby.nexe
CC = $(NACL_BIN_PATH)/nacl-gcc
else
$(error This sample only works with nacl builds.)
endif
SUBSYSTEM :=
RUBY_DIR := ruby-1.8.6-p368
RUBY_URL := ftp://ftp.ruby-lang.org/pub/ruby/1.8/ruby-1.8.6-p368.tar.gz

CCFLAGS=-Wall
NAME:=ruby

LIBS = -lgoogle_nacl_npruntime
LIBS+= -lgoogle_nacl_imc
LIBS+= -lnosys
NACL_INCLUDE = -I$(GOOGLE_CLIENT)/third_party/npapi/files/include \
               -I$(GOOGLE_CLIENT)/native_client/tests/ruby/$(RUBY_DIR)/

RUBY_FILES := array.c \
              bignum.c \
              class.c \
              compar.c \
              dir.c \
              dln.c \
              dmyext.c \
              enum.c \
              error.c \
              eval.c \
              file.c \
              gc.c \
              hash.c \
              inits.c \
              io.c \
              marshal.c \
              math.c \
              missing/crypt.c \
              missing/flock.c \
              numeric.c \
              object.c \
              pack.c \
              parse.c \
              prec.c \
              process.c \
              random.c \
              range.c \
              re.c \
              regex.c \
              ruby.c \
              signal.c \
              signal_missing.c \
              sprintf.c \
              st.c \
              string.c \
              struct.c \
              time.c \
              util.c \
              variable.c \
              version.c

RUBY_OBJS = $(addprefix $(RUBY_DIR)/, $(patsubst %.c, %.o, $(RUBY_FILES)))

FILES:=main.c $(RUBY_OBJS)

include ../../common/Makefile.mk

$(RUBY_OBJS): %.o: %.c
	$(CC) $(INCLUDE) $(CCFLAGS) $(OPT) $(XRAY_OPT) $(DBG) -c $< -o $@

.PHONY: allclean download

golden:
	$(SEL_LDR) ruby.nexe < stdin > stdout
	diff stdout stdout_golden
	@echo Golden tests passed.

download:
	../../tools/download.sh $(RUBY_URL) ruby.tgz
	rm -rf $(RUBY_DIR)
	tar -xzf ruby.tgz
	rm -f ruby.tgz
	cp -rf appendix/* $(RUBY_DIR)/

allclean: clean
	rm -rf ./ruby-1.8.6-p368
	rm -f ./*.nexe
	rm -f $(RUBY_OBJS)
