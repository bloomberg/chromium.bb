    package Math::MPC;
    use strict;
    use warnings;
    use Math::MPFR;

    use constant  MPC_RNDNN => 0;
    use constant  MPC_RNDZN => 1;
    use constant  MPC_RNDUN => 2;
    use constant  MPC_RNDDN => 3;

    use constant  MPC_RNDNZ => 16;
    use constant  MPC_RNDZZ => 17;
    use constant  MPC_RNDUZ => 18;
    use constant  MPC_RNDDZ => 19;

    use constant  MPC_RNDNU => 32;
    use constant  MPC_RNDZU => 33;
    use constant  MPC_RNDUU => 34;
    use constant  MPC_RNDDU => 35;

    use constant  MPC_RNDND => 48;
    use constant  MPC_RNDZD => 49;
    use constant  MPC_RNDUD => 50;
    use constant  MPC_RNDDD => 51;

    use constant  _UOK_T   => 1;
    use constant  _IOK_T   => 2;
    use constant  _NOK_T   => 3;
    use constant  _POK_T   => 4;
    use constant  _MATH_MPFR_T   => 5;
    use constant  _MATH_GMPf_T   => 6;
    use constant  _MATH_GMPq_T   => 7;
    use constant  _MATH_GMPz_T   => 8;
    use constant  _MATH_GMP_T    => 9;
    use constant  _MATH_MPC_T    => 10;

    use subs qw(MPC_VERSION MPC_VERSION_MAJOR MPC_VERSION_MINOR
                MPC_VERSION_PATCHLEVEL MPC_VERSION_STRING
                MPC_VERSION MPC_VERSION_NUM);

    use overload
    '+'    => \&overload_add,
    '-'    => \&overload_sub,
    '*'    => \&overload_mul,
    '/'    => \&overload_div,
    '**'   => \&overload_pow,
    '+='   => \&overload_add_eq,
    '-='   => \&overload_sub_eq,
    '*='   => \&overload_mul_eq,
    '/='   => \&overload_div_eq,
    '**='  => \&overload_pow_eq,
    '=='   => \&overload_equiv,
    '!='   => \&overload_not_equiv,
    '!'    => \&overload_not,
    '='    => \&overload_copy,
    '""'   => \&overload_string,
    'abs'  => \&overload_abs,
    'bool' => \&overload_true,
    'exp'  => \&overload_exp,
    'log'  => \&overload_log,
    'sqrt' => \&overload_sqrt,
    'sin'  => \&overload_sin,
    'cos'  => \&overload_cos,
    'atan2'=> \&overload_atan2;

    require Exporter;
    *import = \&Exporter::import;
    require DynaLoader;

    @Math::MPC::EXPORT_OK = qw(
MPC_RNDNN MPC_RNDND MPC_RNDNU MPC_RNDNZ MPC_RNDDN MPC_RNDUN MPC_RNDZN MPC_RNDDD
MPC_RNDDU MPC_RNDDZ MPC_RNDZD MPC_RNDUD MPC_RNDUU MPC_RNDUZ MPC_RNDZU MPC_RNDZZ
MPC_VERSION_MAJOR MPC_VERSION_MINOR MPC_VERSION_PATCHLEVEL MPC_VERSION_STRING
MPC_VERSION MPC_VERSION_NUM Rmpc_get_version
Rmpc_set_default_rounding_mode Rmpc_get_default_rounding_mode
Rmpc_set_prec Rmpc_set_default_prec Rmpc_get_default_prec
Rmpc_set_default_prec2 Rmpc_get_default_prec2
Rmpc_set_re_prec Rmpc_set_im_prec
Rmpc_get_prec Rmpc_get_prec2 Rmpc_get_re_prec Rmpc_get_im_prec
Rmpc_get_dc Rmpc_get_ldc
Rmpc_get_DC Rmpc_get_LDC Rmpc_get_F128C
RMPC_RE RMPC_IM RMPC_INEX_RE RMPC_INEX_IM
Rmpc_clear Rmpc_clear_ptr Rmpc_clear_mpc
Rmpc_deref4 Rmpc_get_str
Rmpc_init2 Rmpc_init3
Rmpc_init2_nobless Rmpc_init3_nobless
Rmpc_strtoc Rmpc_set_str
Rmpc_set Rmpc_set_ui Rmpc_set_si Rmpc_set_d Rmpc_set_uj Rmpc_set_sj Rmpc_set_ld
Rmpc_set_z Rmpc_set_q Rmpc_set_f Rmpc_set_fr
Rmpc_set_z_z Rmpc_set_q_q Rmpc_set_f_f
Rmpc_set_ui_ui Rmpc_set_ui_si Rmpc_set_ui_d Rmpc_set_ui_uj Rmpc_set_ui_sj Rmpc_set_ui_ld Rmpc_set_ui_fr
Rmpc_set_si_ui Rmpc_set_si_si Rmpc_set_si_d Rmpc_set_si_uj Rmpc_set_si_sj Rmpc_set_si_ld Rmpc_set_si_fr
Rmpc_set_d_ui Rmpc_set_d_si Rmpc_set_d_d Rmpc_set_d_uj Rmpc_set_d_sj Rmpc_set_d_ld Rmpc_set_d_fr
Rmpc_set_uj_ui Rmpc_set_uj_si Rmpc_set_uj_d Rmpc_set_uj_uj Rmpc_set_uj_sj Rmpc_set_uj_ld Rmpc_set_uj_ld Rmpc_set_uj_fr
Rmpc_set_sj_ui Rmpc_set_sj_si Rmpc_set_sj_d Rmpc_set_sj_uj Rmpc_set_sj_sj Rmpc_set_sj_ld Rmpc_set_sj_fr
Rmpc_set_ld_ui Rmpc_set_ld_si Rmpc_set_ld_uj Rmpc_set_ld_d Rmpc_set_ld_sj Rmpc_set_ld_ld Rmpc_set_ld_fr
Rmpc_set_fr_ui Rmpc_set_fr_si Rmpc_set_fr_d Rmpc_set_fr_uj Rmpc_set_fr_sj Rmpc_set_fr_ld Rmpc_set_fr_fr

Rmpc_set_f_ui Rmpc_set_q_ui Rmpc_set_z_ui Rmpc_set_ui_f Rmpc_set_ui_q Rmpc_set_ui_z
Rmpc_set_f_si Rmpc_set_q_si Rmpc_set_z_si Rmpc_set_si_f Rmpc_set_si_q Rmpc_set_si_z
Rmpc_set_f_d Rmpc_set_q_d Rmpc_set_z_d Rmpc_set_d_f Rmpc_set_d_q Rmpc_set_d_z
Rmpc_set_f_uj Rmpc_set_q_uj Rmpc_set_z_uj Rmpc_set_uj_f Rmpc_set_uj_q Rmpc_set_uj_z
Rmpc_set_f_sj Rmpc_set_q_sj Rmpc_set_z_sj Rmpc_set_sj_f Rmpc_set_sj_q Rmpc_set_sj_z
Rmpc_set_f_ld Rmpc_set_q_ld Rmpc_set_z_ld Rmpc_set_ld_f Rmpc_set_ld_q Rmpc_set_ld_z
Rmpc_set_f_q Rmpc_set_q_f Rmpc_set_f_z Rmpc_set_z_f Rmpc_set_z_q Rmpc_set_q_z
Rmpc_set_f_fr Rmpc_set_q_fr Rmpc_set_z_fr Rmpc_set_fr_f Rmpc_set_fr_q Rmpc_set_fr_z

Rmpc_set_dc Rmpc_set_ldc Rmpc_set_NV Rmpc_set_NV_NV
Rmpc_set_DC Rmpc_set_LDC Rmpc_set_F128C

Rmpc_add Rmpc_add_ui Rmpc_add_fr
Rmpc_sub Rmpc_sub_ui Rmpc_ui_sub Rmpc_ui_ui_sub
Rmpc_mul Rmpc_mul_ui Rmpc_mul_si Rmpc_mul_fr Rmpc_mul_i Rmpc_sqr Rmpc_mul_2exp
Rmpc_mul_2si Rmpc_mul_2ui
Rmpc_div Rmpc_div_ui Rmpc_ui_div Rmpc_div_fr Rmpc_sqrt Rmpc_div_2exp
Rmpc_div_2si Rmpc_div_2ui
Rmpc_neg Rmpc_abs Rmpc_conj Rmpc_norm Rmpc_exp Rmpc_log Rmpc_log10
Rmpc_cmp Rmpc_cmp_si Rmpc_cmp_si_si Rmpc_cmp_abs
Rmpc_out_str Rmpc_inp_str c_string r_string i_string
TRmpc_out_str TRmpc_inp_str
Rmpc_sin Rmpc_cos Rmpc_sin_cos Rmpc_tan Rmpc_sinh Rmpc_cosh Rmpc_tanh
Rmpc_asin Rmpc_acos Rmpc_atan Rmpc_asinh Rmpc_acosh Rmpc_atanh
Rmpc_real Rmpc_imag Rmpc_arg Rmpc_proj
Rmpc_pow Rmpc_pow_d Rmpc_pow_ld Rmpc_pow_si Rmpc_pow_ui Rmpc_pow_z Rmpc_pow_fr Rmpc_rootofunity
Rmpc_set_nan Rmpc_swap
Rmpc_mul_sj Rmpc_mul_ld Rmpc_mul_d Rmpc_div_sj Rmpc_sj_div Rmpc_div_ld Rmpc_ld_div Rmpc_div_d Rmpc_d_div
);

    our $VERSION = '1.08';
    #$VERSION = eval $VERSION;

    DynaLoader::bootstrap Math::MPC $VERSION;

    %Math::MPC::EXPORT_TAGS =(mpc => [qw(
MPC_RNDNN MPC_RNDND MPC_RNDNU MPC_RNDNZ MPC_RNDDN MPC_RNDUN MPC_RNDZN MPC_RNDDD
MPC_RNDDU MPC_RNDDZ MPC_RNDZD MPC_RNDUD MPC_RNDUU MPC_RNDUZ MPC_RNDZU MPC_RNDZZ
MPC_VERSION_MAJOR MPC_VERSION_MINOR MPC_VERSION_PATCHLEVEL MPC_VERSION_STRING
MPC_VERSION MPC_VERSION_NUM Rmpc_get_version
Rmpc_set_default_rounding_mode Rmpc_get_default_rounding_mode
Rmpc_set_prec Rmpc_set_default_prec Rmpc_get_default_prec
Rmpc_set_default_prec2 Rmpc_get_default_prec2
Rmpc_set_re_prec Rmpc_set_im_prec
Rmpc_get_prec Rmpc_get_prec2 Rmpc_get_re_prec Rmpc_get_im_prec
Rmpc_get_dc Rmpc_get_ldc
Rmpc_get_DC Rmpc_get_LDC Rmpc_get_F128C
RMPC_RE RMPC_IM RMPC_INEX_RE RMPC_INEX_IM
Rmpc_clear Rmpc_clear_ptr Rmpc_clear_mpc
Rmpc_deref4 Rmpc_get_str
Rmpc_init2 Rmpc_init3
Rmpc_init2_nobless Rmpc_init3_nobless
Rmpc_strtoc Rmpc_set_str
Rmpc_set Rmpc_set_ui Rmpc_set_si Rmpc_set_d Rmpc_set_uj Rmpc_set_sj Rmpc_set_ld
Rmpc_set_z Rmpc_set_q Rmpc_set_f Rmpc_set_fr
Rmpc_set_z_z Rmpc_set_q_q Rmpc_set_f_f
Rmpc_set_ui_ui Rmpc_set_ui_si Rmpc_set_ui_d Rmpc_set_ui_uj Rmpc_set_ui_sj Rmpc_set_ui_ld Rmpc_set_ui_fr
Rmpc_set_si_ui Rmpc_set_si_si Rmpc_set_si_d Rmpc_set_si_uj Rmpc_set_si_sj Rmpc_set_si_ld Rmpc_set_si_fr
Rmpc_set_d_ui Rmpc_set_d_si Rmpc_set_d_d Rmpc_set_d_uj Rmpc_set_d_sj Rmpc_set_d_ld Rmpc_set_d_fr
Rmpc_set_uj_ui Rmpc_set_uj_si Rmpc_set_uj_d Rmpc_set_uj_uj Rmpc_set_uj_sj Rmpc_set_uj_ld Rmpc_set_uj_ld Rmpc_set_uj_fr
Rmpc_set_sj_ui Rmpc_set_sj_si Rmpc_set_sj_d Rmpc_set_sj_uj Rmpc_set_sj_sj Rmpc_set_sj_ld Rmpc_set_sj_fr
Rmpc_set_ld_ui Rmpc_set_ld_si Rmpc_set_ld_uj Rmpc_set_ld_d Rmpc_set_ld_sj Rmpc_set_ld_ld Rmpc_set_ld_fr
Rmpc_set_fr_ui Rmpc_set_fr_si Rmpc_set_fr_d Rmpc_set_fr_uj Rmpc_set_fr_sj Rmpc_set_fr_ld Rmpc_set_fr_fr

Rmpc_set_f_ui Rmpc_set_q_ui Rmpc_set_z_ui Rmpc_set_ui_f Rmpc_set_ui_q Rmpc_set_ui_z
Rmpc_set_f_si Rmpc_set_q_si Rmpc_set_z_si Rmpc_set_si_f Rmpc_set_si_q Rmpc_set_si_z
Rmpc_set_f_d Rmpc_set_q_d Rmpc_set_z_d Rmpc_set_d_f Rmpc_set_d_q Rmpc_set_d_z
Rmpc_set_f_uj Rmpc_set_q_uj Rmpc_set_z_uj Rmpc_set_uj_f Rmpc_set_uj_q Rmpc_set_uj_z
Rmpc_set_f_sj Rmpc_set_q_sj Rmpc_set_z_sj Rmpc_set_sj_f Rmpc_set_sj_q Rmpc_set_sj_z
Rmpc_set_f_ld Rmpc_set_q_ld Rmpc_set_z_ld Rmpc_set_ld_f Rmpc_set_ld_q Rmpc_set_ld_z
Rmpc_set_f_q Rmpc_set_q_f Rmpc_set_f_z Rmpc_set_z_f Rmpc_set_z_q Rmpc_set_q_z
Rmpc_set_f_fr Rmpc_set_q_fr Rmpc_set_z_fr Rmpc_set_fr_f Rmpc_set_fr_q Rmpc_set_fr_z

Rmpc_set_dc Rmpc_set_ldc Rmpc_set_NV Rmpc_set_NV_NV
Rmpc_set_DC Rmpc_set_LDC Rmpc_set_F128C

Rmpc_add Rmpc_add_ui Rmpc_add_fr
Rmpc_sub Rmpc_sub_ui Rmpc_ui_sub Rmpc_ui_ui_sub
Rmpc_mul Rmpc_mul_ui Rmpc_mul_si Rmpc_mul_fr Rmpc_mul_i Rmpc_sqr Rmpc_mul_2exp
Rmpc_mul_2si Rmpc_mul_2ui
Rmpc_div Rmpc_div_ui Rmpc_ui_div Rmpc_div_fr Rmpc_sqrt Rmpc_div_2exp
Rmpc_div_2si Rmpc_div_2ui
Rmpc_neg Rmpc_abs Rmpc_conj Rmpc_norm Rmpc_exp Rmpc_log Rmpc_log10
Rmpc_cmp Rmpc_cmp_si Rmpc_cmp_si_si Rmpc_cmp_abs
Rmpc_out_str Rmpc_inp_str c_string r_string i_string
TRmpc_out_str TRmpc_inp_str
Rmpc_sin Rmpc_cos Rmpc_sin_cos Rmpc_tan Rmpc_sinh Rmpc_cosh Rmpc_tanh
Rmpc_asin Rmpc_acos Rmpc_atan Rmpc_asinh Rmpc_acosh Rmpc_atanh
Rmpc_real Rmpc_imag Rmpc_arg Rmpc_proj
Rmpc_pow Rmpc_pow_d Rmpc_pow_ld Rmpc_pow_si Rmpc_pow_ui Rmpc_pow_z Rmpc_pow_fr Rmpc_rootofunity
Rmpc_set_nan Rmpc_swap
Rmpc_mul_sj Rmpc_mul_ld Rmpc_mul_d Rmpc_div_sj Rmpc_sj_div Rmpc_div_ld Rmpc_ld_div Rmpc_div_d Rmpc_d_div
)]);

$Math::MPC::NOK_POK = 0; # Set to 1 to allow warnings in new() and overloaded operations when
                          # a scalar that has set both NOK (NV) and POK (PV) flags is encountered

eval {require Math::Complex_C::Q;};

if($@) {$Math::MPC::no_complex_c_q = $@}
else   {$Math::MPC::no_complex_c_q = 0 }

*TRmpc_out_str = \&Rmpc_out_str;
*TRmpc_inp_str = \&Rmpc_inp_str;

*Rmpc_set_uj_si = \&Rmpc_set_uj_sj;
*Rmpc_set_ui_sj = \&Rmpc_set_uj_sj;

*Rmpc_set_sj_ui = \&Rmpc_set_sj_uj;
*Rmpc_set_si_uj = \&Rmpc_set_sj_uj;

*Rmpc_set_uj_ui = \&Rmpc_set_uj_uj;
*Rmpc_set_ui_uj = \&Rmpc_set_uj_uj;

*Rmpc_set_sj_si = \&Rmpc_set_sj_sj;
*Rmpc_set_si_sj = \&Rmpc_set_sj_sj;

*Rmpc_set_d_ld  = \&Rmpc_set_ld_ld;
*Rmpc_set_ld_d  = \&Rmpc_set_ld_ld;

# Beginning with mpc-1.0, mpc_mul_2exp and mpc_div_2exp
# were renamed to mpc_mul_2ui and mpc_div_2ui.
*Rmpc_mul_2exp = \&Rmpc_mul_2ui;
*Rmpc_div_2exp = \&Rmpc_div_2ui;

sub dl_load_flags {0} # Prevent DynaLoader from complaining and croaking

sub overload_not_equiv {
    return 0 if overload_equiv($_[0], $_[1], $_[2]);
    return 1;
}

sub overload_string {
     return "(" . _get_str($_[0], 10, 0, Rmpc_get_default_rounding_mode()) . ")";

}

### Was originally called Rmpc_get_str ###
sub _get_str {
    my ($r_s, $i_s) = c_string($_[0], $_[1], $_[2], $_[3]);
    # Changed to stay in step with change to mpc_out_str() format
    #my $sep = $i_s =~ /\-/ ? ' -I*' : ' +I*';
    #$i_s =~ s/\-//;
    #my $s = $r_s . $sep . $i_s;
    #return $s;
    return $r_s . " " . $i_s;
}

sub c_string {
    my $r_s = r_string($_[0], $_[1], $_[2], $_[3]);
    my $i_s = i_string($_[0], $_[1], $_[2], $_[3]);
    return ($r_s, $i_s);
}

sub r_string {
    my($mantissa, $exponent) = _get_r_string($_[0], $_[1], $_[2], $_[3]);
    if($mantissa =~ /\@nan\@/i || $mantissa =~ /\@inf\@/i) {return $mantissa}
    if($mantissa =~ /\-/ && $mantissa !~ /[^0,\-]/) {return '-0'}
    if($mantissa !~ /[^0,\-]/ ) {return '0'}
    my $sep = $_[1] <= 10 ? 'e' : '@';

    my $len = substr($mantissa, 0, 1) eq '-' ? 2 : 1;

    if(!$_[2]) {
      while(length($mantissa) > $len && substr($mantissa, -1, 1) eq '0') {
           substr($mantissa, -1, 1, '');
      }
    }

    $exponent--;

    if(length($mantissa) == $len) {
      if($exponent) {return $mantissa . $sep . $exponent}
      return $mantissa;
    }

    substr($mantissa, $len, 0, '.');
    if($exponent) {return $mantissa . $sep . $exponent}
    return $mantissa;
}

sub i_string {
    my($mantissa, $exponent) = _get_i_string($_[0], $_[1], $_[2], $_[3]);
    if($mantissa =~ /\@nan\@/i || $mantissa =~ /\@inf\@/i) {return $mantissa}
    if($mantissa =~ /\-/ && $mantissa !~ /[^0,\-]/) {return '-0'}
    if($mantissa !~ /[^0,\-]/ ) {return '0'}

    my $sep = $_[1] <= 10 ? 'e' : '@';

    my $len = substr($mantissa, 0, 1) eq '-' ? 2 : 1;

    if(!$_[2]) {
      while(length($mantissa) > $len && substr($mantissa, -1, 1) eq '0') {
           substr($mantissa, -1, 1, '');
      }
    }

    $exponent--;

    if(length($mantissa) == $len) {
      if($exponent) {return $mantissa . $sep . $exponent}
      return $mantissa;
    }

    substr($mantissa, $len, 0, '.');
    if($exponent) {return $mantissa . $sep . $exponent}
    return $mantissa;
}

sub Rmpc_deref4 {
    my ($r_m, $r_e) = _get_r_string($_[0], $_[1], $_[2], $_[3]);
    my ($i_m, $i_e) = _get_i_string($_[0], $_[1], $_[2], $_[3]);
    return ($r_m, $r_e, $i_m, $i_e);
}

sub Rmpc_set_F128C {
  if($Math::MPC::no_complex_c_q) {die "In Rmpc_set_F128C(): $Math::MPC::no_complex_c_q"}
  my $t_re = Math::MPFR::Rmpfr_init2(113);
  my $t_im = Math::MPFR::Rmpfr_init2(113);
  Math::MPFR::Rmpfr_set_float128($t_re, Math::Complex_C::Q::real_cq($_[1]), 0); # Round to nearest
  Math::MPFR::Rmpfr_set_float128($t_im, Math::Complex_C::Q::imag_cq($_[1]), 0);   # Round to nearest
  return Rmpc_set_fr_fr($_[0], $t_re, $t_im, $_[2]); # Now use specified rounding mode
}

sub Rmpc_get_F128C {
  if($Math::MPC::no_complex_c_q) {die "In Rmpc_set_F128C(): $Math::MPC::no_complex_c_q"}
  my $t = Math::MPFR::Rmpfr_init2(113);
  RMPC_RE($t, $_[1]);
  Math::Complex_C::Q::set_real_cq($_[0], Math::MPFR::Rmpfr_get_float128($t, $_[2] & 3));
  RMPC_IM($t, $_[1]);
  Math::Complex_C::Q::set_imag_cq($_[0], Math::MPFR::Rmpfr_get_float128($t, $_[2] / 16));
}


sub new {

    # This function caters for 2 possibilities:
    # 1) that 'new' has been called OOP style - in which
    #    case there will be a maximum of 3 args
    # 2) that 'new' has been called as a function - in
    #    which case there will be a maximum of 2 args.
    # If there are no args, then we just want to return an
    # initialized Math::MPC object
    my @prec = Rmpc_get_default_prec2();
    if(!@_) {return Rmpc_init3(@prec)}

    if(@_ > 3) {die "Too many arguments supplied to new()"}

    # If 'new' has been called OOP style, the first arg is the string "Math::MPC"
    # which we don't need - so let's remove it. However, if the first
    # arg is a Math::MPFR or Math::MPC object (which is a possibility),
    # then we'll get a fatal error when we check it for equivalence to
    # the string "Math::MPC". So we first need to check that it's not
    # an object - which we'll do by using the ref() function:
    if(!ref($_[0]) && $_[0] eq "Math::MPC") {
      shift;
      if(!@_) {return Rmpc_init3(@prec)}
      }

    if(_itsa($_[0]) == _MATH_MPC_T) {
      if(@_ > 1) {die "Too many arguments supplied to new() - expected no more than one"}
      my $mpc = Rmpc_init3(@prec);
      Rmpc_set($mpc, $_[0], Rmpc_get_default_rounding_mode());
      return $mpc;
    }

    # @_ can now contain a maximum of 2 args - the real and (optionally)
    # the imaginary components.
    if(@_ > 2) {die "Too many arguments supplied to new() - expected no more than two"}

    my ($arg1, $arg2, $type1, $type2);

    # $_[0] is the real component, $_[1] (if supplied)
    # is the imaginary component.
    $arg1 = shift;
    $type1 = _itsa($arg1);

    $arg2 = 0;
    if(@_) {$arg2 = shift}
    $type2 = _itsa($arg2);

    # Die if either of the args are unacceptable.
    if($type1 == 0)
      {die "First argument to new() is inappropriate"}
    if($type2 == 0)
      {die "Second argument to new() is inappropriate"}

    # Return a Math::MPC object that has $arg1 as its
    # real component, and $arg2 as its imaginary component.
    return _new_real_im($arg1, $arg2);
}

sub Rmpc_out_str {
    if(@_ == 5) {
      die "Inappropriate 4th arg supplied to Rmpc_out_str" if _itsa($_[3]) != _MATH_MPC_T;
      return _Rmpc_out_str($_[0], $_[1], $_[2], $_[3], $_[4]);
    }
    if(@_ == 6) {
      if(_itsa($_[3]) == _MATH_MPC_T) {return _Rmpc_out_strS($_[0], $_[1], $_[2], $_[3], $_[4], $_[5])}
      die "Incorrect args supplied to Rmpc_out_str" if _itsa($_[4]) != _MATH_MPC_T;
      return _Rmpc_out_strP($_[0], $_[1], $_[2], $_[3], $_[4], $_[5]);
    }
    if(@_ == 7) {
      die "Inappropriate 5th arg supplied to Rmpc_out_str" if _itsa($_[4]) != _MATH_MPC_T;
      return _Rmpc_out_strPS($_[0], $_[1], $_[2], $_[3], $_[4], $_[5], $_[6]);
    }
    die "Wrong number of arguments supplied to Rmpc_out_str()";
}

sub MPC_VERSION {return _MPC_VERSION()}
sub MPC_VERSION_MAJOR {return _MPC_VERSION_MAJOR()}
sub MPC_VERSION_MINOR {return _MPC_VERSION_MINOR()}
sub MPC_VERSION_PATCHLEVEL {return _MPC_VERSION_PATCHLEVEL()}
sub MPC_VERSION_STRING {return _MPC_VERSION_STRING()}
sub MPC_VERSION_NUM {return _MPC_VERSION_NUM(@_)}

1;

__END__

