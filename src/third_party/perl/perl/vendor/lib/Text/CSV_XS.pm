package Text::CSV_XS;

# Copyright (c) 2007-2019 H.Merijn Brand.  All rights reserved.
# Copyright (c) 1998-2001 Jochen Wiedmann. All rights reserved.
# Copyright (c) 1997 Alan Citterman.       All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

# HISTORY
#
# 0.24 -
#    H.Merijn Brand (h.m.brand@xs4all.nl)
# 0.10 - 0.23
#    Jochen Wiedmann <joe@ispsoft.de>
# Based on (the original) Text::CSV by:
#    Alan Citterman <alan@mfgrtl.com>

require 5.006001;

use strict;
use warnings;

require Exporter;
use XSLoader;
use Carp;

use vars   qw( $VERSION @ISA @EXPORT_OK );
$VERSION   = "1.39";
@ISA       = qw( Exporter );
@EXPORT_OK = qw( csv );
XSLoader::load "Text::CSV_XS", $VERSION;

sub PV { 0 }
sub IV { 1 }
sub NV { 2 }

if ($] < 5.008002) {
    no warnings "redefine";
    *utf8::decode = sub {};
    }

# version
#
#   class/object method expecting no arguments and returning the version
#   number of Text::CSV.  there are no side-effects.

sub version {
    return $VERSION;
    } # version

# new
#
#   class/object method expecting no arguments and returning a reference to
#   a newly created Text::CSV object.

my %def_attr = (
    eol				=> '',
    sep_char			=> ',',
    quote_char			=> '"',
    escape_char			=> '"',
    binary			=> 0,
    decode_utf8			=> 1,
    auto_diag			=> 0,
    diag_verbose		=> 0,
    strict			=> 0,
    blank_is_undef		=> 0,
    empty_is_undef		=> 0,
    allow_whitespace		=> 0,
    allow_loose_quotes		=> 0,
    allow_loose_escapes		=> 0,
    allow_unquoted_escape	=> 0,
    always_quote		=> 0,
    quote_empty			=> 0,
    quote_space			=> 1,
    quote_binary		=> 1,
    escape_null			=> 1,
    keep_meta_info		=> 0,
    verbatim			=> 0,
    formula			=> 0,
    undef_str			=> undef,
    types			=> undef,
    callbacks			=> undef,

    _EOF			=> 0,
    _RECNO			=> 0,
    _STATUS			=> undef,
    _FIELDS			=> undef,
    _FFLAGS			=> undef,
    _STRING			=> undef,
    _ERROR_INPUT		=> undef,
    _COLUMN_NAMES		=> undef,
    _BOUND_COLUMNS		=> undef,
    _AHEAD			=> undef,

    ENCODING			=> undef,
    );
my %attr_alias = (
    quote_always		=> "always_quote",
    verbose_diag		=> "diag_verbose",
    quote_null			=> "escape_null",
    escape			=> "escape_char",
    );
my $last_new_err = Text::CSV_XS->SetDiag (0);

# NOT a method: is also used before bless
sub _unhealthy_whitespace {
    my ($self, $aw) = @_;
    $aw or return 0; # no checks needed without allow_whitespace

    my $quo = $self->{quote};
    defined $quo && length ($quo) or $quo = $self->{quote_char};
    my $esc = $self->{escape_char};

    defined $quo && $quo =~ m/^[ \t]/ and return 1002;
    defined $esc && $esc =~ m/^[ \t]/ and return 1002;

    return 0;
    } # _unhealty_whitespace

sub _check_sanity {
    my $self = shift;

    my $eol = $self->{eol};
    my $sep = $self->{sep};
    defined $sep && length ($sep) or $sep = $self->{sep_char};
    my $quo = $self->{quote};
    defined $quo && length ($quo) or $quo = $self->{quote_char};
    my $esc = $self->{escape_char};

#    use DP;::diag ("SEP: '", DPeek ($sep),
#	        "', QUO: '", DPeek ($quo),
#	        "', ESC: '", DPeek ($esc),"'");

    # sep_char should not be undefined
    $sep ne ""			or  return 1008;
    length ($sep) > 16		and return 1006;
    $sep =~ m/[\r\n]/		and return 1003;

    if (defined $quo) {
	$quo eq $sep		and return 1001;
	length ($quo) > 16	and return 1007;
	$quo =~ m/[\r\n]/	and return 1003;
	}
    if (defined $esc) {
	$esc eq $sep		and return 1001;
	$esc =~ m/[\r\n]/	and return 1003;
	}
    if (defined $eol) {
	length ($eol) > 16	and return 1005;
	}

    return _unhealthy_whitespace ($self, $self->{allow_whitespace});
    } # _check_sanity

sub known_attributes {
    sort grep !m/^_/ => "sep", "quote", keys %def_attr;
    } # known_attributes

sub new {
    $last_new_err = Text::CSV_XS->SetDiag (1000,
	"usage: my \$csv = Text::CSV_XS->new ([{ option => value, ... }]);");

    my $proto = shift;
    my $class = ref ($proto) || $proto	or  return;
    @_ > 0 &&   ref $_[0] ne "HASH"	and return;
    my $attr  = shift || {};
    my %attr  = map {
	my $k = m/^[a-zA-Z]\w+$/ ? lc $_ : $_;
	exists $attr_alias{$k} and $k = $attr_alias{$k};
	$k => $attr->{$_};
	} keys %$attr;

    my $sep_aliased = 0;
    if (exists $attr{sep}) {
	$attr{sep_char} = delete $attr{sep};
	$sep_aliased = 1;
	}
    my $quote_aliased = 0;
    if (exists $attr{quote}) {
	$attr{quote_char} = delete $attr{quote};
	$quote_aliased = 1;
	}
    exists $attr{formula_handling} and
	$attr{formula} = delete $attr{formula_handling};
    exists $attr{formula} and
	$attr{formula} = _supported_formula (undef, $attr{formula});
    for (keys %attr) {
	if (m/^[a-z]/ && exists $def_attr{$_}) {
	    # uncoverable condition false
	    defined $attr{$_} && m/_char$/ and utf8::decode ($attr{$_});
	    next;
	    }
#	croak?
	$last_new_err = Text::CSV_XS->SetDiag (1000, "INI - Unknown attribute '$_'");
	$attr{auto_diag} and error_diag ();
	return;
	}
    if ($sep_aliased) {
	my @b = unpack "U0C*", $attr{sep_char};
	if (@b > 1) {
	    $attr{sep} = $attr{sep_char};
	    $attr{sep_char} = "\0";
	    }
	else {
	    $attr{sep} = undef;
	    }
	}
    if ($quote_aliased and defined $attr{quote_char}) {
	my @b = unpack "U0C*", $attr{quote_char};
	if (@b > 1) {
	    $attr{quote} = $attr{quote_char};
	    $attr{quote_char} = "\0";
	    }
	else {
	    $attr{quote} = undef;
	    }
	}

    my $self = { %def_attr, %attr };
    if (my $ec = _check_sanity ($self)) {
	$last_new_err = Text::CSV_XS->SetDiag ($ec);
	$attr{auto_diag} and error_diag ();
	return;
	}
    if (defined $self->{callbacks} && ref $self->{callbacks} ne "HASH") {
	carp "The 'callbacks' attribute is set but is not a hash: ignored\n";
	$self->{callbacks} = undef;
	}

    $last_new_err = Text::CSV_XS->SetDiag (0);
    defined $\ && !exists $attr{eol} and $self->{eol} = $\;
    bless $self, $class;
    defined $self->{types} and $self->types ($self->{types});
    $self;
    } # new

# Keep in sync with XS!
my %_cache_id = ( # Only expose what is accessed from within PM
    quote_char			=>  0,
    escape_char			=>  1,
    sep_char			=>  2,
    sep				=> 39,	# 39 .. 55
    binary			=>  3,
    keep_meta_info		=>  4,
    always_quote		=>  5,
    allow_loose_quotes		=>  6,
    allow_loose_escapes		=>  7,
    allow_unquoted_escape	=>  8,
    allow_whitespace		=>  9,
    blank_is_undef		=> 10,
    eol				=> 11,
    quote			=> 15,
    verbatim			=> 22,
    empty_is_undef		=> 23,
    auto_diag			=> 24,
    diag_verbose		=> 33,
    quote_space			=> 25,
    quote_empty			=> 37,
    quote_binary		=> 32,
    escape_null			=> 31,
    decode_utf8			=> 35,
    _has_ahead			=> 30,
    _has_hooks			=> 36,
    _is_bound			=> 26,	# 26 .. 29
    formula			=> 38,
    strict			=> 42,
    undef_str			=> 46,
    );

# A `character'
sub _set_attr_C {
    my ($self, $name, $val, $ec) = @_;
    defined $val or $val = 0;
    utf8::decode ($val);
    $self->{$name} = $val;
    $ec = _check_sanity ($self) and croak ($self->SetDiag ($ec));
    $self->_cache_set ($_cache_id{$name}, $val);
    } # _set_attr_C

# A flag
sub _set_attr_X {
    my ($self, $name, $val) = @_;
    defined $val or $val = 0;
    $self->{$name} = $val;
    $self->_cache_set ($_cache_id{$name}, 0 + $val);
    } # _set_attr_X

# A number
sub _set_attr_N {
    my ($self, $name, $val) = @_;
    $self->{$name} = $val;
    $self->_cache_set ($_cache_id{$name}, 0 + $val);
    } # _set_attr_N

# Accessor methods.
#   It is unwise to change them halfway through a single file!
sub quote_char {
    my $self = shift;
    if (@_) {
	$self->_set_attr_C ("quote_char", shift);
	$self->_cache_set ($_cache_id{quote}, "");
	}
    $self->{quote_char};
    } # quote_char

sub quote {
    my $self = shift;
    if (@_) {
	my $quote = shift;
	defined $quote or $quote = "";
	utf8::decode ($quote);
	my @b = unpack "U0C*", $quote;
	if (@b > 1) {
	    @b > 16 and croak ($self->SetDiag (1007));
	    $self->quote_char ("\0");
	    }
	else {
	    $self->quote_char ($quote);
	    $quote = "";
	    }
	$self->{quote} = $quote;

	my $ec = _check_sanity ($self);
	$ec and croak ($self->SetDiag ($ec));

	$self->_cache_set ($_cache_id{quote}, $quote);
	}
    my $quote = $self->{quote};
    defined $quote && length ($quote) ? $quote : $self->{quote_char};
    } # quote

sub escape_char {
    my $self = shift;
    if (@_) {
	my $ec = shift;
	$self->_set_attr_C ("escape_char", $ec);
	$ec or $self->_set_attr_X ("escape_null", 0);
	}
    $self->{escape_char};
    } # escape_char

sub sep_char {
    my $self = shift;
    if (@_) {
	$self->_set_attr_C ("sep_char", shift);
	$self->_cache_set ($_cache_id{sep}, "");
	}
    $self->{sep_char};
    } # sep_char

sub sep {
    my $self = shift;
    if (@_) {
	my $sep = shift;
	defined $sep or $sep = "";
	utf8::decode ($sep);
	my @b = unpack "U0C*", $sep;
	if (@b > 1) {
	    @b > 16 and croak ($self->SetDiag (1006));
	    $self->sep_char ("\0");
	    }
	else {
	    $self->sep_char ($sep);
	    $sep = "";
	    }
	$self->{sep} = $sep;

	my $ec = _check_sanity ($self);
	$ec and croak ($self->SetDiag ($ec));

	$self->_cache_set ($_cache_id{sep}, $sep);
	}
    my $sep = $self->{sep};
    defined $sep && length ($sep) ? $sep : $self->{sep_char};
    } # sep

sub eol {
    my $self = shift;
    if (@_) {
	my $eol = shift;
	defined $eol or $eol = "";
	length ($eol) > 16 and croak ($self->SetDiag (1005));
	$self->{eol} = $eol;
	$self->_cache_set ($_cache_id{eol}, $eol);
	}
    $self->{eol};
    } # eol

sub always_quote {
    my $self = shift;
    @_ and $self->_set_attr_X ("always_quote", shift);
    $self->{always_quote};
    } # always_quote

sub quote_space {
    my $self = shift;
    @_ and $self->_set_attr_X ("quote_space", shift);
    $self->{quote_space};
    } # quote_space

sub quote_empty {
    my $self = shift;
    @_ and $self->_set_attr_X ("quote_empty", shift);
    $self->{quote_empty};
    } # quote_empty

sub escape_null {
    my $self = shift;
    @_ and $self->_set_attr_X ("escape_null", shift);
    $self->{escape_null};
    } # escape_null
sub quote_null { goto &escape_null; }

sub quote_binary {
    my $self = shift;
    @_ and $self->_set_attr_X ("quote_binary", shift);
    $self->{quote_binary};
    } # quote_binary

sub binary {
    my $self = shift;
    @_ and $self->_set_attr_X ("binary", shift);
    $self->{binary};
    } # binary

sub strict {
    my $self = shift;
    @_ and $self->_set_attr_X ("strict", shift);
    $self->{strict};
    } # always_quote

sub _SetDiagInfo {
    my ($self, $err, $msg) = @_;
    $self->SetDiag ($err);
    my $em  = $self->error_diag;
    $em =~ s/^\d+$// and $msg =~ s/^/# /;
    my $sep = $em =~ m/[;\n]$/ ? "\n\t" : ": ";
    join $sep => grep m/\S\S\S/ => $em, $msg;
    } # _SetDiagInfo

sub _supported_formula {
    my ($self, $f) = @_;
    defined $f or return 5;
    $f =~ m/^(?: 0 | none    )$/xi ? 0 :
    $f =~ m/^(?: 1 | die     )$/xi ? 1 :
    $f =~ m/^(?: 2 | croak   )$/xi ? 2 :
    $f =~ m/^(?: 3 | diag    )$/xi ? 3 :
    $f =~ m/^(?: 4 | empty | )$/xi ? 4 :
    $f =~ m/^(?: 5 | undef   )$/xi ? 5 : do {
	$self ||= "Text::CSV_XS";
	croak ($self->_SetDiagInfo (1500, "formula-handling '$f' is not supported"));
	};
    } # _supported_formula

sub formula {
    my $self = shift;
    @_ and $self->_set_attr_N ("formula", _supported_formula ($self, shift));
    [qw( none die croak diag empty undef )]->[_supported_formula ($self, $self->{formula})];
    } # always_quote
sub formula_handling {
    my $self = shift;
    $self->formula (@_);
    } # formula_handling

sub decode_utf8 {
    my $self = shift;
    @_ and $self->_set_attr_X ("decode_utf8", shift);
    $self->{decode_utf8};
    } # decode_utf8

sub keep_meta_info {
    my $self = shift;
    if (@_) {
	my $v = shift;
	!defined $v || $v eq "" and $v = 0;
	$v =~ m/^[0-9]/ or $v = lc $v eq "false" ? 0 : 1; # true/truth = 1
	$self->_set_attr_X ("keep_meta_info", $v);
	}
    $self->{keep_meta_info};
    } # keep_meta_info

sub allow_loose_quotes {
    my $self = shift;
    @_ and $self->_set_attr_X ("allow_loose_quotes", shift);
    $self->{allow_loose_quotes};
    } # allow_loose_quotes

sub allow_loose_escapes {
    my $self = shift;
    @_ and $self->_set_attr_X ("allow_loose_escapes", shift);
    $self->{allow_loose_escapes};
    } # allow_loose_escapes

sub allow_whitespace {
    my $self = shift;
    if (@_) {
	my $aw = shift;
	_unhealthy_whitespace ($self, $aw) and
	    croak ($self->SetDiag (1002));
	$self->_set_attr_X ("allow_whitespace", $aw);
	}
    $self->{allow_whitespace};
    } # allow_whitespace

sub allow_unquoted_escape {
    my $self = shift;
    @_ and $self->_set_attr_X ("allow_unquoted_escape", shift);
    $self->{allow_unquoted_escape};
    } # allow_unquoted_escape

sub blank_is_undef {
    my $self = shift;
    @_ and $self->_set_attr_X ("blank_is_undef", shift);
    $self->{blank_is_undef};
    } # blank_is_undef

sub empty_is_undef {
    my $self = shift;
    @_ and $self->_set_attr_X ("empty_is_undef", shift);
    $self->{empty_is_undef};
    } # empty_is_undef

sub verbatim {
    my $self = shift;
    @_ and $self->_set_attr_X ("verbatim", shift);
    $self->{verbatim};
    } # verbatim

sub undef_str {
    my $self = shift;
    if (@_) {
	my $v = shift;
	$self->{undef_str} = defined $v ? "$v" : undef;
	$self->_cache_set ($_cache_id{undef_str}, $self->{undef_str});
	}
    $self->{undef_str};
    } # undef_str

sub auto_diag {
    my $self = shift;
    if (@_) {
	my $v = shift;
	!defined $v || $v eq "" and $v = 0;
	$v =~ m/^[0-9]/ or $v = lc $v eq "false" ? 0 : 1; # true/truth = 1
	$self->_set_attr_X ("auto_diag", $v);
	}
    $self->{auto_diag};
    } # auto_diag

sub diag_verbose {
    my $self = shift;
    if (@_) {
	my $v = shift;
	!defined $v || $v eq "" and $v = 0;
	$v =~ m/^[0-9]/ or $v = lc $v eq "false" ? 0 : 1; # true/truth = 1
	$self->_set_attr_X ("diag_verbose", $v);
	}
    $self->{diag_verbose};
    } # diag_verbose

# status
#
#   object method returning the success or failure of the most recent
#   combine () or parse ().  there are no side-effects.

sub status {
    my $self = shift;
    return $self->{_STATUS};
    } # status

sub eof {
    my $self = shift;
    return $self->{_EOF};
    } # status

sub types {
    my $self = shift;
    if (@_) {
	if (my $types = shift) {
	    $self->{_types} = join "", map { chr } @{$types};
	    $self->{types}  = $types;
	    }
	else {
	    delete $self->{types};
	    delete $self->{_types};
	    undef;
	    }
	}
    else {
	$self->{types};
	}
    } # types

sub callbacks {
    my $self = shift;
    if (@_) {
	my $cb;
	my $hf = 0x00;
	if (defined $_[0]) {
	    grep { !defined } @_ and croak ($self->SetDiag (1004));
	    $cb = @_ == 1 && ref $_[0] eq "HASH" ? shift
	        : @_ % 2 == 0                    ? { @_ }
	        : croak ($self->SetDiag (1004));
	    foreach my $cbk (keys %$cb) {
		# A key cannot be a ref. That would be stored as the *string
		# 'SCALAR(0x1f3e710)' or 'ARRAY(0x1a5ae18)'
		$cbk =~ m/^[\w.]+$/ && ref $cb->{$cbk} eq "CODE" or
		    croak ($self->SetDiag (1004));
		}
	    exists $cb->{error}        and $hf |= 0x01;
	    exists $cb->{after_parse}  and $hf |= 0x02;
	    exists $cb->{before_print} and $hf |= 0x04;
	    }
	elsif (@_ > 1) {
	    # (undef, whatever)
	    croak ($self->SetDiag (1004));
	    }
	$self->_set_attr_X ("_has_hooks", $hf);
	$self->{callbacks} = $cb;
	}
    $self->{callbacks};
    } # callbacks

# error_diag
#
#   If (and only if) an error occurred, this function returns a code that
#   indicates the reason of failure

sub error_diag {
    my $self = shift;
    my @diag = (0 + $last_new_err, $last_new_err, 0, 0, 0);

    # Docs state to NEVER use UNIVERSAL::isa, because it will *never* call an
    # overridden isa method in any class. Well, that is exacly what I want here
    if ($self && ref $self && # Not a class method or direct call
	 UNIVERSAL::isa ($self, __PACKAGE__) && exists $self->{_ERROR_DIAG}) {
	$diag[0] = 0 + $self->{_ERROR_DIAG};
	$diag[1] =     $self->{_ERROR_DIAG};
	$diag[2] = 1 + $self->{_ERROR_POS} if exists $self->{_ERROR_POS};
	$diag[3] =     $self->{_RECNO};
	$diag[4] =     $self->{_ERROR_FLD} if exists $self->{_ERROR_FLD};

	$diag[0] && $self->{callbacks} && $self->{callbacks}{error} and
	    return $self->{callbacks}{error}->(@diag);
	}

    my $context = wantarray;
    unless (defined $context) {	# Void context, auto-diag
	if ($diag[0] && $diag[0] != 2012) {
	    my $msg = "# CSV_XS ERROR: $diag[0] - $diag[1] \@ rec $diag[3] pos $diag[2]\n";
	    $diag[4] and $msg =~ s/$/ field $diag[4]/;

	    unless ($self && ref $self) {	# auto_diag
	    	# called without args in void context
		warn $msg;
		return;
		}

	    if ($self->{diag_verbose} and $self->{_ERROR_INPUT}) {
		$msg .= "$self->{_ERROR_INPUT}'\n";
		$msg .= " " x ($diag[2] - 1);
		$msg .= "^\n";
		}

	    my $lvl = $self->{auto_diag};
	    if ($lvl < 2) {
		my @c = caller (2);
		if (@c >= 11 && $c[10] && ref $c[10] eq "HASH") {
		    my $hints = $c[10];
		    (exists $hints->{autodie} && $hints->{autodie} or
		     exists $hints->{"guard Fatal"} &&
		    !exists $hints->{"no Fatal"}) and
			$lvl++;
		    # Future releases of autodie will probably set $^H{autodie}
		    #  to "autodie @args", like "autodie :all" or "autodie open"
		    #  so we can/should check for "open" or "new"
		    }
		}
	    $lvl > 1 ? die $msg : warn $msg;
	    }
	return;
	}
    return $context ? @diag : $diag[1];
    } # error_diag

sub record_number {
    my $self = shift;
    return $self->{_RECNO};
    } # record_number

# string
#
#   object method returning the result of the most recent combine () or the
#   input to the most recent parse (), whichever is more recent.  there are
#   no side-effects.

sub string {
    my $self = shift;
    return ref $self->{_STRING} ? ${$self->{_STRING}} : undef;
    } # string

# fields
#
#   object method returning the result of the most recent parse () or the
#   input to the most recent combine (), whichever is more recent.  there
#   are no side-effects.

sub fields {
    my $self = shift;
    return ref $self->{_FIELDS} ? @{$self->{_FIELDS}} : undef;
    } # fields

# meta_info
#
#   object method returning the result of the most recent parse () or the
#   input to the most recent combine (), whichever is more recent.  there
#   are no side-effects. meta_info () returns (if available)  some of the
#   field's properties

sub meta_info {
    my $self = shift;
    return ref $self->{_FFLAGS} ? @{$self->{_FFLAGS}} : undef;
    } # meta_info

sub is_quoted {
    my ($self, $idx) = @_;
    ref $self->{_FFLAGS} &&
	$idx >= 0 && $idx < @{$self->{_FFLAGS}} or return;
    $self->{_FFLAGS}[$idx] & 0x0001 ? 1 : 0;
    } # is_quoted

sub is_binary {
    my ($self, $idx) = @_;
    ref $self->{_FFLAGS} &&
	$idx >= 0 && $idx < @{$self->{_FFLAGS}} or return;
    $self->{_FFLAGS}[$idx] & 0x0002 ? 1 : 0;
    } # is_binary

sub is_missing {
    my ($self, $idx) = @_;
    $idx < 0 || !ref $self->{_FFLAGS} and return;
    $idx >= @{$self->{_FFLAGS}} and return 1;
    $self->{_FFLAGS}[$idx] & 0x0010 ? 1 : 0;
    } # is_missing

# combine
#
#  Object method returning success or failure. The given arguments are
#  combined into a single comma-separated value. Failure can be the
#  result of no arguments or an argument containing an invalid character.
#  side-effects include:
#      setting status ()
#      setting fields ()
#      setting string ()
#      setting error_input ()

sub combine {
    my $self = shift;
    my $str  = "";
    $self->{_FIELDS} = \@_;
    $self->{_STATUS} = (@_ > 0) && $self->Combine (\$str, \@_, 0);
    $self->{_STRING} = \$str;
    $self->{_STATUS};
    } # combine

# parse
#
#  Object method returning success or failure. The given argument is
#  expected to be a valid comma-separated value. Failure can be the
#  result of no arguments or an argument containing an invalid sequence
#  of characters. Side-effects include:
#      setting status ()
#      setting fields ()
#      setting meta_info ()
#      setting string ()
#      setting error_input ()

sub parse {
    my ($self, $str) = @_;

    ref $str and croak ($self->SetDiag (1500));

    my $fields = [];
    my $fflags = [];
    $self->{_STRING} = \$str;
    if (defined $str && $self->Parse ($str, $fields, $fflags)) {
	$self->{_FIELDS} = $fields;
	$self->{_FFLAGS} = $fflags;
	$self->{_STATUS} = 1;
	}
    else {
	$self->{_FIELDS} = undef;
	$self->{_FFLAGS} = undef;
	$self->{_STATUS} = 0;
	}
    $self->{_STATUS};
    } # parse

sub column_names {
    my ($self, @keys) = @_;
    @keys or
	return defined $self->{_COLUMN_NAMES} ? @{$self->{_COLUMN_NAMES}} : ();

    @keys == 1 && ! defined $keys[0] and
	return $self->{_COLUMN_NAMES} = undef;

    if (@keys == 1 && ref $keys[0] eq "ARRAY") {
	@keys = @{$keys[0]};
	}
    elsif (join "", map { defined $_ ? ref $_ : "" } @keys) {
	croak ($self->SetDiag (3001));
	}

    $self->{_BOUND_COLUMNS} && @keys != @{$self->{_BOUND_COLUMNS}} and
	croak ($self->SetDiag (3003));

    $self->{_COLUMN_NAMES} = [ map { defined $_ ? $_ : "\cAUNDEF\cA" } @keys ];
    @{$self->{_COLUMN_NAMES}};
    } # column_names

sub header {
    my ($self, $fh, @args) = @_;

    $fh or croak ($self->SetDiag (1014));

    my (@seps, %args);
    for (@args) {
	if (ref $_ eq "ARRAY") {
	    push @seps, @$_;
	    next;
	    }
	if (ref $_ eq "HASH") {
	    %args = %$_;
	    next;
	    }
	croak (q{usage: $csv->header ($fh, [ seps ], { options })});
	}

    defined $args{munge} && !defined $args{munge_column_names} and
	$args{munge_column_names} = $args{munge}; # munge as alias
    defined $args{detect_bom}         or $args{detect_bom}         = 1;
    defined $args{set_column_names}   or $args{set_column_names}   = 1;
    defined $args{munge_column_names} or $args{munge_column_names} = "lc";

    # Reset any previous leftovers
    $self->{_RECNO}		= 0;
    $self->{_AHEAD}		= undef;
    $self->{_COLUMN_NAMES}	= undef if $args{set_column_names};
    $self->{_BOUND_COLUMNS}	= undef if $args{set_column_names};

    if (defined $args{sep_set}) {
	ref $args{sep_set} eq "ARRAY" or
	    croak ($self->_SetDiagInfo (1500, "sep_set should be an array ref"));
	@seps =  @{$args{sep_set}};
	}

    $^O eq "MSWin32" and binmode $fh;
    my $hdr = <$fh>;
    # check if $hdr can be empty here, I don't think so
    defined $hdr && $hdr ne "" or croak ($self->SetDiag (1010));

    my %sep;
    @seps or @seps = (",", ";");
    foreach my $sep (@seps) {
	index ($hdr, $sep) >= 0 and $sep{$sep}++;
	}

    keys %sep >= 2 and croak ($self->SetDiag (1011));

    $self->sep (keys %sep);
    my $enc = "";
    if ($args{detect_bom}) { # UTF-7 is not supported
	   if ($hdr =~ s/^\x00\x00\xfe\xff//) { $enc = "utf-32be"   }
	elsif ($hdr =~ s/^\xff\xfe\x00\x00//) { $enc = "utf-32le"   }
	elsif ($hdr =~ s/^\xfe\xff//)         { $enc = "utf-16be"   }
	elsif ($hdr =~ s/^\xff\xfe//)         { $enc = "utf-16le"   }
	elsif ($hdr =~ s/^\xef\xbb\xbf//)     { $enc = "utf-8"      }
	elsif ($hdr =~ s/^\xf7\x64\x4c//)     { $enc = "utf-1"      }
	elsif ($hdr =~ s/^\xdd\x73\x66\x73//) { $enc = "utf-ebcdic" }
	elsif ($hdr =~ s/^\x0e\xfe\xff//)     { $enc = "scsu"       }
	elsif ($hdr =~ s/^\xfb\xee\x28//)     { $enc = "bocu-1"     }
	elsif ($hdr =~ s/^\x84\x31\x95\x33//) { $enc = "gb-18030"   }
	elsif ($hdr =~ s/^\x{feff}//)         { $enc = ""           }

	$self->{ENCODING} = uc $enc;

	$hdr eq "" and croak ($self->SetDiag (1010));

	if ($enc) {
	    if ($enc =~ m/([13]).le$/) {
		my $l = 0 + $1;
		my $x;
		$hdr .= "\0" x $l;
		read $fh, $x, $l;
		}
	    if ($enc ne "utf-8") {
		require Encode;
		$hdr = Encode::decode ($enc, $hdr);
		}
	    binmode $fh, ":encoding($enc)";
	    }
	}

    my ($ahead, $eol);
    if ($hdr =~ s/^([^\r\n]+)([\r\n]+)([^\r\n].+)\z/$1/s) {
	$eol   = $2;
	$ahead = $3;
	}

    $args{munge_column_names} eq "lc" and $hdr = lc $hdr;
    $args{munge_column_names} eq "uc" and $hdr = uc $hdr;

    my $hr = \$hdr; # Will cause croak on perl-5.6.x
    open my $h, "<", $hr or croak ($self->SetDiag (1010));

    my $row = $self->getline ($h) or croak;
    close $h;

    if ($ahead) { # Must be after getline, which creates the cache
	$self->_cache_set ($_cache_id{_has_ahead}, 1);
	$self->{_AHEAD} = $ahead;
	$eol =~ m/^\r([^\n]|\z)/ and $self->eol ($eol);
	}

    my @hdr = @$row;
    ref $args{munge_column_names} eq "CODE" and
	@hdr = map { $args{munge_column_names}->($_)       } @hdr;
    ref $args{munge_column_names} eq "HASH" and
	@hdr = map { $args{munge_column_names}->{$_} || $_ } @hdr;
    my %hdr; $hdr{$_}++ for @hdr;
    exists $hdr{""} and croak ($self->SetDiag (1012));
    unless (keys %hdr == @hdr) {
	croak ($self->_SetDiagInfo (1013, join ", " =>
	    map { "$_ ($hdr{$_})" } grep { $hdr{$_} > 1 } keys %hdr));
	}
    $args{set_column_names} and $self->column_names (@hdr);
    wantarray ? @hdr : $self;
    } # header

sub bind_columns {
    my ($self, @refs) = @_;
    @refs or
	return defined $self->{_BOUND_COLUMNS} ? @{$self->{_BOUND_COLUMNS}} : undef;

    if (@refs == 1 && ! defined $refs[0]) {
	$self->{_COLUMN_NAMES} = undef;
	return $self->{_BOUND_COLUMNS} = undef;
	}

    $self->{_COLUMN_NAMES} && @refs != @{$self->{_COLUMN_NAMES}} and
	croak ($self->SetDiag (3003));

    join "", map { ref $_ eq "SCALAR" ? "" : "*" } @refs and
	croak ($self->SetDiag (3004));

    $self->_set_attr_N ("_is_bound", scalar @refs);
    $self->{_BOUND_COLUMNS} = [ @refs ];
    @refs;
    } # bind_columns

sub getline_hr {
    my ($self, @args, %hr) = @_;
    $self->{_COLUMN_NAMES} or croak ($self->SetDiag (3002));
    my $fr = $self->getline (@args) or return;
    if (ref $self->{_FFLAGS}) { # missing
	$self->{_FFLAGS}[$_] = 0x0010
	    for (@$fr ? $#{$fr} + 1 : 0) .. $#{$self->{_COLUMN_NAMES}};
	@$fr == 1 && (!defined $fr->[0] || $fr->[0] eq "") and
	    $self->{_FFLAGS}[0] ||= 0x0010;
	}
    @hr{@{$self->{_COLUMN_NAMES}}} = @$fr;
    \%hr;
    } # getline_hr

sub getline_hr_all {
    my ($self, @args) = @_;
    $self->{_COLUMN_NAMES} or croak ($self->SetDiag (3002));
    my @cn = @{$self->{_COLUMN_NAMES}};
    [ map { my %h; @h{@cn} = @$_; \%h } @{$self->getline_all (@args)} ];
    } # getline_hr_all

sub say {
    my ($self, $io, @f) = @_;
    my $eol = $self->eol;
    $eol eq "" and $self->eol ($\ || $/);
    # say ($fh, undef) does not propage actual undef to print ()
    my $state = $self->print ($io, @f == 1 && !defined $f[0] ? undef : @f);
    $self->eol ($eol);
    return $state;
    } # say

sub print_hr {
    my ($self, $io, $hr) = @_;
    $self->{_COLUMN_NAMES} or croak ($self->SetDiag (3009));
    ref $hr eq "HASH"      or croak ($self->SetDiag (3010));
    $self->print ($io, [ map { $hr->{$_} } $self->column_names ]);
    } # print_hr

sub fragment {
    my ($self, $io, $spec) = @_;

    my $qd = qr{\s* [0-9]+ \s* }x;		# digit
    my $qs = qr{\s* (?: [0-9]+ | \* ) \s*}x;	# digit or star
    my $qr = qr{$qd (?: - $qs )?}x;		# range
    my $qc = qr{$qr (?: ; $qr )*}x;		# list
    defined $spec && $spec =~ m{^ \s*
	\x23 ? \s*				# optional leading #
	( row | col | cell ) \s* =
	( $qc					# for row and col
	| $qd , $qd (?: - $qs , $qs)?		# for cell (ranges)
	  (?: ; $qd , $qd (?: - $qs , $qs)? )*	# and cell (range) lists
	) \s* $}xi or croak ($self->SetDiag (2013));
    my ($type, $range) = (lc $1, $2);

    my @h = $self->column_names ();

    my @c;
    if ($type eq "cell") {
	my @spec;
	my $min_row;
	my $max_row = 0;
	for (split m/\s*;\s*/ => $range) {
	    my ($tlr, $tlc, $brr, $brc) = (m{
		    ^ \s* ([0-9]+     ) \s* , \s* ([0-9]+     ) \s*
		(?: - \s* ([0-9]+ | \*) \s* , \s* ([0-9]+ | \*) \s* )?
		    $}x) or croak ($self->SetDiag (2013));
	    defined $brr or ($brr, $brc) = ($tlr, $tlc);
	    $tlr == 0 || $tlc == 0 ||
		($brr ne "*" && ($brr == 0 || $brr < $tlr)) ||
		($brc ne "*" && ($brc == 0 || $brc < $tlc))
		    and croak ($self->SetDiag (2013));
	    $tlc--;
	    $brc-- unless $brc eq "*";
	    defined $min_row or $min_row = $tlr;
	    $tlr < $min_row and $min_row = $tlr;
	    $brr eq "*" || $brr > $max_row and
		$max_row = $brr;
	    push @spec, [ $tlr, $tlc, $brr, $brc ];
	    }
	my $r = 0;
	while (my $row = $self->getline ($io)) {
	    ++$r < $min_row and next;
	    my %row;
	    my $lc;
	    foreach my $s (@spec) {
		my ($tlr, $tlc, $brr, $brc) = @$s;
		$r <  $tlr || ($brr ne "*" && $r > $brr) and next;
		!defined $lc || $tlc < $lc and $lc = $tlc;
		my $rr = $brc eq "*" ? $#$row : $brc;
		$row{$_} = $row->[$_] for $tlc .. $rr;
		}
	    push @c, [ @row{sort { $a <=> $b } keys %row } ];
	    if (@h) {
		my %h; @h{@h} = @{$c[-1]};
		$c[-1] = \%h;
		}
	    $max_row ne "*" && $r == $max_row and last;
	    }
	return \@c;
	}

    # row or col
    my @r;
    my $eod = 0;
    for (split m/\s*;\s*/ => $range) {
	my ($from, $to) = m/^\s* ([0-9]+) (?: \s* - \s* ([0-9]+ | \* ))? \s* $/x
	    or croak ($self->SetDiag (2013));
	$to ||= $from;
	$to eq "*" and ($to, $eod) = ($from, 1);
	# $to cannot be <= 0 due to regex and ||=
	$from <= 0 || $to < $from and croak ($self->SetDiag (2013));
	$r[$_] = 1 for $from .. $to;
	}

    my $r = 0;
    $type eq "col" and shift @r;
    $_ ||= 0 for @r;
    while (my $row = $self->getline ($io)) {
	$r++;
	if ($type eq "row") {
	    if (($r > $#r && $eod) || $r[$r]) {
		push @c, $row;
		if (@h) {
		    my %h; @h{@h} = @{$c[-1]};
		    $c[-1] = \%h;
		    }
		}
	    next;
	    }
	push @c, [ map { ($_ > $#r && $eod) || $r[$_] ? $row->[$_] : () } 0..$#$row ];
	if (@h) {
	    my %h; @h{@h} = @{$c[-1]};
	    $c[-1] = \%h;
	    }
	}

    return \@c;
    } # fragment

my $csv_usage = q{usage: my $aoa = csv (in => $file);};

sub _csv_attr {
    my %attr = (@_ == 1 && ref $_[0] eq "HASH" ? %{$_[0]} : @_) or croak;

    $attr{binary} = 1;

    my $enc = delete $attr{enc} || delete $attr{encoding} || "";
    $enc eq "auto" and ($attr{detect_bom}, $enc) = (1, "");
    $enc =~ m/^[-\w.]+$/ and $enc = ":encoding($enc)";

    my $fh;
    my $sink = 0;
    my $cls  = 0;	# If I open a file, I have to close it
    my $in   = delete $attr{in}  || delete $attr{file} or croak $csv_usage;
    my $out  = exists $attr{out} && !$attr{out} ? \"skip"
	     : delete $attr{out} || delete $attr{file};

    ref $in eq "CODE" || ref $in eq "ARRAY" and $out ||= \*STDOUT;

    $in && $out && !ref $in && !ref $out and croak join "\n" =>
	qq{Cannot use a string for both in and out. Instead use:},
	qq{ csv (in => csv (in => "$in"), out => "$out");\n};

    if ($out) {
	if ((ref $out and "SCALAR" ne ref $out) or "GLOB" eq ref \$out) {
	    $fh = $out;
	    }
	elsif (ref $out and "SCALAR" eq ref $out and defined $$out and $$out eq "skip") {
	    delete $attr{out};
	    $sink = 1;
	    }
	else {
	    open $fh, ">", $out or croak "$out: $!";
	    $cls = 1;
	    }
	if ($fh) {
	    $enc and binmode $fh, $enc;
	    unless (defined $attr{eol}) {
		my @layers = eval { PerlIO::get_layers ($fh) };
		$attr{eol} = (grep m/crlf/ => @layers) ? "\n" : "\r\n";
		}
	    }
	}

    if (   ref $in eq "CODE" or ref $in eq "ARRAY") {
	# All done
	}
    elsif (ref $in eq "SCALAR") {
	# Strings with code points over 0xFF may not be mapped into in-memory file handles
	# "<$enc" does not change that :(
	open $fh, "<", $in or croak "Cannot open from SCALAR using PerlIO";
	$cls = 1;
	}
    elsif (ref $in or "GLOB" eq ref \$in) {
	if (!ref $in && $] < 5.008005) {
	    $fh = \*$in; # uncoverable statement ancient perl version required
	    }
	else {
	    $fh = $in;
	    }
	}
    else {
	open $fh, "<$enc", $in or croak "$in: $!";
	$cls = 1;
	}
    $fh || $sink or croak qq{No valid source passed. "in" is required};

    my $hdrs = delete $attr{headers};
    my $frag = delete $attr{fragment};
    my $key  = delete $attr{key};
    my $val  = delete $attr{value};
    my $kh   = delete $attr{keep_headers}	    ||
	       delete $attr{keep_column_names}      ||
	       delete $attr{kh};

    my $cbai = delete $attr{callbacks}{after_in}    ||
	       delete $attr{after_in}               ||
	       delete $attr{callbacks}{after_parse} ||
	       delete $attr{after_parse};
    my $cbbo = delete $attr{callbacks}{before_out}  ||
	       delete $attr{before_out};
    my $cboi = delete $attr{callbacks}{on_in}       ||
	       delete $attr{on_in};

    my $hd_s = delete $attr{sep_set}                ||
	       delete $attr{seps};
    my $hd_b = delete $attr{detect_bom}             ||
	       delete $attr{bom};
    my $hd_m = delete $attr{munge}                  ||
	       delete $attr{munge_column_names};
    my $hd_c = delete $attr{set_column_names};

    for ([ quo    => "quote"		],
	 [ esc    => "escape"		],
	 [ escape => "escape_char"	],
	 ) {
	my ($f, $t) = @$_;
	exists $attr{$f} and !exists $attr{$t} and $attr{$t} = delete $attr{$f};
	}

    my $fltr = delete $attr{filter};
    my %fltr = (
	not_blank => sub { @{$_[1]} > 1 or defined $_[1][0] && $_[1][0] ne "" },
	not_empty => sub { grep { defined && $_ ne "" } @{$_[1]} },
	filled    => sub { grep { defined && m/\S/    } @{$_[1]} },
	);
    defined $fltr && !ref $fltr && exists $fltr{$fltr} and
	$fltr = { 0 => $fltr{$fltr} };
    ref $fltr eq "CODE" and $fltr = { 0 => $fltr };
    ref $fltr eq "HASH" or  $fltr = undef;

    exists $attr{formula} and
	$attr{formula} = _supported_formula (undef, $attr{formula});

    defined $attr{auto_diag}   or $attr{auto_diag}   = 1;
    defined $attr{escape_null} or $attr{escape_null} = 0;
    my $csv = delete $attr{csv} || Text::CSV_XS->new (\%attr)
	or croak $last_new_err;

    return {
	csv  => $csv,
	attr => { %attr },
	fh   => $fh,
	cls  => $cls,
	in   => $in,
	sink => $sink,
	out  => $out,
	enc  => $enc,
	hdrs => $hdrs,
	key  => $key,
	val  => $val,
	kh   => $kh,
	frag => $frag,
	fltr => $fltr,
	cbai => $cbai,
	cbbo => $cbbo,
	cboi => $cboi,
	hd_s => $hd_s,
	hd_b => $hd_b,
	hd_m => $hd_m,
	hd_c => $hd_c,
	};
    } # _csv_attr

sub csv {
    @_ && ref $_[0] eq __PACKAGE__ and splice @_, 0, 0, "csv";
    @_ or croak $csv_usage;

    my $c = _csv_attr (@_);

    my ($csv, $in, $fh, $hdrs) = @{$c}{"csv", "in", "fh", "hdrs"};
    my %hdr;
    if (ref $hdrs eq "HASH") {
	%hdr  = %$hdrs;
	$hdrs = "auto";
	}

    if ($c->{out} && !$c->{sink}) {
	if (ref $in eq "CODE") {
	    my $hdr = 1;
	    while (my $row = $in->($csv)) {
		if (ref $row eq "ARRAY") {
		    $csv->print ($fh, $row);
		    next;
		    }
		if (ref $row eq "HASH") {
		    if ($hdr) {
			$hdrs ||= [ map { $hdr{$_} || $_ } keys %$row ];
			$csv->print ($fh, $hdrs);
			$hdr = 0;
			}
		    $csv->print ($fh, [ @{$row}{@$hdrs} ]);
		    }
		}
	    }
	elsif (ref $in->[0] eq "ARRAY") { # aoa
	    ref $hdrs and $csv->print ($fh, $hdrs);
	    for (@{$in}) {
		$c->{cboi} and $c->{cboi}->($csv, $_);
		$c->{cbbo} and $c->{cbbo}->($csv, $_);
		$csv->print ($fh, $_);
		}
	    }
	else { # aoh
	    my @hdrs = ref $hdrs ? @{$hdrs} : keys %{$in->[0]};
	    defined $hdrs or $hdrs = "auto";
	    ref $hdrs || $hdrs eq "auto" and
		$csv->print ($fh, [ map { $hdr{$_} || $_ } @hdrs ]);
	    for (@{$in}) {
		local %_;
		*_ = $_;
		$c->{cboi} and $c->{cboi}->($csv, $_);
		$c->{cbbo} and $c->{cbbo}->($csv, $_);
		$csv->print ($fh, [ @{$_}{@hdrs} ]);
		}
	    }

	$c->{cls} and close $fh;
	return 1;
	}

    my @row1;
    if (defined $c->{hd_s} || defined $c->{hd_b} || defined $c->{hd_m} || defined $c->{hd_c}) {
	my %harg;
	defined $c->{hd_s} and $harg{set_set}            = $c->{hd_s};
	defined $c->{hd_d} and $harg{detect_bom}         = $c->{hd_b};
	defined $c->{hd_m} and $harg{munge_column_names} = $hdrs ? "none" : $c->{hd_m};
	defined $c->{hd_c} and $harg{set_column_names}   = $hdrs ? 0      : $c->{hd_c};
	@row1 = $csv->header ($fh, \%harg);
	my @hdr = $csv->column_names;
	@hdr and $hdrs ||= \@hdr;
	}

    if ($c->{kh}) {
	ref $c->{kh} eq "ARRAY" or croak ($csv->SetDiag (1501));
	$hdrs ||= "auto";
	}

    my $key = $c->{key};
    if ($key) {
	!ref $key or ref $key eq "ARRAY" && @$key > 1 or croak ($csv->SetDiag (1501));
	$hdrs ||= "auto";
	}
    my $val = $c->{val};
    if ($val) {
	$key					      or croak ($csv->SetDiag (1502));
	!ref $val or ref $val eq "ARRAY" && @$val > 0 or croak ($csv->SetDiag (1503));
	}

    $c->{fltr} && grep m/\D/ => keys %{$c->{fltr}} and $hdrs ||= "auto";
    if (defined $hdrs) {
	if (!ref $hdrs) {
	    if ($hdrs eq "skip") {
		$csv->getline ($fh); # discard;
		}
	    elsif ($hdrs eq "auto") {
		my $h = $csv->getline ($fh) or return;
		$hdrs = [ map {      $hdr{$_} || $_ } @$h ];
		}
	    elsif ($hdrs eq "lc") {
		my $h = $csv->getline ($fh) or return;
		$hdrs = [ map { lc ($hdr{$_} || $_) } @$h ];
		}
	    elsif ($hdrs eq "uc") {
		my $h = $csv->getline ($fh) or return;
		$hdrs = [ map { uc ($hdr{$_} || $_) } @$h ];
		}
	    }
	elsif (ref $hdrs eq "CODE") {
	    my $h  = $csv->getline ($fh) or return;
	    my $cr = $hdrs;
	    $hdrs  = [ map {  $cr->($hdr{$_} || $_) } @$h ];
	    }
	$c->{kh} and $hdrs and @{$c->{kh}} = @$hdrs;
	}

    if ($c->{fltr}) {
	my %f = %{$c->{fltr}};
	# convert headers to index
	my @hdr;
	if (ref $hdrs) {
	    @hdr = @{$hdrs};
	    for (0 .. $#hdr) {
		exists $f{$hdr[$_]} and $f{$_ + 1} = delete $f{$hdr[$_]};
		}
	    }
	$csv->callbacks (after_parse => sub {
	    my ($CSV, $ROW) = @_; # lexical sub-variables in caps
	    foreach my $FLD (sort keys %f) {
		local $_ = $ROW->[$FLD - 1];
		local %_;
		@hdr and @_{@hdr} = @$ROW;
		$f{$FLD}->($CSV, $ROW) or return \"skip";
		$ROW->[$FLD - 1] = $_;
		}
	    });
	}

    my $frag = $c->{frag};
    my $ref = ref $hdrs
	? # aoh
	  do {
	    my @h = $csv->column_names ($hdrs);
	    my %h; $h{$_}++ for @h;
	    exists $h{""} and croak ($csv->SetDiag (1012));
	    unless (keys %h == @h) {
		croak ($csv->_SetDiagInfo (1013, join ", " =>
		    map { "$_ ($h{$_})" } grep { $h{$_} > 1 } keys %h));
		}
	    $frag ? $csv->fragment ($fh, $frag) :
	    $key  ? do {
			my ($k, $j, @f) = ref $key ? (undef, @$key) : ($key);
			if (my @mk = grep { !exists $h{$_} } grep { defined } $k, @f) {
			    croak ($csv->_SetDiagInfo (4001, join ", " => @mk));
			    }
			+{ map {
			    my $r = $_;
			    my $K = defined $k ? $r->{$k} : join $j => @{$r}{@f};
			    ( $K => (
			    $val
				? ref $val
				    ? { map { $_ => $r->{$_} } @$val }
				    : $r->{$val}
			        : $r ));
			    } @{$csv->getline_hr_all ($fh)} }
			}
		  : $csv->getline_hr_all ($fh);
	    }
	: # aoa
	    $frag ? $csv->fragment ($fh, $frag)
		  : $csv->getline_all ($fh);
    if ($ref) {
	@row1 && !$c->{hd_c} && !ref $hdrs and unshift @$ref, \@row1;
	}
    else {
	Text::CSV_XS->auto_diag;
	}
    $c->{cls} and close $fh;
    if ($ref and $c->{cbai} || $c->{cboi}) {
	# Default is ARRAYref, but with key =>, you'll get a hashref
	foreach my $r (ref $ref eq "ARRAY" ? @{$ref} : values %{$ref}) {
	    local %_;
	    ref $r eq "HASH" and *_ = $r;
	    $c->{cbai} and $c->{cbai}->($csv, $r);
	    $c->{cboi} and $c->{cboi}->($csv, $r);
	    }
	}

    $c->{sink} and return;

    defined wantarray or
	return csv (%{$c->{attr}}, in => $ref, headers => $hdrs, %{$c->{attr}});

    return $ref;
    } # csv

1;

__END__

=encoding utf-8

=head1 NAME

Text::CSV_XS - comma-separated values manipulation routines

=head1 SYNOPSIS

 # Functional interface
 use Text::CSV_XS qw( csv );

 # Read whole file in memory
 my $aoa = csv (in => "data.csv");    # as array of array
 my $aoh = csv (in => "data.csv",
                headers => "auto");   # as array of hash

 # Write array of arrays as csv file
 csv (in => $aoa, out => "file.csv", sep_char=> ";");

 # Only show lines where "code" is odd
 csv (in => "data.csv", filter => { code => sub { $_ % 2 }});


 # Object interface
 use Text::CSV_XS;

 my @rows;
 # Read/parse CSV
 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<:encoding(utf8)", "test.csv" or die "test.csv: $!";
 while (my $row = $csv->getline ($fh)) {
     $row->[2] =~ m/pattern/ or next; # 3rd field should match
     push @rows, $row;
     }
 close $fh;

 # and write as CSV
 open $fh, ">:encoding(utf8)", "new.csv" or die "new.csv: $!";
 $csv->say ($fh, $_) for @rows;
 close $fh or die "new.csv: $!";

=head1 DESCRIPTION

Text::CSV_XS  provides facilities for the composition  and decomposition of
comma-separated values.  An instance of the Text::CSV_XS class will combine
fields into a C<CSV> string and parse a C<CSV> string into fields.

The module accepts either strings or files as input  and support the use of
user-specified characters for delimiters, separators, and escapes.

=head2 Embedded newlines

B<Important Note>:  The default behavior is to accept only ASCII characters
in the range from C<0x20> (space) to C<0x7E> (tilde).   This means that the
fields can not contain newlines. If your data contains newlines embedded in
fields, or characters above C<0x7E> (tilde), or binary data, you B<I<must>>
set C<< binary => 1 >> in the call to L</new>. To cover the widest range of
parsing options, you will always want to set binary.

But you still have the problem  that you have to pass a correct line to the
L</parse> method, which is more complicated from the usual point of usage:

 my $csv = Text::CSV_XS->new ({ binary => 1, eol => $/ });
 while (<>) {		#  WRONG!
     $csv->parse ($_);
     my @fields = $csv->fields ();
     }

this will break, as the C<while> might read broken lines:  it does not care
about the quoting. If you need to support embedded newlines,  the way to go
is to  B<not>  pass L<C<eol>|/eol> in the parser  (it accepts C<\n>, C<\r>,
B<and> C<\r\n> by default) and then

 my $csv = Text::CSV_XS->new ({ binary => 1 });
 open my $fh, "<", $file or die "$file: $!";
 while (my $row = $csv->getline ($fh)) {
     my @fields = @$row;
     }

The old(er) way of using global file handles is still supported

 while (my $row = $csv->getline (*ARGV)) { ... }

=head2 Unicode

Unicode is only tested to work with perl-5.8.2 and up.

See also L</BOM>.

The simplest way to ensure the correct encoding is used for  in- and output
is by either setting layers on the filehandles, or setting the L</encoding>
argument for L</csv>.

 open my $fh, "<:encoding(UTF-8)", "in.csv"  or die "in.csv: $!";
or
 my $aoa = csv (in => "in.csv",     encoding => "UTF-8");

 open my $fh, ">:encoding(UTF-8)", "out.csv" or die "out.csv: $!";
or
 csv (in => $aoa, out => "out.csv", encoding => "UTF-8");

On parsing (both for  L</getline> and  L</parse>),  if the source is marked
being UTF8, then all fields that are marked binary will also be marked UTF8.

On combining (L</print>  and  L</combine>):  if any of the combining fields
was marked UTF8, the resulting string will be marked as UTF8.  Note however
that all fields  I<before>  the first field marked UTF8 and contained 8-bit
characters that were not upgraded to UTF8,  these will be  C<bytes>  in the
resulting string too, possibly causing unexpected errors.  If you pass data
of different encoding,  or you don't know if there is  different  encoding,
force it to be upgraded before you pass them on:

 $csv->print ($fh, [ map { utf8::upgrade (my $x = $_); $x } @data ]);

For complete control over encoding, please use L<Text::CSV::Encoded>:

 use Text::CSV::Encoded;
 my $csv = Text::CSV::Encoded->new ({
     encoding_in  => "iso-8859-1", # the encoding comes into   Perl
     encoding_out => "cp1252",     # the encoding comes out of Perl
     });

 $csv = Text::CSV::Encoded->new ({ encoding  => "utf8" });
 # combine () and print () accept *literally* utf8 encoded data
 # parse () and getline () return *literally* utf8 encoded data

 $csv = Text::CSV::Encoded->new ({ encoding  => undef }); # default
 # combine () and print () accept UTF8 marked data
 # parse () and getline () return UTF8 marked data

=head2 BOM

BOM  (or Byte Order Mark)  handling is available only inside the L</header>
method.   This method supports the following encodings: C<utf-8>, C<utf-1>,
C<utf-32be>, C<utf-32le>, C<utf-16be>, C<utf-16le>, C<utf-ebcdic>, C<scsu>,
C<bocu-1>, and C<gb-18030>. See L<Wikipedia|https://en.wikipedia.org/wiki/Byte_order_mark>.

If a file has a BOM, the easiest way to deal with that is

 my $aoh = csv (in => $file, detect_bom => 1);

All records will be encoded based on the detected BOM.

This implies a call to the  L</header>  method,  which defaults to also set
the L</column_names>. So this is B<not> the same as

 my $aoh = csv (in => $file, headers => "auto");

which only reads the first record to set  L</column_names>  but ignores any
meaning of possible present BOM.

=head1 SPECIFICATION

While no formal specification for CSV exists, L<RFC 4180|http://tools.ietf.org/html/rfc4180>
(I<1>) describes the common format and establishes  C<text/csv> as the MIME
type registered with the IANA. L<RFC 7111|http://tools.ietf.org/html/rfc7111>
(I<2>) adds fragments to CSV.

Many informal documents exist that describe the C<CSV> format.   L<"How To:
The Comma Separated Value (CSV) File Format"|http://www.creativyst.com/Doc/Articles/CSV/CSV01.htm>
(I<3>)  provides an overview of the  C<CSV>  format in the most widely used
applications and explains how it can best be used and supported.

 1) http://tools.ietf.org/html/rfc4180
 2) http://tools.ietf.org/html/rfc7111
 3) http://www.creativyst.com/Doc/Articles/CSV/CSV01.htm

The basic rules are as follows:

B<CSV>  is a delimited data format that has fields/columns separated by the
comma character and records/rows separated by newlines. Fields that contain
a special character (comma, newline, or double quote),  must be enclosed in
double quotes. However, if a line contains a single entry that is the empty
string, it may be enclosed in double quotes.  If a field's value contains a
double quote character it is escaped by placing another double quote
character next to it. The C<CSV> file format does not require a specific
character encoding, byte order, or line terminator format.

=over 2

=item *

Each record is a single line ended by a line feed  (ASCII/C<LF>=C<0x0A>) or
a carriage return and line feed pair (ASCII/C<CRLF>=C<0x0D 0x0A>), however,
line-breaks may be embedded.

=item *

Fields are separated by commas.

=item *

Allowable characters within a C<CSV> field include C<0x09> (C<TAB>) and the
inclusive range of C<0x20> (space) through C<0x7E> (tilde).  In binary mode
all characters are accepted, at least in quoted fields.

=item *

A field within  C<CSV>  must be surrounded by  double-quotes to  contain  a
separator character (comma).

=back

Though this is the most clear and restrictive definition,  Text::CSV_XS  is
way more liberal than this, and allows extension:

=over 2

=item *

Line termination by a single carriage return is accepted by default

=item *

The separation-, escape-, and escape- characters can be any ASCII character
in the range from  C<0x20> (space) to  C<0x7E> (tilde).  Characters outside
this range may or may not work as expected.  Multibyte characters, like UTF
C<U+060C> (ARABIC COMMA),   C<U+FF0C> (FULLWIDTH COMMA),  C<U+241B> (SYMBOL
FOR ESCAPE), C<U+2424> (SYMBOL FOR NEWLINE), C<U+FF02> (FULLWIDTH QUOTATION
MARK), and C<U+201C> (LEFT DOUBLE QUOTATION MARK) (to give some examples of
what might look promising) work for newer versions of perl for C<sep_char>,
and C<quote_char> but not for C<escape_char>.

If you use perl-5.8.2 or higher these three attributes are utf8-decoded, to
increase the likelihood of success. This way C<U+00FE> will be allowed as a
quote character.

=item *

A field in  C<CSV>  must be surrounded by double-quotes to make an embedded
double-quote, represented by a pair of consecutive double-quotes, valid. In
binary mode you may additionally use the sequence  C<"0> for representation
of a NULL byte. Using C<0x00> in binary mode is just as valid.

=item *

Several violations of the above specification may be lifted by passing some
options as attributes to the object constructor.

=back

=head1 METHODS

=head2 version
X<version>

(Class method) Returns the current module version.

=head2 new
X<new>

(Class method) Returns a new instance of class Text::CSV_XS. The attributes
are described by the (optional) hash ref C<\%attr>.

 my $csv = Text::CSV_XS->new ({ attributes ... });

The following attributes are available:

=head3 eol
X<eol>

 my $csv = Text::CSV_XS->new ({ eol => $/ });
           $csv->eol (undef);
 my $eol = $csv->eol;

The end-of-line string to add to rows for L</print> or the record separator
for L</getline>.

When not passed in a B<parser> instance,  the default behavior is to accept
C<\n>, C<\r>, and C<\r\n>, so it is probably safer to not specify C<eol> at
all. Passing C<undef> or the empty string behave the same.

When not passed in a B<generating> instance,  records are not terminated at
all, so it is probably wise to pass something you expect. A safe choice for
C<eol> on output is either C<$/> or C<\r\n>.

Common values for C<eol> are C<"\012"> (C<\n> or Line Feed),  C<"\015\012">
(C<\r\n> or Carriage Return, Line Feed),  and C<"\015">  (C<\r> or Carriage
Return). The L<C<eol>|/eol> attribute cannot exceed 7 (ASCII) characters.

If both C<$/> and L<C<eol>|/eol> equal C<"\015">, parsing lines that end on
only a Carriage Return without Line Feed, will be L</parse>d correct.

=head3 sep_char
X<sep_char>

 my $csv = Text::CSV_XS->new ({ sep_char => ";" });
         $csv->sep_char (";");
 my $c = $csv->sep_char;

The char used to separate fields, by default a comma. (C<,>).  Limited to a
single-byte character, usually in the range from C<0x20> (space) to C<0x7E>
(tilde). When longer sequences are required, use L<C<sep>|/sep>.

The separation character can not be equal to the quote character  or to the
escape character.

See also L</CAVEATS>

=head3 sep
X<sep>

 my $csv = Text::CSV_XS->new ({ sep => "\N{FULLWIDTH COMMA}" });
           $csv->sep (";");
 my $sep = $csv->sep;

The chars used to separate fields, by default undefined. Limited to 8 bytes.

When set, overrules L<C<sep_char>|/sep_char>.  If its length is one byte it
acts as an alias to L<C<sep_char>|/sep_char>.

See also L</CAVEATS>

=head3 quote_char
X<quote_char>

 my $csv = Text::CSV_XS->new ({ quote_char => "'" });
         $csv->quote_char (undef);
 my $c = $csv->quote_char;

The character to quote fields containing blanks or binary data,  by default
the double quote character (C<">).  A value of undef suppresses quote chars
(for simple cases only). Limited to a single-byte character, usually in the
range from  C<0x20> (space) to  C<0x7E> (tilde).  When longer sequences are
required, use L<C<quote>|/quote>.

C<quote_char> can not be equal to L<C<sep_char>|/sep_char>.

=head3 quote
X<quote>

 my $csv = Text::CSV_XS->new ({ quote => "\N{FULLWIDTH QUOTATION MARK}" });
             $csv->quote ("'");
 my $quote = $csv->quote;

The chars used to quote fields, by default undefined. Limited to 8 bytes.

When set, overrules L<C<quote_char>|/quote_char>. If its length is one byte
it acts as an alias to L<C<quote_char>|/quote_char>.

See also L</CAVEATS>

=head3 escape_char
X<escape_char>

 my $csv = Text::CSV_XS->new ({ escape_char => "\\" });
         $csv->escape_char (":");
 my $c = $csv->escape_char;

The character to  escape  certain characters inside quoted fields.  This is
limited to a  single-byte  character,  usually  in the  range from  C<0x20>
(space) to C<0x7E> (tilde).

The C<escape_char> defaults to being the double-quote mark (C<">). In other
words the same as the default L<C<quote_char>|/quote_char>. This means that
doubling the quote mark in a field escapes it:

 "foo","bar","Escape ""quote mark"" with two ""quote marks""","baz"

If  you  change  the   L<C<quote_char>|/quote_char>  without  changing  the
C<escape_char>,  the  C<escape_char> will still be the double-quote (C<">).
If instead you want to escape the  L<C<quote_char>|/quote_char> by doubling
it you will need to also change the  C<escape_char>  to be the same as what
you have changed the L<C<quote_char>|/quote_char> to.

Setting C<escape_char> to <undef> or C<""> will disable escaping completely
and is greatly discouraged. This will also disable C<escape_null>.

The escape character can not be equal to the separation character.

=head3 binary
X<binary>

 my $csv = Text::CSV_XS->new ({ binary => 1 });
         $csv->binary (0);
 my $f = $csv->binary;

If this attribute is C<1>,  you may use binary characters in quoted fields,
including line feeds, carriage returns and C<NULL> bytes. (The latter could
be escaped as C<"0>.) By default this feature is off.

If a string is marked UTF8,  C<binary> will be turned on automatically when
binary characters other than C<CR> and C<NL> are encountered.   Note that a
simple string like C<"\x{00a0}"> might still be binary, but not marked UTF8,
so setting C<< { binary => 1 } >> is still a wise option.

=head3 strict
X<strict>

 my $csv = Text::CSV_XS->new ({ strict => 1 });
         $csv->strict (0);
 my $f = $csv->strict;

If this attribute is set to C<1>, any row that parses to a different number
of fields than the previous row will cause the parser to throw error 2014.

=head3 formula_handling

=head3 formula
X<formula_handling>
X<formula>

 my $csv = Text::CSV_XS->new ({ formula => "none" });
         $csv->formula ("none");
 my $f = $csv->formula;

This defines the behavior of fields containing I<formulas>. As formulas are
considered dangerous in spreadsheets, this attribute can define an optional
action to be taken if a field starts with an equal sign (C<=>).

For purpose of code-readability, this can also be written as

 my $csv = Text::CSV_XS->new ({ formula_handling => "none" });
         $csv->formula_handling ("none");
 my $f = $csv->formula_handling;

Possible values for this attribute are

=over 2

=item none

Take no specific action. This is the default.

 $csv->formula ("none");

=item die

Cause the process to C<die> whenever a leading C<=> is encountered.

 $csv->formula ("die");

=item croak

Cause the process to C<croak> whenever a leading C<=> is encountered.  (See
L<Carp>)

 $csv->formula ("croak");

=item diag

Report position and content of the field whenever a leading  C<=> is found.
The value of the field is unchanged.

 $csv->formula ("diag");

=item empty

Replace the content of fields that start with a C<=> with the empty string.

 $csv->formula ("empty");
 $csv->formula ("");

=item undef

Replace the content of fields that start with a C<=> with C<undef>.

 $csv->formula ("undef");
 $csv->formula (undef);

=back

All other values will give a warning and then fallback to C<diag>.

=head3 decode_utf8
X<decode_utf8>

 my $csv = Text::CSV_XS->new ({ decode_utf8 => 1 });
         $csv->decode_utf8 (0);
 my $f = $csv->decode_utf8;

This attributes defaults to TRUE.

While I<parsing>,  fields that are valid UTF-8, are automatically set to be
UTF-8, so that

  $csv->parse ("\xC4\xA8\n");

results in

  PV("\304\250"\0) [UTF8 "\x{128}"]

Sometimes it might not be a desired action.  To prevent those upgrades, set
this attribute to false, and the result will be

  PV("\304\250"\0)

=head3 auto_diag
X<auto_diag>

 my $csv = Text::CSV_XS->new ({ auto_diag => 1 });
         $csv->auto_diag (2);
 my $l = $csv->auto_diag;

Set this attribute to a number between C<1> and C<9> causes  L</error_diag>
to be automatically called in void context upon errors.

In case of error C<2012 - EOF>, this call will be void.

If C<auto_diag> is set to a numeric value greater than C<1>, it will C<die>
on errors instead of C<warn>.  If set to anything unrecognized,  it will be
silently ignored.

Future extensions to this feature will include more reliable auto-detection
of  C<autodie>  being active in the scope of which the error occurred which
will increment the value of C<auto_diag> with  C<1> the moment the error is
detected.

=head3 diag_verbose
X<diag_verbose>

 my $csv = Text::CSV_XS->new ({ diag_verbose => 1 });
         $csv->diag_verbose (2);
 my $l = $csv->diag_verbose;

Set the verbosity of the output triggered by C<auto_diag>.   Currently only
adds the current  input-record-number  (if known)  to the diagnostic output
with an indication of the position of the error.

=head3 blank_is_undef
X<blank_is_undef>

 my $csv = Text::CSV_XS->new ({ blank_is_undef => 1 });
         $csv->blank_is_undef (0);
 my $f = $csv->blank_is_undef;

Under normal circumstances, C<CSV> data makes no distinction between quoted-
and unquoted empty fields.  These both end up in an empty string field once
read, thus

 1,"",," ",2

is read as

 ("1", "", "", " ", "2")

When I<writing>  C<CSV> files with either  L<C<always_quote>|/always_quote>
or  L<C<quote_empty>|/quote_empty> set, the unquoted  I<empty> field is the
result of an undefined value.   To enable this distinction when  I<reading>
C<CSV>  data,  the  C<blank_is_undef>  attribute will cause  unquoted empty
fields to be set to C<undef>, causing the above to be parsed as

 ("1", "", undef, " ", "2")

note that this is specifically important when loading  C<CSV> fields into a
database that allows C<NULL> values,  as the perl equivalent for C<NULL> is
C<undef> in L<DBI> land.

=head3 empty_is_undef
X<empty_is_undef>

 my $csv = Text::CSV_XS->new ({ empty_is_undef => 1 });
         $csv->empty_is_undef (0);
 my $f = $csv->empty_is_undef;

Going one  step  further  than  L<C<blank_is_undef>|/blank_is_undef>,  this
attribute converts all empty fields to C<undef>, so

 1,"",," ",2

is read as

 (1, undef, undef, " ", 2)

Note that this effects only fields that are  originally  empty,  not fields
that are empty after stripping allowed whitespace. YMMV.

=head3 allow_whitespace
X<allow_whitespace>

 my $csv = Text::CSV_XS->new ({ allow_whitespace => 1 });
         $csv->allow_whitespace (0);
 my $f = $csv->allow_whitespace;

When this option is set to true,  the whitespace  (C<TAB>'s and C<SPACE>'s)
surrounding  the  separation character  is removed when parsing.  If either
C<TAB> or C<SPACE> is one of the three characters L<C<sep_char>|/sep_char>,
L<C<quote_char>|/quote_char>, or L<C<escape_char>|/escape_char> it will not
be considered whitespace.

Now lines like:

 1 , "foo" , bar , 3 , zapp

are parsed as valid C<CSV>, even though it violates the C<CSV> specs.

Note that  B<all>  whitespace is stripped from both  start and  end of each
field.  That would make it  I<more> than a I<feature> to enable parsing bad
C<CSV> lines, as

 1,   2.0,  3,   ape  , monkey

will now be parsed as

 ("1", "2.0", "3", "ape", "monkey")

even if the original line was perfectly acceptable C<CSV>.

=head3 allow_loose_quotes
X<allow_loose_quotes>

 my $csv = Text::CSV_XS->new ({ allow_loose_quotes => 1 });
         $csv->allow_loose_quotes (0);
 my $f = $csv->allow_loose_quotes;

By default, parsing unquoted fields containing L<C<quote_char>|/quote_char>
characters like

 1,foo "bar" baz,42

would result in parse error 2034.  Though it is still bad practice to allow
this format,  we  cannot  help  the  fact  that  some  vendors  make  their
applications spit out lines styled this way.

If there is B<really> bad C<CSV> data, like

 1,"foo "bar" baz",42

or

 1,""foo bar baz"",42

there is a way to get this data-line parsed and leave the quotes inside the
quoted field as-is.  This can be achieved by setting  C<allow_loose_quotes>
B<AND> making sure that the L<C<escape_char>|/escape_char> is  I<not> equal
to L<C<quote_char>|/quote_char>.

=head3 allow_loose_escapes
X<allow_loose_escapes>

 my $csv = Text::CSV_XS->new ({ allow_loose_escapes => 1 });
         $csv->allow_loose_escapes (0);
 my $f = $csv->allow_loose_escapes;

Parsing fields  that  have  L<C<escape_char>|/escape_char>  characters that
escape characters that do not need to be escaped, like:

 my $csv = Text::CSV_XS->new ({ escape_char => "\\" });
 $csv->parse (qq{1,"my bar\'s",baz,42});

would result in parse error 2025.   Though it is bad practice to allow this
format,  this attribute enables you to treat all escape character sequences
equal.

=head3 allow_unquoted_escape
X<allow_unquoted_escape>

 my $csv = Text::CSV_XS->new ({ allow_unquoted_escape => 1 });
         $csv->allow_unquoted_escape (0);
 my $f = $csv->allow_unquoted_escape;

A backward compatibility issue where L<C<escape_char>|/escape_char> differs
from L<C<quote_char>|/quote_char>  prevents  L<C<escape_char>|/escape_char>
to be in the first position of a field.  If L<C<quote_char>|/quote_char> is
equal to the default C<"> and L<C<escape_char>|/escape_char> is set to C<\>,
this would be illegal:

 1,\0,2

Setting this attribute to C<1>  might help to overcome issues with backward
compatibility and allow this style.

=head3 always_quote
X<always_quote>

 my $csv = Text::CSV_XS->new ({ always_quote => 1 });
         $csv->always_quote (0);
 my $f = $csv->always_quote;

By default the generated fields are quoted only if they I<need> to be.  For
example, if they contain the separator character. If you set this attribute
to C<1> then I<all> defined fields will be quoted. (C<undef> fields are not
quoted, see L</blank_is_undef>). This makes it quite often easier to handle
exported data in external applications.   (Poor creatures who are better to
use Text::CSV_XS. :)

=head3 quote_space
X<quote_space>

 my $csv = Text::CSV_XS->new ({ quote_space => 1 });
         $csv->quote_space (0);
 my $f = $csv->quote_space;

By default,  a space in a field would trigger quotation.  As no rule exists
this to be forced in C<CSV>,  nor any for the opposite, the default is true
for safety.   You can exclude the space  from this trigger  by setting this
attribute to 0.

=head3 quote_empty
X<quote_empty>

 my $csv = Text::CSV_XS->new ({ quote_empty => 1 });
         $csv->quote_empty (0);
 my $f = $csv->quote_empty;

By default the generated fields are quoted only if they I<need> to be.   An
empty (defined) field does not need quotation. If you set this attribute to
C<1> then I<empty> defined fields will be quoted.  (C<undef> fields are not
quoted, see L</blank_is_undef>). See also L<C<always_quote>|/always_quote>.

=head3 quote_binary
X<quote_binary>

 my $csv = Text::CSV_XS->new ({ quote_binary => 1 });
         $csv->quote_binary (0);
 my $f = $csv->quote_binary;

By default,  all "unsafe" bytes inside a string cause the combined field to
be quoted.  By setting this attribute to C<0>, you can disable that trigger
for bytes >= C<0x7F>.

=head3 escape_null
X<escape_null>
X<quote_null>

 my $csv = Text::CSV_XS->new ({ escape_null => 1 });
         $csv->escape_null (0);
 my $f = $csv->escape_null;

By default, a C<NULL> byte in a field would be escaped. This option enables
you to treat the  C<NULL>  byte as a simple binary character in binary mode
(the C<< { binary => 1 } >> is set).  The default is true.  You can prevent
C<NULL> escapes by setting this attribute to C<0>.

When the C<escape_char> attribute is set to undefined,  this attribute will
be set to false.

The default setting will encode "=\x00=" as

 "="0="

With C<escape_null> set, this will result in

 "=\x00="

The default when using the C<csv> function is C<false>.

For backward compatibility reasons,  the deprecated old name  C<quote_null>
is still recognized.

=head3 keep_meta_info
X<keep_meta_info>

 my $csv = Text::CSV_XS->new ({ keep_meta_info => 1 });
         $csv->keep_meta_info (0);
 my $f = $csv->keep_meta_info;

By default, the parsing of input records is as simple and fast as possible.
However,  some parsing information - like quotation of the original field -
is lost in that process.  Setting this flag to true enables retrieving that
information after parsing with  the methods  L</meta_info>,  L</is_quoted>,
and L</is_binary> described below.  Default is false for performance.

If you set this attribute to a value greater than 9,   than you can control
output quotation style like it was used in the input of the the last parsed
record (unless quotation was added because of other reasons).

 my $csv = Text::CSV_XS->new ({
    binary         => 1,
    keep_meta_info => 1,
    quote_space    => 0,
    });

 my $row = $csv->parse (q{1,,"", ," ",f,"g","h""h",help,"help"});

 $csv->print (*STDOUT, \@row);
 # 1,,, , ,f,g,"h""h",help,help
 $csv->keep_meta_info (11);
 $csv->print (*STDOUT, \@row);
 # 1,,"", ," ",f,"g","h""h",help,"help"

=head3 undef_str
X<undef_str>

 my $csv = Text::CSV_XS->new ({ undef_str => "\\N" });
         $csv->undef_str (undef);
 my $s = $csv->undef_str;

This attribute optionally defines the output of undefined fields. The value
passed is not changed at all, so if it needs quotation, the quotation needs
to be included in the value of the attribute.  Use with caution, as passing
a value like  C<",",,,,""">  will for sure mess up your output. The default
for this attribute is C<undef>, meaning no special treatment.

This attribute is useful when exporting  CSV data  to be imported in custom
loaders, like for MySQL, that recognize special sequences for C<NULL> data.

This attribute has no meaning when parsing CSV data.

=head3 verbatim
X<verbatim>

 my $csv = Text::CSV_XS->new ({ verbatim => 1 });
         $csv->verbatim (0);
 my $f = $csv->verbatim;

This is a quite controversial attribute to set,  but makes some hard things
possible.

The rationale behind this attribute is to tell the parser that the normally
special characters newline (C<NL>) and Carriage Return (C<CR>)  will not be
special when this flag is set,  and be dealt with  as being ordinary binary
characters. This will ease working with data with embedded newlines.

When  C<verbatim>  is used with  L</getline>,  L</getline>  auto-C<chomp>'s
every line.

Imagine a file format like

 M^^Hans^Janssen^Klas 2\n2A^Ja^11-06-2007#\r\n

where, the line ending is a very specific C<"#\r\n">, and the sep_char is a
C<^> (caret).   None of the fields is quoted,   but embedded binary data is
likely to be present. With the specific line ending, this should not be too
hard to detect.

By default,  Text::CSV_XS'  parse function is instructed to only know about
C<"\n"> and C<"\r">  to be legal line endings,  and so has to deal with the
embedded newline as a real C<end-of-line>,  so it can scan the next line if
binary is true, and the newline is inside a quoted field. With this option,
we tell L</parse> to parse the line as if C<"\n"> is just nothing more than
a binary character.

For L</parse> this means that the parser has no more idea about line ending
and L</getline> C<chomp>s line endings on reading.

=head3 types

A set of column types; the attribute is immediately passed to the L</types>
method.

=head3 callbacks
X<callbacks>

See the L</Callbacks> section below.

=head3 accessors

To sum it up,

 $csv = Text::CSV_XS->new ();

is equivalent to

 $csv = Text::CSV_XS->new ({
     eol                   => undef, # \r, \n, or \r\n
     sep_char              => ',',
     sep                   => undef,
     quote_char            => '"',
     quote                 => undef,
     escape_char           => '"',
     binary                => 0,
     decode_utf8           => 1,
     auto_diag             => 0,
     diag_verbose          => 0,
     blank_is_undef        => 0,
     empty_is_undef        => 0,
     allow_whitespace      => 0,
     allow_loose_quotes    => 0,
     allow_loose_escapes   => 0,
     allow_unquoted_escape => 0,
     always_quote          => 0,
     quote_empty           => 0,
     quote_space           => 1,
     escape_null           => 1,
     quote_binary          => 1,
     keep_meta_info        => 0,
     strict                => 0,
     formula               => 0,
     verbatim              => 0,
     undef_str             => undef,
     types                 => undef,
     callbacks             => undef,
     });

For all of the above mentioned flags, an accessor method is available where
you can inquire the current value, or change the value

 my $quote = $csv->quote_char;
 $csv->binary (1);

It is not wise to change these settings halfway through writing C<CSV> data
to a stream. If however you want to create a new stream using the available
C<CSV> object, there is no harm in changing them.

If the L</new> constructor call fails,  it returns C<undef>,  and makes the
fail reason available through the L</error_diag> method.

 $csv = Text::CSV_XS->new ({ ecs_char => 1 }) or
     die "".Text::CSV_XS->error_diag ();

L</error_diag> will return a string like

 "INI - Unknown attribute 'ecs_char'"

=head2 known_attributes
X<known_attributes>

 @attr = Text::CSV_XS->known_attributes;
 @attr = Text::CSV_XS::known_attributes;
 @attr = $csv->known_attributes;

This method will return an ordered list of all the supported  attributes as
described above.   This can be useful for knowing what attributes are valid
in classes that use or extend Text::CSV_XS.

=head2 print
X<print>

 $status = $csv->print ($fh, $colref);

Similar to  L</combine> + L</string> + L</print>,  but much more efficient.
It expects an array ref as input  (not an array!)  and the resulting string
is not really  created,  but  immediately  written  to the  C<$fh>  object,
typically an IO handle or any other object that offers a L</print> method.

For performance reasons  C<print>  does not create a result string,  so all
L</string>, L</status>, L</fields>, and L</error_input> methods will return
undefined information after executing this method.

If C<$colref> is C<undef>  (explicit,  not through a variable argument) and
L</bind_columns>  was used to specify fields to be printed,  it is possible
to make performance improvements, as otherwise data would have to be copied
as arguments to the method call:

 $csv->bind_columns (\($foo, $bar));
 $status = $csv->print ($fh, undef);

A short benchmark

 my @data = ("aa" .. "zz");
 $csv->bind_columns (\(@data));

 $csv->print ($fh, [ @data ]);   # 11800 recs/sec
 $csv->print ($fh,  \@data  );   # 57600 recs/sec
 $csv->print ($fh,   undef  );   # 48500 recs/sec

=head2 say
X<say>

 $status = $csv->say ($fh, $colref);

Like L<C<print>|/print>, but L<C<eol>|/eol> defaults to C<$\>.

=head2 print_hr
X<print_hr>

 $csv->print_hr ($fh, $ref);

Provides an easy way  to print a  C<$ref>  (as fetched with L</getline_hr>)
provided the column names are set with L</column_names>.

It is just a wrapper method with basic parameter checks over

 $csv->print ($fh, [ map { $ref->{$_} } $csv->column_names ]);

=head2 combine
X<combine>

 $status = $csv->combine (@fields);

This method constructs a C<CSV> record from  C<@fields>,  returning success
or failure.   Failure can result from lack of arguments or an argument that
contains an invalid character.   Upon success,  L</string> can be called to
retrieve the resultant C<CSV> string.  Upon failure,  the value returned by
L</string> is undefined and L</error_input> could be called to retrieve the
invalid argument.

=head2 string
X<string>

 $line = $csv->string ();

This method returns the input to  L</parse>  or the resultant C<CSV> string
of L</combine>, whichever was called more recently.

=head2 getline
X<getline>

 $colref = $csv->getline ($fh);

This is the counterpart to  L</print>,  as L</parse>  is the counterpart to
L</combine>:  it parses a row from the C<$fh>  handle using the L</getline>
method associated with C<$fh>  and parses this row into an array ref.  This
array ref is returned by the function or C<undef> for failure.  When C<$fh>
does not support C<getline>, you are likely to hit errors.

When fields are bound with L</bind_columns> the return value is a reference
to an empty list.

The L</string>, L</fields>, and L</status> methods are meaningless again.

=head2 getline_all
X<getline_all>

 $arrayref = $csv->getline_all ($fh);
 $arrayref = $csv->getline_all ($fh, $offset);
 $arrayref = $csv->getline_all ($fh, $offset, $length);

This will return a reference to a list of L<getline ($fh)|/getline> results.
In this call, C<keep_meta_info> is disabled.  If C<$offset> is negative, as
with C<splice>, only the last  C<abs ($offset)> records of C<$fh> are taken
into consideration.

Given a CSV file with 10 lines:

 lines call
 ----- ---------------------------------------------------------
 0..9  $csv->getline_all ($fh)         # all
 0..9  $csv->getline_all ($fh,  0)     # all
 8..9  $csv->getline_all ($fh,  8)     # start at 8
 -     $csv->getline_all ($fh,  0,  0) # start at 0 first 0 rows
 0..4  $csv->getline_all ($fh,  0,  5) # start at 0 first 5 rows
 4..5  $csv->getline_all ($fh,  4,  2) # start at 4 first 2 rows
 8..9  $csv->getline_all ($fh, -2)     # last 2 rows
 6..7  $csv->getline_all ($fh, -4,  2) # first 2 of last  4 rows

=head2 getline_hr
X<getline_hr>

The L</getline_hr> and L</column_names> methods work together  to allow you
to have rows returned as hashrefs.  You must call L</column_names> first to
declare your column names.

 $csv->column_names (qw( code name price description ));
 $hr = $csv->getline_hr ($fh);
 print "Price for $hr->{name} is $hr->{price} EUR\n";

L</getline_hr> will croak if called before L</column_names>.

Note that  L</getline_hr>  creates a hashref for every row and will be much
slower than the combined use of L</bind_columns>  and L</getline> but still
offering the same ease of use hashref inside the loop:

 my @cols = @{$csv->getline ($fh)};
 $csv->column_names (@cols);
 while (my $row = $csv->getline_hr ($fh)) {
     print $row->{price};
     }

Could easily be rewritten to the much faster:

 my @cols = @{$csv->getline ($fh)};
 my $row = {};
 $csv->bind_columns (\@{$row}{@cols});
 while ($csv->getline ($fh)) {
     print $row->{price};
     }

Your mileage may vary for the size of the data and the number of rows. With
perl-5.14.2 the comparison for a 100_000 line file with 14 rows:

            Rate hashrefs getlines
 hashrefs 1.00/s       --     -76%
 getlines 4.15/s     313%       --

=head2 getline_hr_all
X<getline_hr_all>

 $arrayref = $csv->getline_hr_all ($fh);
 $arrayref = $csv->getline_hr_all ($fh, $offset);
 $arrayref = $csv->getline_hr_all ($fh, $offset, $length);

This will return a reference to a list of   L<getline_hr ($fh)|/getline_hr>
results.  In this call, L<C<keep_meta_info>|/keep_meta_info> is disabled.

=head2 parse
X<parse>

 $status = $csv->parse ($line);

This method decomposes a  C<CSV>  string into fields,  returning success or
failure.   Failure can result from a lack of argument  or the given  C<CSV>
string is improperly formatted.   Upon success, L</fields> can be called to
retrieve the decomposed fields. Upon failure calling L</fields> will return
undefined data and  L</error_input>  can be called to retrieve  the invalid
argument.

You may use the L</types>  method for setting column types.  See L</types>'
description below.

The C<$line> argument is supposed to be a simple scalar. Everything else is
supposed to croak and set error 1500.

=head2 fragment
X<fragment>

This function tries to implement RFC7111  (URI Fragment Identifiers for the
text/csv Media Type) - http://tools.ietf.org/html/rfc7111

 my $AoA = $csv->fragment ($fh, $spec);

In specifications,  C<*> is used to specify the I<last> item, a dash (C<->)
to indicate a range.   All indices are C<1>-based:  the first row or column
has index C<1>. Selections can be combined with the semi-colon (C<;>).

When using this method in combination with  L</column_names>,  the returned
reference  will point to a  list of hashes  instead of a  list of lists.  A
disjointed  cell-based combined selection  might return rows with different
number of columns making the use of hashes unpredictable.

 $csv->column_names ("Name", "Age");
 my $AoH = $csv->fragment ($fh, "col=3;8");

If the L</after_parse> callback is active,  it is also called on every line
parsed and skipped before the fragment.

=over 2

=item row

 row=4
 row=5-7
 row=6-*
 row=1-2;4;6-*

=item col

 col=2
 col=1-3
 col=4-*
 col=1-2;4;7-*

=item cell

In cell-based selection, the comma (C<,>) is used to pair row and column

 cell=4,1

The range operator (C<->) using C<cell>s can be used to define top-left and
bottom-right C<cell> location

 cell=3,1-4,6

The C<*> is only allowed in the second part of a pair

 cell=3,2-*,2    # row 3 till end, only column 2
 cell=3,2-3,*    # column 2 till end, only row 3
 cell=3,2-*,*    # strip row 1 and 2, and column 1

Cells and cell ranges may be combined with C<;>, possibly resulting in rows
with different number of columns

 cell=1,1-2,2;3,3-4,4;1,4;4,1

Disjointed selections will only return selected cells.   The cells that are
not  specified  will  not  be  included  in the  returned set,  not even as
C<undef>.  As an example given a C<CSV> like

 11,12,13,...19
 21,22,...28,29
 :            :
 91,...97,98,99

with C<cell=1,1-2,2;3,3-4,4;1,4;4,1> will return:

 11,12,14
 21,22
 33,34
 41,43,44

Overlapping cell-specs will return those cells only once, So
C<cell=1,1-3,3;2,2-4,4;2,3;4,2> will return:

 11,12,13
 21,22,23,24
 31,32,33,34
 42,43,44

=back

L<RFC7111|http://tools.ietf.org/html/rfc7111> does  B<not>  allow different
types of specs to be combined   (either C<row> I<or> C<col> I<or> C<cell>).
Passing an invalid fragment specification will croak and set error 2013.

=head2 column_names
X<column_names>

Set the "keys" that will be used in the  L</getline_hr>  calls.  If no keys
(column names) are passed, it will return the current setting as a list.

L</column_names> accepts a list of scalars  (the column names)  or a single
array_ref, so you can pass the return value from L</getline> too:

 $csv->column_names ($csv->getline ($fh));

L</column_names> does B<no> checking on duplicates at all, which might lead
to unexpected results.   Undefined entries will be replaced with the string
C<"\cAUNDEF\cA">, so

 $csv->column_names (undef, "", "name", "name");
 $hr = $csv->getline_hr ($fh);

Will set C<< $hr->{"\cAUNDEF\cA"} >> to the 1st field,  C<< $hr->{""} >> to
the 2nd field, and C<< $hr->{name} >> to the 4th field,  discarding the 3rd
field.

L</column_names> croaks on invalid arguments.

=head2 header

This method does NOT work in perl-5.6.x

Parse the CSV header and set L<C<sep>|/sep>, column_names and encoding.

 my @hdr = $csv->header ($fh);
 $csv->header ($fh, { sep_set => [ ";", ",", "|", "\t" ] });
 $csv->header ($fh, { detect_bom => 1, munge_column_names => "lc" });

The first argument should be a file handle.

This method resets some object properties,  as it is supposed to be invoked
only once per file or stream.  It will leave attributes C<column_names> and
C<bound_columns> alone of setting column names is disabled. Reading headers
on previously process objects might fail on perl-5.8.0 and older.

Assuming that the file opened for parsing has a header, and the header does
not contain problematic characters like embedded newlines,   read the first
line from the open handle then auto-detect whether the header separates the
column names with a character from the allowed separator list.

If any of the allowed separators matches,  and none of the I<other> allowed
separators match,  set  L<C<sep>|/sep>  to that  separator  for the current
CSV_XS instance and use it to parse the first line, map those to lowercase,
and use that to set the instance L</column_names>:

 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<", "file.csv";
 binmode $fh; # for Windows
 $csv->header ($fh);
 while (my $row = $csv->getline_hr ($fh)) {
     ...
     }

If the header is empty,  contains more than one unique separator out of the
allowed set,  contains empty fields,   or contains identical fields  (after
folding), it will croak with error 1010, 1011, 1012, or 1013 respectively.

If the header contains embedded newlines or is not valid  CSV  in any other
way, this method will croak and leave the parse error untouched.

A successful call to C<header>  will always set the  L<C<sep>|/sep>  of the
C<$csv> object. This behavior can not be disabled.

=head3 return value

On error this method will croak.

In list context,  the headers will be returned whether they are used to set
L</column_names> or not.

In scalar context, the instance itself is returned.  B<Note>: the values as
found in the header will effectively be  B<lost> if  C<set_column_names> is
false.

=head3 Options

=over 2

=item sep_set
X<sep_set>

 $csv->header ($fh, { sep_set => [ ";", ",", "|", "\t" ] });

The list of legal separators defaults to C<[ ";", "," ]> and can be changed
by this option.  As this is probably the most often used option,  it can be
passed on its own as an unnamed argument:

 $csv->header ($fh, [ ";", ",", "|", "\t", "::", "\x{2063}" ]);

Multi-byte  sequences are allowed,  both multi-character and  Unicode.  See
L<C<sep>|/sep>.

=item detect_bom
X<detect_bom>

 $csv->header ($fh, { detect_bom => 1 });

The default behavior is to detect if the header line starts with a BOM.  If
the header has a BOM, use that to set the encoding of C<$fh>.  This default
behavior can be disabled by passing a false value to C<detect_bom>.

Supported encodings from BOM are: UTF-8, UTF-16BE, UTF-16LE, UTF-32BE,  and
UTF-32LE. BOM's also support UTF-1, UTF-EBCDIC, SCSU, BOCU-1,  and GB-18030
but L<Encode> does not (yet). UTF-7 is not supported.

If a supported BOM was detected as start of the stream, it is stored in the
abject attribute C<ENCODING>.

 my $enc = $csv->{ENCODING};

The encoding is used with C<binmode> on C<$fh>.

If the handle was opened in a (correct) encoding,  this method will  B<not>
alter the encoding, as it checks the leading B<bytes> of the first line. In
case the stream starts with a decode BOM (C<U+FEFF>), C<{ENCODING}> will be
C<""> (empty) instead of the default C<undef>.

=item munge_column_names
X<munge_column_names>

This option offers the means to modify the column names into something that
is most useful to the application.   The default is to map all column names
to lower case.

 $csv->header ($fh, { munge_column_names => "lc" });

The following values are available:

  lc     - lower case
  uc     - upper case
  none   - do not change
  \%hash - supply a mapping
  \&cb   - supply a callback

Literal:

 $csv->header ($fh, { munge_column_names => "none" });

Hash:

 $csv->header ($fh, { munge_column_names => { foo => "sombrero" });

if a value does not exist, the original value is used unchanged

Callback:

 $csv->header ($fh, { munge_column_names => sub { fc } });
 $csv->header ($fh, { munge_column_names => sub { "column_".$col++ } });
 $csv->header ($fh, { munge_column_names => sub { lc (s/\W+/_/gr) } });

As this callback is called in a C<map>, you can use C<$_> directly.

=item set_column_names
X<set_column_names>

 $csv->header ($fh, { set_column_names => 1 });

The default is to set the instances column names using  L</column_names> if
the method is successful,  so subsequent calls to L</getline_hr> can return
a hash. Disable setting the header can be forced by using a false value for
this option.

As described in L</return value> above, content is lost in scalar context.

=back

=head3 Validation

When receiving CSV files from external sources,  this method can be used to
protect against changes in the layout by restricting to known headers  (and
typos in the header fields).

 my %known = (
     "record key" => "c_rec",
     "rec id"     => "c_rec",
     "id_rec"     => "c_rec",
     "kode"       => "code",
     "code"       => "code",
     "vaule"      => "value",
     "value"      => "value",
     );
 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<", $source or die "$source: $!";
 $csv->header ($fh, { munge_column_names => sub {
     s/\s+$//;
     s/^\s+//;
     $known{lc $_} or die "Unknown column '$_' in $source";
     }});
 while (my $row = $csv->getline_hr ($fh)) {
     say join "\t", $row->{c_rec}, $row->{code}, $row->{value};
     }

=head2 bind_columns
X<bind_columns>

Takes a list of scalar references to be used for output with  L</print>  or
to store in the fields fetched by L</getline>.  When you do not pass enough
references to store the fetched fields in, L</getline> will fail with error
C<3006>.  If you pass more than there are fields to return,  the content of
the remaining references is left untouched.

 $csv->bind_columns (\$code, \$name, \$price, \$description);
 while ($csv->getline ($fh)) {
     print "The price of a $name is \x{20ac} $price\n";
     }

To reset or clear all column binding, call L</bind_columns> with the single
argument C<undef>. This will also clear column names.

 $csv->bind_columns (undef);

If no arguments are passed at all, L</bind_columns> will return the list of
current bindings or C<undef> if no binds are active.

Note that in parsing with  C<bind_columns>,  the fields are set on the fly.
That implies that if the third field of a row causes an error  (or this row
has just two fields where the previous row had more),  the first two fields
already have been assigned the values of the current row, while the rest of
the fields will still hold the values of the previous row.  If you want the
parser to fail in these cases, use the L<C<strict>|/strict> attribute.

=head2 eof
X<eof>

 $eof = $csv->eof ();

If L</parse> or  L</getline>  was used with an IO stream,  this method will
return true (1) if the last call hit end of file,  otherwise it will return
false ('').  This is useful to see the difference between a failure and end
of file.

Note that if the parsing of the last line caused an error,  C<eof> is still
true.  That means that if you are I<not> using L</auto_diag>, an idiom like

 while (my $row = $csv->getline ($fh)) {
     # ...
     }
 $csv->eof or $csv->error_diag;

will I<not> report the error. You would have to change that to

 while (my $row = $csv->getline ($fh)) {
     # ...
     }
 +$csv->error_diag and $csv->error_diag;

=head2 types
X<types>

 $csv->types (\@tref);

This method is used to force that  (all)  columns are of a given type.  For
example, if you have an integer column,  two  columns  with  doubles  and a
string column, then you might do a

 $csv->types ([Text::CSV_XS::IV (),
               Text::CSV_XS::NV (),
               Text::CSV_XS::NV (),
               Text::CSV_XS::PV ()]);

Column types are used only for I<decoding> columns while parsing,  in other
words by the L</parse> and L</getline> methods.

You can unset column types by doing a

 $csv->types (undef);

or fetch the current type settings with

 $types = $csv->types ();

=over 4

=item IV
X<IV>

Set field type to integer.

=item NV
X<NV>

Set field type to numeric/float.

=item PV
X<PV>

Set field type to string.

=back

=head2 fields
X<fields>

 @columns = $csv->fields ();

This method returns the input to   L</combine>  or the resultant decomposed
fields of a successful L</parse>, whichever was called more recently.

Note that the return value is undefined after using L</getline>, which does
not fill the data structures returned by L</parse>.

=head2 meta_info
X<meta_info>

 @flags = $csv->meta_info ();

This method returns the "flags" of the input to L</combine> or the flags of
the resultant  decomposed fields of  L</parse>,   whichever was called more
recently.

For each field,  a meta_info field will hold  flags that  inform  something
about  the  field  returned  by  the  L</fields>  method or  passed to  the
L</combine> method. The flags are bit-wise-C<or>'d like:

=over 2

=item C< >0x0001

The field was quoted.

=item C< >0x0002

The field was binary.

=back

See the C<is_***> methods below.

=head2 is_quoted
X<is_quoted>

 my $quoted = $csv->is_quoted ($column_idx);

Where  C<$column_idx> is the  (zero-based)  index of the column in the last
result of L</parse>.

This returns a true value  if the data in the indicated column was enclosed
in L<C<quote_char>|/quote_char> quotes.  This might be important for fields
where content C<,20070108,> is to be treated as a numeric value,  and where
C<,"20070108",> is explicitly marked as character string data.

This method is only valid when L</keep_meta_info> is set to a true value.

=head2 is_binary
X<is_binary>

 my $binary = $csv->is_binary ($column_idx);

Where  C<$column_idx> is the  (zero-based)  index of the column in the last
result of L</parse>.

This returns a true value if the data in the indicated column contained any
byte in the range C<[\x00-\x08,\x10-\x1F,\x7F-\xFF]>.

This method is only valid when L</keep_meta_info> is set to a true value.

=head2 is_missing
X<is_missing>

 my $missing = $csv->is_missing ($column_idx);

Where  C<$column_idx> is the  (zero-based)  index of the column in the last
result of L</getline_hr>.

 $csv->keep_meta_info (1);
 while (my $hr = $csv->getline_hr ($fh)) {
     $csv->is_missing (0) and next; # This was an empty line
     }

When using  L</getline_hr>,  it is impossible to tell if the  parsed fields
are C<undef> because they where not filled in the C<CSV> stream  or because
they were not read at all, as B<all> the fields defined by L</column_names>
are set in the hash-ref.    If you still need to know if all fields in each
row are provided, you should enable L<C<keep_meta_info>|/keep_meta_info> so
you can check the flags.

If  L<C<keep_meta_info>|/keep_meta_info>  is C<false>,  C<is_missing>  will
always return C<undef>, regardless of C<$column_idx> being valid or not. If
this attribute is C<true> it will return either C<0> (the field is present)
or C<1> (the field is missing).

A special case is the empty line.  If the line is completely empty -  after
dealing with the flags - this is still a valid CSV line:  it is a record of
just one single empty field. However, if C<keep_meta_info> is set, invoking
C<is_missing> with index C<0> will now return true.

=head2 status
X<status>

 $status = $csv->status ();

This method returns the status of the last invoked L</combine> or L</parse>
call. Status is success (true: C<1>) or failure (false: C<undef> or C<0>).

=head2 error_input
X<error_input>

 $bad_argument = $csv->error_input ();

This method returns the erroneous argument (if it exists) of L</combine> or
L</parse>,  whichever was called more recently.  If the last invocation was
successful, C<error_input> will return C<undef>.

=head2 error_diag
X<error_diag>

 Text::CSV_XS->error_diag ();
 $csv->error_diag ();
 $error_code               = 0  + $csv->error_diag ();
 $error_str                = "" . $csv->error_diag ();
 ($cde, $str, $pos, $rec, $fld) = $csv->error_diag ();

If (and only if) an error occurred,  this function returns  the diagnostics
of that error.

If called in void context,  this will print the internal error code and the
associated error message to STDERR.

If called in list context,  this will return  the error code  and the error
message in that order.  If the last error was from parsing, the rest of the
values returned are a best guess at the location  within the line  that was
being parsed. Their values are 1-based.  The position currently is index of
the byte at which the parsing failed in the current record. It might change
to be the index of the current character in a later release. The records is
the index of the record parsed by the csv instance. The field number is the
index of the field the parser thinks it is currently  trying to  parse. See
F<examples/csv-check> for how this can be used.

If called in  scalar context,  it will return  the diagnostics  in a single
scalar, a-la C<$!>.  It will contain the error code in numeric context, and
the diagnostics message in string context.

When called as a class method or a  direct function call,  the  diagnostics
are that of the last L</new> call.

=head2 record_number
X<record_number>

 $recno = $csv->record_number ();

Returns the records parsed by this csv instance.  This value should be more
accurate than C<$.> when embedded newlines come in play. Records written by
this instance are not counted.

=head2 SetDiag
X<SetDiag>

 $csv->SetDiag (0);

Use to reset the diagnostics if you are dealing with errors.

=head1 FUNCTIONS

=head2 csv
X<csv>

This function is not exported by default and should be explicitly requested:

 use Text::CSV_XS qw( csv );

This is an high-level function that aims at simple (user) interfaces.  This
can be used to read/parse a C<CSV> file or stream (the default behavior) or
to produce a file or write to a stream (define the  C<out>  attribute).  It
returns an array- or hash-reference on parsing (or C<undef> on fail) or the
numeric value of  L</error_diag>  on writing.  When this function fails you
can get to the error using the class call to L</error_diag>

 my $aoa = csv (in => "test.csv") or
     die Text::CSV_XS->error_diag;

This function takes the arguments as key-value pairs. This can be passed as
a list or as an anonymous hash:

 my $aoa = csv (  in => "test.csv", sep_char => ";");
 my $aoh = csv ({ in => $fh, headers => "auto" });

The arguments passed consist of two parts:  the arguments to L</csv> itself
and the optional attributes to the  C<CSV>  object used inside the function
as enumerated and explained in L</new>.

If not overridden, the default option used for CSV is

 auto_diag   => 1
 escape_null => 0

The option that is always set and cannot be altered is

 binary      => 1

As this function will likely be used in one-liners,  it allows  C<quote> to
be abbreviated as C<quo>,  and  C<escape_char> to be abbreviated as  C<esc>
or C<escape>.

Alternative invocations:

 my $aoa = Text::CSV_XS::csv (in => "file.csv");

 my $csv = Text::CSV_XS->new ();
 my $aoa = $csv->csv (in => "file.csv");

In the latter case, the object attributes are used from the existing object
and the attribute arguments in the function call are ignored:

 my $csv = Text::CSV_XS->new ({ sep_char => ";" });
 my $aoh = $csv->csv (in => "file.csv", bom => 1);

will parse using C<;> as C<sep_char>, not C<,>.

=head3 in
X<in>

Used to specify the source.  C<in> can be a file name (e.g. C<"file.csv">),
which will be  opened for reading  and closed when finished,  a file handle
(e.g.  C<$fh> or C<FH>),  a reference to a glob (e.g. C<\*ARGV>),  the glob
itself (e.g. C<*STDIN>), or a reference to a scalar (e.g. C<\q{1,2,"csv"}>).

When used with L</out>, C<in> should be a reference to a CSV structure (AoA
or AoH)  or a CODE-ref that returns an array-reference or a hash-reference.
The code-ref will be invoked with no arguments.

 my $aoa = csv (in => "file.csv");

 open my $fh, "<", "file.csv";
 my $aoa = csv (in => $fh);

 my $csv = [ [qw( Foo Bar )], [ 1, 2 ], [ 2, 3 ]];
 my $err = csv (in => $csv, out => "file.csv");

If called in void context without the L</out> attribute, the resulting ref
will be used as input to a subsequent call to csv:

 csv (in => "file.csv", filter => { 2 => sub { length > 2 }})

will be a shortcut to

 csv (in => csv (in => "file.csv", filter => { 2 => sub { length > 2 }}))

where, in the absence of the C<out> attribute, this is a shortcut to

 csv (in  => csv (in => "file.csv", filter => { 2 => sub { length > 2 }}),
      out => *STDOUT)

=head3 out
X<out>

 csv (in => $aoa, out => "file.csv");
 csv (in => $aoa, out => $fh);
 csv (in => $aoa, out =>   STDOUT);
 csv (in => $aoa, out =>  *STDOUT);
 csv (in => $aoa, out => \*STDOUT);
 csv (in => $aoa, out => \my $data);
 csv (in => $aoa, out =>  undef);
 csv (in => $aoa, out => \"skip");

In output mode, the default CSV options when producing CSV are

 eol       => "\r\n"

The L</fragment> attribute is ignored in output mode.

C<out> can be a file name  (e.g.  C<"file.csv">),  which will be opened for
writing and closed when finished,  a file handle (e.g. C<$fh> or C<FH>),  a
reference to a glob (e.g. C<\*STDOUT>),  the glob itself (e.g. C<*STDOUT>),
or a reference to a scalar (e.g. C<\my $data>).

 csv (in => sub { $sth->fetch },            out => "dump.csv");
 csv (in => sub { $sth->fetchrow_hashref }, out => "dump.csv",
      headers => $sth->{NAME_lc});

When a code-ref is used for C<in>, the output is generated  per invocation,
so no buffering is involved. This implies that there is no size restriction
on the number of records. The C<csv> function ends when the coderef returns
a false value.

If C<out> is set to a reference of the literal string C<"skip">, the output
will be suppressed completely,  which might be useful in combination with a
filter for side effects only.

 my %cache;
 csv (in    => "dump.csv",
      out   => \"skip",
      on_in => sub { $cache{$_[1][1]}++ });

Currently,  setting C<out> to any false value  (C<undef>, C<"">, 0) will be
equivalent to C<\"skip">.

=head3 encoding
X<encoding>

If passed,  it should be an encoding accepted by the  C<:encoding()> option
to C<open>. There is no default value. This attribute does not work in perl
5.6.x.  C<encoding> can be abbreviated to C<enc> for ease of use in command
line invocations.

If C<encoding> is set to the literal value C<"auto">, the method L</header>
will be invoked on the opened stream to check if there is a BOM and set the
encoding accordingly.   This is equal to passing a true value in the option
L<C<detect_bom>|/detect_bom>.

=head3 detect_bom
X<detect_bom>

If  C<detect_bom>  is given, the method  L</header>  will be invoked on the
opened stream to check if there is a BOM and set the encoding accordingly.

C<detect_bom> can be abbreviated to C<bom>.

This is the same as setting L<C<encoding>|/encoding> to C<"auto">.

Note that as the method  L</header> is invoked,  its default is to also set
the headers.

=head3 headers
X<headers>

If this attribute is not given, the default behavior is to produce an array
of arrays.

If C<headers> is supplied,  it should be an anonymous list of column names,
an anonymous hashref, a coderef, or a literal flag:  C<auto>, C<lc>, C<uc>,
or C<skip>.

=over 2

=item skip
X<skip>

When C<skip> is used, the header will not be included in the output.

 my $aoa = csv (in => $fh, headers => "skip");

=item auto
X<auto>

If C<auto> is used, the first line of the C<CSV> source will be read as the
list of field headers and used to produce an array of hashes.

 my $aoh = csv (in => $fh, headers => "auto");

=item lc
X<lc>

If C<lc> is used,  the first line of the  C<CSV> source will be read as the
list of field headers mapped to  lower case and used to produce an array of
hashes. This is a variation of C<auto>.

 my $aoh = csv (in => $fh, headers => "lc");

=item uc
X<uc>

If C<uc> is used,  the first line of the  C<CSV> source will be read as the
list of field headers mapped to  upper case and used to produce an array of
hashes. This is a variation of C<auto>.

 my $aoh = csv (in => $fh, headers => "uc");

=item CODE
X<CODE>

If a coderef is used,  the first line of the  C<CSV> source will be read as
the list of mangled field headers in which each field is passed as the only
argument to the coderef. This list is used to produce an array of hashes.

 my $aoh = csv (in      => $fh,
                headers => sub { lc ($_[0]) =~ s/kode/code/gr });

this example is a variation of using C<lc> where all occurrences of C<kode>
are replaced with C<code>.

=item ARRAY
X<ARRAY>

If  C<headers>  is an anonymous list,  the entries in the list will be used
as field names. The first line is considered data instead of headers.

 my $aoh = csv (in => $fh, headers => [qw( Foo Bar )]);
 csv (in => $aoa, out => $fh, headers => [qw( code description price )]);

=item HASH
X<HASH>

If C<headers> is an hash reference, this implies C<auto>, but header fields
for that exist as key in the hashref will be replaced by the value for that
key. Given a CSV file like

 post-kode,city,name,id number,fubble
 1234AA,Duckstad,Donald,13,"X313DF"

using

 csv (headers => { "post-kode" => "pc", "id number" => "ID" }, ...

will return an entry like

 { pc     => "1234AA",
   city   => "Duckstad",
   name   => "Donald",
   ID     => "13",
   fubble => "X313DF",
   }

=back

See also L<C<munge_column_names>|/munge_column_names> and
L<C<set_column_names>|/set_column_names>.

=head3 munge_column_names
X<munge_column_names>

If C<munge_column_names> is set,  the method  L</header>  is invoked on the
opened stream with all matching arguments to detect and set the headers.

C<munge_column_names> can be abbreviated to C<munge>.

=head3 key
X<key>

If passed,  will default  L<C<headers>|/headers>  to C<"auto"> and return a
hashref instead of an array of hashes. Allowed values are simple scalars or
array-references where the first element is the joiner and the rest are the
fields to join to combine the key.

 my $ref = csv (in => "test.csv", key => "code");
 my $ref = csv (in => "test.csv", key => [ ":" => "code", "color" ]);

with test.csv like

 code,product,price,color
 1,pc,850,gray
 2,keyboard,12,white
 3,mouse,5,black

the first example will return

  { 1   => {
        code    => 1,
        color   => 'gray',
        price   => 850,
        product => 'pc'
        },
    2   => {
        code    => 2,
        color   => 'white',
        price   => 12,
        product => 'keyboard'
        },
    3   => {
        code    => 3,
        color   => 'black',
        price   => 5,
        product => 'mouse'
        }
    }

the second example will return

  { "1:gray"    => {
        code    => 1,
        color   => 'gray',
        price   => 850,
        product => 'pc'
        },
    "2:white"   => {
        code    => 2,
        color   => 'white',
        price   => 12,
        product => 'keyboard'
        },
    "3:black"   => {
        code    => 3,
        color   => 'black',
        price   => 5,
        product => 'mouse'
        }
    }

The C<key> attribute can be combined with L<C<headers>|/headers> for C<CSV>
date that has no header line, like

 my $ref = csv (
     in      => "foo.csv",
     headers => [qw( c_foo foo bar description stock )],
     key     =>     "c_foo",
     );

=head3 value
X<value>

Used to create key-value hashes.

Only allowed when C<key> is valid. A C<value> can be either a single column
label or an anonymous list of column labels.  In the first case,  the value
will be a simple scalar value, in the latter case, it will be a hashref.

 my $ref = csv (in => "test.csv", key   => "code",
                                  value => "price");
 my $ref = csv (in => "test.csv", key   => "code",
                                  value => [ "product", "price" ]);
 my $ref = csv (in => "test.csv", key   => [ ":" => "code", "color" ],
                                  value => "price");
 my $ref = csv (in => "test.csv", key   => [ ":" => "code", "color" ],
                                  value => [ "product", "price" ]);

with test.csv like

 code,product,price,color
 1,pc,850,gray
 2,keyboard,12,white
 3,mouse,5,black

the first example will return

  { 1 => 850,
    2 =>  12,
    3 =>   5,
    }

the second example will return

  { 1   => {
        price   => 850,
        product => 'pc'
        },
    2   => {
        price   => 12,
        product => 'keyboard'
        },
    3   => {
        price   => 5,
        product => 'mouse'
        }
    }

the third example will return

  { "1:gray"    => 850,
    "2:white"   =>  12,
    "3:black"   =>   5,
    }

the fourth example will return

  { "1:gray"    => {
        price   => 850,
        product => 'pc'
        },
    "2:white"   => {
        price   => 12,
        product => 'keyboard'
        },
    "3:black"   => {
        price   => 5,
        product => 'mouse'
        }
    }

=head3 keep_headers
X<keep_headers>
X<keep_column_names>
X<kh>

When using hashes,  keep the column names into the arrayref passed,  so all
headers are available after the call in the original order.

 my $aoh = csv (in => "file.csv", keep_headers => \my @hdr);

This attribute can be abbreviated to C<kh> or passed as C<keep_column_names>.

This attribute implies a default of C<auto> for the C<headers> attribute.

=head3 fragment
X<fragment>

Only output the fragment as defined in the L</fragment> method. This option
is ignored when I<generating> C<CSV>. See L</out>.

Combining all of them could give something like

 use Text::CSV_XS qw( csv );
 my $aoh = csv (
     in       => "test.txt",
     encoding => "utf-8",
     headers  => "auto",
     sep_char => "|",
     fragment => "row=3;6-9;15-*",
     );
 say $aoh->[15]{Foo};

=head3 sep_set
X<sep_set>
X<seps>

If C<sep_set> is set, the method L</header> is invoked on the opened stream
to detect and set L<C<sep_char>|/sep_char> with the given set.

C<sep_set> can be abbreviated to C<seps>.

Note that as the  L</header> method is invoked,  its default is to also set
the headers.

=head3 set_column_names
X<set_column_names>

If  C<set_column_names> is passed,  the method L</header> is invoked on the
opened stream with all arguments meant for L</header>.

If C<set_column_names> is passed as a false value, the content of the first
row is only preserved if the output is AoA:

With an input-file like

 bAr,foo
 1,2
 3,4,5

This call

 my $aoa = csv (in => $file, set_column_names => 0);

will result in

 [[ "bar", "foo"     ],
  [ "1",   "2"       ],
  [ "3",   "4",  "5" ]]

and

 my $aoa = csv (in => $file, set_column_names => 0, munge => "none");

will result in

 [[ "bAr", "foo"     ],
  [ "1",   "2"       ],
  [ "3",   "4",  "5" ]]

=head2 Callbacks
X<Callbacks>

Callbacks enable actions triggered from the I<inside> of Text::CSV_XS.

While most of what this enables  can easily be done in an  unrolled loop as
described in the L</SYNOPSIS> callbacks can be used to meet special demands
or enhance the L</csv> function.

=over 2

=item error
X<error>

 $csv->callbacks (error => sub { $csv->SetDiag (0) });

the C<error>  callback is invoked when an error occurs,  but  I<only>  when
L</auto_diag> is set to a true value. A callback is invoked with the values
returned by L</error_diag>:

 my ($c, $s);

 sub ignore3006
 {
     my ($err, $msg, $pos, $recno, $fldno) = @_;
     if ($err == 3006) {
         # ignore this error
         ($c, $s) = (undef, undef);
         Text::CSV_XS->SetDiag (0);
         }
     # Any other error
     return;
     } # ignore3006

 $csv->callbacks (error => \&ignore3006);
 $csv->bind_columns (\$c, \$s);
 while ($csv->getline ($fh)) {
     # Error 3006 will not stop the loop
     }

=item after_parse
X<after_parse>

 $csv->callbacks (after_parse => sub { push @{$_[1]}, "NEW" });
 while (my $row = $csv->getline ($fh)) {
     $row->[-1] eq "NEW";
     }

This callback is invoked after parsing with  L</getline>  only if no  error
occurred.  The callback is invoked with two arguments:   the current C<CSV>
parser object and an array reference to the fields parsed.

The return code of the callback is ignored  unless it is a reference to the
string "skip", in which case the record will be skipped in L</getline_all>.

 sub add_from_db
 {
     my ($csv, $row) = @_;
     $sth->execute ($row->[4]);
     push @$row, $sth->fetchrow_array;
     } # add_from_db

 my $aoa = csv (in => "file.csv", callbacks => {
     after_parse => \&add_from_db });

This hook can be used for validation:
X<validation>

=over 2

=item FAIL

Die if any of the records does not validate a rule:

 after_parse => sub {
     $_[1][4] =~ m/^[0-9]{4}\s?[A-Z]{2}$/ or
         die "5th field does not have a valid Dutch zipcode";
     }

=item DEFAULT

Replace invalid fields with a default value:

 after_parse => sub { $_[1][2] =~ m/^\d+$/ or $_[1][2] = 0 }

=item SKIP

Skip records that have invalid fields (only applies to L</getline_all>):

 after_parse => sub { $_[1][0] =~ m/^\d+$/ or return \"skip"; }

=back

=item before_print
X<before_print>

 my $idx = 1;
 $csv->callbacks (before_print => sub { $_[1][0] = $idx++ });
 $csv->print (*STDOUT, [ 0, $_ ]) for @members;

This callback is invoked  before printing with  L</print>  only if no error
occurred.  The callback is invoked with two arguments:  the current  C<CSV>
parser object and an array reference to the fields passed.

The return code of the callback is ignored.

 sub max_4_fields
 {
     my ($csv, $row) = @_;
     @$row > 4 and splice @$row, 4;
     } # max_4_fields

 csv (in => csv (in => "file.csv"), out => *STDOUT,
     callbacks => { before print => \&max_4_fields });

This callback is not active for L</combine>.

=back

=head3 Callbacks for csv ()

The L</csv> allows for some callbacks that do not integrate in XS internals
but only feature the L</csv> function.

  csv (in        => "file.csv",
       callbacks => {
           filter       => { 6 => sub { $_ > 15 } },    # first
           after_parse  => sub { say "AFTER PARSE";  }, # first
           after_in     => sub { say "AFTER IN";     }, # second
           on_in        => sub { say "ON IN";        }, # third
           },
       );

  csv (in        => $aoh,
       out       => "file.csv",
       callbacks => {
           on_in        => sub { say "ON IN";        }, # first
           before_out   => sub { say "BEFORE OUT";   }, # second
           before_print => sub { say "BEFORE PRINT"; }, # third
           },
       );

=over 2

=item filter
X<filter>

This callback can be used to filter records.  It is called just after a new
record has been scanned.  The callback accepts a:

=over 2

=item hashref

The keys are the index to the row (the field name or field number, 1-based)
and the values are subs to return a true or false value.

 csv (in => "file.csv", filter => {
            3 => sub { m/a/ },       # third field should contain an "a"
            5 => sub { length > 4 }, # length of the 5th field minimal 5
            });

 csv (in => "file.csv", filter => { foo => sub { $_ > 4 }});

If the keys to the filter hash contain any character that is not a digit it
will also implicitly set L</headers> to C<"auto">  unless  L</headers>  was
already passed as argument.  When headers are active, returning an array of
hashes, the filter is not applicable to the header itself.

All sub results should match, as in AND.

The context of the callback sets  C<$_> localized to the field indicated by
the filter. The two arguments are as with all other callbacks, so the other
fields in the current row can be seen:

 filter => { 3 => sub { $_ > 100 ? $_[1][1] =~ m/A/ : $_[1][6] =~ m/B/ }}

If the context is set to return a list of hashes  (L</headers> is defined),
the current record will also be available in the localized C<%_>:

 filter => { 3 => sub { $_ > 100 && $_{foo} =~ m/A/ && $_{bar} < 1000  }}

If the filter is used to I<alter> the content by changing C<$_>,  make sure
that the sub returns true in order not to have that record skipped:

 filter => { 2 => sub { $_ = uc }}

will upper-case the second field, and then skip it if the resulting content
evaluates to false. To always accept, end with truth:

 filter => { 2 => sub { $_ = uc; 1 }}

=item coderef

 csv (in => "file.csv", filter => sub { $n++; 0; });

If the argument to C<filter> is a coderef,  it is an alias or shortcut to a
filter on column 0:

 csv (filter => sub { $n++; 0 });

is equal to

 csv (filter => { 0 => sub { $n++; 0 });

=item filter-name

 csv (in => "file.csv", filter => "not_blank");
 csv (in => "file.csv", filter => "not_empty");
 csv (in => "file.csv", filter => "filled");

These are predefined filters

Given a file like (line numbers prefixed for doc purpose only):

 1:1,2,3
 2:
 3:,
 4:""
 5:,,
 6:, ,
 7:"",
 8:" "
 9:4,5,6

=over 2

=item not_blank

Filter out the blank lines

This filter is a shortcut for

 filter => { 0 => sub { @{$_[1]} > 1 or
             defined $_[1][0] && $_[1][0] ne "" } }

Due to the implementation,  it is currently impossible to also filter lines
that consists only of a quoted empty field. These lines are also considered
blank lines.

With the given example, lines 2 and 4 will be skipped.

=item not_empty

Filter out lines where all the fields are empty.

This filter is a shortcut for

 filter => { 0 => sub { grep { defined && $_ ne "" } @{$_[1]} } }

A space is not regarded being empty, so given the example data, lines 2, 3,
4, 5, and 7 are skipped.

=item filled

Filter out lines that have no visible data

This filter is a shortcut for

 filter => { 0 => sub { grep { defined && m/\S/ } @{$_[1]} } }

This filter rejects all lines that I<not> have at least one field that does
not evaluate to the empty string.

With the given example data, this filter would skip lines 2 through 8.

=back

=back

=item after_in
X<after_in>

This callback is invoked for each record after all records have been parsed
but before returning the reference to the caller.  The hook is invoked with
two arguments:  the current  C<CSV>  parser object  and a  reference to the
record.   The reference can be a reference to a  HASH  or a reference to an
ARRAY as determined by the arguments.

This callback can also be passed as  an attribute without the  C<callbacks>
wrapper.

=item before_out
X<before_out>

This callback is invoked for each record before the record is printed.  The
hook is invoked with two arguments:  the current C<CSV> parser object and a
reference to the record.   The reference can be a reference to a  HASH or a
reference to an ARRAY as determined by the arguments.

This callback can also be passed as an attribute  without the  C<callbacks>
wrapper.

This callback makes the row available in C<%_> if the row is a hashref.  In
this case C<%_> is writable and will change the original row.

=item on_in
X<on_in>

This callback acts exactly as the L</after_in> or the L</before_out> hooks.

This callback can also be passed as an attribute  without the  C<callbacks>
wrapper.

This callback makes the row available in C<%_> if the row is a hashref.  In
this case C<%_> is writable and will change the original row. So e.g. with

  my $aoh = csv (
      in      => \"foo\n1\n2\n",
      headers => "auto",
      on_in   => sub { $_{bar} = 2; },
      );

C<$aoh> will be:

  [ { foo => 1,
      bar => 2,
      }
    { foo => 2,
      bar => 2,
      }
    ]

=item csv

The I<function>  L</csv> can also be called as a method or with an existing
Text::CSV_XS object. This could help if the function is to be invoked a lot
of times and the overhead of creating the object internally over  and  over
again would be prevented by passing an existing instance.

 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });

 my $aoa = $csv->csv (in => $fh);
 my $aoa = csv (in => $fh, csv => $csv);

both act the same. Running this 20000 times on a 20 lines CSV file,  showed
a 53% speedup.

=back

=head1 INTERNALS

=over 4

=item Combine (...)

=item Parse (...)

=back

The arguments to these internal functions are deliberately not described or
documented in order to enable the  module authors make changes it when they
feel the need for it.  Using them is  highly  discouraged  as  the  API may
change in future releases.

=head1 EXAMPLES

=head2 Reading a CSV file line by line:

 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<", "file.csv" or die "file.csv: $!";
 while (my $row = $csv->getline ($fh)) {
     # do something with @$row
     }
 close $fh or die "file.csv: $!";

or

 my $aoa = csv (in => "file.csv", on_in => sub {
     # do something with %_
     });

=head3 Reading only a single column

 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<", "file.csv" or die "file.csv: $!";
 # get only the 4th column
 my @column = map { $_->[3] } @{$csv->getline_all ($fh)};
 close $fh or die "file.csv: $!";

with L</csv>, you could do

 my @column = map { $_->[0] }
     @{csv (in => "file.csv", fragment => "col=4")};

=head2 Parsing CSV strings:

 my $csv = Text::CSV_XS->new ({ keep_meta_info => 1, binary => 1 });

 my $sample_input_string =
     qq{"I said, ""Hi!""",Yes,"",2.34,,"1.09","\x{20ac}",};
 if ($csv->parse ($sample_input_string)) {
     my @field = $csv->fields;
     foreach my $col (0 .. $#field) {
         my $quo = $csv->is_quoted ($col) ? $csv->{quote_char} : "";
         printf "%2d: %s%s%s\n", $col, $quo, $field[$col], $quo;
         }
     }
 else {
     print STDERR "parse () failed on argument: ",
         $csv->error_input, "\n";
     $csv->error_diag ();
     }

=head3 Parsing CSV from memory

Given a complete CSV data-set in scalar C<$data>,  generate a list of lists
to represent the rows and fields

 # The data
 my $data = join "\r\n" => map { join "," => 0 .. 5 } 0 .. 5;

 # in a loop
 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1 });
 open my $fh, "<", \$data;
 my @foo;
 while (my $row = $csv->getline ($fh)) {
     push @foo, $row;
     }
 close $fh;

 # a single call
 my $foo = csv (in => \$data);

=head2 Printing CSV data

=head3 The fast way: using L</print>

An example for creating C<CSV> files using the L</print> method:

 my $csv = Text::CSV_XS->new ({ binary => 1, eol => $/ });
 open my $fh, ">", "foo.csv" or die "foo.csv: $!";
 for (1 .. 10) {
     $csv->print ($fh, [ $_, "$_" ]) or $csv->error_diag;
     }
 close $fh or die "$tbl.csv: $!";

=head3 The slow way: using L</combine> and L</string>

or using the slower L</combine> and L</string> methods:

 my $csv = Text::CSV_XS->new;

 open my $csv_fh, ">", "hello.csv" or die "hello.csv: $!";

 my @sample_input_fields = (
     'You said, "Hello!"',   5.67,
     '"Surely"',   '',   '3.14159');
 if ($csv->combine (@sample_input_fields)) {
     print $csv_fh $csv->string, "\n";
     }
 else {
     print "combine () failed on argument: ",
         $csv->error_input, "\n";
     }
 close $csv_fh or die "hello.csv: $!";

=head3 Generating CSV into memory

Format a data-set (C<@foo>) into a scalar value in memory (C<$data>):

 # The data
 my @foo = map { [ 0 .. 5 ] } 0 .. 3;

 # in a loop
 my $csv = Text::CSV_XS->new ({ binary => 1, auto_diag => 1, eol => "\r\n" });
 open my $fh, ">", \my $data;
 $csv->print ($fh, $_) for @foo;
 close $fh;

 # a single call
 csv (in => \@foo, out => \my $data);

=head2 Rewriting CSV

Rewrite C<CSV> files with C<;> as separator character to well-formed C<CSV>:

 use Text::CSV_XS qw( csv );
 csv (in => csv (in => "bad.csv", sep_char => ";"), out => *STDOUT);

As C<STDOUT> is now default in L</csv>, a one-liner converting a UTF-16 CSV
file with BOM and TAB-separation to valid UTF-8 CSV could be:

 $ perl -C3 -MText::CSV_XS=csv -we\
    'csv(in=>"utf16tab.csv",encoding=>"utf16",sep=>"\t")' >utf8.csv

=head2 Dumping database tables to CSV

Dumping a database table can be simple as this (TIMTOWTDI):

 my $dbh = DBI->connect (...);
 my $sql = "select * from foo";

 # using your own loop
 open my $fh, ">", "foo.csv" or die "foo.csv: $!\n";
 my $csv = Text::CSV_XS->new ({ binary => 1, eol => "\r\n" });
 my $sth = $dbh->prepare ($sql); $sth->execute;
 $csv->print ($fh, $sth->{NAME_lc});
 while (my $row = $sth->fetch) {
     $csv->print ($fh, $row);
     }

 # using the csv function, all in memory
 csv (out => "foo.csv", in => $dbh->selectall_arrayref ($sql));

 # using the csv function, streaming with callbacks
 my $sth = $dbh->prepare ($sql); $sth->execute;
 csv (out => "foo.csv", in => sub { $sth->fetch            });
 csv (out => "foo.csv", in => sub { $sth->fetchrow_hashref });

Note that this does not discriminate between "empty" values and NULL-values
from the database,  as both will be the same empty field in CSV.  To enable
distinction between the two, use L<C<quote_empty>|/quote_empty>.

 csv (out => "foo.csv", in => sub { $sth->fetch }, quote_empty => 1);

If the database import utility supports special sequences to insert C<NULL>
values into the database,  like MySQL/MariaDB supports C<\N>,  use a filter
or a map

 csv (out => "foo.csv", in => sub { $sth->fetch },
                     on_in => sub { $_ //= "\\N" for @{$_[1]} });

 while (my $row = $sth->fetch) {
     $csv->print ($fh, [ map { $_ // "\\N" } @$row ]);
     }

note that this will not work as expected when choosing the backslash (C<\>)
as C<escape_char>, as that will cause the C<\> to need to be escaped by yet
another C<\>,  which will cause the field to need quotation and thus ending
up as C<"\\N"> instead of C<\N>. See also L<C<undef_str>|/undef_str>.

 csv (out => "foo.csv", in => sub { $sth->fetch }, undef_str => "\\N");

these special sequences are not recognized by  Text::CSV_XS  on parsing the
CSV generated like this, but map and filter are your friends again

 while (my $row = $csv->getline ($fh)) {
     $sth->execute (map { $_ eq "\\N" ? undef : $_ } @$row);
     }

 csv (in => "foo.csv", filter => { 1 => sub {
     $sth->execute (map { $_ eq "\\N" ? undef : $_ } @{$_[1]}); 0; }});

=head2 The examples folder

For more extended examples, see the F<examples/> C<1>. sub-directory in the
original distribution or the git repository C<2>.

 1. https://github.com/Tux/Text-CSV_XS/tree/master/examples
 2. https://github.com/Tux/Text-CSV_XS

The following files can be found there:

=over 2

=item parser-xs.pl
X<parser-xs.pl>

This can be used as a boilerplate to parse invalid C<CSV>  and parse beyond
(expected) errors alternative to using the L</error> callback.

 $ perl examples/parser-xs.pl bad.csv >good.csv

=item csv-check
X<csv-check>

This is a command-line tool that uses parser-xs.pl  techniques to check the
C<CSV> file and report on its content.

 $ csv-check files/utf8.csv
 Checked files/utf8.csv  with csv-check 1.9
 using Text::CSV_XS 1.32 with perl 5.26.0 and Unicode 9.0.0
 OK: rows: 1, columns: 2
     sep = <,>, quo = <">, bin = <1>, eol = <"\n">

=item csv2xls
X<csv2xls>

A script to convert C<CSV> to Microsoft Excel (C<XLS>). This requires extra
modules L<Date::Calc> and L<Spreadsheet::WriteExcel>. The converter accepts
various options and can produce UTF-8 compliant Excel files.

=item csv2xlsx
X<csv2xlsx>

A script to convert C<CSV> to Microsoft Excel (C<XLSX>).  This requires the
modules L<Date::Calc> and L<Spreadsheet::Writer::XLSX>.  The converter does
accept various options including merging several C<CSV> files into a single
Excel file.

=item csvdiff
X<csvdiff>

A script that provides colorized diff on sorted CSV files,  assuming  first
line is header and first field is the key. Output options include colorized
ANSI escape codes or HTML.

 $ csvdiff --html --output=diff.html file1.csv file2.csv

=item rewrite.pl
X<rewrite.pl>

A script to rewrite (in)valid CSV into valid CSV files.  Script has options
to generate confusing CSV files or CSV files that conform to Dutch MS-Excel
exports (using C<;> as separation).

Script - by default - honors BOM  and auto-detects separation converting it
to default standard CSV with C<,> as separator.

=back

=head1 CAVEATS

Text::CSV_XS  is I<not> designed to detect the characters used to quote and
separate fields.  The parsing is done using predefined  (default) settings.
In the examples  sub-directory,  you can find scripts  that demonstrate how
you could try to detect these characters yourself.

=head2 Microsoft Excel

The import/export from Microsoft Excel is a I<risky task>, according to the
documentation in C<Text::CSV::Separator>.  Microsoft uses the system's list
separator defined in the regional settings, which happens to be a semicolon
for Dutch, German and Spanish (and probably some others as well).   For the
English locale,  the default is a comma.   In Windows however,  the user is
free to choose a  predefined locale,  and then change  I<every>  individual
setting in it, so checking the locale is no solution.

As of version 1.17, a lone first line with just

  sep=;

will be recognized and honored when parsing with L</getline>.

=head1 TODO

=over 2

=item More Errors & Warnings

New extensions ought to be  clear and concise  in reporting what  error has
occurred where and why, and maybe also offer a remedy to the problem.

L</error_diag> is a (very) good start, but there is more work to be done in
this area.

Basic calls  should croak or warn on  illegal parameters.  Errors should be
documented.

=item setting meta info

Future extensions might include extending the L</meta_info>, L</is_quoted>,
and  L</is_binary>  to accept setting these  flags for  fields,  so you can
specify which fields are quoted in the L</combine>/L</string> combination.

 $csv->meta_info (0, 1, 1, 3, 0, 0);
 $csv->is_quoted (3, 1);

L<Metadata Vocabulary for Tabular Data|http://w3c.github.io/csvw/metadata/>
(a W3C editor's draft) could be an example for supporting more metadata.

=item Parse the whole file at once

Implement new methods or functions  that enable parsing of a  complete file
at once, returning a list of hashes. Possible extension to this could be to
enable a column selection on the call:

 my @AoH = $csv->parse_file ($filename, { cols => [ 1, 4..8, 12 ]});

Returning something like

 [ { fields => [ 1, 2, "foo", 4.5, undef, "", 8 ],
     flags  => [ ... ],
     },
   { fields => [ ... ],
     .
     },
   ]

Note that the L</csv> function already supports most of this,  but does not
return flags. L</getline_all> returns all rows for an open stream, but this
will not return flags either.  L</fragment>  can reduce the  required  rows
I<or> columns, but cannot combine them.

=item Cookbook

Write a document that has recipes for  most known  non-standard  (and maybe
some standard)  C<CSV> formats,  including formats that use  C<TAB>,  C<;>,
C<|>, or other non-comma separators.

Examples could be taken from W3C's L<CSV on the Web: Use Cases and
Requirements|http://w3c.github.io/csvw/use-cases-and-requirements/index.html>

=item Steal

Steal good new ideas and features from L<PapaParse|http://papaparse.com> or
L<csvkit|http://csvkit.readthedocs.org>.

=item Perl6 support

I'm already working on perl6 support L<here|https://github.com/Tux/CSV>. No
promises yet on when it is finished (or fast). Trying to keep the API alike
as much as possible.

=back

=head2 NOT TODO

=over 2

=item combined methods

Requests for adding means (methods) that combine L</combine> and L</string>
in a single call will B<not> be honored (use L</print> instead).   Likewise
for L</parse> and L</fields>  (use L</getline> instead), given the problems
with embedded newlines.

=back

=head2 Release plan

No guarantees, but this is what I had in mind some time ago:

=over 2

=item *

DIAGNOSTICS section in pod to *describe* the errors (see below)

=back

=head1 EBCDIC

The current hard-coding of characters and character ranges  makes this code
unusable on C<EBCDIC> systems. Recent work in perl-5.20 might change that.

Opening C<EBCDIC> encoded files on  C<ASCII>+  systems is likely to succeed
using Encode's C<cp37>, C<cp1047>, or C<posix-bc>:

 open my $fh, "<:encoding(cp1047)", "ebcdic_file.csv" or die "...";

=head1 DIAGNOSTICS

Still under construction ...

If an error occurs,  C<< $csv->error_diag >> can be used to get information
on the cause of the failure. Note that for speed reasons the internal value
is never cleared on success,  so using the value returned by L</error_diag>
in normal cases - when no error occurred - may cause unexpected results.

If the constructor failed, the cause can be found using L</error_diag> as a
class method, like C<< Text::CSV_XS->error_diag >>.

The C<< $csv->error_diag >> method is automatically invoked upon error when
the contractor was called with  L<C<auto_diag>|/auto_diag>  set to  C<1> or
C<2>, or when L<autodie> is in effect.  When set to C<1>, this will cause a
C<warn> with the error message,  when set to C<2>, it will C<die>. C<2012 -
EOF> is excluded from L<C<auto_diag>|/auto_diag> reports.

Errors can be (individually) caught using the L</error> callback.

The errors as described below are available. I have tried to make the error
itself explanatory enough, but more descriptions will be added. For most of
these errors, the first three capitals describe the error category:

=over 2

=item *
INI

Initialization error or option conflict.

=item *
ECR

Carriage-Return related parse error.

=item *
EOF

End-Of-File related parse error.

=item *
EIQ

Parse error inside quotation.

=item *
EIF

Parse error inside field.

=item *
ECB

Combine error.

=item *
EHR

HashRef parse related error.

=back

And below should be the complete list of error codes that can be returned:

=over 2

=item *
1001 "INI - sep_char is equal to quote_char or escape_char"
X<1001>

The  L<separation character|/sep_char>  cannot be equal to  L<the quotation
character|/quote_char> or to L<the escape character|/escape_char>,  as this
would invalidate all parsing rules.

=item *
1002 "INI - allow_whitespace with escape_char or quote_char SP or TAB"
X<1002>

Using the  L<C<allow_whitespace>|/allow_whitespace>  attribute  when either
L<C<quote_char>|/quote_char> or L<C<escape_char>|/escape_char>  is equal to
C<SPACE> or C<TAB> is too ambiguous to allow.

=item *
1003 "INI - \r or \n in main attr not allowed"
X<1003>

Using default L<C<eol>|/eol> characters in either L<C<sep_char>|/sep_char>,
L<C<quote_char>|/quote_char>,   or  L<C<escape_char>|/escape_char>  is  not
allowed.

=item *
1004 "INI - callbacks should be undef or a hashref"
X<1004>

The L<C<callbacks>|/Callbacks>  attribute only allows one to be C<undef> or
a hash reference.

=item *
1005 "INI - EOL too long"
X<1005>

The value passed for EOL is exceeding its maximum length (16).

=item *
1006 "INI - SEP too long"
X<1006>

The value passed for SEP is exceeding its maximum length (16).

=item *
1007 "INI - QUOTE too long"
X<1007>

The value passed for QUOTE is exceeding its maximum length (16).

=item *
1008 "INI - SEP undefined"
X<1008>

The value passed for SEP should be defined and not empty.

=item *
1010 "INI - the header is empty"
X<1010>

The header line parsed in the L</header> is empty.

=item *
1011 "INI - the header contains more than one valid separator"
X<1011>

The header line parsed in the  L</header>  contains more than one  (unique)
separator character out of the allowed set of separators.

=item *
1012 "INI - the header contains an empty field"
X<1012>

The header line parsed in the L</header> is contains an empty field.

=item *
1013 "INI - the header contains nun-unique fields"
X<1013>

The header line parsed in the  L</header>  contains at least  two identical
fields.

=item *
1014 "INI - header called on undefined stream"
X<1014>

The header line cannot be parsed from an undefined sources.

=item *
1500 "PRM - Invalid/unsupported argument(s)"
X<1500>

Function or method called with invalid argument(s) or parameter(s).

=item *
1501 "PRM - The key attribute is passed as an unsupported type"
X<1501>

The C<key> attribute is of an unsupported type.

=item *
1502 "PRM - The value attribute is passed without the key attribute"
X<1502>

The C<value> attribute is only allowed when a valid key is given.

=item *
1503 "PRM - The value attribute is passed as an unsupported type"
X<1503>

The C<value> attribute is of an unsupported type.

=item *
2010 "ECR - QUO char inside quotes followed by CR not part of EOL"
X<2010>

When  L<C<eol>|/eol>  has  been  set  to  anything  but the  default,  like
C<"\r\t\n">,  and  the  C<"\r">  is  following  the   B<second>   (closing)
L<C<quote_char>|/quote_char>, where the characters following the C<"\r"> do
not make up the L<C<eol>|/eol> sequence, this is an error.

=item *
2011 "ECR - Characters after end of quoted field"
X<2011>

Sequences like C<1,foo,"bar"baz,22,1> are not allowed. C<"bar"> is a quoted
field and after the closing double-quote, there should be either a new-line
sequence or a separation character.

=item *
2012 "EOF - End of data in parsing input stream"
X<2012>

Self-explaining. End-of-file while inside parsing a stream. Can happen only
when reading from streams with L</getline>,  as using  L</parse> is done on
strings that are not required to have a trailing L<C<eol>|/eol>.

=item *
2013 "INI - Specification error for fragments RFC7111"
X<2013>

Invalid specification for URI L</fragment> specification.

=item *
2014 "ENF - Inconsistent number of fields"
X<2014>

Inconsistent number of fields under strict parsing.

=item *
2021 "EIQ - NL char inside quotes, binary off"
X<2021>

Sequences like C<1,"foo\nbar",22,1> are allowed only when the binary option
has been selected with the constructor.

=item *
2022 "EIQ - CR char inside quotes, binary off"
X<2022>

Sequences like C<1,"foo\rbar",22,1> are allowed only when the binary option
has been selected with the constructor.

=item *
2023 "EIQ - QUO character not allowed"
X<2023>

Sequences like C<"foo "bar" baz",qu> and C<2023,",2008-04-05,"Foo, Bar",\n>
will cause this error.

=item *
2024 "EIQ - EOF cannot be escaped, not even inside quotes"
X<2024>

The escape character is not allowed as last character in an input stream.

=item *
2025 "EIQ - Loose unescaped escape"
X<2025>

An escape character should escape only characters that need escaping.

Allowing  the escape  for other characters  is possible  with the attribute
L</allow_loose_escapes>.

=item *
2026 "EIQ - Binary character inside quoted field, binary off"
X<2026>

Binary characters are not allowed by default.    Exceptions are fields that
contain valid UTF-8,  that will automatically be upgraded if the content is
valid UTF-8. Set L<C<binary>|/binary> to C<1> to accept binary data.

=item *
2027 "EIQ - Quoted field not terminated"
X<2027>

When parsing a field that started with a quotation character,  the field is
expected to be closed with a quotation character.   When the parsed line is
exhausted before the quote is found, that field is not terminated.

=item *
2030 "EIF - NL char inside unquoted verbatim, binary off"
X<2030>

=item *
2031 "EIF - CR char is first char of field, not part of EOL"
X<2031>

=item *
2032 "EIF - CR char inside unquoted, not part of EOL"
X<2032>

=item *
2034 "EIF - Loose unescaped quote"
X<2034>

=item *
2035 "EIF - Escaped EOF in unquoted field"
X<2035>

=item *
2036 "EIF - ESC error"
X<2036>

=item *
2037 "EIF - Binary character in unquoted field, binary off"
X<2037>

=item *
2110 "ECB - Binary character in Combine, binary off"
X<2110>

=item *
2200 "EIO - print to IO failed. See errno"
X<2200>

=item *
3001 "EHR - Unsupported syntax for column_names ()"
X<3001>

=item *
3002 "EHR - getline_hr () called before column_names ()"
X<3002>

=item *
3003 "EHR - bind_columns () and column_names () fields count mismatch"
X<3003>

=item *
3004 "EHR - bind_columns () only accepts refs to scalars"
X<3004>

=item *
3006 "EHR - bind_columns () did not pass enough refs for parsed fields"
X<3006>

=item *
3007 "EHR - bind_columns needs refs to writable scalars"
X<3007>

=item *
3008 "EHR - unexpected error in bound fields"
X<3008>

=item *
3009 "EHR - print_hr () called before column_names ()"
X<3009>

=item *
3010 "EHR - print_hr () called with invalid arguments"
X<3010>

=back

=head1 SEE ALSO

L<IO::File>,  L<IO::Handle>,  L<IO::Wrap>,  L<Text::CSV>,  L<Text::CSV_PP>,
L<Text::CSV::Encoded>,     L<Text::CSV::Separator>,    L<Text::CSV::Slurp>,
L<Spreadsheet::CSV> and L<Spreadsheet::Read>, and of course L<perl>.

If you are using perl6,  you can have a look at  C<Text::CSV>  in the perl6
ecosystem, offering the same features.

=head3 non-perl

A CSV parser in JavaScript,  also used by L<W3C|http://www.w3.org>,  is the
multi-threaded in-browser L<PapaParse|http://papaparse.com/>.

L<csvkit|http://csvkit.readthedocs.org> is a python CSV parsing toolkit.

=head1 AUTHOR

Alan Citterman F<E<lt>alan@mfgrtl.comE<gt>> wrote the original Perl module.
Please don't send mail concerning Text::CSV_XS to Alan, who is not involved
in the C/XS part that is now the main part of the module.

Jochen Wiedmann F<E<lt>joe@ispsoft.deE<gt>> rewrote the en- and decoding in
C by implementing a simple finite-state machine.   He added variable quote,
escape and separator characters, the binary mode and the print and getline
methods. See F<ChangeLog> releases 0.10 through 0.23.

H.Merijn Brand F<E<lt>h.m.brand@xs4all.nlE<gt>> cleaned up the code,  added
the field flags methods,  wrote the major part of the test suite, completed
the documentation,   fixed most RT bugs,  added all the allow flags and the
L</csv> function. See ChangeLog releases 0.25 and on.

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2007-2019 H.Merijn Brand.  All rights reserved.
 Copyright (C) 1998-2001 Jochen Wiedmann. All rights reserved.
 Copyright (C) 1997      Alan Citterman.  All rights reserved.

This library is free software;  you can redistribute and/or modify it under
the same terms as Perl itself.

=cut

=for elvis
:ex:se gw=75|color guide #ff0000:

=cut
