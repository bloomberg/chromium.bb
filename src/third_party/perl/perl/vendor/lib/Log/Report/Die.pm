# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Die;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Exporter';

use warnings;
use strict;

our @EXPORT = qw/die_decode exception_decode/;

use POSIX  qw/locale_h/;


sub die_decode($%)
{   my ($text, %args) = @_;

    my @text   = split /\n/, $text;
    @text or return ();
    chomp $text[-1];

    # Try to catch the error directly, to remove it from the error text
    my %opt    = (errno => $! + 0);
    my $err    = "$!";

    my $dietxt = $text[0];
    if($text[0] =~ s/ at (.+) line (\d+)\.?$// )
    {   $opt{location} = [undef, $1, $2, undef];
    }
    elsif(@text > 1 && $text[1] =~ m/^\s*at (.+) line (\d+)\.?$/ )
    {   # sometimes people carp/confess with \n, folding the line
        $opt{location} = [undef, $1, $2, undef];
        splice @text, 1, 1;
    }

    $text[0] =~ s/\s*[.:;]?\s*$err\s*$//  # the $err is translation sensitive
        or delete $opt{errno};

    my @msg = shift @text;
    length $msg[0] or $msg[0] = 'stopped';

    my @stack;
    foreach (@text)
    {   if(m/^\s*(.*?)\s+called at (.*?) line (\d+)\s*$/)
             { push @stack, [ $1, $2, $3 ] }
        else { push @msg, $_ }
    }
    $opt{stack}   = \@stack;
    $opt{classes} = [ 'perl', (@stack ? 'confess' : 'die') ];

    my $reason
      = $opt{errno} ? 'FAULT'
      : @stack      ? 'PANIC'
      :               $args{on_die} || 'ERROR';

    ($dietxt, \%opt, $reason, join("\n", @msg));
}


sub _exception_dbix($$)
{   my ($exception, $args) = @_;
	my $on_die = delete $args->{on_die};
	my %opts   = %$args;

    my @lines  = split /\n/, "$exception";  # accessor missing to get msg
    my $first  = shift @lines;
    my ($sub, $message, $fn, $linenr) = $first =~
       m/^ (?: ([\w:]+?) \(\)\: [ ] | \{UNKNOWN\}\: [ ] )?
           (.*?) 
           \s+ at [ ] (.+) [ ] line [ ] ([0-9]+)\.?
         $/x;
    my $pkg    = defined $sub && $sub =~ s/^([\w:]+)\:\:// ? $1 : $0;

    $opts{location} ||= [ $pkg, $fn, $linenr, $sub ];

    my @stack;
    foreach (@lines)
    {   my ($func, $fn, $linenr)
           = /^\s+(.*?)\(\)\s+called at (.*?) line ([0-9]+)$/ or next;
        push @stack, [ $func, $fn, $linenr ];
    }
	$opts{stack} ||= \@stack if @stack;

    my $reason
      = $opts{errno} ? 'FAULT'
      : @stack       ? 'PANIC'
      :                $on_die || 'ERROR';

    ('caught '.ref $exception, \%opts, $reason, $message);
}

my %_libxml_errno2reason = (1 => 'WARNING', 2 => 'MISTAKE', 3 => 'ERROR');

sub _exception_libxml($$)
{   my ($exc, $args) = @_;
	my $on_die = delete $args->{on_die};
	my %opts   = %$args;

	$opts{errno}    ||= $exc->code + 13000;
	$opts{location} ||= [ 'libxml', $exc->file, $exc->line, $exc->domain ];

	my $msg = $exc->message . $exc->context . "\n"
            . (' ' x $exc->column) . '^'
            . ' (' . $exc->domain . ' error ' . $exc->code . ')';

	my $reason = $_libxml_errno2reason{$exc->level} || 'PANIC';
    ('caught '.ref $exc, \%opts, $reason, $msg);
}

sub exception_decode($%)
{   my ($exception, %args) = @_;
	my $errno = $! + 0;

    return _exception_dbix($exception, \%args)
	    if $exception->isa('DBIx::Class::Exception');

	return _exception_libxml($exception, \%args)
	    if $exception->isa('XML::LibXML::Error');

    # Unsupported exception system, sane guesses
    my %opt =
    ( classes => [ 'unknown exception', 'die', ref $exception ]
    , errno   => $errno
    );

    my $reason = $errno ? 'FAULT' : $args{on_die} || 'ERROR';

    # hopefully stringification is overloaded
    ( "caught ".ref $exception, \%opt, $reason, "$exception");
}

"to die or not to die, that's the question";
