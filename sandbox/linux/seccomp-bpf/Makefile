DEF_CFLAGS = -g -O3 -Wall -Werror -Wextra -Wno-missing-field-initializers -fPIC -I.
DEF_CPPFLAGS = -D_GNU_SOURCE -DSECCOMP_BPF_STANDALONE -iquote ../../..
DEF_LDFLAGS = -g -lpthread
DEPFLAGS = -MMD -MF .$@.d
MODS := demo sandbox_bpf basicblock codegen die errorcode syscall syscall_iterator trap verifier
OBJS64 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o64/')
OBJS32 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o32/')
ALL_OBJS = $(OBJS32) $(OBJS64)
DEP_FILES = $(wildcard $(foreach f,$(ALL_OBJS),.$(f).d))

.SUFFIXES: .o64 .o32

all: demo32 demo64

clean:
	$(RM) demo32 demo64
	$(RM) *.o *.o32 *.o64 .*.d
	$(RM) core core.* vgcore vgcore.* strace.log*

-include $(DEP_FILES)

demo32: ${OBJS32}
	${CXX} -m32 -o $@ $+ ${DEF_LDFLAGS} ${LDFLAGS}
demo64: ${OBJS64}
	${CXX} -m64 -o $@ $+ ${DEF_LDFLAGS} ${LDFLAGS}

.cc.o32:
	${CXX} -m32 ${DEF_CFLAGS} ${DEF_CPPFLAGS} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -c -o $@ $<
.cc.o64:
	${CXX} -m64 ${DEF_CFLAGS} ${DEF_CPPFLAGS} ${CFLAGS} ${CPPFLAGS} ${DEPFLAGS} -c -o $@ $<
