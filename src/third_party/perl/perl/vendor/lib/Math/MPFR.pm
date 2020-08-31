    package Math::MPFR;
    use strict;
    use warnings;
    use POSIX;
    use Config;
    use Math::MPFR::Prec;
    use Math::MPFR::Random;

    use constant  GMP_RNDN              => 0;
    use constant  GMP_RNDZ              => 1;
    use constant  GMP_RNDU              => 2;
    use constant  GMP_RNDD              => 3;
    use constant  MPFR_RNDN             => 0;
    use constant  MPFR_RNDZ             => 1;
    use constant  MPFR_RNDU             => 2;
    use constant  MPFR_RNDD             => 3;
    use constant  MPFR_RNDA             => 4;
    use constant  MPFR_RNDF             => 5;
    use constant  _UOK_T                => 1;
    use constant  _IOK_T                => 2;
    use constant  _NOK_T                => 3;
    use constant  _POK_T                => 4;
    use constant  _MATH_MPFR_T          => 5;
    use constant  _MATH_GMPf_T          => 6;
    use constant  _MATH_GMPq_T          => 7;
    use constant  _MATH_GMPz_T          => 8;
    use constant  _MATH_GMP_T           => 9;
    use constant  _MATH_MPC_T           => 10;
    use constant MPFR_FLAGS_UNDERFLOW   => 1;
    use constant MPFR_FLAGS_OVERFLOW    => 2;
    use constant MPFR_FLAGS_NAN         => 4;
    use constant MPFR_FLAGS_INEXACT     => 8;
    use constant MPFR_FLAGS_ERANGE      => 16;
    use constant MPFR_FLAGS_DIVBY0      => 32;
    use constant MPFR_FLAGS_ALL         => 63;
    use constant MPFR_FREE_LOCAL_CACHE  => 1;
    use constant MPFR_FREE_GLOBAL_CACHE => 2;


    use subs qw(MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
                MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING
                RMPFR_PREC_MIN RMPFR_PREC_MAX
                MPFR_DBL_DIG MPFR_LDBL_DIG MPFR_FLT128_DIG
                GMP_LIMB_BITS GMP_NAIL_BITS
                );

    use overload
    '++'   => \&overload_inc,
    '--'   => \&overload_dec,
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
MPFR_RNDN MPFR_RNDZ MPFR_RNDU MPFR_RNDD MPFR_RNDA MPFR_RNDF
MPFR_FLAGS_UNDERFLOW MPFR_FLAGS_OVERFLOW MPFR_FLAGS_DIVBY0 MPFR_FLAGS_NAN
MPFR_FLAGS_INEXACT MPFR_FLAGS_ERANGE MPFR_FLAGS_ALL
MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING RMPFR_VERSION_NUM
RMPFR_PREC_MIN RMPFR_PREC_MAX
MPFR_FREE_LOCAL_CACHE MPFR_FREE_GLOBAL_CACHE
Rmpfr_randclear Rmpfr_randinit_mt
Rmpfr_randinit_default Rmpfr_randinit_lc_2exp
Rmpfr_randinit_lc_2exp_size Rmpfr_randseed Rmpfr_randseed_ui
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
Rmpfr_q_div Rmpfr_z_div
Rmpfr_eq Rmpfr_equal_p Rmpfr_erangeflag_p Rmpfr_erf Rmpfr_erfc Rmpfr_exp
Rmpfr_exp10 Rmpfr_exp2 Rmpfr_expm1 Rmpfr_fac_ui
Rmpfr_fits_intmax_p Rmpfr_fits_sint_p Rmpfr_fits_slong_p
Rmpfr_fits_sshort_p Rmpfr_fits_uint_p Rmpfr_fits_uintmax_p
Rmpfr_fits_ulong_p Rmpfr_fits_IV_p Rmpfr_fits_UV_p
Rmpfr_fits_ushort_p Rmpfr_floor Rmpfr_fma Rmpfr_frac Rmpfr_gamma
Rmpfr_free_cache Rmpfr_free_cache2 Rmpfr_free_pool
Rmpfr_get_d Rmpfr_get_IV Rmpfr_get_UV Rmpfr_get_NV
Rmpfr_get_d_2exp Rmpfr_get_d1 Rmpfr_get_default_prec Rmpfr_get_default_rounding_mode
Rmpfr_get_emax Rmpfr_get_emax_max Rmpfr_get_emax_min Rmpfr_get_emin
Rmpfr_get_emin_max Rmpfr_get_emin_min Rmpfr_get_exp Rmpfr_get_f Rmpfr_get_ld Rmpfr_get_LD
Rmpfr_get_ld_2exp
Rmpfr_get_prec Rmpfr_get_si Rmpfr_get_sj Rmpfr_get_str Rmpfr_get_ui
Rmpfr_get_uj Rmpfr_get_q
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
Rmpfr_print_rnd_mode
Rmpfr_random2 Rmpfr_reldiff Rmpfr_rint Rmpfr_rint_ceil
Rmpfr_rint_floor Rmpfr_rint_round Rmpfr_rint_trunc Rmpfr_root Rmpfr_rootn_ui Rmpfr_round
Rmpfr_sec Rmpfr_sech Rmpfr_set Rmpfr_set_d Rmpfr_set_default_prec
Rmpfr_set_default_rounding_mode Rmpfr_set_emax Rmpfr_set_emin Rmpfr_set_erangeflag
Rmpfr_set_exp Rmpfr_set_f Rmpfr_set_inexflag Rmpfr_set_inf Rmpfr_set_ld Rmpfr_set_LD
Rmpfr_set_NV Rmpfr_cmp_NV
Rmpfr_set_nan Rmpfr_set_nanflag Rmpfr_set_overflow Rmpfr_set_prec
Rmpfr_set_prec_raw Rmpfr_set_q Rmpfr_set_si Rmpfr_set_si_2exp Rmpfr_set_sj
Rmpfr_set_sj_2exp Rmpfr_set_str Rmpfr_set_ui Rmpfr_set_ui_2exp
Rmpfr_set_uj Rmpfr_set_uj_2exp
Rmpfr_set_DECIMAL64 Rmpfr_get_DECIMAL64 Rmpfr_set_float128 Rmpfr_get_float128
Rmpfr_set_FLOAT128 Rmpfr_get_FLOAT128 Rmpfr_set_DECIMAL128 Rmpfr_get_DECIMAL128
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
Rmpfr_rec_sqrt Rmpfr_rec_root Rmpfr_li2 Rmpfr_modf Rmpfr_fmod
Rmpfr_printf Rmpfr_fprintf Rmpfr_sprintf Rmpfr_snprintf
Rmpfr_buildopt_tls_p Rmpfr_buildopt_decimal_p Rmpfr_regular_p Rmpfr_set_zero Rmpfr_digamma
Rmpfr_ai Rmpfr_set_flt Rmpfr_get_flt Rmpfr_urandom Rmpfr_set_z_2exp
Rmpfr_set_divby0 Rmpfr_clear_divby0 Rmpfr_divby0_p
Rmpfr_buildopt_tune_case Rmpfr_frexp Rmpfr_grandom Rmpfr_z_sub Rmpfr_buildopt_gmpinternals_p
Rmpfr_buildopt_float128_p Rmpfr_buildopt_sharedcache_p prec_cast bytes
MPFR_DBL_DIG MPFR_LDBL_DIG MPFR_FLT128_DIG
mpfr_max_orig_len mpfr_min_inter_prec mpfr_min_inter_base mpfr_max_orig_base
Rmpfr_fmodquo Rmpfr_fpif_export Rmpfr_fpif_import Rmpfr_flags_clear Rmpfr_flags_set
Rmpfr_flags_test Rmpfr_flags_save Rmpfr_flags_restore Rmpfr_rint_roundeven Rmpfr_roundeven
Rmpfr_nrandom Rmpfr_erandom Rmpfr_fmma Rmpfr_fmms Rmpfr_log_ui Rmpfr_gamma_inc Rmpfr_beta
Rmpfr_round_nearest_away rndna
atonv nvtoa atodouble Rmpfr_dot Rmpfr_get_str_ndigits
);

    our $VERSION = '4.11';
    #$VERSION = eval $VERSION;

    DynaLoader::bootstrap Math::MPFR $VERSION;

    %Math::MPFR::EXPORT_TAGS =(mpfr => [qw(
GMP_RNDN GMP_RNDZ GMP_RNDU GMP_RNDD
MPFR_RNDN MPFR_RNDZ MPFR_RNDU MPFR_RNDD MPFR_RNDA MPFR_RNDF
MPFR_FLAGS_UNDERFLOW MPFR_FLAGS_OVERFLOW MPFR_FLAGS_DIVBY0 MPFR_FLAGS_NAN
MPFR_FLAGS_INEXACT MPFR_FLAGS_ERANGE MPFR_FLAGS_ALL
MPFR_VERSION MPFR_VERSION_MAJOR MPFR_VERSION_MINOR
MPFR_VERSION_PATCHLEVEL MPFR_VERSION_STRING RMPFR_VERSION_NUM
RMPFR_PREC_MIN RMPFR_PREC_MAX
MPFR_FREE_LOCAL_CACHE MPFR_FREE_GLOBAL_CACHE
Rmpfr_randclear Rmpfr_randinit_mt
Rmpfr_randinit_default Rmpfr_randinit_lc_2exp
Rmpfr_randinit_lc_2exp_size Rmpfr_randseed Rmpfr_randseed_ui
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
Rmpfr_q_div Rmpfr_z_div
Rmpfr_eq Rmpfr_equal_p Rmpfr_erangeflag_p Rmpfr_erf Rmpfr_erfc Rmpfr_exp
Rmpfr_exp10 Rmpfr_exp2 Rmpfr_expm1 Rmpfr_fac_ui
Rmpfr_fits_intmax_p Rmpfr_fits_sint_p Rmpfr_fits_slong_p
Rmpfr_fits_sshort_p Rmpfr_fits_uint_p Rmpfr_fits_uintmax_p
Rmpfr_fits_ulong_p Rmpfr_fits_IV_p Rmpfr_fits_UV_p
Rmpfr_fits_ushort_p Rmpfr_floor Rmpfr_fma Rmpfr_frac Rmpfr_gamma
Rmpfr_free_cache Rmpfr_free_cache2 Rmpfr_free_pool
Rmpfr_get_d Rmpfr_get_IV Rmpfr_get_UV Rmpfr_get_NV
Rmpfr_get_d_2exp Rmpfr_get_d1 Rmpfr_get_default_prec Rmpfr_get_default_rounding_mode
Rmpfr_get_emax Rmpfr_get_emax_max Rmpfr_get_emax_min Rmpfr_get_emin
Rmpfr_get_emin_max Rmpfr_get_emin_min Rmpfr_get_exp Rmpfr_get_f Rmpfr_get_ld Rmpfr_get_LD
Rmpfr_get_ld_2exp
Rmpfr_get_prec Rmpfr_get_si Rmpfr_get_sj Rmpfr_get_str Rmpfr_get_ui
Rmpfr_get_uj
Rmpfr_get_version Rmpfr_get_z Rmpfr_get_z_exp
Rmpfr_get_z_2exp Rmpfr_greater_p Rmpfr_greaterequal_p
Rmpfr_hypot Rmpfr_get_q
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
Rmpfr_print_rnd_mode
Rmpfr_random2 Rmpfr_reldiff Rmpfr_rint Rmpfr_rint_ceil
Rmpfr_rint_floor Rmpfr_rint_round Rmpfr_rint_trunc Rmpfr_root Rmpfr_rootn_ui Rmpfr_round
Rmpfr_sec Rmpfr_sech Rmpfr_set Rmpfr_set_d Rmpfr_set_default_prec
Rmpfr_set_default_rounding_mode Rmpfr_set_emax Rmpfr_set_emin Rmpfr_set_erangeflag
Rmpfr_set_exp Rmpfr_set_f Rmpfr_set_inexflag Rmpfr_set_inf Rmpfr_set_ld Rmpfr_set_LD
Rmpfr_set_NV Rmpfr_cmp_NV
Rmpfr_set_nan Rmpfr_set_nanflag Rmpfr_set_overflow Rmpfr_set_prec
Rmpfr_set_prec_raw Rmpfr_set_q Rmpfr_set_si Rmpfr_set_si_2exp Rmpfr_set_sj
Rmpfr_set_sj_2exp Rmpfr_set_str Rmpfr_set_ui Rmpfr_set_ui_2exp
Rmpfr_set_uj Rmpfr_set_uj_2exp
Rmpfr_set_DECIMAL64 Rmpfr_get_DECIMAL64 Rmpfr_set_float128 Rmpfr_get_float128
Rmpfr_set_FLOAT128 Rmpfr_get_FLOAT128 Rmpfr_set_DECIMAL128 Rmpfr_get_DECIMAL128
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
Rmpfr_rec_sqrt Rmpfr_rec_root Rmpfr_li2 Rmpfr_modf Rmpfr_fmod
Rmpfr_printf Rmpfr_fprintf Rmpfr_sprintf Rmpfr_snprintf
Rmpfr_buildopt_tls_p Rmpfr_buildopt_decimal_p Rmpfr_regular_p Rmpfr_set_zero Rmpfr_digamma
Rmpfr_ai Rmpfr_set_flt Rmpfr_get_flt Rmpfr_urandom Rmpfr_set_z_2exp
Rmpfr_set_divby0 Rmpfr_clear_divby0 Rmpfr_divby0_p
Rmpfr_buildopt_tune_case Rmpfr_frexp Rmpfr_grandom Rmpfr_z_sub Rmpfr_buildopt_gmpinternals_p
Rmpfr_buildopt_float128_p Rmpfr_buildopt_sharedcache_p prec_cast
MPFR_DBL_DIG MPFR_LDBL_DIG MPFR_FLT128_DIG
mpfr_max_orig_len mpfr_min_inter_prec mpfr_min_inter_base mpfr_max_orig_base
Rmpfr_fmodquo Rmpfr_fpif_export Rmpfr_fpif_import Rmpfr_flags_clear Rmpfr_flags_set
Rmpfr_flags_test Rmpfr_flags_save Rmpfr_flags_restore Rmpfr_rint_roundeven Rmpfr_roundeven
Rmpfr_nrandom Rmpfr_erandom Rmpfr_fmma Rmpfr_fmms Rmpfr_log_ui Rmpfr_gamma_inc Rmpfr_beta
Rmpfr_round_nearest_away rndna
atonv nvtoa atodouble Rmpfr_dot Rmpfr_get_str_ndigits
)]);


$Math::MPFR::NNW = 0; # Set to 1 to allow "non-numeric" warnings for operations involving
                      # strings that contain non-numeric characters.

$Math::MPFR::NOK_POK = 0; # Set to 1 to allow warnings in new() and overloaded operations when
                          # a scalar that has set both NOK (NV) and POK (PV) flags is encountered

%Math::MPFR::NV_properties = _get_NV_properties();

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

    # @_ can now contain a maximum of 2 args - the value, and if the value is
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
      my $ret = Rmpfr_init();
      Rmpfr_set_NV($ret, $arg1, Rmpfr_get_default_rounding_mode());
      return $ret;
    }

    if($type == _POK_T) {
      if(@_ > 1) {die "Too many arguments supplied to new() - expected no more than two"}
      if(_SvNOK($arg1)) {
        set_nok_pok(nok_pokflag() + 1);
        if($Math::MPFR::NOK_POK) {
          warn "Scalar passed to new() is both NV and PV. Using PV (string) value";
        }
      }
      $base = shift if @_;
      if($base < 0 || $base == 1 || $base > 36) {die "Invalid value for base"}
      @ret = Rmpfr_init_set_str($arg1, $base, Rmpfr_get_default_rounding_mode());
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
    if(@_ == 5){
      $len = wrap_mpfr_sprintf_rnd(@_);
      return $len;
    }
    die "Rmpfr_sprintf must take 4 or 5 arguments: buffer, format string, [rounding,], variable and buffer size" if @_ != 4;
    $len = wrap_mpfr_sprintf(@_);
    return $len;
}

sub Rmpfr_snprintf {
    my $len;
    if(@_ == 6){
      $len = wrap_mpfr_snprintf_rnd(@_);
      return $len;
    }
    die "Rmpfr_snprintf must take 5 or 6 arguments: buffer, bytes written, format string, [rounding,], variable and buffer size" if @_ != 5;
    $len = wrap_mpfr_snprintf(@_);
    return $len;
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

sub MPFR_VERSION            () {return _MPFR_VERSION()}
sub MPFR_VERSION_MAJOR      () {return _MPFR_VERSION_MAJOR()}
sub MPFR_VERSION_MINOR      () {return _MPFR_VERSION_MINOR()}
sub MPFR_VERSION_PATCHLEVEL () {return _MPFR_VERSION_PATCHLEVEL()}
sub MPFR_VERSION_STRING     () {return _MPFR_VERSION_STRING()}
sub MPFR_DBL_DIG            () {return _DBL_DIG()}
sub MPFR_LDBL_DIG           () {return _LDBL_DIG()}
sub MPFR_FLT128_DIG         () {return _FLT128_DIG()}
sub GMP_LIMB_BITS           () {return _GMP_LIMB_BITS()}
sub GMP_NAIL_BITS           () {return _GMP_NAIL_BITS()}

sub mpfr_min_inter_prec {
    die "Wrong number of args to minimum_intermediate_prec()" if @_ != 3;
    my $orig_base = shift;
    my $orig_length = shift;
    my $to_base = shift;
    return ceil(1 + ($orig_length * log($orig_base) / log($to_base)));
}

sub mpfr_min_inter_base {
    die "Wrong number of args to minimum_intermediate_base()" if @_ != 3;
    my $orig_base = shift;
    my $orig_length = shift;
    my $to_prec = shift;
    return ceil(exp($orig_length * log($orig_base) / ($to_prec - 1)));
}

sub mpfr_max_orig_len {
    die "Wrong number of args to maximum_orig_length()" if @_ != 3;
    my $orig_base = shift;
    my $to_base = shift;
    my $to_prec = shift;
    return floor(1 / (log($orig_base) / log($to_base) / ($to_prec - 1)));
}

sub mpfr_max_orig_base {
    die "Wrong number of args to maximum_orig_base()" if @_ != 3;
    my $orig_length = shift;
    my $to_base = shift;
    my $to_prec = shift;
    return floor(exp(1 / ($orig_length / log($to_base) / ($to_prec -1))));
}

sub bytes {
  my($val, $type, $ret) = (shift, shift);
  my $itsa = _itsa($val);
  die "1st arg to Math::MPFR::bytes must be either a string or a Math::MPFR object"
    if($itsa != 4 && $itsa != 5);

  if(lc($type) eq 'double') {
    $ret = $itsa == 4 ? join '', _d_bytes   ($val, 53)
                      : join '', _d_bytes_fr($val, 53);
    return $ret;
  }

  if(lc($type) eq 'long double') {
    $ret = $itsa == 4 ? join '', _ld_bytes   ($val, 64)
                      : join '', _ld_bytes_fr($val, 64);
    return $ret;
  }

  if(lc($type) eq 'ieee long double') {
    $ret = $itsa == 4 ? join '', _ld_bytes   ($val, 113)
                      : join '', _ld_bytes_fr($val, 113);
    return $ret;
  }

  if(lc($type) eq 'double-double') {
    $ret = $itsa == 4 ? join '', _dd_bytes   ($val, 106)
                      : join '', _dd_bytes_fr($val, 106);
    return $ret;
  }

  if(lc($type) eq '__float128') {
    $ret = $itsa == 4 ? join '', _f128_bytes   ($val, 113)
                      : join '', _f128_bytes_fr($val, 113);
    return $ret;
  }

  die "2nd arg to Math::MPFR::bytes must be (case-insensitive) either 'double', 'double-double', 'long double' or '__float128'";
}

sub rndna {
  my $coderef = shift;
  my $rop = shift;
  my $big_prec = Rmpfr_get_prec($rop) + 1;
  my $ret;

  if($coderef == \&Rmpfr_prec_round) {
    my $temp = Rmpfr_init2($big_prec); # need a temp object
    Rmpfr_set($temp, $rop, MPFR_RNDN);
    $ret = Rmpfr_prec_round($temp, $_[0] + 1, MPFR_RNDN);

    if(!$ret) {return Rmpfr_prec_round($rop, $_[0], MPFR_RNDA)}
    return Rmpfr_prec_round($rop, $_[0], MPFR_RNDN);
  }

  Rmpfr_prec_round($rop, $big_prec, MPFR_RNDN);
  $ret =  $coderef->($rop, @_, MPFR_RNDN);

  if($ret) { # not a midpoint value
    Rmpfr_prec_round($rop, $big_prec - 1, $ret < 0 ? MPFR_RNDA : MPFR_RNDZ);
    return $ret;
  }

  if(_lsb($rop) == 0) {
    Rmpfr_prec_round($rop, $big_prec - 1, MPFR_RNDZ);
    return 0;
  }

  return Rmpfr_prec_round($rop, $big_prec - 1, MPFR_RNDA);
}

sub Rmpfr_round_nearest_away {
  my $coderef = shift;
  my $rop = shift;
  my $big_prec = Rmpfr_get_prec($rop) + 1;
  my $ret;

  my $emin = Rmpfr_get_emin();

  if($emin <= Rmpfr_get_emin_min()) {
    warn "\n Rmpfr_round_nearest_away requires that emin ($emin)\n",
         " be greater than or equal to emin_min (", Rmpfr_get_emin_min(), ")\n";
    die " You need to set emin (using Rmpfr_set_emin()) accordingly";
  }

  Rmpfr_set_emin($emin - 1);

  if($coderef == \&Rmpfr_prec_round) {
    my $temp = Rmpfr_init2($big_prec); # need a temp object
    Rmpfr_set($temp, $rop, MPFR_RNDN);
    $ret = Rmpfr_prec_round($temp, $_[0] + 1, MPFR_RNDN);

    if(!$ret) {
      $ret = Rmpfr_prec_round($rop, $_[0], MPFR_RNDA);
      Rmpfr_set_emin($emin);
      return $ret;
    }
    $ret = Rmpfr_prec_round($rop, $_[0], MPFR_RNDN);
    Rmpfr_set_emin($emin);
    return $ret;
  }

  Rmpfr_prec_round($rop, $big_prec, MPFR_RNDN);
  $ret =  $coderef->($rop, @_, MPFR_RNDN);

  if($ret) { # not a midpoint value
    Rmpfr_prec_round($rop, $big_prec - 1, $ret < 0 ? MPFR_RNDA : MPFR_RNDZ);
    Rmpfr_set_emin($emin);
    return $ret;
  }

  my $nuisance = Rmpfr_init();
  Rmpfr_set_ui ($nuisance, 2, MPFR_RNDD);
  Rmpfr_pow_si ($nuisance, $nuisance, Rmpfr_get_emin(), MPFR_RNDD);
  Rmpfr_div_2ui($nuisance, $nuisance, 1, MPFR_RNDD);

  if(abs($rop) == $nuisance) {
    Rmpfr_mul_ui($rop, $rop, 2, MPFR_RNDD);
    Rmpfr_set_emin($emin);
    return (Rmpfr_signbit($rop) ? -1 : 1);
  }

  if(_lsb($rop) == 0) {
    Rmpfr_prec_round($rop, $big_prec - 1, MPFR_RNDZ);
    Rmpfr_set_emin($emin);
    return 0;
  }

  $ret = Rmpfr_prec_round($rop, $big_prec - 1, MPFR_RNDA);
  Rmpfr_set_emin($emin);
  return $ret;
}

sub _get_NV_properties {

  my($bits, $PREC, $max_dig, $min_pow, $normal_min, $NV_MAX, $nvtype, $emax, $emin);

  if   ($Config{nvtype} eq 'double')     {
    $bits = 53;  $PREC = 64;  $max_dig = 17; $min_pow = -1074;
    $normal_min = 2 ** -1022; $NV_MAX = POSIX::DBL_MAX; $emin = -1073; $emax = 1024;
  }

  elsif($Config{nvtype} eq '__float128') {
    $bits = 113; $PREC = 128; $max_dig = 36; $min_pow = -16494; $normal_min = 2 ** -16382;
    $NV_MAX = 1.18973149535723176508575932662800702e+4932; $emin = -16493; $emax = 16384;
  }

  elsif($Config{nvtype} eq 'long double') {

    if(_required_ldbl_mant_dig() == 53)      {
      $bits = 53;  $PREC = 64;  $max_dig = 17; $min_pow = -1074;
      $normal_min = 2 ** -1022; $NV_MAX = POSIX::DBL_MAX; $emin = -1073; $emax = 1024;
    }

    elsif(_required_ldbl_mant_dig() == 113)  {
      $bits = 113; $PREC = 128; $max_dig = 36; $min_pow = -16494;
      $normal_min = 2 ** -16382; $NV_MAX = POSIX::LDBL_MAX; $emin = -16493; $emax = 16384;
    }

    elsif(_required_ldbl_mant_dig() == 64)   {
      $bits = 64;  $PREC = 80;  $max_dig = 21; $min_pow = -16445;
      $normal_min = 2 ** -16382; $NV_MAX = POSIX::LDBL_MAX; $emin = -16444; $emax = 16384;
    }

    elsif(_required_ldbl_mant_dig() == 2098) {
      $bits = 2098;  $PREC = 2104;  $max_dig = 33; $min_pow = -1074;
      $normal_min = 2 ** -1022; $NV_MAX = POSIX::LDBL_MAX; $emin = -1073; $emax = 1024;
    }

    else {
      my %properties = ('type' => 'unknown long double type');
      return %properties;
    }
  }
  else {
      my %properties = ('type' => 'unknown nv type');
      return %properties;
  }

  my %properties = (
    'bits'       => $bits,
    'PREC'       => $PREC,
    'max_dig'    => $max_dig,
    'min_pow'    => $min_pow,
    'normal_min' => $normal_min,
    'NV_MAX'     => $NV_MAX,
    'emin'       => $emin,
    'emax'       => $emax,
                   );

  return %properties;
}


*Rmpfr_get_z_exp             = \&Rmpfr_get_z_2exp;
*prec_cast                   = \&Math::MPFR::Prec::prec_cast;
*Rmpfr_randinit_default      = \&Math::MPFR::Random::Rmpfr_randinit_default;
*Rmpfr_randinit_mt           = \&Math::MPFR::Random::Rmpfr_randinit_mt;
*Rmpfr_randinit_lc_2exp      = \&Math::MPFR::Random::Rmpfr_randinit_lc_2exp;
*Rmpfr_randinit_lc_2exp_size = \&Math::MPFR::Random::Rmpfr_randinit_lc_2exp_size;

1;

__END__
