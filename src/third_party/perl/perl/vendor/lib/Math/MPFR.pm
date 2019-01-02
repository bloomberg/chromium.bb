    package Math::MPFR;
    use strict;
    use warnings;

    use constant  GMP_RNDN       => 0;
    use constant  GMP_RNDZ       => 1;
    use constant  GMP_RNDU       => 2;
    use constant  GMP_RNDD       => 3;
    use constant  MPFR_RNDN      => 0;
    use constant  MPFR_RNDZ      => 1;
    use constant  MPFR_RNDU      => 2;
    use constant  MPFR_RNDD      => 3;
    use constant  MPFR_RNDA      => 4;
    use constant  _UOK_T         => 1;
    use constant  _IOK_T         => 2;
    use constant  _NOK_T         => 3;
    use constant  _POK_T         => 4;
    use constant  _MATH_MPFR_T   => 5;
    use constant  _MATH_GMPf_T   => 6;
    use constant  _MATH_GMPq_T   => 7;
    use constant  _MATH_GMPz_T   => 8;
    use constant  _MATH_GMP_T    => 9;
    use constant  _MATH_MPC_T    => 10;


    use subs qw(MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
                MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING
                RMPFR_PREC_MIN RMPFR_PREC_MAX);

    use overload
    '+'    => \&overload_add,
    '-'    => \&overload_sub,
    '*'    => \&overload_mul,
    '/'    => \&overload_div,
    '+='   => \&overload_add_eq,
    '-='   => \&overload_sub_eq,
    '*='   => \&overload_mul_eq,
    '/='   => \&overload_div_eq,
    '""'   => \&overload_string,
    '>'    => \&overload_gt,
    '>='   => \&overload_gte,
    '<'    => \&overload_lt,
    '<='   => \&overload_lte,
    '<=>'  => \&overload_spaceship,
    '=='   => \&overload_equiv,
    '!='   => \&overload_not_equiv,
    '!'    => \&overload_not,
    'bool' => \&overload_true,
    '='    => \&overload_copy,
    'abs'  => \&overload_abs,
    '**'   => \&overload_pow,
    '**='  => \&overload_pow_eq,
    'atan2'=> \&overload_atan2,
    'cos'  => \&overload_cos,
    'sin'  => \&overload_sin,
    'log'  => \&overload_log,
    'exp'  => \&overload_exp,
    'int'  => \&overload_int,
    'sqrt' => \&overload_sqrt;

    require Exporter;
    *import = \&Exporter::import;
    require DynaLoader;

    @Math::MPFR::EXPORT_OK = qw(GMP_RNDN GMP_RNDZ GMP_RNDU GMP_RNDD
MPFR_RNDN MPFR_RNDZ MPFR_RNDU MPFR_RNDD MPFR_RNDA
MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING RMPFR_VERSION_NUM
RMPFR_PREC_MIN RMPFR_PREC_MAX
Rgmp_randclear Rgmp_randinit_mt
Rgmp_randinit_default Rgmp_randinit_lc_2exp 
Rgmp_randinit_lc_2exp_size Rgmp_randseed Rgmp_randseed_ui 
Rmpfr_abs Rmpfr_acos Rmpfr_acosh Rmpfr_add Rmpfr_add_q
Rmpfr_add_si Rmpfr_add_ui Rmpfr_add_z 
Rmpfr_agm Rmpfr_asin Rmpfr_asinh Rmpfr_atan Rmpfr_atan2 Rmpfr_atanh 
Rmpfr_can_round Rmpfr_cbrt Rmpfr_ceil Rmpfr_check_range Rmpfr_clear Rmpfr_clears
Rmpfr_clear_erangeflag Rmpfr_clear_flags Rmpfr_clear_inexflag Rmpfr_clear_nanflag
Rmpfr_clear_overflow Rmpfr_clear_underflow Rmpfr_cmp Rmpfr_cmp_d Rmpfr_cmp_f 
Rmpfr_cmp_ld Rmpfr_cmp_q Rmpfr_cmp_si Rmpfr_cmp_si_2exp Rmpfr_cmp_ui 
Rmpfr_cmp_ui_2exp Rmpfr_cmp_z Rmpfr_cmpabs Rmpfr_const_catalan Rmpfr_const_euler 
Rmpfr_const_log2 Rmpfr_const_pi Rmpfr_cos Rmpfr_cosh Rmpfr_cot Rmpfr_coth 
Rmpfr_csc Rmpfr_csch
Rmpfr_deref2 Rmpfr_dim Rmpfr_div Rmpfr_div_2exp Rmpfr_div_2si Rmpfr_div_2ui 
Rmpfr_div_q Rmpfr_div_si Rmpfr_div_ui Rmpfr_div_z Rmpfr_dump Rmpfr_eint 
Rmpfr_eq Rmpfr_equal_p Rmpfr_erangeflag_p Rmpfr_erf Rmpfr_erfc Rmpfr_exp 
Rmpfr_exp10 Rmpfr_exp2 Rmpfr_expm1 Rmpfr_fac_ui
Rmpfr_fits_intmax_p Rmpfr_fits_sint_p Rmpfr_fits_slong_p
Rmpfr_fits_sshort_p Rmpfr_fits_uint_p Rmpfr_fits_uintmax_p
Rmpfr_fits_ulong_p Rmpfr_fits_IV_p Rmpfr_fits_UV_p
Rmpfr_fits_ushort_p Rmpfr_floor Rmpfr_fma Rmpfr_frac Rmpfr_free_cache Rmpfr_gamma 
Rmpfr_get_d Rmpfr_get_IV Rmpfr_get_UV Rmpfr_get_NV
Rmpfr_get_d_2exp Rmpfr_get_d1 Rmpfr_get_default_prec Rmpfr_get_default_rounding_mode 
Rmpfr_get_emax Rmpfr_get_emax_max Rmpfr_get_emax_min Rmpfr_get_emin 
Rmpfr_get_emin_max Rmpfr_get_emin_min Rmpfr_get_exp Rmpfr_get_f Rmpfr_get_ld
Rmpfr_get_ld_2exp 
Rmpfr_get_prec Rmpfr_get_si Rmpfr_get_sj Rmpfr_get_str Rmpfr_get_ui 
Rmpfr_get_uj
Rmpfr_get_version Rmpfr_get_z Rmpfr_get_z_exp
Rmpfr_get_z_2exp Rmpfr_greater_p Rmpfr_greaterequal_p
Rmpfr_hypot 
Rmpfr_inexflag_p Rmpfr_inf_p Rmpfr_init Rmpfr_init2 Rmpfr_init2_nobless
Rmpfr_inits Rmpfr_inits2 Rmpfr_inits_nobless Rmpfr_inits2_nobless
Rmpfr_init_nobless Rmpfr_init_set Rmpfr_init_set_d Rmpfr_init_set_d_nobless 
Rmpfr_init_set_f Rmpfr_init_set_f_nobless  
Rmpfr_init_set_nobless Rmpfr_init_set_q 
Rmpfr_init_set_q_nobless Rmpfr_init_set_si Rmpfr_init_set_si_nobless 
Rmpfr_init_set_str Rmpfr_init_set_str_nobless Rmpfr_init_set_ui 
Rmpfr_init_set_ui_nobless Rmpfr_init_set_z Rmpfr_init_set_z_nobless Rmpfr_inp_str 
TRmpfr_inp_str
Rmpfr_integer_p Rmpfr_integer_string
Rmpfr_less_p Rmpfr_lessequal_p Rmpfr_lessgreater_p Rmpfr_lngamma 
Rmpfr_log Rmpfr_log10 Rmpfr_log1p Rmpfr_log2 Rmpfr_max Rmpfr_min 
Rmpfr_min_prec Rmpfr_mul Rmpfr_mul_2exp Rmpfr_mul_2si Rmpfr_mul_2ui Rmpfr_mul_q 
Rmpfr_mul_si Rmpfr_mul_ui Rmpfr_mul_z Rmpfr_nan_p Rmpfr_nanflag_p Rmpfr_neg 
Rmpfr_nextabove Rmpfr_nextbelow Rmpfr_nexttoward Rmpfr_number_p Rmpfr_out_str 
TRmpfr_out_str
Rmpfr_overflow_p Rmpfr_pow Rmpfr_pow_si Rmpfr_pow_ui Rmpfr_pow_z Rmpfr_prec_round 
Rmpfr_print_binary Rmpfr_random2 Rmpfr_reldiff Rmpfr_rint Rmpfr_rint_ceil 
Rmpfr_rint_floor Rmpfr_rint_round Rmpfr_rint_trunc Rmpfr_root Rmpfr_round 
Rmpfr_sec Rmpfr_sech Rmpfr_set Rmpfr_set_d Rmpfr_set_default_prec 
Rmpfr_set_default_rounding_mode Rmpfr_set_emax Rmpfr_set_emin Rmpfr_set_erangeflag
Rmpfr_set_exp Rmpfr_set_f Rmpfr_set_inexflag Rmpfr_set_inf Rmpfr_set_ld 
Rmpfr_set_nan Rmpfr_set_nanflag Rmpfr_set_overflow Rmpfr_set_prec 
Rmpfr_set_prec_raw Rmpfr_set_q Rmpfr_set_si Rmpfr_set_si_2exp Rmpfr_set_sj 
Rmpfr_set_sj_2exp Rmpfr_set_str Rmpfr_set_str_binary Rmpfr_set_ui Rmpfr_set_ui_2exp
Rmpfr_set_uj Rmpfr_set_uj_2exp 
Rmpfr_set_underflow Rmpfr_set_z Rmpfr_sgn Rmpfr_si_div Rmpfr_si_sub Rmpfr_sin 
Rmpfr_sin_cos Rmpfr_sinh_cosh 
Rmpfr_sinh Rmpfr_sqr Rmpfr_sqrt Rmpfr_sqrt_ui Rmpfr_strtofr Rmpfr_sub 
Rmpfr_sub_q Rmpfr_sub_si Rmpfr_sub_ui Rmpfr_sub_z Rmpfr_subnormalize 
Rmpfr_sum Rmpfr_swap 
Rmpfr_tan Rmpfr_tanh Rmpfr_trunc Rmpfr_ui_div Rmpfr_ui_pow Rmpfr_ui_pow_ui 
Rmpfr_ui_sub Rmpfr_underflow_p Rmpfr_unordered_p Rmpfr_urandomb Rmpfr_zero_p 
Rmpfr_zeta Rmpfr_zeta_ui
Rmpfr_j0 Rmpfr_j1 Rmpfr_jn Rmpfr_y0 Rmpfr_y1 Rmpfr_yn Rmpfr_lgamma
Rmpfr_signbit Rmpfr_setsign Rmpfr_copysign Rmpfr_get_patches
Rmpfr_remainder Rmpfr_remquo Rmpfr_fms Rmpfr_init_set_ld
Rmpfr_add_d Rmpfr_sub_d Rmpfr_d_sub Rmpfr_mul_d Rmpfr_div_d Rmpfr_d_div
Rmpfr_rec_sqrt Rmpfr_li2 Rmpfr_modf Rmpfr_fmod
Rmpfr_printf Rmpfr_fprintf Rmpfr_sprintf Rmpfr_sprintf_ret Rmpfr_snprintf Rmpfr_snprintf_ret
Rmpfr_buildopt_tls_p Rmpfr_buildopt_decimal_p Rmpfr_regular_p Rmpfr_set_zero Rmpfr_digamma
Rmpfr_ai Rmpfr_set_flt Rmpfr_get_flt Rmpfr_urandom Rmpfr_set_z_2exp
Rmpfr_set_divby0 Rmpfr_clear_divby0 Rmpfr_divby0_p
Rmpfr_buildopt_tune_case Rmpfr_frexp Rmpfr_grandom Rmpfr_z_sub Rmpfr_buildopt_gmpinternals_p
);

    $Math::MPFR::VERSION = '3.12';

    DynaLoader::bootstrap Math::MPFR $Math::MPFR::VERSION;

    %Math::MPFR::EXPORT_TAGS =(mpfr => [qw(
GMP_RNDN GMP_RNDZ GMP_RNDU GMP_RNDD
MPFR_RNDN MPFR_RNDZ MPFR_RNDU MPFR_RNDD MPFR_RNDA
MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING RMPFR_VERSION_NUM 
RMPFR_PREC_MIN RMPFR_PREC_MAX
Rgmp_randclear Rgmp_randinit_mt
Rgmp_randinit_default Rgmp_randinit_lc_2exp 
Rgmp_randinit_lc_2exp_size Rgmp_randseed Rgmp_randseed_ui 
Rmpfr_abs Rmpfr_acos Rmpfr_acosh Rmpfr_add Rmpfr_add_q
Rmpfr_add_si Rmpfr_add_ui Rmpfr_add_z 
Rmpfr_agm Rmpfr_asin Rmpfr_asinh Rmpfr_atan Rmpfr_atan2 Rmpfr_atanh 
Rmpfr_can_round Rmpfr_cbrt Rmpfr_ceil Rmpfr_check_range Rmpfr_clear Rmpfr_clears
Rmpfr_clear_erangeflag Rmpfr_clear_flags Rmpfr_clear_inexflag Rmpfr_clear_nanflag
Rmpfr_clear_overflow Rmpfr_clear_underflow Rmpfr_cmp Rmpfr_cmp_d Rmpfr_cmp_f 
Rmpfr_cmp_ld Rmpfr_cmp_q Rmpfr_cmp_si Rmpfr_cmp_si_2exp Rmpfr_cmp_ui 
Rmpfr_cmp_ui_2exp Rmpfr_cmp_z Rmpfr_cmpabs Rmpfr_const_catalan Rmpfr_const_euler 
Rmpfr_const_log2 Rmpfr_const_pi Rmpfr_cos Rmpfr_cosh Rmpfr_cot Rmpfr_coth 
Rmpfr_csc Rmpfr_csch
Rmpfr_deref2 Rmpfr_dim Rmpfr_div Rmpfr_div_2exp Rmpfr_div_2si Rmpfr_div_2ui 
Rmpfr_div_q Rmpfr_div_si Rmpfr_div_ui Rmpfr_div_z Rmpfr_dump Rmpfr_eint 
Rmpfr_eq Rmpfr_equal_p Rmpfr_erangeflag_p Rmpfr_erf Rmpfr_erfc Rmpfr_exp 
Rmpfr_exp10 Rmpfr_exp2 Rmpfr_expm1 Rmpfr_fac_ui
Rmpfr_fits_intmax_p Rmpfr_fits_sint_p Rmpfr_fits_slong_p
Rmpfr_fits_sshort_p Rmpfr_fits_uint_p Rmpfr_fits_uintmax_p
Rmpfr_fits_ulong_p Rmpfr_fits_IV_p Rmpfr_fits_UV_p
Rmpfr_fits_ushort_p Rmpfr_floor Rmpfr_fma Rmpfr_frac Rmpfr_free_cache Rmpfr_gamma 
Rmpfr_get_d Rmpfr_get_IV Rmpfr_get_UV Rmpfr_get_NV
Rmpfr_get_d_2exp Rmpfr_get_d1 Rmpfr_get_default_prec Rmpfr_get_default_rounding_mode 
Rmpfr_get_emax Rmpfr_get_emax_max Rmpfr_get_emax_min Rmpfr_get_emin 
Rmpfr_get_emin_max Rmpfr_get_emin_min Rmpfr_get_exp Rmpfr_get_f Rmpfr_get_ld
Rmpfr_get_ld_2exp 
Rmpfr_get_prec Rmpfr_get_si Rmpfr_get_sj Rmpfr_get_str Rmpfr_get_ui
Rmpfr_get_uj 
Rmpfr_get_version Rmpfr_get_z Rmpfr_get_z_exp
Rmpfr_get_z_2exp Rmpfr_greater_p Rmpfr_greaterequal_p
Rmpfr_hypot 
Rmpfr_inexflag_p Rmpfr_inf_p Rmpfr_init Rmpfr_init2 Rmpfr_init2_nobless 
Rmpfr_inits Rmpfr_inits2 Rmpfr_inits_nobless Rmpfr_inits2_nobless
Rmpfr_init_nobless Rmpfr_init_set Rmpfr_init_set_d Rmpfr_init_set_d_nobless 
Rmpfr_init_set_f Rmpfr_init_set_f_nobless  
Rmpfr_init_set_nobless Rmpfr_init_set_q 
Rmpfr_init_set_q_nobless Rmpfr_init_set_si Rmpfr_init_set_si_nobless 
Rmpfr_init_set_str Rmpfr_init_set_str_nobless Rmpfr_init_set_ui 
Rmpfr_init_set_ui_nobless Rmpfr_init_set_z Rmpfr_init_set_z_nobless Rmpfr_inp_str 
TRmpfr_inp_str
Rmpfr_integer_p Rmpfr_integer_string
Rmpfr_less_p Rmpfr_lessequal_p Rmpfr_lessgreater_p Rmpfr_lngamma 
Rmpfr_log Rmpfr_log10 Rmpfr_log1p Rmpfr_log2 Rmpfr_max Rmpfr_min 
Rmpfr_min_prec Rmpfr_mul Rmpfr_mul_2exp Rmpfr_mul_2si Rmpfr_mul_2ui Rmpfr_mul_q 
Rmpfr_mul_si Rmpfr_mul_ui Rmpfr_mul_z Rmpfr_nan_p Rmpfr_nanflag_p Rmpfr_neg 
Rmpfr_nextabove Rmpfr_nextbelow Rmpfr_nexttoward Rmpfr_number_p Rmpfr_out_str 
TRmpfr_out_str
Rmpfr_overflow_p Rmpfr_pow Rmpfr_pow_si Rmpfr_pow_ui Rmpfr_pow_z Rmpfr_prec_round 
Rmpfr_print_binary Rmpfr_random2 Rmpfr_reldiff Rmpfr_rint Rmpfr_rint_ceil 
Rmpfr_rint_floor Rmpfr_rint_round Rmpfr_rint_trunc Rmpfr_root Rmpfr_round 
Rmpfr_sec Rmpfr_sech Rmpfr_set Rmpfr_set_d Rmpfr_set_default_prec 
Rmpfr_set_default_rounding_mode Rmpfr_set_emax Rmpfr_set_emin Rmpfr_set_erangeflag
Rmpfr_set_exp Rmpfr_set_f Rmpfr_set_inexflag Rmpfr_set_inf Rmpfr_set_ld 
Rmpfr_set_nan Rmpfr_set_nanflag Rmpfr_set_overflow Rmpfr_set_prec 
Rmpfr_set_prec_raw Rmpfr_set_q Rmpfr_set_si Rmpfr_set_si_2exp Rmpfr_set_sj 
Rmpfr_set_sj_2exp Rmpfr_set_str Rmpfr_set_str_binary Rmpfr_set_ui Rmpfr_set_ui_2exp
Rmpfr_set_uj Rmpfr_set_uj_2exp
Rmpfr_set_underflow Rmpfr_set_z Rmpfr_sgn Rmpfr_si_div Rmpfr_si_sub Rmpfr_sin 
Rmpfr_sin_cos Rmpfr_sinh_cosh
Rmpfr_sinh Rmpfr_sqr Rmpfr_sqrt Rmpfr_sqrt_ui Rmpfr_strtofr Rmpfr_sub 
Rmpfr_sub_q Rmpfr_sub_si Rmpfr_sub_ui Rmpfr_sub_z Rmpfr_subnormalize
Rmpfr_sum Rmpfr_swap 
Rmpfr_tan Rmpfr_tanh Rmpfr_trunc Rmpfr_ui_div Rmpfr_ui_pow Rmpfr_ui_pow_ui 
Rmpfr_ui_sub Rmpfr_underflow_p Rmpfr_unordered_p Rmpfr_urandomb Rmpfr_zero_p 
Rmpfr_zeta Rmpfr_zeta_ui
Rmpfr_j0 Rmpfr_j1 Rmpfr_jn Rmpfr_y0 Rmpfr_y1 Rmpfr_yn Rmpfr_lgamma
Rmpfr_signbit Rmpfr_setsign Rmpfr_copysign Rmpfr_get_patches
Rmpfr_remainder Rmpfr_remquo Rmpfr_fms Rmpfr_init_set_ld
Rmpfr_add_d Rmpfr_sub_d Rmpfr_d_sub Rmpfr_mul_d Rmpfr_div_d Rmpfr_d_div
Rmpfr_rec_sqrt Rmpfr_li2 Rmpfr_modf Rmpfr_fmod
Rmpfr_printf Rmpfr_fprintf Rmpfr_sprintf Rmpfr_sprintf_ret Rmpfr_snprintf Rmpfr_snprintf_ret
Rmpfr_buildopt_tls_p Rmpfr_buildopt_decimal_p Rmpfr_regular_p Rmpfr_set_zero Rmpfr_digamma
Rmpfr_ai Rmpfr_set_flt Rmpfr_get_flt Rmpfr_urandom Rmpfr_set_z_2exp
Rmpfr_set_divby0 Rmpfr_clear_divby0 Rmpfr_divby0_p
Rmpfr_buildopt_tune_case Rmpfr_frexp Rmpfr_grandom Rmpfr_z_sub Rmpfr_buildopt_gmpinternals_p
)]);

sub dl_load_flags {0} # Prevent DynaLoader from complaining and croaking

sub Rmpfr_out_str {
    if(@_ == 4) {
       die "Inappropriate 1st arg supplied to Rmpfr_out_str" if _itsa($_[0]) != _MATH_MPFR_T;
       return _Rmpfr_out_str($_[0], $_[1], $_[2], $_[3]);
    }
    if(@_ == 5) {
      if(_itsa($_[0]) == _MATH_MPFR_T) {return _Rmpfr_out_strS($_[0], $_[1], $_[2], $_[3], $_[4])}
      die "Incorrect args supplied to Rmpfr_out_str" if _itsa($_[1]) != _MATH_MPFR_T;
      return _Rmpfr_out_strP($_[0], $_[1], $_[2], $_[3], $_[4]);
    }
    if(@_ == 6) {
      die "Inappropriate 2nd arg supplied to Rmpfr_out_str" if _itsa($_[1]) != _MATH_MPFR_T;
      return _Rmpfr_out_strPS($_[0], $_[1], $_[2], $_[3], $_[4], $_[5]);
    }
    die "Wrong number of arguments supplied to Rmpfr_out_str()";
}

sub TRmpfr_out_str {
    if(@_ == 5) {
      die "Inappropriate 4th arg supplied to TRmpfr_out_str"
         if _itsa($_[3]) != _MATH_MPFR_T;
      return _TRmpfr_out_str($_[0], $_[1], $_[2], $_[3], $_[4]);
    }
    if(@_ == 6) {
      if(_itsa($_[3]) == _MATH_MPFR_T) {return _TRmpfr_out_strS($_[0], $_[1], $_[2], $_[3], $_[4], $_[5])}
      die "Incorrect args supplied to TRmpfr_out_str"
         if _itsa($_[4]) != _MATH_MPFR_T;
      return _TRmpfr_out_strP($_[0], $_[1], $_[2], $_[3], $_[4], $_[5]);
    }
    if(@_ == 7) {
      die "Inappropriate 5th arg supplied to TRmpfr_out_str"
         if _itsa($_[4]) != _MATH_MPFR_T;
      return _TRmpfr_out_strPS($_[0], $_[1], $_[2], $_[3], $_[4], $_[5], $_[6]);
    }
    die "Wrong number of arguments supplied to TRmpfr_out_str()";
}

sub Rmpfr_get_str {
    my ($mantissa, $exponent) = Rmpfr_deref2($_[0], $_[1], $_[2], $_[3]);

    if($mantissa =~ /\@Inf\@/i || $mantissa =~ /\@NaN\@/i) {return $mantissa}
    if($mantissa =~ /\-/ && $mantissa !~ /[^0,\-]/) {return '-0'}
    if($mantissa !~ /[^0]/ ) {return '0'}

    my $len = substr($mantissa, 0, 1) eq '-' ? 2 : 1;

    if(!$_[2]) {
      while(length($mantissa) > $len && substr($mantissa, -1, 1) eq '0') {
           substr($mantissa, -1, 1, '');
      }
    }

    $exponent--;

    my $sep = $_[1] <= 10 ? 'e' : '@';

    if(length($mantissa) == $len) {
      if($exponent) {return $mantissa . $sep . $exponent}
      return $mantissa;
    }

    substr($mantissa, $len, 0, '.');
    if($exponent) {return $mantissa . $sep . $exponent}
    return $mantissa;
}

sub overload_string {
    return Rmpfr_get_str($_[0], 10, 0, Rmpfr_get_default_rounding_mode());
}

sub Rmpfr_integer_string {
    if($_[1] < 2 || $_[1] > 36) {die("Second argument supplied to Rmpfr_integer_string() is not in acceptable range")}
    my($mantissa, $exponent) = Rmpfr_deref2($_[0], $_[1], 0, $_[2]);
    if($mantissa =~ /\@Inf\@/i || $mantissa =~ /\@NaN\@/i) {return $mantissa}
    if($mantissa =~ /\-/ && $mantissa !~ /[^0,\-]/) {return '-0'}
    return 0 if $exponent < 1;
    my $sign = substr($mantissa, 0, 1) eq '-' ? 1 : 0;
    $mantissa = substr($mantissa, 0, $exponent + $sign);
    return $mantissa; 
}


sub new {

    # This function caters for 2 possibilities:
    # 1) that 'new' has been called OOP style - in which 
    #    case there will be a maximum of 3 args
    # 2) that 'new' has been called as a function - in
    #    which case there will be a maximum of 2 args.
    # If there are no args, then we just want to return an
    # initialized Math::MPFR object
    if(!@_) {return Rmpfr_init()}
   
    if(@_ > 3) {die "Too many arguments supplied to new()"}

    # If 'new' has been called OOP style, the first arg is the string
    # "Math::MPFR" which we don't need - so let's remove it. However,
    # if the first arg is a Math::MPFR object (which is a possibility),
    # then we'll get a fatal error when we check it for equivalence to
    # the string "Math::MPFR". So we first need to check that it's not
    # an object - which we'll do by using the ref() function:
    if(!ref($_[0]) && $_[0] eq "Math::MPFR") {
      shift;
      if(!@_) {return Rmpfr_init()}
      } 

    # @_ can now contain a maximum of 2 args - the value, and iff the value is
    # a string, (optionally) the base of the numeric string.
    if(@_ > 2) {die "Too many arguments supplied to new() - expected no more than two"}

    my ($arg1, $type, $base);

    # $_[0] is the value, $_[1] (if supplied) is the base of the number
    # in the string $[_0].
    $arg1 = shift;
    $base = 0;

    $type = _itsa($arg1);
    if(!$type) {die "Inappropriate argument supplied to new()"}

    my @ret;

    # Create a Math::MPFR object that has $arg1 as its value.
    # Die if there are any additional args (unless $type == 4)
    if($type == _UOK_T) {
      if(@_ ) {die "Too many arguments supplied to new() - expected only one"}
      if(Math::MPFR::_has_longlong()) {
        my $ret = Rmpfr_init();
	Rmpfr_set_uj($ret, $arg1, Rmpfr_get_default_rounding_mode());
	return $ret;
      }
      else {
        @ret = Rmpfr_init_set_ui($arg1, Rmpfr_get_default_rounding_mode());
        return $ret[0];
      }
    }

    if($type == _IOK_T) {
      if(@_ ) {die "Too many arguments supplied to new() - expected only one"}
      if(Math::MPFR::_has_longlong()) {
        my $ret = Rmpfr_init();
	Rmpfr_set_sj($ret, $arg1, Rmpfr_get_default_rounding_mode());
	return $ret;
      }
      else {
        @ret = Rmpfr_init_set_si($arg1, Rmpfr_get_default_rounding_mode());
        return $ret[0];
      }
    }

    if($type == _NOK_T) {
      if(@_ ) {die "Too many arguments supplied to new() - expected only one"}
      if(Math::MPFR::_has_longdouble()) {
         @ret = Rmpfr_init_set_ld($arg1, Rmpfr_get_default_rounding_mode());
         return $ret[0];
      }
      else {
         @ret = Rmpfr_init_set_d($arg1, Rmpfr_get_default_rounding_mode());
         return $ret[0];
      }

    }

    if($type == _POK_T) {
      if(@_ > 1) {die "Too many arguments supplied to new() - expected no more than two"}
      $base = shift if @_;
      if($base < 0 || $base == 1 || $base > 36) {die "Invalid value for base"}
      @ret = Rmpfr_init_set_str($arg1, $base, Rmpfr_get_default_rounding_mode());
      if($ret[1]) {warn "string supplied to new() contained invalid characters"}
      return $ret[0];
    }

    if($type == _MATH_MPFR_T) {
      if(@_) {die "Too many arguments supplied to new() - expected only one"}
      @ret = Rmpfr_init_set($arg1, Rmpfr_get_default_rounding_mode());
      return $ret[0];
    }

    if($type == _MATH_GMPf_T) {
      if(@_) {die "Too many arguments supplied to new() - expected only one"}
      @ret = Rmpfr_init_set_f($arg1, Rmpfr_get_default_rounding_mode());
      return $ret[0];
    }

    if($type == _MATH_GMPq_T) {
      if(@_) {die "Too many arguments supplied to new() - expected only one"}
      @ret = Rmpfr_init_set_q($arg1, Rmpfr_get_default_rounding_mode());
      return $ret[0];
    }

    if($type == _MATH_GMPz_T || $type == _MATH_GMP_T) {
      if(@_) {die "Too many arguments supplied to new() - expected only one"}
      @ret = Rmpfr_init_set_z($arg1, Rmpfr_get_default_rounding_mode());
      return $ret[0];
    }  
}

sub Rmpfr_printf {
    if(@_ == 3){wrap_mpfr_printf_rnd(@_)}
    else {die "Rmpfr_printf must take 2 or 3 arguments: format string, [rounding,], and variable" if @_ != 2;
    wrap_mpfr_printf(@_)}
}

sub Rmpfr_fprintf {
    if(@_ == 4){wrap_mpfr_fprintf_rnd(@_)}
    else {die "Rmpfr_fprintf must take 3 or 4 arguments: filehandle, format string, [rounding,], and variable" if @_ != 3;
    wrap_mpfr_fprintf(@_)}
}

sub Rmpfr_sprintf {
    my $len;
    if(@_ == 4){$len = wrap_mpfr_sprintf_rnd(@_)}
    else {die "Rmpfr_sprintf must take 3 or 4 arguments: buffer, format string, [rounding,], and variable" if @_ != 3;
    $len = wrap_mpfr_sprintf(@_)}
    $_[0] = substr($_[0], 0, $len);
    return $len;
}

sub Rmpfr_sprintf_ret {
    my $len;
    if(@_ == 4){$len = wrap_mpfr_sprintf_rnd(@_)}
    else {die "Rmpfr_sprintf must take 3 or 4 arguments: buffer, format string, [rounding,], and variable" if @_ != 3;
    $len = wrap_mpfr_sprintf(@_)}
    return substr($_[0], 0, $len);
}

sub Rmpfr_snprintf {
    my $len;
    if(@_ == 5){$len = wrap_mpfr_snprintf_rnd(@_)}
    else {die "Rmpfr_snprintf must take 4 or 5 arguments: buffer, bytes written, format string, [rounding,], and variable" if @_ != 4;
    $len = wrap_mpfr_snprintf(@_)}
    $_[0] = substr($_[0], 0, $_[1] - 1);
    return $len;
}

sub Rmpfr_snprintf_ret {
    my $len;
    if(@_ == 5){$len = wrap_mpfr_snprintf_rnd(@_)}
    else {die "Rmpfr_snprintf must take 4 or 5 arguments: buffer, bytes written, format string, [rounding,], and variable" if @_ != 4;
    $len = wrap_mpfr_snprintf(@_)}
    return substr($_[0], 0, $_[1] - 1);
}

sub Rmpfr_inits {
    my @ret;
    for(1 .. $_[0]) {
       $ret[$_ - 1] = Rmpfr_init();
    }
    return @ret;
}

sub Rmpfr_inits2 {
    my @ret;
    for(1 .. $_[1]) {
       $ret[$_ - 1] = Rmpfr_init2($_[0]);
    }
    return @ret;
}

sub Rmpfr_inits_nobless {
    my @ret;
    for(1 .. $_[0]) {
       $ret[$_ - 1] = Rmpfr_init_nobless();
    }
    return @ret;
}

sub Rmpfr_inits2_nobless {
    my @ret;
    for(1 .. $_[1]) {
       $ret[$_ - 1] = Rmpfr_init2_nobless($_[0]);
    }
    return @ret;
}

sub MPFR_VERSION {return _MPFR_VERSION()}
sub MPFR_VERSION_MAJOR {return _MPFR_VERSION_MAJOR()}
sub MPFR_VERSION_MINOR {return _MPFR_VERSION_MINOR()}
sub MPFR_VERSION_PATCHLEVEL {return _MPFR_VERSION_PATCHLEVEL()}
sub MPFR_VERSION_STRING {return _MPFR_VERSION_STRING()}

*Rmpfr_get_z_exp = \&Rmpfr_get_z_2exp;

1;

__END__

=head1 NAME

Math::MPFR - perl interface to the MPFR (floating point) library.

=head1 DEPENDENCIES

   This module needs the MPFR and GMP C libraries. (Install GMP
   first as it is a pre-requisite for MPFR.)

   The GMP library is availble from http://gmplib.org
   The MPFR library is available from http://www.mpfr.org/

=head1 DESCRIPTION

   A bigfloat module utilising the MPFR library. Basically
   this module simply wraps the 'mpfr' floating point functions
   provided by that library.
   Operator overloading is also available.
   The following documentation heavily plagiarises the mpfr
   documentation.
   See also the Math::MPFR test suite for some examples of
   usage.

=head1 SYNOPSIS

   use Math::MPFR qw(:mpfr);
   
   # '@' can be used to separate mantissa from exponent. For bases
   # that are <= 10, 'e' or 'E' can also be used.
   # Use single quotes for string assignment if you're using '@' as
   # the separator. If you must use double quotes, you'll have to 
   # escape the '@'.

   my $str = '.123542@2'; # mantissa = (.)123452
                         # exponent = 2
   #Alternatively:
   # my $str = ".123542\@2";
   # or:
   # my $str = '12.3542';
   # or:
   # my $str = '1.23542e1';
   # or:
   # my $str = '1.23542E1';

   my $base = 10;
   my $rnd = MPFR_RNDZ; # See 'ROUNDING MODE'

   # Create an Math::MPFR object that holds an initial
   # value of $str (in base $base) and has the default
   # precision. $bn1 is the number. $nok will either be 0 
   # indicating that the string was a valid number string, or
   # -1, indicating that the string contained at least one
   # invalid numeric character. 
   # See 'COMBINED INITIALISATION AND ASSIGNMENT', below.
   my ($bn1, $nok) = Rmpfr_init_set_str($str, $base, $rnd);

   # Or use the new() constructor - also documented below
   # in 'COMBINED INITIALISATION AND ASSIGNMENT'.
   # my $bn1 = Math::MPFR->new($str);

   # Create another Math::MPFR object with precision
   # of 100 bits and an initial value of NaN.
   my $bn2 = Rmpfr_init2(100);

   # Assign the value -2314.451 to $bn1.
   Rmpfr_set_d($bn2, -2314.451, MPFR_RNDN);

   # Create another Math::MPFR object that holds
   # an initial value of NaN and has the default precision.
   my $bn3 = Rmpfr_init();

   # Or using instead the new() constructor:
   # my $bn3 = Math::MPFR->new();

   # Perform some operations ... see 'FUNCTIONS' below.
   # see 'OPERATOR OVERLOADING' below for docs re
   # operator overloading

   .
   .

   # print out the value held by $bn1 (in octal):
   print Rmpfr_get_str($bn1, 8, 0, $rnd), "\n"; 

   # print out the value held by $bn1 (in decimal):
   print Rmpfr_get_str($bn1, 10, 0, $rnd), "\n";
   # or just make use of overloading :
   print $bn1, "\n"; # is base 10, and uses 'e' rather than '@'.

   # print out the value held by $bn1 (in base 16) using the
   # 'TRmpfr_out_str' function. (No newline is printed - unless
   # it's supplied as the optional fifth arg. See the
   # 'TRmpfr_out_str' documentation below.)
   TRmpfr_out_str(*stdout, 16, 0, $bn1, $rnd);

=head1 ROUNDING MODE


   One of 4 values:
    GMP_RNDN (numeric value = 0): Round to nearest.
    GMP_RNDZ (numeric value = 1): Round towards zero.
    GMP_RNDU (numeric value = 2): Round towards +infinity.
    GMP_RNDD (numeric value = 3): Round towards -infinity.

   With the release of mpfr-3.0.0, the same rounding values
   are renamed to:
    MPFR_RNDN (numeric value = 0): Round to nearest.
    MPFR_RNDZ (numeric value = 1): Round towards zero.
    MPFR_RNDU (numeric value = 2): Round towards +infinity.
    MPFR_RNDD (numeric value = 3): Round towards -infinity.

   You can use either rendition with Math-MPFR-3.0 or later.

   The mpfr-3.0.0 library also provides:
    MPFR_RNDA (numeric value = 4): Round away from zero.

   It, too, can be used with Math-MPFR-3.0 or later, but 
   will cause a fatal error iff the mpfr library against
   which Math::MPFR is built is earlier than version 3.0.0.

    The `round to nearest' mode works as in the IEEE
    P754 standard: in case the number to be rounded
    lies exactly in the middle of two representable 
    numbers, it is rounded to the one with the least
    significant bit set to zero.  For example, the 
    number 5, which is represented by (101) in binary,
    is rounded to (100)=4 with a precision of two bits,
    and not to (110)=6.  This rule avoids the "drift"
    phenomenon mentioned by Knuth in volume 2 of 
    The Art of Computer Programming (section 4.2.2,
    pages 221-222).

    Most Math::MPFR functions take as first argument the
    destination variable, as second and following arguments 
    the input variables, as last argument a rounding mode,
    and have a return value of type `int'. If this value
    is zero, it means that the value stored in the 
    destination variable is the exact result of the 
    corresponding mathematical function. If the
    returned value is positive (resp. negative), it means
    the value stored in the destination variable is greater
    (resp. lower) than the exact result.  For example with 
    the `GMP_RNDU' rounding mode, the returned value is 
    usually positive, except when the result is exact, in
    which case it is zero.  In the case of an infinite
    result, it is considered as inexact when it was
    obtained by overflow, and exact otherwise.  A
    NaN result (Not-a-Number) always corresponds to an
    inexact return value.

=head1 MEMORY MANAGEMENT

   Objects are created with new() or with the Rmpfr_init*
   functions. All of these functions return an object that has
   been blessed into the package Math::MPFR.
   They will therefore be automatically cleaned up by the
   DESTROY() function whenever they go out of scope.

   For each Rmpfr_init* function there is a corresponding function
   called Rmpfr_init*_nobless which returns an unblessed object.
   If you create Math::MPFR objects using the '_nobless'
   versions, it will then be up to you to clean up the memory
   associated with these objects by calling Rmpfr_clear($op) 
   for each object, or Rmpfr_clears($op1, $op2, ....).
   Alternatively such objects will be cleaned up when the script
   ends. I don't know why you would want to create unblessed
   objects. The point is that you can if you want to.
   The test suite does no testing of unblessed objects ... beware
   of bugs if you go down that path.

=head1 MIXING GMP OBJECTS WITH MPFR OBJECTS

   Some of the Math::MPFR functions below take as arguments
   one or more of the GMP types mpz (integer), mpq
   (rational) and mpf (floating point). (Such functions are
   marked as taking mpz/mpq/mpf arguments.)
   For these functions to work you need to have loaded either:

   1) Math::GMP from CPAN. (This module provides access to mpz
      objects only - NOT mpf and mpq objects.)

   AND/OR

   2) Math::GMPz (for mpz types), Math::GMPq (for mpq types)
      and Math::GMPf (for mpf types). 

   You may also be able to use objects from the GMP module
   that ships with the GMP sources. I get occasional 
   segfaults when I try to do that, so I've stopped
   recommending it - and don't support the practice.     

=head1 FUNCTIONS

   These next 3 functions are demonstrated above:
   $rop = Rmpfr_init();
   $rop = Rmpfr_init2($p);
   $str = Rmpfr_get_str($op, $base, $digits, $rnd); # 1 < $base < 37 
   The third argument to Rmpfr_get_str() specifies the number of digits
   required to be output in the mantissa. (Trailing zeroes are removed.) 
   If $digits is 0, the number of digits of the mantissa is chosen
   large enough so that re-reading the printed value with the same
   precision, assuming both output and input use rounding to nearest,
   will recover the original value of $op.

   The following functions are simply wrappers around an mpfr
   function of the same name. eg. Rmpfr_swap() is a wrapper around
   mpfr_swap().

   "$rop", "$op1", "$op2", etc. are Math::MPFR objects - the
   return value of one of the Rmpfr_init* functions. They are in fact 
   references to mpfr structures. The "$op" variables are the operands
   and "$rop" is the variable that stores the result of the operation.
   Generally, $rop, $op1, $op2, etc. can be the same perl variable 
   referencing the same mpfr structure, though often they will be 
   distinct perl variables referencing distinct mpfr structures.
   Eg something like Rmpfr_add($r1, $r1, $r1, $rnd),
   where $r1 *is* the same reference to the same mpfr structure,
   would add $r1 to itself and store the result in $r1. Alternatively,
   you could (courtesy of operator overloading) simply code it
   as $r1 += $r1. Otoh, Rmpfr_add($r1, $r2, $r3, $rnd), where each of the
   arguments is a different reference to a different mpfr structure
   would add $r2 to $r3 and store the result in $r1. Alternatively
   it could be coded as $r1 = $r2 + $r3.

   "$ui" means any integer that will fit into a C 'unsigned long int',

   "$si" means any integer that will fit into a C 'signed long int'.

   "$sj" means any integer that will fit into a C 'intmax_t'. Don't
   use any of these functions unless your perl was compiled with 64
   bit support.

   "$double" is a C double and "$float" is a C float ... but both will
   be represented in Perl as an NV.

   "$bool" means a value (usually a 'signed long int') in which
   the only interest is whether it evaluates as false or true.

   "$str" simply means a string of symbols that represent a number,
   eg '1234567890987654321234567@7' which might be a base 10 number,
   or 'zsa34760sdfgq123r5@11' which would have to represent at least
   a base 36 number (because "z" is a valid digit only in bases 36
   and above). Valid bases for MPFR numbers are 0 and 2 to 36 (2 to 62
   if Math::MPFR has been built against mpfr-3.0.0 or later).

   "$rnd" is simply one of the 4 rounding mode values (discussed above).

   "$p" is the (signed int) value for precision.

   ##############

   ROUNDING MODES

   Rmpfr_set_default_rounding_mode($rnd);
    Sets the default rounding mode to $rnd.
    The default rounding mode is to nearest initially (GMP_RNDN).
    The default rounding mode is the rounding mode that
    is used in overloaded operations.

   $si = Rmpfr_get_default_rounding_mode();
    Returns the numeric value (0, 1, 2 or 3) of the
    current default rounding mode. This will initially be 0.

   $si = Rmpfr_prec_round($rop, $p, $rnd); 
    Rounds $rop according to $rnd with precision $p, which may be
    different from that of $rop.  If $p is greater or equal to the
    precision of $rop, then new space is allocated for the mantissa,
    and it is filled with zeroes.  Otherwise, the mantissa is rounded
    to precision $p with the given direction. In both cases, the
    precision of $rop is changed to $p.  The returned value is zero
    when the result is exact, positive when it is greater than the
    original value of $rop, and negative when it is smaller.  The
    precision $p can be any integer between RMPFR_PREC_MIN and
    RMPFR_PREC_MAX.  

   ##########

   EXCEPTIONS

   $si =  Rmpfr_get_emin();
   $si =  Rmpfr_get_emax();
    Return the (current) smallest and largest exponents
    allowed for a floating-point variable.

   $si = Rmpfr_get_emin_min();
   $si = Rmpfr_get_emin_max();
   $si = Rmpfr_get_emax_min();
   $si = Rmpfr_get_emax_max();
    Return the minimum and maximum of the smallest and largest
    exponents allowed for `mpfr_set_emin' and `mpfr_set_emax'. These
    values are implementation dependent

   $bool =  Rmpfr_set_emin($si);
   $bool =  Rmpfr_set_emax($si);
    Set the smallest and largest exponents allowed for a
    floating-point variable.  Return a non-zero value when $si is not
    in the range of exponents accepted by the implementation (in that
    case the smallest or largest exponent is not changed), and zero
    otherwise. If the user changes the exponent range, it is her/his
    responsibility to check that all current floating-point variables
    are in the new allowed range (for example using `Rmpfr_check_range',
    otherwise the subsequent behaviour will be undefined, in the sense
    of the ISO C standard. 

   $si2 = Rmpfr_check_range($op, $si1, $rnd);
    This function has changed from earlier implementations.
    It now forces $op to be in the current range of acceptable
    values, $si1 the current ternary value: negative if $op is
    smaller than the exact value, positive if $op is larger than the
    exact value and zero if $op is exact (before the call). It generates
    an underflow or an overflow if the exponent of $op is outside the
    current allowed range; the value of $si1 may be used to avoid a
    double rounding. This function returns zero if the rounded result
    is equal to the exact one, a positive value if the rounded result
    is larger than the exact one, a negative value if the rounded
    result is smaller than the exact one. Note that unlike most
    functions, the result is compared to the exact one, not the input
    value $op, i.e. the ternary value is propagated.
    Note: If $op is an infinity and $si1 is different from zero
    (i.e., if the rounded result is an inexact infinity), then the
    overflow flag is set.

   Rmpfr_set_underflow();
   Rmpfr_set_overflow();
   Rmpfr_set_nanflag();
   Rmpfr_set_inexflag();
   Rmpfr_set_erangeflag();
   Rmpfr_set_divby0();     # mpfr-3.1.0 and later only
   Rmpfr_clear_underflow();
   Rmpfr_clear_overflow();
   Rmpfr_clear_nanflag();
   Rmpfr_clear_inexflag();
   Rmpfr_clear_erangeflag();
   Rmpfr_clear_divby0();   # mpfr-3.1.0 and later only
    Set/clear the underflow, overflow, invalid, inexact, erange and
    divide-by-zero flags.

   Rmpfr_clear_flags();
    Clear all global flags (underflow, overflow, inexact, invalid,
    erange and divide-by-zero).

   $bool = Rmpfr_underflow_p();
   $bool = Rmpfr_overflow_p();
   $bool = Rmpfr_nanflag_p();
   $bool = Rmpfr_inexflag_p();
   $bool = Rmpfr_erangeflag_p();
   $bool = Rmpfr_divby0_p();   # mpfr-3.1.0 and later only
    Return the corresponding (underflow, overflow, invalid, inexact,
    erange, divide-by-zero) flag, which is non-zero iff the flag is set.

   $si = Rmpfr_subnormalize ($op, $si, $rnd);
    See the MPFR documentation for mpfr_subnormalize().

   ##############

   INITIALIZATION

   A variable should be initialized once only.

   First read the section 'MEMORY MANAGEMENT' (above).

   Rmpfr_set_default_prec($p);
    Set the default precision to be *exactly* $p bits.  The
    precision of a variable means the number of bits used to store its
    mantissa.  All subsequent calls to `mpfr_init' will use this
    precision, but previously initialized variables are unaffected.
    This default precision is set to 53 bits initially.  The precision
    can be any integer between RMPFR_PREC_MIN and RMPFR_PREC_MAX.

   $ui = Rmpfr_get_default_prec();
    Returns the default MPFR precision in bits.

   $rop = Math::MPFR->new();
   $rop = Math::MPFR::new();
   $rop = new Math::MPFR();
   $rop = Rmpfr_init();
   $rop = Rmpfr_init_nobless();
    Initialize $rop, and set its value to NaN. The precision 
    of $rop is the default precision, which can be changed
    by a call to `Rmpfr_set_default_prec'.

   $rop = Rmpfr_init2($p);
   $rop = Rmpfr_init2_nobless($p);
    Initialize $rop, set its precision to be *exactly* $p bits,
    and set its value to NaN.  To change the precision of a
    variable which has already been initialized,
    use `Rmpfr_set_prec' instead.  The precision $p can be
    any integer between RMPFR_PREC_MIN and RMPFR_PREC_MAX.

   @rops = Rmpfr_inits($how_many);
   @rops = Rmpfr_inits_nobless($how_many);
    Returns an array of $how_many Math::MPFR objects - initialized,
    with a value of NaN, and with default precision.
    (These functions do not wrap mpfr_inits.)

   @rops = Rmpfr_inits2($p, $how_many);
   @rops = Rmpfr_inits2_nobless($p, $how_many);
    Returns an array of $how_many Math::MPFR objects - initialized,
    with a value of NaN, and with precision of $p.
    (These functions do not wrap mpfr_inits2.)
    

   Rmpfr_set_prec($op, $p);
    Reset the precision of $op to be *exactly* $p bits.
    The previous value stored in $op is lost.  The precision
    $p can be any integer between RMPFR_PREC_MIN and
    RMPFR_PREC_MAX. If you want to keep the previous
    value stored in $op, use 'Rmpfr_prec_round' instead.

   $si = Rmpfr_get_prec($op);
    Return the precision actually used for assignments of $op,
   i.e. the number of bits used to store its mantissa.

   Rmpfr_set_prec_raw($rop, $p);
    Reset the precision of $rop to be *exactly* $p bits.  The only
    difference with `mpfr_set_prec' is that $p is assumed to be small
    enough so that the mantissa fits into the current allocated
    memory space for $rop. Otherwise an error will occur.

   $min_prec = Rmpfr_min_prec($op);
    (This function is implemented only when Math::MPFR is built
    against mpfr-3.0.0 or later. The mpfr_min_prec function was
    not present in earlier versions of mpfr.)
    $min_prec is set to the minimal number of bits required to store
    the significand of $op, and 0 for special values, including 0.
   (Warning: the returned value can be less than RMPFR_PREC_MIN.)

   $minimum_precision = RMPFR_PREC_MIN;
   $maximum_precision = RMPFR_PREC_MAX;
    Returns the minimum/maximum precision for Math::MPFR objects
    allowed by the mpfr library being used.

   ##########

   ASSIGNMENT

   $si = Rmpfr_set($rop, $op, $rnd);
   $si = Rmpfr_set_ui($rop, $ui, $rnd);
   $si = Rmpfr_set_si($rop, $si, $rnd);
   $si = Rmpfr_set_sj($rop, $sj, $rnd); # 64 bit
   $si = Rmpfr_set_uj($rop, $uj, $rnd); # 64 bit
   $si = Rmpfr_set_d($rop, $double, $rnd);
   $si = Rmpfr_set_ld($rop, $ld, $rnd); # long double
   $si = Rmpfr_set_z($rop, $z, $rnd); # $z is a mpz object.
   $si = Rmpfr_set_q($rop, $q, $rnd); # $q is a mpq object.
   $si = Rmpfr_set_f($rop, $f, $rnd); # $f is a mpf object.
   $si = Rmpfr_set_flt($rop, $float, $rnd); # mpfr-3.0.0 and later only
    Set the value of $rop from 2nd arg, rounded to the precision of
    $rop towards the given direction $rnd.  Please note that even a 
    'long int' may have to be rounded if the destination precision
    is less than the machine word width.  The return value is zero
    when $rop=2nd arg, positive when $rop>2nd arg, and negative when 
    $rop<2nd arg.  For `mpfr_set_d', be careful that the input
    number $double may not be exactly representable as a double-precision
    number (this happens for 0.1 for instance), in which case it is
    first rounded by the C compiler to a double-precision number,
    and then only to a mpfr floating-point number.

   $si = Rmpfr_set_ui_2exp($rop, $ui, $exp, $rnd);
   $si = Rmpfr_set_si_2exp($rop, $si, $exp, $rnd);
   $si = Rmpfr_set_uj_2exp($rop, $sj, $exp, $rnd); # 64 bit
   $si = Rmpfr_set_sj_2exp($rop, $sj, $exp, $rnd); # 64 bit
   $si = Rmpfr_set_z_2exp($rop, $z, $exp, $rnd); # mpfr-3.0.0 and later only
    Set the value of $rop from the 2nd arg multiplied by two to the
    power $exp, rounded towards the given direction $rnd.  Note that
    the input 0 is converted to +0. ($z is a GMP mpz object.)

   $si = Rmpfr_set_str($rop, $str, $base, $rnd);
    Set $rop to the value of $str in base $base (0,2..36 or, if
    Math::MPFR has been built against mpfr-3.0.0 or later, 0,2..62),
    rounded in direction $rnd to the precision of $rop. 
    The exponent is read in decimal.  This function returns 0 if
    the entire string is a valid number in base $base. otherwise
    it returns -1. If $base is zero, the base is set according to 
    the following rules:
     if the string starts with '0b' or '0B' the base is set to 2;
     if the string starts with '0x' or '0X' the base is set to 16;
     otherwise the base is set to 10.
    The following exponent symbols can be used:
     '@' - can be used for any base;
     'e' or 'E' - can be used only with bases <= 10;
     'p' or 'P' - can be used to introduce binary exponents with
                  hexadecimal or binary strings.
    See the MPFR library documentation for more details. See also
    'Rmpfr_inp_str' (below). 
    Because of the special significance of the '@' symbol in perl,
    make sure you assign to strings using single quotes, not
    double quotes, when using '@' as the exponent marker. If you 
    must use double quotes (which is hard to believe) then you
    need to escape the '@'. ie the following two assignments are
    equivalent:
     Rmpfr_set_str($rop, '.1234@-5', 10, GMP_RNDN);
     Rmpfr_set_str($rop, ".1234\@-5", 10, GMP_RNDN);
    But the following assignment won't do what you want:
     Rmpfr_set_str($rop, ".1234@-5", 10, GMP_RNDN); 

   Rmpfr_strtofr($rop, $str, $base, $rnd);
    Read a floating point number from a string $str in base $base,
    rounded in the direction $rnd. If successful, the result is
    stored in $rop. If $str doesn't start with a valid number then
    $rop is set to zero.
    Parsing follows the standard C `strtod' function with some
    extensions.  Case is ignored. After optional leading whitespace,
    one has a subject sequence consisting of an optional sign (`+' or
    `-'), and either numeric data or special data. The subject
    sequence is defined as the longest initial subsequence of the
    input string, starting with the first non-whitespace character,
    that is of the expected form.
    The form of numeric data is a non-empty sequence of significand
    digits with an optional decimal point, and an optional exponent
    consisting of an exponent prefix followed by an optional sign and
    a non-empty sequence of decimal digits. A significand digit is
    either a decimal digit or a Latin letter (62 possible characters),
    with `a' = 10, `b' = 11, ..., `z' = 36; its value must be strictly
    less than the base.  The decimal point can be either the one
    defined by the current locale or the period (the first one is
    accepted for consistency with the C standard and the practice, the
    second one is accepted to allow the programmer to provide MPFR
    numbers from strings in a way that does not depend on the current
    locale).  The exponent prefix can be `e' or `E' for bases up to
    10, or `@' in any base; it indicates a multiplication by a power
    of the base. In bases 2 and 16, the exponent prefix can also be
    `p' or `P', in which case it introduces a binary exponent: it
    indicates a multiplication by a power of 2 (there is a difference
    only for base 16).  The value of an exponent is always written in
    base 10.  In base 2, the significand can start with `0b' or `0B',
    and in base 16, it can start with `0x' or `0X'.

    If the argument $base is 0, then the base is automatically detected
    as follows. If the significand starts with `0b' or `0B', base 2 is
    assumed. If the significand starts with `0x' or `0X', base 16 is
    assumed. Otherwise base 10 is assumed. Other allowable values for 
    $base are 2 to 36 (2 to 62 if Math::MPFR has been built against
    mpfr-3.0.0 or later).

    Note: The exponent must contain at least a digit. Otherwise the
    possible exponent prefix and sign are not part of the number
    (which ends with the significand). Similarly, if `0b', `0B', `0x'
    or `0X' is not followed by a binary/hexadecimal digit, then the
    subject sequence stops at the character `0'.
    Special data (for infinities and NaN) can be `@inf@' or
    `@nan@(n-char-sequence)', and if BASE <= 16, it can also be
    `infinity', `inf', `nan' or `nan(n-char-sequence)', all case
    insensitive.  A `n-char-sequence' is a non-empty string containing
    only digits, Latin letters and the underscore (0, 1, 2, ..., 9, a,
    b, ..., z, A, B, ..., Z, _). Note: one has an optional sign for
    all data, even NaN.
    The function returns a usual ternary value.

   Rmpfr_set_str_binary($rop, $str);
    Set $rop to the value of the binary number in $str, which has to
    be of the form +/-xxxx.xxxxxxEyy. The exponent is read in decimal,
    but is interpreted as the power of two to be multiplied by the
    mantissa.  The mantissa length of $str has to be less or equal to
    the precision of $rop, otherwise an error occurs.  If $str starts
    with `N', it is interpreted as NaN (Not-a-Number); if it starts
    with `I' after the sign, it is interpreted as infinity, with the
    corresponding sign.

   Rmpfr_set_inf($rop, $si);
   Rmpfr_set_nan($rop);
   Rmpfr_set_zero($rop, $si); # mpfr-3.0.0 and later only.
    Set the variable $rop to infinity or NaN (Not-a-Number) or zero
    respectively. In 'mpfr_set_inf' and 'mpfr_set_zero', the sign of $rop
    is positive if 2nd arg >= 0. Else the sign is negative.

   Rmpfr_swap($op1, $op2); 
    Swap the values $op1 and $op2 efficiently. Warning: the precisions
    are exchanged too; in case the precisions are different, `mpfr_swap'
    is thus not equivalent to three `mpfr_set' calls using a third
    auxiliary variable.

   ################################################

   COMBINED INITIALIZATION AND ASSIGNMENT

   NOTE: Do NOT use these functions if $rop has already
   been initialised. Use the Rmpfr_set* functions in the
   section 'ASSIGNMENT' (above).

   First read the section 'MEMORY MANAGEMENT' (above).

   $rop = Math::MPFR->new($arg);
   $rop = Math::MPFR::new($arg);
   $rop = new Math::MPFR($arg);
    Returns a Math::MPFR object with the value of $arg, rounded
    in the default rounding direction, with default precision.
    $arg can be either a number (signed integer, unsigned integer,
    signed fraction or unsigned fraction), a string that 
    represents a numeric value, or an object (of type Math::GMPf,
    Math::GMPq, Math::GMPz, orMath::GMP) If $arg is a string, an
    optional additional argument that specifies the base of the
    number can be supplied to new(). Legal values for base are 0
    and 2 to 36 (2 to 62 if Math::MPFR has been built against
    mpfr-3.0.0 or later). If $arg is a string and no 
    additional argument is supplied, an attempt is made to deduce 
    base. See 'Rmpfr_set_str' above for an explanation of how
    that deduction is attempted. For finer grained control, use
    one of the 'Rmpfr_init_set_*' functions documented immediately
    below.

   ($rop, $si) = Rmpfr_init_set($op, $rnd);
   ($rop, $si) = Rmpfr_init_set_nobless($op, $rnd);
   ($rop, $si) = Rmpfr_init_set_ui($ui, $rnd);
   ($rop, $si) = Rmpfr_init_set_ui_nobless($ui, $rnd);
   ($rop, $si) = Rmpfr_init_set_si($si, $rnd);
   ($rop, $si) = Rmpfr_init_set_si_nobless($si, $rnd);
   ($rop, $si) = Rmpfr_init_set_d($double, $rnd);
   ($rop, $si) = Rmpfr_init_set_d_nobless($double, $rnd);
   ($rop, $si) = Rmpfr_init_set_ld($double, $rnd);
   ($rop, $si) = Rmpfr_init_set_ld_nobless($double, $rnd);
   ($rop, $si) = Rmpfr_init_set_f($f, $rnd);# $f is a mpf object
   ($rop, $si) = Rmpfr_init_set_f_nobless($f, $rnd);# $f is a mpf object
   ($rop, $si) = Rmpfr_init_set_z($z, $rnd);# $z is a mpz object
   ($rop, $si) = Rmpfr_init_set_z_nobless($z, $rnd);# $z is a mpz object
   ($rop, $si) = Rmpfr_init_set_q($q, $rnd);# $q is a mpq object
   ($rop, $si) = Rmpfr_init_set_q_nobless($q, $rnd);# $q is a mpq object
    Initialize $rop and set its value from the 1st arg, rounded to
    direction $rnd. The precision of $rop will be taken from the
    active default precision, as set by `Rmpfr_set_default_prec'.
    If $rop = 1st arg, $si is zero. If $rop > 1st arg, $si is positive.
    If $rop < 1st arg, $si is negative.

   ($rop, $si) = Rmpfr_init_set_str($str, $base, $rnd);
   ($rop, $si) = Rmpfr_init_set_str_nobless($str, $base, $rnd);
     Initialize $rop and set its value from $str in base $base,
     rounded to direction $rnd. If $str was a valid number, then
     $si will be set to 0. Else it will be set to -1.
     See `Rmpfr_set_str' (above) and 'Rmpfr_inp_str' (below).

   ##########

   CONVERSION

   $str = Rmpfr_get_str($op, $base, $digits, $rnd); 
    Returns a string of the form, eg, '8.3456712@2'
    which means '834.56712'.
    The third argument to Rmpfr_get_str() specifies the number of digits
    required to be output in the mantissa. (Trailing zeroes are removed.)
    If $digits is 0, the number of digits of the mantissa is chosen
    large enough so that re-reading the printed value with the same
    precision, assuming both output and input use rounding to nearest,
    will recover the original value of $op.

   ($str, $si) = Rmpfr_deref2($op, $base, $digits, $rnd);
    Returns the mantissa to $str (as a string of digits, prefixed with
    a minus sign if $op is negative), and returns the exponent to $si.
    There's an implicit decimal point to the left of the first digit in
    $str. The third argument to Rmpfr_deref2() specifies the number of
    digits required to be output in the mantissa. 
    If $digits is 0, the number of digits of the mantissa is chosen
    large enough so that re-reading the printed value with the same
    precision, assuming both output and input use rounding to nearest,
    will recover the original value of $op.

   $str = Rmpfr_integer_string($op, $base, $rnd);
    Returns the truncated integer value of $op as a string. (No exponent
    is returned). For example, if $op contains the value 2.3145679e2,
    $str will be set to "231".
    (This function is mainly to provide a simple means of getting 'sj'
    and 'uj' values on a 64-bit perl where the MPFR library does not
    support mpfr_get_uj and mpfr_get_sj functions - which may happen,
    for example, with libraries built with Microsoft Compilers.)

   $bool = Rmpfr_fits_ushort_p($op, $rnd); # fits in unsigned long
   $bool = Rmpfr_fits_sshort_p($op, $rnd); # fits in signed long
   $bool = Rmpfr_fits_uint_p($op, $rnd); # fits in unsigned long
   $bool = Rmpfr_fits_sint_p($op, $rnd); # fits in signed long
   $bool = Rmpfr_fits_ulong_p($op, $rnd); # fits in unsigned long
   $bool = Rmpfr_fits_slong_p($op, $rnd); # fits in signed long
   $bool = Rmpfr_fits_uintmax_p($op, $rnd); # fits in unsigned long
   $bool = Rmpfr_fits_intmax_p($op, $rnd); # fits in signed long
   $bool = Rmpfr_fits_IV_p($op, $rnd); # fits in perl IV
   $bool = Rmpfr_fits_UV_p($op, $rnd); # fits in perl UV
    Return non-zero if $op would fit in the respective data
    type, when rounded to an integer in the direction $rnd.

   $ui = Rmpfr_get_ui($op, $rnd); 
   $si = Rmpfr_get_si($op, $rnd);
   $sj = Rmpfr_get_sj($op, $rnd); # 64 bit builds only
   $uj = Rmpfr_get_uj($op, $rnd); # 64 bit builds only
   $uv = Rmpfr_get_UV($op, $rnd); # 32 and 64 bit
   $iv = Rmpfr_get_IV($op, $rnd); # 32 and 64 bit
    Convert $op to an 'unsigned long long', a 'signed long', a
   'signed long long', an `unsigned long long', a 'UV', or an
   'IV' - after rounding it with respect to $rnd.
    If $op is NaN, the result is undefined. If $op is too big
    for the return type, it returns the maximum or the minimum
    of the corresponding C type, depending on the direction of
    the overflow. The flag erange is then also set.

   $double = Rmpfr_get_d($op, $rnd);
   $ld     = Rmpfr_get_ld($op, $rnd);
   $nv     = Rmpfr_get_NV($op, $rnd);
   $float  = Rmpfr_get_flt($op, $rnd); # mpfr-3.0.0 and later only
    Convert $op to a 'double' a 'long double' an 'NV', or a float
    using the rounding mode $rnd. (I don't know how perl can do
    anything useful with Rmpfr_get_flt.)

   $double = Rmpfr_get_d1($op);
    Convert $op to a double, using the default MPFR rounding mode
    (see function `mpfr_set_default_rounding_mode').

   $si = Rmpfr_get_z_exp($z, $op); # $z is a mpz object
   $si = Rmpfr_get_z_2exp($z, $op); # $z is a mpz object
    (Identical functions. Use either - 'get_z_exp' might one day
    be removed.)
    Puts the mantissa of $rop into $z, and returns the exponent 
    $si such that $rop == $z * (2 ** $ui).

   $si = Rmpfr_get_z($z, $op, $rnd); # $z is a mpz object.
    Convert $op to an mpz object ($z), after rounding it with respect
    to RND. If built against mpfr-3.0.0 or later, return the usual
    ternary value. (The function returns undef when using mpfr-2.x.x.)
    If $op is NaN or Inf, the result is undefined.

   $si = Rmpfr_get_f ($f, $op, $rnd); # $f is an mpf object.
    Convert $op to a `mpf_t', after rounding it with respect to $rnd.
    When built against mpfr-3.0.0 or later, this function returns the
    usual ternary value. (If $op is NaN or Inf, then the erange flag
    will be set.) When built against earlier versions of mpfr,
    return zero iff no error occurred.In particular a non-zero value
    is returned if $op is NaN or Inf. which do not exist in `mpf'.

   $d = Rmpfr_get_d_2exp ($exp, $op, $rnd); # $d is NV (double)
   $d = Rmpfr_get_ld_2exp ($exp, $op, $rnd); # $d is NV (long double)
    Set $exp and $d such that 0.5<=abs($d)<1 and $d times 2 raised
    to $exp equals $op rounded to double (resp. long double)
    precision, using the given rounding mode.  If $op is zero, then a
    zero of the same sign (or an unsigned zero, if the implementation
    does not have signed zeros) is returned, and $exp is set to 0.
    If $op is NaN or an infinity, then the corresponding double
    precision (resp. long-double precision) value is returned, and 
    $exp is undefined.

   $si1 = Rmpfr_frexp($si2, $rop, $op, $rnd); # mpfr-3.1.0 and later only
    Set $si and $rop such that 0.5<=abs($rop)<1 and $rop * (2 ** $exp)
    equals $op rounded to the precision of $rop, using the given
    rounding mode. If $op is zero, then $rop is set to zero (of the same
    sign) and $exp is set to 0. If $op is  NaN or an infinity, then $rop
    is set to the same value and the value of $exp is meaningless (and
    should be ignored).

   ##########

   ARITHMETIC

   $si = Rmpfr_add($rop, $op1, $op2, $rnd);
   $si = Rmpfr_add_ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_add_si($rop, $op, $si1, $rnd);
   $si = Rmpfr_add_d($rop, $op, $double, $rnd);
   $si = Rmpfr_add_z($rop, $op, $z, $rnd); # $z is a mpz object.
   $si = Rmpfr_add_q($rop, $op, $q, $rnd); # $q is a mpq object.
    Set $rop to 2nd arg + 3rd arg rounded in the direction $rnd.
    The return  value is zero if $rop is exactly 2nd arg + 3rd arg,
    positive if $rop is larger than 2nd arg + 3rd arg, and negative
    if $rop is smaller than 2nd arg + 3rd arg.

   $si = Rmpfr_sum($rop, \@ops, scalar(@ops), $rnd);
    @ops is an array consisting entirely of Math::MPFR objects.
    Set $rop to the sum of all members of @ops, rounded in the direction
    $rnd. $si is zero when the computed value is the exact value, and
    non-zero when this cannot be guaranteed, without giving the direction
    of the error as the other functions do. 

   $si = Rmpfr_sub($rop, $op1, $op2, $rnd);
   $si = Rmpfr_sub_ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_sub_z($rop, $op, $z, $rnd); # $z is a mpz object.
   $si = Rmpfr_z_sub($rop, $z, $op, $rnd); # mpfr-3.1.0 and later only
   $si = Rmpfr_sub_q($rop, $op, $q, $rnd); # $q is a mpq object.
   $si = Rmpfr_ui_sub($rop, $ui, $op, $rnd);
   $si = Rmpfr_si_sub($rop, $si1, $op, $rnd);
   $si = Rmpfr_sub_si($rop, $op, $si1, $rnd);
   $si = Rmpfr_sub_d($rop, $op, $double, $rnd);
   $si = Rmpfr_d_sub($rop, $double, $op, $rnd);
    Set $rop to 2nd arg - 3rd arg rounded in the direction $rnd.
    The return value is zero if $rop is exactly 2nd arg - 3rd arg,
    positive if $rop is larger than 2nd arg - 3rd arg, and negative
    if $rop is smaller than 2nd arg - 3rd arg.

   $si = Rmpfr_mul($rop, $op1, $op2, $rnd);
   $si = Rmpfr_mul_ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_mul_si($rop, $op, $si1, $rnd);
   $si = Rmpfr_mul_d($rop, $op, $double, $rnd);
   $si = Rmpfr_mul_z($rop, $op, $z, $rnd); # $z is a mpz object.
   $si = Rmpfr_mul_q($rop, $op, $q, $rnd); # $q is a mpq object.
    Set $rop to 2nd arg * 3rd arg rounded in the direction $rnd.
    Return 0 if the result is exact, a positive value if $rop is 
    greater than 2nd arg times 3rd arg, a negative value otherwise.

   $si = Rmpfr_div($rop, $op1, $op2, $rnd);
   $si = Rmpfr_div_ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_ui_div($rop, $ui, $op, $rnd);
   $si = Rmpfr_div_si($rop, $op, $si1, $rnd);
   $si = Rmpfr_si_div($rop, $si1, $op, $rnd);
   $si = Rmpfr_div_d($rop, $op, $double, $rnd);
   $si = Rmpfr_d_div($rop, $double, $op, $rnd);
   $si = Rmpfr_div_z($rop, $op, $z, $rnd); # $z is a mpz object.
   $si = Rmpfr_div_q($rop, $op, $q, $rnd); # $q is a mpq object.
    Set $rop to 2nd arg / 3rd arg rounded in the direction $rnd. 
    These functions return 0 if the division is exact, a positive
    value when $rop is larger than 2nd arg divided by 3rd arg,
    and a negative value otherwise.

   $si = Rmpfr_sqr($rop, $op, $rnd);
    Set $rop to the square of $op, rounded in direction $rnd.

   $si = Rmpfr_sqrt($rop, $op, $rnd);
   $si = Rmpfr_sqrt_ui($rop, $ui, $rnd);
    Set $rop to the square root of the 2nd arg rounded in the
    direction $rnd. Set $rop to NaN if 2nd arg is negative.
    Return 0 if the operation is exact, a non-zero value otherwise.

   $si = Rmpfr_rec_sqrt($rop, $op, $rnd);
    Set $rop to the reciprocal square root of $op rounded in the
    direction $rnd. Return +Inf if OP is ±0, and +0 if OP is +Inf.
    Set $rop to NaN if $op is negative.

   $si = Rmpfr_cbrt($rop, $op, $rnd);
    Set $rop to the cubic root (defined over the real numbers)
    of $op, rounded in the direction $rnd.

   $si = Rmpfr_root($rop, $op, $ui $rnd);
    Set $rop to the $ui'th root of $op, rounded in the direction
    $rnd.  Return 0 if the operation is exact, a non-zero value
    otherwise.

   $si = Rmpfr_pow_ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_pow_si($rop, $op, $si, $rnd);
   $si = Rmpfr_ui_pow_ui($rop, $ui, $ui, $rnd);
   $si = Rmpfr_ui_pow($rop, $ui, $op, $rnd);
   $si = Rmpfr_pow($rop, $op1, $op2, $rnd);
   $si = Rmpfr_pow_z($rop, $op1, $z, $rnd); # $z is a mpz object
    Set $rop to 2nd arg raised to 3rd arg, rounded to the directio
    $rnd with the precision of $rop.  Return zero iff the result is
    exact, a positive value when the result is greater than 2nd arg
    to the power 3rd arg, and a negative value when it is smaller.
    See the MPFR documentation for documentation regarding special 
    cases.

   $si = Rmpfr_neg($rop, $op, $rnd);
    Set $rop to -$op rounded in the direction $rnd. Just
    changes the sign if $rop and $op are the same variable.

   $si = Rmpfr_abs($rop, $op, $rnd);
    Set $rop to the absolute value of $op, rounded in the direction
    $rnd. Return 0 if the result is exact, a positive value if ROP
    is larger than the absolute value of $op, and a negative value 
    otherwise.

   $si = Rmpfr_dim($rop, $op1, $op2, $rnd);
    Set $rop to the positive difference of $op1 and $op2, i.e.,
    $op1 - $op2 rounded in the direction $rnd if $op1 > $op2, and
    +0 otherwise. $rop is set to NaN when $op1 or $op2 is NaN.

   $si = Rmpfr_mul_2exp($rop, $op, $ui, $rnd);
   $si = Rmpfr_mul_2ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_mul_2si($rop, $op, $si, $rnd);
    Set $rop to 2nd arg times 2 raised to 3rd arg rounded to the
    direction $rnd. Just increases the exponent by 3rd arg when
    $rop and 2nd arg are identical. Return zero when $rop = 2nd
    arg, a positive value when $rop > 2nd arg, and a negative
    value when $rop < 2nd arg.  Note: The `Rmpfr_mul_2exp' function
    is defined for compatibility reasons; you should use
    `Rmpfr_mul_2ui' (or `Rmpfr_mul_2si') instead.

   $si = Rmpfr_div_2exp($rop, $op, $ui, $rnd);
   $si = Rmpfr_div_2ui($rop, $op, $ui, $rnd);
   $si = Rmpfr_div_2si($rop, $op, $si, $rnd);
    Set $rop to 2nd arg divided by 2 raised to 3rd arg rounded to
    the direction $rnd. Just decreases the exponent by 3rd arg
    when $rop and 2nd arg are identical.  Return zero when 
    $rop = 2nd arg, a positive value when $rop > 2nd arg, and a
    negative value when $rop < 2nd arg.  Note: The `Rmpfr_div_2exp'
    function is defined for compatibility reasons; you should
    use `Rmpfr_div_2ui' (or `Rmpfr_div_2si') instead.

   ##########
     
   COMPARISON

   $si = Rmpfr_cmp($op1, $op2);
   $si = Rmpfr_cmpabs($op1, $op2);
   $si = Rmpfr_cmp_ui($op, $ui);
   $si = Rmpfr_cmp_si($op, $si);
   $si = Rmpfr_cmp_d($op, $double);
   $si = Rmpfr_cmp_ld($op, $ld); # long double
   $si = Rmpfr_cmp_z($op, $z); # $z is a mpz object
   $si = Rmpfr_cmp_q($op, $q); # $q is a mpq object
   $si = Rmpfr_cmp_f($op, $f); # $f is a mpf object
    Compare 1st and 2nd args. In the case of 'Rmpfr_cmpabs()'
    compare the absolute values of the 2 args.  Return a positive
    value if 1st arg > 2nd arg, zero if 1st arg = 2nd arg, and a 
    negative value if 1st arg < 2nd arg.  Both args are considered
    to their full own precision, which may differ. In case 1st and 
    2nd args are of same sign but different, the absolute value 
    returned is one plus the absolute difference of their exponents.
    If one of the operands is NaN (Not-a-Number), return zero 
    and set the erange flag.


   $si = Rmpfr_cmp_ui_2exp($op, $ui, $si);
   $si = Rmpfr_cmp_si_2exp($op, $si, $si);
    Compare 1st arg and 2nd arg multiplied by two to the power 
    3rd arg.

   $bool = Rmpfr_eq($op1, $op2, $ui);
    The mpfr library function mpfr_eq may change in future 
    releases of the mpfr library (post 2.4.0). If that happens,
    the change will also be relected in Rmpfr_eq.
    Return non-zero if the first $ui bits of $op1 and $op2 are
    equal, zero otherwise.  I.e., tests if $op1 and $op2 are 
    approximately equal.

   $bool = Rmpfr_nan_p($op);
    Return non-zero if $op is Not-a-Number (NaN), zero otherwise.

   $bool = Rmpfr_inf_p($op);
    Return non-zero if $op is plus or minus infinity, zero otherwise.

   $bool = Rmpfr_number_p($op);
    Return non-zero if $op is an ordinary number, i.e. neither
    Not-a-Number nor plus or minus infinity.

   $bool = Rmpfr_zero_p($op);
    Return non-zero if $op is zero. Else return 0.

   $bool = Rmpfr_regular_p($op); # mpfr-3.0.0 and later only
    Return non-zero if $op is a regular number (i.e. neither NaN,
    nor an infinity nor zero). Return zero otherwise.

   Rmpfr_reldiff($rop, $op1, $op2, $rnd);
    Compute the relative difference between $op1 and $op2 and 
    store the result in $rop.  This function does not guarantee
    the exact rounding on the relative difference; it just
    computes abs($op1-$op2)/$op1, using the rounding mode
    $rnd for all operations.

   $si = Rmpfr_sgn($op);
    Return a positive value if op > 0, zero if $op = 0, and a
    negative value if $op < 0.  Its result is not specified
    when $op is NaN (Not-a-Number).

   $bool = Rmpfr_greater_p($op1, $op2);
    Return non-zero if $op1 > $op2, zero otherwise.

   $bool = Rmpfr_greaterequal_p($op1, $op2);
    Return non-zero if $op1 >= $op2, zero otherwise.

   $bool = Rmpfr_less_p($op1, $op2);
    Return non-zero if $op1 < $op2, zero otherwise.

   $bool = Rmpfr_lessequal_p($op1, $op2);
    Return non-zero if $op1 <= $op2, zero otherwise.

   $bool = Rmpfr_lessgreater_p($op1, $op2);
    Return non-zero if $op1 < $op2 or $op1 > $op2 (i.e. neither
    $op1, nor $op2 is NaN, and $op1 <> $op2), zero otherwise
    (i.e. $op1 and/or $op2 are NaN, or $op1 = $op2).

   $bool = Rmpfr_equal_p($op1, $op2);
    Return non-zero if $op1 = $op2, zero otherwise
    (i.e. $op1 and/or $op2 are NaN, or $op1 <> $op2).

   $bool = Rmpfr_unordered_p($op1, $op2);
     Return non-zero if $op1 or $op2 is a NaN
     (i.e. they cannot be compared), zero otherwise.

   #######

   SPECIAL

   $si = Rmpfr_log($rop, $op, $rnd);
   $si = Rmpfr_log2($rop, $op, $rnd);
   $si = Rmpfr_log10($rop, $op, $rnd);
    Set $rop to the natural logarithm of $op, log2($op) or 
    log10($op), respectively, rounded in the direction rnd.

   $si = Rmpfr_exp($rop, $op, $rnd);
   $si = Rmpfr_exp2($rop, $op, $rnd);
   $si = Rmpfr_exp10($rop, $op, $rnd);
    Set rop to the exponential of op, to 2 power of op or to
    10 power of op, respectively, rounded in the direction rnd. 

   $si = Rmpfr_sin($rop $op, $rnd);
   $si = Rmpfr_cos($rop, $op, $rnd);
   $si = Rmpfr_tan($rop, $op, $rnd);
    Set $rop to the sine/cosine/tangent respectively of $op,
    rounded to the direction $rnd with the precision of $rop.
    Return 0 iff the result is exact (this occurs in fact only
    when $op is 0 i.e. the sine is 0, the cosine is 1, and the
    tangent is 0). Return a negative value iff the result is less
    than the actual value. Return a positive result iff the
    return is greater than the actual value.

   $si = Rmpfr_sin_cos($rop1, $rop2, $op, $rnd);
    Set simultaneously $rop1 to the sine of $op and
    $rop2 to the cosine of $op, rounded to the direction $rnd
    with their corresponding precisions.  Return 0 iff both
    results are exact.

   $si = Rmpfr_sinh_cosh($rop1, $rop2, $op, $rnd);
    Set simultaneously $rop1SOP to the hyperbolic sine of $op and
    $rop2 to the hyperbolic cosine of $op, rounded in the direction
    $rnd with the corresponding precision of $rop1 and $rop2 which
    must be different variables. Return 0 iff both results are
    exact.

   $si = Rmpfr_acos($rop, $op, $rnd);
   $si = Rmpfr_asin($rop, $op, $rnd);
   $si = Rmpfr_atan($rop, $op, $rnd);
    Set $rop to the arc-cosine, arc-sine or arc-tangent of $op,
    rounded to the direction $rnd with the precision of $rop.
    Return 0 iff the result is exact. Return a negative value iff
    the result is less than the actual value. Return a positive 
    result iff the return is greater than the actual value.

   $si = Rmpfr_atan2($rop, $op1, $op2, $rnd);
    Set $rop to the tangent of $op1/$op2, rounded to the 
    direction $rnd with the precision of $rop.
    Return 0 iff the result is exact. Return a negative value iff
    the result is less than the actual value. Return a positive 
    result iff the return is greater than the actual value.
    See the MPFR documentation for details regarding special cases.   


   $si = Rmpfr_cosh($rop, $op, $rnd);
   $si = Rmpfr_sinh($rop, $op, $rnd);
   $si = Rmpfr_tanh($rop, $op, $rnd);
    Set $rop to the hyperbolic cosine/hyperbolic sine/hyperbolic
    tangent respectively of $op, rounded to the direction $rnd
    with the precision of $rop.  Return 0 iff the result is exact
    (this occurs in fact only when OP is 0 i.e. the result is 1).
    Return a negative value iff the result is less than the actual
    value. Return a positive result iff the return is greater than
    the actual value.

   $si = Rmpfr_acosh($rop, $op, $rnd);
   $si = Rmpfr_asinh($rop, $op, $rnd);
   $si = Rmpfr_atanh($rop, $op, $rnd);
    Set $rop to the inverse hyperbolic cosine, sine or tangent
    of $op, rounded to the direction $rnd with the precision of
    $rop.  Return 0 iff the result is exact.

   $si = Rmpfr_sec ($rop, $op, $rnd);
   $si = Rmpfr_csc ($rop, $op, $rnd);
   $si = Rmpfr_cot ($rop, $op, $rnd);
    Set $rop to the secant of $op, cosecant of $op,
    cotangent of $op, rounded in the direction RND. Return 0 
    iff the result is exact. Return a negative value iff the
    result is less than the actual value. Return a positive 
    result iff the return is greater than the actual value.

   $si = Rmpfr_sech ($rop, $op, $rnd);
   $si = Rmpfr_csch ($rop, $op, $rnd);
   $si = Rmpfr_coth ($rop, $op, $rnd);
    Set $rop to the hyperbolic secant of $op, cosecant of $op,
    cotangent of $op, rounded in the direction RND. Return 0 
    iff the result is exact. Return a negative value iff the
    result is less than the actual value. Return a positive 
    result iff the return is greater than the actual value.

   $bool = Rmpfr_fac_ui($rop, $ui, $rnd);
    Set $rop to the factorial of $ui, rounded to the direction
    $rnd with the precision of $rop.  Return 0 iff the
    result is exact.

   $bool = Rmpfr_log1p($rop, $op, $rnd);
    Set $rop to the logarithm of one plus $op, rounded to the
    direction $rnd with the precision of $rop.  Return 0 iff 
    the result is exact (this occurs in fact only when OP is 0
    i.e. the result is 0).

   $bool = Rmpfr_expm1($rop, $op, $rnd);
    Set $rop to the exponential of $op minus one, rounded to the
    direction $rnd with the precision of $rop.  Return 0 iff the
    result is exact (this occurs in fact only when OP is 0 i.e
    the result is 0).

   $si = Rmpfr_fma($rop, $op1, $op2, $op3, $rnd);
    Set $rop to $op1 * $op2 + $op3, rounded to the direction
    $rnd.

   $si = Rmpfr_fms($rop, $op1, $op2, $op3, $rnd);
    Set $rop to $op1 * $op2 - $op3, rounded to the direction
    $rnd.

   $si = Rmpfr_agm($rop, $op1, $op2, $rnd);
    Set $rop to the arithmetic-geometric mean of $op1 and $op2,
    rounded to the direction $rnd with the precision of $rop.
    Return zero if $rop is exact, a positive value if $rop is
    larger than the exact value, or a negative value if $rop 
    is less than the exact value.

   $si = Rmpfr_hypot ($rop, $op1, $op2, $rnd);
    Set $rop to the Euclidean norm of $op1 and $op2, i.e. the 
    square root of the sum of the squares of $op1 and $op2, 
    rounded in the direction $rnd. Special values are currently
    handled as described in Section F.9.4.3 of the ISO C99 
    standard, for the hypot function (note this may change in 
    future versions): If $op1 or $op2 is an infinity, then plus
    infinity is returned in $rop, even if the other number is
    NaN.

   $si = Rmpfr_ai($rop, $op, $rnd); # mpfr-3.0.0 and later only
    Set $rop to the value of the Airy function Ai on $op,
    rounded in the direction $rnd.  When $op is NaN, $rop is
    always set to NaN. When $op is +Inf or -Inf, $rop is +0.
    The current implementation is not intended to be used with
    large arguments.  It works with $op typically smaller than
    500. For larger arguments, other methods should be used and
    will be implemented soon.


   $si = Rmpfr_const_log2($rop, $rnd);
    Set $rop to the logarithm of 2 rounded to the direction
    $rnd with the precision of $rop. This function stores the
    computed value to avoid another calculation if a lower or
    equal precision is requested.
    Return zero if $rop is exact, a positive value if $rop is
    larger than the exact value, or a negative value if $rop 
    is less than the exact value.

   $si = Rmpfr_const_pi($rop, $rnd);
    Set $rop to the value of Pi rounded to the direction $rnd
    with the precision of $rop. This function uses the Borwein,
    Borwein, Plouffe formula which directly gives the expansion
    of Pi in base 16.
    Return zero if $rop is exact, a positive value if $rop is
    larger than the exact value, or a negative value if $rop 
    is less than the exact value.

   $si = Rmpfr_const_euler($rop, $rnd);
    Set $rop to the value of Euler's constant 0.577...  rounded
    to the direction $rnd with the precision of $rop.
    Return zero if $rop is exact, a positive value if $rop is
    larger than the exact value, or a negative value if $rop 
    is less than the exact value.

   $si = Rmpfr_const_catalan($rop, $rnd);
    Set $rop to the value of Catalan's constant 0.915...
    rounded to the direction $rnd with the precision of $rop.
    Return zero if $rop is exact, a positive value if $rop is
    larger than the exact value, or a negative value if $rop 
    is less than the exact value.

   Rmpfr_free_cache();
    Free the cache used by the functions computing constants if
    needed (currently `mpfr_const_log2', `mpfr_const_pi' and
    `mpfr_const_euler').

   $si = Rmpfr_gamma($rop, $op, $rnd);
   $si = Rmpfr_lngamma($rop, $op, $rnd);
    Set $rop to the value of the Gamma function on $op 
   (and, respectively, its natural logarithm) rounded
    to the direction $rnd. Return zero if $rop is exact, a
    positive value if $rop is larger than the exact value, or a
    negative value if $rop is less than the exact value.

   ($signp, $si) = Rmpfr_lgamma ($rop, $op, $rnd);
    Set $rop to the value of the logarithm of the absolute value
    of the Gamma function on $op, rounded in the direction $rnd.
    The sign (1 or -1) of Gamma($op) is returned in $signp.
    When $op is an infinity or a non-positive integer, +Inf is
    returned. When $op is NaN, -Inf or a negative integer, $signp
    is undefined, and when OP is ±0, $signp is the sign of the zero.

   $si = Rmpfr_digamma ($rop, $op, $rnd); # mpfr-3.0.0 and later only
    Set $rop to the value of the Digamma (sometimes also called Psi)
    function on $op, rounded in the direction $rnd.  When $op is a
    negative integer, set $rop to NaN.

   $si = Rmpfr_zeta($rop, $op, $rnd);
   $si = Rmpfr_zeta_ui($rop, $ul, $rnd);
    Set $rop to the value of the Riemann Zeta function on 2nd arg,
    rounded to the direction $rnd. Return zero if $rop is exact,
    a positive value if $rop is larger than the exact value, or
    a negative value if $rop is less than the exact value.

   $si = Rmpfr_erf($rop, $op, $rnd);
    Set $rop to the value of the error function on $op,
    rounded to the direction $rnd. Return zero if $rop is exact,
    a positive value if $rop is larger than the exact value, or
    a negative value if $rop is less than the exact value.

   $si = Rmpfr_erfc($rop, $op, $rnd);
    Set $rop to the complementary error function on $op,
    rounded to the direction $rnd. Return zero if $rop is exact,
    a positive value if $rop is larger than the exact value, or
    a negative value if $rop is less than the exact value.

   $si = Rmpfr_j0 ($rop, $op, $rnd);
   $si = Rmpfr_j1 ($rop, $op, $rnd);
   $si = Rmpfr_jn ($rop, $si2, $op, $rnd);
    Set $rop to the value of the first order Bessel function of
    order 0, 1 and $si2 on $op, rounded in the direction $rnd.
    When $op is NaN, $rop is always set to NaN. When $op is plus
    or minus Infinity, $rop is set to +0. When $op is zero, and
    $si2 is not zero, $rop is +0 or -0 depending on the parity 
    and sign of $si2, and the sign of $op.

   $si = Rmpfr_y0 ($rop, $op, $rnd);
   $si = Rmpfr_y1 ($rop, $op, $rnd);
   $si = Rmpfr_yn ($rop, $si2, $op, $rnd);
     Set $rop to the value of the second order Bessel function of
     order 0, 1 and $si2 on OP, rounded in the direction $rnd.
     When $op is NaN or negative, $rop is always set to NaN.
     When $op is +Inf, $rop is +0. When $op is zero, $rop is +Inf
     or -Inf depending on the parity and sign of $si2.

   $si = Rmpfr_eint ($rop, $op, $rnd)
     Set $rop to the exponential integral of $op, rounded in the
     direction $rnd. See the MPFR documentation for details.

   $si = Rmpfr_li2 ($rop, $op, $rnd);
    Set $rop to real part of the dilogarithm of $op, rounded in the
    direction $rnd. The dilogarithm function is defined here as
    the integral of -log(1-t)/t from 0 to x.

   #############

   I-O FUNCTIONS

   $ui = Rmpfr_out_str([$prefix,] $op, $base, $digits, $round [, $suffix]);
    BEST TO USE TRmpfr_out_str INSTEAD
    Output $op to STDOUT, as a string of digits in base $base,
    rounded in direction $round.  The base may vary from 2 to 36
    (2 to 62 if Math::MPFR has been built against mpfr-3.0.0 or later).
    Print $digits significant digits exactly, or if $digits is 0,
    enough digits so that $op can be read back exactly
    (see Rmpfr_get_str). In addition to the significant
    digits, a decimal point at the right of the first digit and a
    trailing exponent in base 10, in the form `eNNN', are printed
    If $base is greater than 10, `@' will be used instead of `e'
    as exponent delimiter. The optional arguments, $prefix and 
    $suffix, are strings that will be prepended/appended to the 
    mpfr_out_str output. Return the number of bytes written (not
    counting those contained in $suffix and $prefix), or if an error
    occurred, return 0. (Note that none, one or both of $prefix and
    $suffix can be supplied.)

   $ui = TRmpfr_out_str([$prefix,] $stream, $base, $digits, $op, $round [, $suffix]);
    As for Rmpfr_out_str, except that there's the capability to print
    to somewhere other than STDOUT. Note that the order of the args
    is different (to match the order of the mpfr_out_str args).
    To print to STDERR:
       TRmpfr_out_str(*stderr, $base, $digits, $op, $round);
    To print to an open filehandle (let's call it FH):
       TRmpfr_out_str(\*FH, $base, $digits, $op, $round);

   $ui = Rmpfr_inp_str($rop, $base, $round);
    BEST TO USE TRmpfr_inp_str INSTEAD.
    Input a string in base $base from STDIN, rounded in
    direction $round, and put the read float in $rop.  The string
    is of the form `M@N' or, if the base is 10 or less, alternatively
    `MeN' or `MEN', or, if the base is 16, alternatively `MpB' or
    `MPB'. `M' is the mantissa in the specified base, `N' is the 
    exponent written in decimal for the specified base, and in base 16,
    `B' is the binary exponent written in decimal (i.e. it indicates
    the power of 2 by which the mantissa is to be scaled).
    The argument $base may be in the range 2 to 36 (2 to 62 if Math::MPFR
    has been built against mpfr-3.0.0 or later).
    Special values can be read as follows (the case does not matter):
    `@NaN@', `@Inf@', `+@Inf@' and `-@Inf@', possibly followed by
    other characters; if the base is smaller or equal to 16, the
    following strings are accepted too: `NaN', `Inf', `+Inf' and
    `-Inf'.
    Return the number of bytes read, or if an error occurred, return 0.

   $ui = TRmpfr_inp_str($rop, $stream, $base, $round);
    As for Rmpfr_inp_str, except that there's the capability to read
    from somewhere other than STDIN.
    To read from STDIN:
       TRmpfr_inp_str($rop, *stdin, $base, $round);
    To read from an open filehandle (let's call it FH):
       TRmpfr_inp_str($rop, \*FH, $base,  $round);

   Rmpfr_print_binary($op);
    Output $op on stdout in raw binary format (the exponent is in
    decimal, yet).

   Rmpfr_dump($op);
    Output "$op\n" on stdout in base 2.
    As with 'Rmpfr_print_binary' the exponent is in base 10.

   #############

   MISCELLANEOUS

   $MPFR_version = Rmpfr_get_version();
    Returns the version of the MPFR library (eg 2.1.0) being used by
    Math::MPFR.

   $GMP_version = Math::MPFR::gmp_v();
    Returns the version of the gmp library (eg. 4.1.3) being used by
    the mpfr library that's being used by Math::MPFR.
    The function is not exportable.

   $ui = MPFR_VERSION;
    An integer whose value is dependent upon the 'major', 'minor' and
    'patchlevel' values of the MPFR library against which Math::MPFR 
    was built.
    This value is from the mpfr.h that was in use when the compilation
    of Math::MPFR took place.

   $ui = MPFR_VERSION_MAJOR;
    The 'x' in the 'x.y.z' of the MPFR library version.
    This value is from the mpfr.h that was in use when the compilation
    of Math::MPFR took place.

   $ui = MPFR_VERSION_MINOR;
    The 'y' in the 'x.y.z' of the MPFR library version.
    This value is from the mpfr.h that was in use when the compilation
    of Math::MPFR took place.

   $ui = MPFR_VERSION_PATCHLEVEL;
    The 'z' in the 'x.y.z' of the MPFR library version.
    This value is from the mpfr.h that was in use when the compilation
    of Math::MPFR took place.

   $string = MPFR_VERSION_STRING;
    $string is set to the version of the MPFR library (eg 2.1.0)
    against which Math::MPFR was built.
    This value is from the mpfr.h that was in use when the compilation
    of Math::MPFR took place.

   $ui = MPFR_VERSION_NUM($major, $minor, $patchlevel);
    Returns the value for MPFR_VERSION on "MPFR-$major.$minor.$patchlevel".

   $str = Rmpfr_get_patches();
    Return a string containing the ids of the patches applied to the
    MPFR library (contents of the `PATCHES' file), separated by spaces.
    Note: If the program has been compiled with an older MPFR version and
    is dynamically linked with a new MPFR library version, the ids of the
    patches applied to the old (compile-time) MPFR version are not 
    available (however this information should not have much interest
    in general).

   $bool = Rmpfr_buildopt_tls_p(); # mpfr-3.0.0 and later only
    Return a non-zero value if mpfr was compiled as thread safe using
    compiler-level Thread Local Storage (that is mpfr was built with
    the `--enable-thread-safe' configure option), else return zero.

   $bool = Rmpfr_buildopt_decimal_p(); # mpfr-3.0.0 and later only
    Return a non-zero value if mpfr was compiled with decimal float
    support (that is mpfr was built with the `--enable-decimal-float'
    configure option), return zero otherwise.

   $bool = Rmpfr_buildopt_gmpinternals_p(); # mpfr-3.1.0 and later only
    Return a non-zero value if mpfr was compiled with gmp internals
    (that is, mpfr was built with either '--with-gmp-build' or
    '--enable-gmp-internals' configure option), return zero otherwise.

   $str = Rmpfr_buildopt_tune_case(); # mpfr-3.1.0 and later only
    Return a string saying which thresholds file has been used at
    compile time.  This file is normally selected from the processor
    type.

   $si = Rmpfr_rint($rop, $op, $rnd);
   $si = Rmpfr_ceil($rop, $op);
   $si = Rmpfr_floor($rop, $op);
   $si = Rmpfr_round($rop, $op);
   $si = Rmpfr_trunc($rop, $op);
    Set $rop to $op rounded to an integer. `Rmpfr_ceil' rounds to the
    next higher representable integer, `Rmpfr_floor' to the next lower,
    `Rmpfr_round' to the nearest representable integer, rounding
    halfway cases away from zero, and `Rmpfr_trunc' to the
    representable integer towards zero. `Rmpfr_rint' behaves like one
    of these four functions, depending on the rounding mode.  The
    returned value is zero when the result is exact, positive when it
    is greater than the original value of $op, and negative when it is
    smaller.  More precisely, the returned value is 0 when $op is an
    integer representable in $rop, 1 or -1 when $op is an integer that
    is not representable in $rop, 2 or -2 when $op is not an integer.

    $si = Rmpfr_rint_ceil($rop, $op, $rnd);
    $si = Rmpfr_rint_floor($rop, $op, $rnd);
    $si = Rmpfr_rint_round($rop, $op, $rnd);
    $si = Rmpfr_rint_trunc($rop, $op, $rnd):
     Set $rop to $op rounded to an integer. `Rmpfr_rint_ceil' rounds to
     the next higher or equal integer, `Rmpfr_rint_floor' to the next
     lower or equal integer, `Rmpfr_rint_round' to the nearest integer,
     rounding halfway cases away from zero, and `Rmpfr_rint_trunc' to
     the next integer towards zero.  If the result is not
     representable, it is rounded in the direction $rnd. The returned
     value is the ternary value associated with the considered
     round-to-integer function (regarded in the same way as any other
     mathematical function).

   $si = Rmpfr_frac($rop, $op, $round);
    Set $rop to the fractional part of OP, having the same sign as $op,
    rounded in the direction $round (unlike in `mpfr_rint', $round
    affects only how the exact fractional part is rounded, not how
    the fractional part is generated).

   $si = Rmpfr_modf ($rop1, $rop2, $op, $rnd);
    Set simultaneously $rop1 to the integral part of $op and $rop2
    to the fractional part of $op, rounded in the direction RND with
    the corresponding precision of $rop1 and $rop2 (equivalent to
    `Rmpfr_trunc($rop1, $op, $rnd)' and `Rmpfr_frac($rop1, $op, $rnd)').
    The variables $rop1 and $rop2 must be different. Return 0 iff both
    results are exact.

   $si = Rmpfr_remainder($rop, $op1, $op2, $rnd);
   $si = Rmpfr_fmod($rop, $op1, $op2, $rnd);
   ($si2, $si) = Rmpfr_remquo ($rop, $op1, $op2, $rnd);
    Set $rop to the remainder of the division of $op1 by $op2, with
    quotient rounded toward zero for 'Rmpfr_fmod' and to the nearest
    integer (ties rounded to even) for 'Rmpfr_remainder' and 
    'Rmpfr_remquo', and $rop rounded according to the direction $rnd.
    Special values are handled as described in Section F.9.7.1 of the
    ISO C99 standard: If $op1 is infinite or $op2 is zero, $rop is NaN.
    If $op2 is infinite and $op1 is finite, $rop is $op1 rounded to
    the precision of $rop. If $rop is zero, it has the sign of $op1.
    The return value is the ternary value corresponding to $rop.
    Additionally, `Rmpfr_remquo' stores the low significant bits from
    the quotient in $si2 (more precisely the number of bits in a `long'
    minus one), with the sign of $op1 divided by $op2 (except if those
    low bits are all zero, in which case zero is returned).  Note that
    $op1 may be so large in magnitude relative to $op2 that an exact
    representation of the quotient is not practical.  `Rmpfr_remainder'
    and `Rmpfr_remquo' functions are useful for additive argument
    reduction.

   $si = Rmpfr_integer_p($op);
    Return non-zero iff $op is an integer.

   Rmpfr_nexttoward($op1, $op2);
    If $op1 or $op2 is NaN, set $op1 to NaN. Otherwise, if $op1 is 
    different from $op2, replace $op1 by the next floating-point number
    (with the precision of $op1 and the current exponent range) in the 
    direction of $op2, if there is one (the infinite values are seen as
    the smallest and largest floating-point numbers). If the result is
    zero, it keeps the same sign. No underflow or overflow is generated.

   Rmpfr_nextabove($op1);
    Equivalent to `mpfr_nexttoward' where $op2 is plus infinity.

   Rmpfr_nextbelow($op1);
    Equivalent to `mpfr_nexttoward' where $op2 is minus infinity.

   $si = Rmpfr_min($rop, $op1, $op2, $round);
    Set $rop to the minimum of $op1 and $op2. If $op1 and $op2
    are both NaN, then $rop is set to NaN. If $op1 or $op2 is 
    NaN, then $rop is set to the numeric value. If $op1 and
    $op2 are zeros of different signs, then $rop is set to -0.

   $si = Rmpfr_max($rop, $op1, $op2, $round);
     Set $rop to the maximum of $op1 and $op2. If $op1 and $op2
    are both NaN, then $rop is set to NaN. If $op1 or $op2 is
    NaN, then $rop is set to the numeric value. If $op1 and 
    $op2 are zeros of different signs, then $rop is set to +0.

   ##############

   RANDOM NUMBERS

   Rmpfr_urandomb(@r, $state);
    Requires that one of Math::GMPz, Math::GMPq or Math::GMPf
    is loaded.
    Each member of @r is a Math::MPFR object.
    $state is a reference to a gmp_randstate_t structure.
    Set each member of @r to a uniformly distributed random
    float in the interval 0 <= $_ < 1. 
    Before using this function you must first create $state
    by calling one of the 3 Rgmp_randinit functions, then
    seed $state by calling one of the 2 Rgmp_randseed functions.
    The memory associated with $state will not be freed until
    either you call Rgmp_randclear, or the program ends.

   Rmpfr_random2($rop, $si, $ui); # not implemented in
                                  # mpfr-3.0.0 and later
    Attempting to use this function when Math::MPFR has been
    built against mpfr-3.0.0 will cause the program to die, with
    an appropriate error message.
    Generate a random float of at most abs($si) limbs, with long
    strings of zeros and ones in the binary representation.
    The exponent of the number is in the interval -$ui to
    $ui.  This function is useful for testing functions and
    algorithms, since this kind of random numbers have proven
    to be more likely to trigger corner-case bugs.  Negative
    random numbers are generated when $si is negative.

   $si = Rmpfr_urandom ($rop, $state, $rnd); # mpfr-3.0.0 and
                                             # later only
    Generate a uniformly distributed random float.  The
    floating-point number $rop can be seen as if a random real
    number is generated according to the continuous uniform
    distribution on the interval[0, 1] and then rounded in the
    direction RND.
    Before using this function you must first create $state
    by calling one of the Rgmp_randinit functions (below), then
    seed $state by calling one of the Rgmp_randseed functions.

   $si = Rmpfr_grandom($rop1, $rop2, $state, $rnd);
    Available only with mpfr-3.1.0 and later.
    Generate two random floats according to a standard normal
    gaussian distribution. The floating-point numbers $rop1 and
    $rop2 can be seen as if a random real number were generated
    according to the standard normal gaussian distribution and
    then rounded in the direction $rnd.
    Before using this function you must first create $state
    by calling one of the Rgmp_randinit functions (below), then
    seed $state by calling one of the Rgmp_randseed functions.

   $state = Rgmp_randinit_default();
    Initialise $state with a default algorithm. This will be
    a compromise between speed and randomness, and is 
    recommended for applications with no special requirements.

   $state = Rgmp_randinit_mt();
    Initialize state for a Mersenne Twister algorithm. This
    algorithm is fast and has good randomness properties.

   $state = Rgmp_randinit_lc_2exp($a, $c, $m2exp);
    This function is not tested in the test suite.
    Use with caution - I often select values here that cause
    Rmpf_urandomb() to behave non-randomly.    
    Initialise $state with a linear congruential algorithm:
    X = ($a * X + $c) % 2 ** $m2exp
    The low bits in X are not very random - for this reason
    only the high half of each X is actually used.
    $c and $m2exp sre both unsigned longs.
    $a can be any one of Math::GMP, or Math::GMPz objects.
    Or it can be a string.
    If it is a string of hex digits it must be prefixed with
    either OX or Ox. If it is a string of octal digits it must
    be prefixed with 'O'. Else it is assumed to be a decimal
    integer. No other bases are allowed.

   $state = Rgmp_randinit_lc_2exp_size($ui);
    Initialise state as per Rgmp_randinit_lc_2exp. The values
    for $a, $c. and $m2exp are selected from a table, chosen
    so that $ui bits (or more) of each X will be used.

   Rgmp_randseed($state, $seed);
    $state is a reference to a gmp_randstate_t strucure (the
    return value of one of the Rgmp_randinit functions).
    $seed is the seed. It can be any one of Math::GMP, 
    or Math::GMPz objects. Or it can be a string.
    If it is a string of hex digits it must be prefixed with
    either OX or Ox. If it is a string of octal digits it must
    be prefixed with 'O'. Else it is assumed to be a decimal
    integer. No other bases are allowed.

   Rgmp_randseed_ui($state, $ui);
    Requires that one of Math::GMPz, Math::GMPq or Math::GMPf
    is loaded.
    $state is a reference to a gmp_randstate_t strucure (the
    return value of one of the Rgmp_randinit functions).
    $ui is the seed.

   #########

   INTERNALS

   $bool = Rmpfr_can_round($op, $ui, $rnd1, $rnd2, $p);
    Assuming $op is an approximation of an unknown number X in direction
    $rnd1 with error at most two to the power E(b)-$ui where E(b) is
    the exponent of $op, returns 1 if one is able to round exactly X
    to precision $p with direction $rnd2, and 0 otherwise. This
    function *does not modify* its arguments.

   $si = Rmpfr_get_exp($op);
    Get the exponent of $op, assuming that $op is a non-zero
    ordinary number.

   $si = Rmpfr_set_exp($op, $si);
    Set the exponent of $op if $si is in the current exponent 
    range, and return 0 (even if $op is not a non-zero
    ordinary number); otherwise, return a non-zero value.

   $si = Rmpfr_signbit ($op);
    Return a non-zero value iff $op has its sign bit set (i.e. if it is
    negative, -0, or a NaN whose representation has its sign bit set).

   $si2 = Rmpfr_setsign ($rop, $op, $si, $rnd);
    Set the value of $rop from $op, rounded towards the given direction
    $rnd, then set/clear its sign bit if $si is true/false (even when
    $op is a NaN).

   $si = Rmpfr_copysign ($rop, $op1, $op2, $rnd);
    Set the value of $rop from $op1, rounded towards the given direction
    $rnd, then set its sign bit to that of $op2 (even when $op1 or $op2
    is a NaN). This function is equivalent to:
    Rmpfr_setsign ($rop, $op1, Rmpfr_signbit ($op2), $rnd)'.

   ####################

   OPERATOR OVERLOADING

    Overloading works with numbers, strings (bases 2, 10, and 16
    only - see step '4.' below) and Math::MPFR objects.
    Overloaded operations are performed using the current
    "default rounding mode" (which you can determine using the
    'Rmpfr_get_default_rounding_mode' function, and change using
    the 'Rmpfr_set_default_rounding_mode' function).

    Be aware that when you use overloading with a string operand,
    the overload subroutine converts that string operand to a
    Math::MPFR object with *current default precision*, and using
    the *current default rounding mode*.

    Note that any comparison using the spaceship operator ( <=> )
    will return undef iff either/both of the operands is a NaN.
    All comparisons ( < <= > >= == != <=> ) involving one or more
    NaNs will set the erange flag.

    For the purposes of the overloaded 'not', '!' and 'bool'
    operators, a "false" Math::MPFR object is one whose value is
    either 0 (including -0) or NaN.
    (A "true" Math::MPFR object is, of course, simply one that
    is not "false".)

    The following operators are overloaded:
     + - * / ** sqrt (Return object has default precision)
     += -= *= /= **= (Precision remains unchanged)
     < <= > >= == != <=>
     ! bool
     abs atan2 cos sin log exp (Return object has default precision)
     int (On perl 5.8 only, NA on perl 5.6. The return object
          has default precision)
     = (The copy has the same precision as the copied object.)
     ""

    Attempting to use the overloaded operators with objects that
    have been blessed into some package other than 'Math::MPFR'
    will not (currently) work. It would be fun (and is tempting)
    to implement cross-class overloading - but it could also
    easily lead to user confusion and frustration, so I'll resist
    the temptation until someone convinces me that I should do
    otherwise.
    The workaround is to convert this "foreign" object to a
    format that *will* work with the overloaded operator.

    In those situations where the overload subroutine operates on 2
    perl variables, then obviously one of those perl variables is
    a Math::MPFR object. To determine the value of the other variable
    the subroutine works through the following steps (in order),
    using the first value it finds, or croaking if it gets
    to step 6:

    1. If the variable is an unsigned long then that value is used.
       The variable is considered to be an unsigned long if 
       (perl 5.8) the UOK flag is set or if (perl 5.6) SvIsUV() 
       returns true.(In the case of perls built with
       -Duse64bitint, the variable is treated as an unsigned long
       long int if the UOK flag is set.)

    2. If the variable is a signed long int, then that value is used.
       The variable is considered to be a signed long int if the
       IOK flag is set. (In the case of perls built with
       -Duse64bitint, the variable is treated as a signed long long
       int if the IOK flag is set.)

    3. If the variable is a double, then that value is used. The
       variable is considered to be a double if the NOK flag is set.
       (In the case of perls built with -Duselongdouble, the variable
       is treated as a long double if the NOK flag is set.)

    4. If the variable is a string (ie the POK flag is set) then the
       value of that string is used. If the POK flag is set, but the
       string is not a valid number, the subroutine croaks with an 
       appropriate error message. If the string starts with '0b' or
       '0B' it is regarded as a base 2 number. If it starts with '0x'
       or '0X' it is regarded as a base 16 number. Otherwise it is
       regarded as a base 10 number.

    5. If the variable is a Math::MPFR object then the value of that
       object is used.

    6. If none of the above is true, then the second variable is
       deemed to be of an invalid type. The subroutine croaks with
       an appropriate error message.

   #####################

   FORMATTED OUTPUT

   Rmpfr_printf($format_string, [$rnd,] $var);

    This function (unlike the MPFR counterpart) is limited to taking
    2 or 3 arguments - the format string, optionally a rounding argument,
    and the variable to be formatted.
    That is, you can currently printf only one variable at a time.
    If there's no variable to be formatted, just add a '0' as the final
    argument. ie this will work fine:
     Rmpfr_printf("hello world\n", 0);
    See the mpfr documentation for details re the formatting options.
    Note: Courtesy of operator overloading, you can also use perl's
    printf() function with Math::MPFR objects.

   Rmpfr_fprintf($fh, $format_string, [$rnd,] $var);

    This function (unlike the MPFR counterpart) is limited to taking
    3 or 4 arguments - the filehandle, the format string, optionally a
    rounding argument, and the variable to be formatted. That is, you
    can printf only one variable at a time.
    If there's no variable to be formatted, just add a '0' as the final
    argument. ie this will work fine:
     Rmpfr_fprintf($fh, "hello world\n", 0);
    See the mpfr documentation for details re the formatting options.

   Rmpfr_sprintf($buffer, $format_string, [$rnd,] $var);

    This function (unlike the MPFR counterpart) is limited to taking
    3 or 4 arguments - the buffer, the format string, optionally a
    rounding argument, and the variable to be formatted. $buffer must be
    large enough to accommodate the formatted string, and is truncated
    to the length of that formatted string. If you prefer to have the
    resultant string returned (rather than stored in $buffer), use
    Rmpfrf_sprintf_ret instead - which will also leave the length of
    $buffer unaltered. 
    If there's no variable to be formatted, just add a '0' as the final
    argument. ie this will work fine:
     Rmpfr_sprintf($buffer, "hello world", 0);
    See the mpfr documentation for details re the formatting options.
    Note: Courtesy of operator overloading, you can also use perl's
    sprintf() function with Math::MPFR objects.

   $string = Rmpfr_sprintf_ret($buffer, $format_string, [$rnd,] $var);

    As for Rmpfr_sprintf, but returns the formatted string, rather than
    storing it in $buffer. $buffer needs to be large enough to 
    accommodate the formatted string. The length of $buffer will be
    unaltered.
    See the mpfr documentation for details re the formatting options.
    Note: Courtesy of operator overloading, you can also use perl's
    sprintf() function with Math::MPFR objects.

   Rmpfr_snprintf($buffer, $bytes, $format_string, [$rnd,] $var);

    This function (unlike the MPFR counterpart) is limited to taking
    4 or 5 arguments - the buffer, the number of bytes to be written,
    the format string, optionally a rounding argument, and the variable
    to be formatted. $buffer must be large enough to accommodate the 
    formatted string, and is truncated to the length of that formatted
    string. If you prefer to have the resultant string returned (rather
    than stored in $buffer), use Rmpfrf_sprintf_ret instead - which will
    also leave the length of $buffer unaltered. 
    If there's no variable to be formatted, just add a '0' as the final
    argument. ie this will work fine:
     Rmpfr_snprintf($buffer, 12, "hello world", 0);
    See the mpfr documentation for further details.

   $string = Rmpfr_snprintf_ret($buffer, $bytes, $format_string, [$rnd,] $var);

    As for Rmpfr_snprintf, but returns the formatted string, rather than
    storing it in $buffer. $buffer needs to be large enough to 
    accommodate the formatted string. The length of $buffer will be
    unaltered.

   #####################

=head1 BUGS

    You can get segfaults if you pass the wrong type of
    argument to the functions - so if you get a segfault, the
    first thing to do is to check that the argument types 
    you have supplied are appropriate.

=head1 ACKNOWLEDGEMENTS

    Thanks to Vincent Lefevre for providing corrections to errors
    and omissions, and suggesting improvements (which were duly
    put in place).

=head1 LICENSE

    This program is free software; you may redistribute it and/or 
    modify it under the same terms as Perl itself.
    Copyright 2006-2008, 2009, 2010, 2011 Sisyphus

=head1 AUTHOR

    Sisyphus <sisyphus at(@) cpan dot (.) org>

=cut
