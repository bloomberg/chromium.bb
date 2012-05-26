bin_PROGRAMS = wcap-decode wcap-snapshot

wcap_decode_SOURCES =				\
	args.c					\
	args.h					\
	EbmlIDs.h				\
	EbmlWriter.c				\
	EbmlWriter.h				\
	mem_ops_aligned.h			\
	mem_ops.h				\
	tools_common.c				\
	tools_common.h				\
	vpxenc.c				\
	vpx_timer.h				\
	y4minput.c				\
	y4minput.h				\
	wcap-decode.c				\
	wcap-decode.h

wcap_decode_CPPFLAGS = -DCONFIG_VP8_ENCODER=1
wcap_decode_CFLAGS = $(WCAP_CFLAGS)
wcap_decode_LDADD = $(WCAP_LIBS)

wcap_snapshot_SOURCES =				\
	wcap-snapshot.c				\
	wcap-decode.c				\
	wcap-decode.h

wcap_snapshot_CFLAGS = $(WCAP_CFLAGS)
wcap_snapshot_LDADD = $(WCAP_LIBS)
