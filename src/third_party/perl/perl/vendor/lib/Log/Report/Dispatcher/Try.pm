# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Dispatcher::Try;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Log::Report::Dispatcher';

use warnings;
use strict;

use Log::Report 'log-report', syntax => 'SHORT';
use Log::Report::Exception ();
use Log::Report::Util      qw/%reason_code/;
use List::Util             qw/first/;


use overload
    bool     => 'failed'
  , '""'     => 'showStatus'
  , fallback => 1;

#-----------------

sub init($)
{   my ($self, $args) = @_;
    defined $self->SUPER::init($args) or return;
    $self->{exceptions} = delete $args->{exceptions} || [];
    $self->{died}       = delete $args->{died};
    $self->hide($args->{hide} // 'NONE');
    $self->{on_die}     = $args->{on_die} // 'ERROR';
    $self;
}

#-----------------

sub died(;$)
{   my $self = shift;
    @_ ? ($self->{died} = shift) : $self->{died};
}


sub exceptions() { @{shift->{exceptions}} }


sub hides($)
{   my $h = shift->{hides} or return 0;
    keys %$h ? $h->{(shift)} : 1;
}


sub hide(@)
{   my $self = shift;
    my @h = map { ref $_ eq 'ARRAY' ? @$_ : defined($_) ? $_ : () } @_;

    $self->{hides}
      = @h==0 ? undef
      : @h==1 && $h[0] eq 'ALL'  ? {}    # empty HASH = ALL
      : @h==1 && $h[0] eq 'NONE' ? undef
      :    +{ map +($_ => 1), @h };
}


sub die2reason() { shift->{on_die} }

#-----------------

sub log($$$$)
{   my ($self, $opts, $reason, $message, $domain) = @_;

    unless($opts->{stack})
    {   my $mode = $self->mode;
        $opts->{stack} = $self->collectStack
            if $reason eq 'PANIC'
            || ($mode==2 && $reason_code{$reason} >= $reason_code{ALERT})
            || ($mode==3 && $reason_code{$reason} >= $reason_code{ERROR});
    }

    $opts->{location} ||= '';

    my $e = Log::Report::Exception->new
      ( reason      => $reason
      , report_opts => $opts
      , message     => $message
      );

    push @{$self->{exceptions}}, $e;

#    $self->{died} ||=
#        exists $opts->{is_fatal} ? $opts->{is_fatal} : $e->isFatal;

    $self;
}


sub reportFatal(@) { $_->throw(@_) for shift->wasFatal   }
sub reportAll(@)   { $_->throw(@_) for shift->exceptions }

#-----------------

sub failed()  {   defined shift->{died}}
sub success() { ! defined shift->{died}}



sub wasFatal(@)
{   my ($self, %args) = @_;
    defined $self->{died} or return ();

    # An (hidden) eval between LR::try()s may add more messages
    my $ex = first { $_->isFatal } @{$self->{exceptions}}
        or return ();

    (!$args{class} || $ex->inClass($args{class})) ? $ex : ();
}


sub showStatus()
{   my $self  = shift;
    my $fatal = $self->wasFatal or return '';
    __x"try-block stopped with {reason}: {text}"
      , reason => $fatal->reason
      , text   => $self->died;
}

1;
