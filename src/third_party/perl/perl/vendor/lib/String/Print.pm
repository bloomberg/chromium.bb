# Copyrights 2016-2018 by [Mark Overmeer].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution String-Print.  Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package String::Print;
use vars '$VERSION';
$VERSION = '0.93';


use warnings;
use strict;

#use Log::Report::Optional 'log-report';

use Encode            qw/is_utf8 decode/;
use Unicode::GCString ();
use HTML::Entities    qw/encode_entities/;
use Scalar::Util      qw/blessed reftype/;
use POSIX             qw/strftime/;
use Date::Parse       qw/str2time/;

my @default_modifiers   =
  ( qr/\%\S+/       => \&_modif_format
  , qr/BYTES\b/     => \&_modif_bytes
  , qr/YEAR\b/      => \&_modif_year
  , qr/DT\([^)]*\)/ => \&_modif_dt
  , qr/DT\b/        => \&_modif_dt
  , qr/DATE\b/      => \&_modif_date
  , qr/TIME\b/      => \&_modif_time
  , qr!//(?:\"[^"]*\"|\'[^']*\'|\w+)! => \&_modif_undef
  );

my %default_serializers =
  ( UNDEF     => sub { 'undef' }
  , ''        => sub { $_[1]   }
  , SCALAR    => sub { ${$_[1]} // shift->{SP_seri}{UNDEF}->(@_) }
  , ARRAY     =>
     sub { my $v = $_[1]; my $join = $_[2]{_join} // ', ';
           join $join, map +($_ // 'undef'), @$v;
         }
  , HASH      =>
     sub { my $v = $_[1];
           join ', ', map "$_ => ".($v->{$_} // 'undef'), sort keys %$v;
         }
  # CODE value has different purpose
  );

my %predefined_encodings =
(   HTML =>
      { exclude => [ qr/html$/i ]
      , encode  => sub { encode_entities $_[0] }
      }
);


sub new(@) { my $class = shift; (bless {}, $class)->init( {@_} ) }

sub init($)
{   my ($self, $args) = @_;

    my $modif = $self->{SP_modif} = [ @default_modifiers ];
    if(my $m  = $args->{modifiers})
    {   unshift @$modif, @$m;
    }

    my $s    = $args->{serializers} || {};
    my $seri = $self->{SP_seri}
      = { %default_serializers, (ref $s eq 'ARRAY' ? @$s : %$s) };

    $self->encodeFor($args->{encode_for});
	$self->{SP_missing} = $args->{missing_key} || \&_reportMissingKey;
    $self;
}

sub import(@)
{   my $class = shift;
    my ($oo, %func);
    while(@_)
    {   last if $_[0] !~ m/^s?print[ip]$/;
        $func{shift()} = 1;
    }

    if(@_ && $_[0] eq 'oo')   # only object oriented interface
    {   shift @_;
        @_ and die "no options allowed at import with oo interface";
        return;
    }

    my $all   = !keys %func;
    my $f     = $class->new(@_);   # OO encapsulated
    my ($pkg) = caller;
    no strict 'refs';
    *{"$pkg\::printi"}  = sub { $f->printi(@_)  } if $all || $func{printi};
    *{"$pkg\::sprinti"} = sub { $f->sprinti(@_) } if $all || $func{sprinti};
    *{"$pkg\::printp"}  = sub { $f->printp(@_)  } if $all || $func{printp};
    *{"$pkg\::sprintp"} = sub { $f->sprintp(@_) } if $all || $func{sprintp};
    $class;
}

#-------------

sub addModifiers(@) {my $self = shift; unshift @{$self->{SP_modif}}, @_}



sub encodeFor($)
{   my ($self, $type) = (shift, shift);
    defined $type
        or return $self->{SP_enc} = undef;

    my %def;
    if(ref $type eq 'HASH') {
        %def = %$type;
    }
    else 
    {   my $def = $predefined_encodings{$type}
            or die "ERROR: unknown output encoding type $type\n";
        %def = (%$def, @_);
    }

    my $excls   = $def{exclude} || [];
    my $regexes = join '|'
       , map +(ref $_ eq 'Regexp' ? $_ : qr/(?:^|\.)\Q$_\E$/)
          , ref $excls eq 'ARRAY' ? @$excls : $excls;
    $def{SP_exclude} = qr/$regexes/o;

    $self->{SP_enc} = \%def;
}

# You cannot have functions and methods with the same name in OODoc and POD

#-------------------

sub sprinti($@)
{   my ($self, $format) = (shift, shift);
    my $args = @_==1 ? shift : {@_};
    # $args may be a blessed HASH, for instance a Log::Report::Message

    $args->{_join} //= ', ';
    local $args->{_format} = $format;

    my @frags = split /\{([^}]*)\}/,   # enforce unicode
        is_utf8($format) ? $format : decode(latin1 => $format);

    my @parts;

    # Code parially duplicated for performance!
    if(my $enc = $self->{SP_enc})
    {   my $encode  = $enc->{encode};
        my $exclude = $enc->{SP_exclude};
        push @parts, $encode->($args->{_prepend}) if defined $args->{_prepend};
        push @parts, $encode->(shift @frags);
        while(@frags) {
            my ($name, $tricks) = (shift @frags)
                =~ m!^\s*([\pL\p{Pc}\pM][\w.]*)\s*(.*?)\s*$!o or die $format;

	    push @parts, $name =~ $exclude
              ? $self->_expand($name, $tricks, $args)
              : $encode->($self->_expand($name, $tricks, $args));

            push @parts, $encode->(shift @frags) if @frags;
        }
        push @parts, $encode->($args->{_append}) if defined $args->{_append};
    }
    else
    {   push @parts, $args->{_prepend} if defined $args->{_prepend};
        push @parts, shift @frags;
        while(@frags) {
	    (shift @frags) =~ /^\s*([\pL\p{Pc}\pM][\w.]*)\s*(.*?)\s*$/o
                or die $format;
	    push @parts, $self->_expand($1, $2, $args);
            push @parts, shift @frags if @frags;
        }
        push @parts, $args->{_append} if defined $args->{_append};
    }

    join '', @parts;
}

sub _expand($$$)
{   my ($self, $key, $modifier, $args) = @_;

    my $value;
    if(index($key, '.')== -1)
    {   # simple value
        $value = exists $args->{$key} ? $args->{$key}
          : $self->_missingKey($key, $args);
        $value = $value->($self, $key, $args)
            while ref $value eq 'CODE';
    }
    else
    {   my @parts = split /\./, $key;
		my $key   = shift @parts;
        $value = exists $args->{$key} ? $args->{$key}
          : $self->_missingKey($key, $args);

        $value = $value->($self, $key, $args)
            while ref $value eq 'CODE';

        while(defined $value && @parts)
        {  if(blessed $value)
           {   my $method = shift @parts;
               $value->can($method) or die "object $value cannot $method\n";
               $value = $value->$method;  # parameters not supported here
           }
           elsif(ref $value && reftype $value eq 'HASH')
           {   $value = $value->{shift @parts};
           }
           elsif(index($value, ':') != -1 || $::{$value.'::'})
           {   my $method = shift @parts;
               $value->can($method) or die "class $value cannot $method\n";
               $value = $value->$method;  # parameters not supported here
           }
           else
           {   die "not a HASH, object, or class at $parts[0] in $key\n";
           }

           $value = $value->($self, $key, $args)
               while ref $value eq 'CODE';
        }
    }

    my $mod;
  STACKED:
    while(length $modifier)
    {   my @modif = @{$self->{SP_modif}};
        while(@modif)
        {   my ($regex, $callback) = (shift @modif, shift @modif);
            $modifier =~ s/^($regex)\s*// or next;

            $value = $callback->($self, $1, $value, $args);
            next STACKED;
        }
        return "{unknown modifier '$modifier'}";
    }

    my $seri   = $self->{SP_seri}{defined $value ? ref $value : 'UNDEF'};
    $seri ? $seri->($self, $value, $args) : "$value";
}

sub _missingKey($$)
{   my ($self, $key, $args) = @_;
	$self->{SP_missing}->($self, $key, $args);
}

sub _reportMissingKey($$)
{   my ($self, $key, $args) = @_;

    my $depth = 0;
	my ($filename, $linenr);
    while((my $pkg, $filename, $linenr) = caller $depth++)
    {   last unless
            $pkg->isa(__PACKAGE__)
         || $pkg->isa('Log::Report::Minimal::Domain');
    }

	warn $self->sprinti
      ( "Missing key '{key}' in format '{format}', file {fn} line {line}\n"
      , key => $key, format => $args->{_format}
      , fn => $filename, line => $linenr
      );

    undef;
}

# See dedicated section in explanation in DETAILS
sub _modif_format($$$$)
{   my ($self, $format, $value, $args) = @_;
    defined $value && length $value or return undef;

    use locale;
    if(ref $value eq 'ARRAY')
    {   @$value or return '(none)';
        return [ map $self->_format_print($format, $_, $args), @$value ] ;
    }
    elsif(ref $value eq 'HASH')
    {   keys %$value or return '(none)';
        return { map +($_ => $self->_format_print($format, $value->{$_}, $args))
                   , keys %$value } ;
    }

    $format =~ m/^\%([-+ ]?)([0-9]*)(?:\.([0-9]*))?([sS])$/
        or return sprintf $format, $value;   # simple: not a string

    my ($padding, $width, $max, $u) = ($1, $2, $3, $4);

    # String formats like %10s or %-3.5s count characters, not width.
    # String formats like %10S or %-3.5S are subject to column width.
    # The latter means: minimal 3 chars, max 5, padding right with blanks.
    # All inserted strings are upgraded into utf8.

    my $s = Unicode::GCString->new
      ( is_utf8($value) ? $value : decode(latin1 => $value));

    my $pad;
    if($u eq 'S')
    {   # too large to fit
        return $value if !$max && $width && $width <= $s->columns;

        # wider than max.  Waiting for $s->trim($max) if $max, see
        # https://rt.cpan.org/Public/Bug/Display.html?id=84549
        $s->substr(-1, 1, '')
           while $max && $s->columns > $max;

        $pad = $width ? $width - $s->columns : 0;
    }
    else  # $u eq 's'
    {   return $value if !$max && $width && $width <= length $s;
        $s->substr($max, length($s)-$max, '') if $max && length $s > $max;
        $pad = $width ? $width - length $s : 0;
    }

      $pad==0         ? $s->as_string
    : $padding eq '-' ? $s->as_string . (' ' x $pad)
    :                   (' ' x $pad) . $s->as_string;
}

# See dedicated section in explanation in DETAILS
sub _modif_bytes($$$)
{   my ($self, $format, $value, $args) = @_;
    defined $value && length $value or return undef;

	return sprintf("%3d  B", $value) if $value < 1000;

    my @scale = qw/kB MB GB TB PB EB ZB/;
	$value /= 1024;

	while(@scale > 1 && $value > 999)
    {   shift @scale;
        $value /= 1024;
    }

    return sprintf "%3d $scale[0]", $value + 0.5
        if $value > 9.949;

	sprintf "%3.1f $scale[0]", $value;
}

# Be warned: %F and %T (from C99) are not supported on Windows
my %dt_format =
  ( ASC     => '%a %b %e %H:%M:%S %Y'
  , ISO     => '%Y-%m-%dT%H:%M:%S%z'
  , RFC2822 => '%a, %d %b %Y %H:%M:%S %z'
  , RFC822  => '%a, %d %b %y %H:%M:%S %z'
  , FT      => '%Y-%m-%d %H:%M:%S'
  );

sub _modif_year($$$)
{   my ($self, $format, $value, $args) = @_;
    defined $value && length $value or return undef;

	return $value
        if $value !~ /\D/ && $value < 2200;

	my $stamp = $value =~ /\D/ ? str2time($value) : $value;
	defined $stamp or return "year not found in '$value'";

    strftime "%Y", localtime($stamp);
}

sub _modif_date($$$)
{   my ($self, $format, $value, $args) = @_;
    defined $value && length $value or return undef;

	return sprintf("%4d-%02d-%02d", $1, $2, $3)
        if $value =~ m!^\s*([0-9]{4})[:/.-]([0-9]?[0-9])[:/.-]([0-9]?[0-9])\s*$!
        || $value =~ m!^\s*([0-9]{4})([0-9][0-9])([0-9][0-9])\s*$!;

	my $stamp = $value =~ /\D/ ? str2time($value) : $value;
	defined $stamp or return "date not found in '$value'";

    strftime "%Y-%m-%d", localtime($stamp);
}

sub _modif_time($$$)
{   my ($self, $format, $value, $args) = @_;
    defined $value && length $value or return undef;

	return sprintf "%02d:%02d:%02d", $1, $2, $3||0
        if $value =~ m!^\s*(0?[0-9]|1[0-9]|2[0-3])\:([0-5]?[0-9])(?:\:([0-5]?[0-9]))?\s*$!
        || $value =~ m!^\s*(0[0-9]|1[0-9]|2[0-3])([0-5][0-9])(?:([0-5][0-9]))?\s*$!;

	my $stamp = $value =~ /\D/ ? str2time($value) : $value;
	defined $stamp or return "time not found in '$value'";

    strftime "%H:%M:%S", localtime($stamp);
}

sub _modif_dt($$$)
{   my ($self, $format, $value, $args) = @_;
	defined $value && length $value or return undef;

	my $kind    = ($format =~ m/DT\(([^)]*)\)/ ? $1 : undef) || 'FT';
	my $pattern = $dt_format{$kind}
        or return "dt format $kind not known";

	my $stamp = $value =~ /\D/ ? str2time($value) : $value;
	defined $stamp or return "dt not found in '$value'";

    strftime $pattern, localtime($stamp);
}


sub _modif_undef($$$)
{   my ($self, $format, $value, $args) = @_;
    return $value if defined $value && length $value;
    $format =~ m!//"([^"]*)"|//'([^']*)'|//(\w*)! ? $+ : undef;
}


sub printi($$@)
{   my $self = shift;
    my $fh   = ref $_[0] eq 'GLOB' ? shift : select;
    $fh->print($self->sprinti(@_));
}



sub printp($$@)
{   my $self = shift;
    my $fh   = ref $_[0] eq 'GLOB' ? shift : select;
    $fh->print($self->sprintp(@_));
}


sub _printp_rewrite($)
{   my @params = @{$_[0]};
    my $printp = $params[0];
    my ($printi, @iparam);
    my ($pos, $maxpos) = (1, 1);
    while(length $printp && $printp =~ s/^([^%]+)//s)
    {   $printi .= $1;
        length $printp or last;
        if($printp =~ s/^\%\%//)
        {   $printi .= '%';
            next;
        }
        $printp =~ s/\%(?:([0-9]+)\$)?     # 1=positional
                       ([-+0 \#]*)         # 2=flags
                       ([0-9]*|\*)?        # 3=width
                       (?:\.([0-9]*|\*))?  # 4=precission
                       (?:\{ ([^}]*) \})?  # 5=modifiers
                       (\w)                # 6=conversion
                    //x
            or die "format error at '$printp' in '$params[0]'";

        $pos      = $1 if $1;
        my $width = !defined $3 ? '' : $3 eq '*' ? $params[$pos++] : $3;
        my $prec  = !defined $4 ? '' : $4 eq '*' ? $params[$pos++] : $4;
        my $modif = !defined $5 ? '' : $5;
        my $valpos= $pos++;
        $maxpos   = $pos if $pos > $maxpos;
        push @iparam, "_$valpos" => $params[$valpos];
        my $format= '%'.$2.($width || '').($prec ? ".$prec" : '').$6;
        $format   = '' if $format eq '%s';
        my $sep   = $modif.$format =~ m/^\w/ ? ' ' : '';
        $printi  .= "{_$valpos$sep$modif$format}";
    }
    splice @params, 0, $maxpos, @iparam;
    ($printi, \@params);
}

sub sprintp(@)
{   my $self = shift;
    my ($i, $iparam) = _printp_rewrite \@_;
    $self->sprinti($i, {@$iparam});
}

#-------------------

1;
