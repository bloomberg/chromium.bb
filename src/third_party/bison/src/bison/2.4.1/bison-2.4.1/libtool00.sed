s/~/!/g
s/ln -s/cp -fp/g
/^build_libtool_need_lc/s/=yes/=no/
s/^.*X-lc.*continue.*$$/\#&/
s/^.*X-lm.*continue.*$$/\#&/
#s/pic_flag="/& -D_DLL -D_BUILD_DLL/
#s/export_dynamic_flag_spec="/&\${wl}--export-all-symbols /
#s/export_dynamic=no/export_dynamic=yes/g
s/versuffix="-$major"/versuffix="$major"/
s/--no-undefined/--allow-shlib-undefined/
/^allow_undefined_flag/s/unsupported/--allow-shlib-undefined/
/^deplibs_check_method/s/".*"/"pass_all"/
/^soname_spec/s|/-/|//|g
/^soname_spec=/s/libname/dllname/
/^\trelease)/i\
	dllname)\
	  dllname="$arg"\
	  prev=\
	  continue\
	  ;;
/^[ ]*-release)/i\
	 -dllname)\
	prev=dllname\
	continue\
	;;\

s/-pc-cygwin/-pc-mingw32/g
s/pwd/pwd.sh/g
s/-Wl,-e,_DllMain/-shared &/g
/versuffix="$major"/a\
	  if test "$versuffix" = 0; then\
		versuffix=\
	  fi
s/-DDLL_EXPORT/& /g
