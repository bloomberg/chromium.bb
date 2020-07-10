# Copyrights 2013-2018 by [Mark Overmeer <mark@overmeer.net>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report-Optional. Meta-POD processed
# with OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Util;
use vars '$VERSION';
$VERSION = '1.06';

use base 'Exporter';

use warnings;
use strict;

use String::Print qw(printi);

our @EXPORT = qw/
  @reasons is_reason is_fatal use_errno
  mode_number expand_reasons mode_accepts
  must_show_location must_show_stack
  escape_chars unescape_chars to_html
  parse_locale
  pkg2domain
 /;
# [0.994 parse_locale deprecated, but kept hidden]

our @EXPORT_OK = qw/%reason_code/;

#use Log::Report 'log-report';
sub N__w($) { split ' ', $_[0] }

# ordered!
our @reasons  = N__w('TRACE ASSERT INFO NOTICE WARNING
    MISTAKE ERROR FAULT ALERT FAILURE PANIC');
our %reason_code; { my $i=1; %reason_code = map +($_ => $i++), @reasons }

my @user      = qw/MISTAKE ERROR/;
my @program   = qw/TRACE ASSERT INFO NOTICE WARNING PANIC/;
my @system    = qw/FAULT ALERT FAILURE/;

my %is_fatal  = map +($_ => 1), qw/ERROR FAULT FAILURE PANIC/;
my %use_errno = map +($_ => 1), qw/FAULT ALERT FAILURE/;

my %modes     = (NORMAL => 0, VERBOSE => 1, ASSERT => 2, DEBUG => 3
  , 0 => 0, 1 => 1, 2 => 2, 3 => 3);
my @mode_accepts = ('NOTICE-', 'INFO-', 'ASSERT-', 'ALL');

# horrible mutual dependency with Log::Report(::Minimal)
sub error__x($%)
{   if(Log::Report::Minimal->can('error')) # loaded the ::Mimimal version
         {  Log::Report::Minimal::error(Log::Report::Minimal::__x(@_)) }
    else { Log::Report::error(Log::Report::__x(@_)) }
}


sub expand_reasons($)
{   my $reasons = shift;
    my %r;
    foreach my $r (split m/\,/, $reasons)
    {   if($r =~ m/^([a-z]*)\-([a-z]*)/i )
        {   my $begin = $reason_code{$1 || 'TRACE'};
            my $end   = $reason_code{$2 || 'PANIC'};
            $begin && $end
                or error__x "unknown reason {which} in '{reasons}'"
                     , which => ($begin ? $2 : $1), reasons => $reasons;

            error__x"reason '{begin}' more serious than '{end}' in '{reasons}"
              , begin => $1, end => $2, reasons => $reasons
                 if $begin >= $end;

            $r{$_}++ for $begin..$end;
        }
        elsif($reason_code{$r}) { $r{$reason_code{$r}}++ }
        elsif($r eq 'USER')     { $r{$reason_code{$_}}++ for @user    }
        elsif($r eq 'PROGRAM')  { $r{$reason_code{$_}}++ for @program }
        elsif($r eq 'SYSTEM')   { $r{$reason_code{$_}}++ for @system  }
        elsif($r eq 'ALL')      { $r{$reason_code{$_}}++ for @reasons }
        else
        {   error__x"unknown reason {which} in '{reasons}'"
              , which => $r, reasons => $reasons;
        }
    }
    (undef, @reasons)[sort {$a <=> $b} keys %r];
}


sub is_reason($) { $reason_code{$_[0]} }
sub is_fatal($)  { $is_fatal{$_[0]}    }
sub use_errno($) { $use_errno{$_[0]}   }

#--------------------------

sub mode_number($)  { $modes{$_[0]} }


sub mode_accepts($) { $mode_accepts[$modes{$_[0]}] }


sub must_show_location($$)
{   my ($mode, $reason) = @_;
       $reason eq 'ASSERT'
    || $reason eq 'PANIC'
    || ($mode==2 && $reason_code{$reason} >= $reason_code{WARNING})
    || ($mode==3 && $reason_code{$reason} >= $reason_code{MISTAKE});
}


sub must_show_stack($$)
{   my ($mode, $reason) = @_;
        $reason eq 'PANIC'
     || ($mode==2 && $reason_code{$reason} >= $reason_code{ALERT})
     || ($mode==3 && $reason_code{$reason} >= $reason_code{ERROR});
}

#-------------------------

my %unescape =
  ( '\a' => "\a", '\b' => "\b", '\f' => "\f", '\n' => "\n"
  , '\r' => "\r", '\t' => "\t", '\"' => '"', '\\\\' => '\\'
  , '\e' =>  "\x1b", '\v' => "\x0b"
  );
my %escape   = reverse %unescape;

sub escape_chars($)
{   my $str = shift;
    $str =~ s/([\x00-\x1F\x7F"\\])/$escape{$1} || '?'/ge;
    $str;
}

sub unescape_chars($)
{   my $str = shift;
    $str =~ s/(\\.)/$unescape{$1} || $1/ge;
    $str;
}


my %tohtml = qw/  > gt   < lt   " quot  & amp /;

sub to_html($)
{   my $s = shift;
    $s =~ s/([<>"&])/\&${tohtml{$1}};/g;
    $s;
}


sub parse_locale($)
{   my $locale = shift;
    defined $locale && length $locale
        or return;

    if($locale !~
      m/^ ([a-z_]+)
          (?: \. ([\w-]+) )?  # codeset
          (?: \@ (\S+) )?     # modifier
        $/ix)
    {   # Windows Finnish_Finland.1252?
        $locale =~ s/.*\.//;
        return wantarray ? ($locale) : { language => $locale };
    }

    my ($lang, $codeset, $modifier) = ($1, $2, $3);

    my @subtags  = split /[_-]/, $lang;
    my $primary  = lc shift @subtags;

    my $language
      = $primary eq 'c'             ? 'C'
      : $primary eq 'posix'         ? 'POSIX'
      : $primary =~ m/^[a-z]{2,3}$/ ? $primary            # ISO639-1 and -2
      : $primary eq 'i' && @subtags ? lc(shift @subtags)  # IANA
      : $primary eq 'x' && @subtags ? lc(shift @subtags)  # Private
      : error__x"unknown locale language in locale `{locale}'"
           , locale => $locale;

    my $script;
    $script = ucfirst lc shift @subtags
        if @subtags > 1 && length $subtags[0] > 3;

    my $territory = @subtags ? uc(shift @subtags) : undef;

    return ($language, $territory, $codeset, $modifier)
        if wantarray;

    +{ language  => $language
     , script    => $script
     , territory => $territory
     , codeset   => $codeset
     , modifier  => $modifier
     , variant   => join('-', @subtags)
     };
}


my %pkg2domain;
sub pkg2domain($;$$$)
{   my $pkg = shift;
    my $d   = $pkg2domain{$pkg};
    @_ or return $d ? $d->[0] : 'default';

    my ($domain, $fn, $line) = @_;
    if($d)
    {   # registration already exists
        return $domain if $d->[0] eq $domain;
        printi "conflict: package {pkg} in {domain1} in {file1} line {line1}, but in {domain2} in {file2} line {line2}"
           , pkg => $pkg
           , domain1 => $domain, file1 => $fn, line1 => $line
           , domain2 => $d->[0], file2 => $d->[1], line2 => $d->[2];
    }

    # new registration
    $pkg2domain{$pkg} = [$domain, $fn, $line];
    $domain;
}

1;
