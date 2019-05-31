# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Dispatcher::File;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Log::Report::Dispatcher';

use warnings;
use strict;

use Log::Report  'log-report';
use IO::File     ();
use POSIX        qw/strftime/;

use Encode       qw/find_encoding/;
use Fcntl        qw/:flock/;


sub init($)
{   my ($self, $args) = @_;

    if(!$args->{charset})
    {   my $lc = $ENV{LC_CTYPE} || $ENV{LC_ALL} || $ENV{LANG} || '';
        my $cs = $lc =~ m/\.([\w-]+)/ ? $1 : '';
        $args->{charset} = length $cs && find_encoding $cs ? $cs : undef;
    }

    $self->SUPER::init($args);

    my $name = $self->name;
    $self->{to}      = $args->{to}
        or error __x"dispatcher {name} needs parameter 'to'", name => $name;
    $self->{replace} = $args->{replace} || 0;

    my $format = $args->{format} || sub { '['.localtime()."] $_[0]" };
    $self->{LRDF_format}
      = ref $format eq 'CODE' ? $format
      : $format eq 'LONG'
      ? sub { my $msg    = shift;
              my $domain = shift || '-';
              my $stamp  = strftime "%Y-%m-%dT%H:%M:%S", gmtime;
              "[$stamp $$] $domain $msg"
            }
      : error __x"unknown format parameter `{what}'"
          , what => ref $format || $format;

    $self;
}



sub close()
{   my $self = shift;
    $self->SUPER::close
        or return;

    my $to = $self->{to};
    my @close
      = ref $to eq 'CODE' ? values %{$self->{LRDF_out}}
      : $self->{LRDF_filename} ? $self->{LRDF_output}
      : ();

    $_->close for @close;
    $self;
}

#-----------

sub filename() {shift->{LRDF_filename}}
sub format()   {shift->{LRDF_format}}


sub output($)
{   # fast simple case
    return $_[0]->{LRDF_output} if $_[0]->{LRDF_output};

    my ($self, $msg) = @_;
    my $name = $self->name;

    my $to   = $self->{to};
    if(!ref $to)
    {   # constant file name
        $self->{LRDF_filename} = $to;
        my $binmode = $self->{replace} ? '>' : '>>';

        my $f = $self->{LRDF_output} = IO::File->new($to, $binmode)
            or fault __x"cannot write log into {file} with mode {binmode}"
                 , binmode => $binmode, file => $to;
        $f->autoflush;
        return $self->{LRDF_output} = $f;
    }

    if(ref $to eq 'CODE')
    {   # variable filename
        my $fn = $self->{LRDF_filename} = $to->($self, $msg);
        return $self->{LRDF_output} = $self->{LRDF_out}{$fn};
    }

    # probably file-handle
    $self->{LRDF_output} = $to;
}


#-----------

sub rotate($)
{   my ($self, $old) = @_;

    my $to   = $self->{to};
    my $logs = ref $to eq 'CODE' ? $self->{LRDF_out}
      : +{ $self->{to} => $self->{LRDF_output} };
    
    while(my ($log, $fh) = each %$logs)
    {   !ref $log
           or error __x"cannot rotate log file which was opened as file-handle";


        my $oldfn = ref $old eq 'CODE' ? $old->($log) : $old;
        trace "rotating $log to $oldfn";

        rename $log, $oldfn
           or fault __x"unable to rotate logfile {fn} to {oldfn}"
               , fn => $log, oldfn => $oldfn;

        $fh->close;   # close after move not possible on Windows?
        my $f = $self->{LRDF_output} = $logs->{$log} = IO::File->new($log, '>>')
               or fault __x"cannot write log into {file}", file => $log;
        $f->autoflush;
    }

    $self;
}

#-----------

sub log($$$$)
{   my ($self, $opts, $reason, $msg, $domain) = @_;
    my $trans = $self->translate($opts, $reason, $msg);
    my $text  = $self->format->($trans, $domain, $msg, %$opts);

    my $out   = $self->output($msg);
    flock $out, LOCK_EX;
    $out->print($text);
    flock $out, LOCK_UN;
}

1;
