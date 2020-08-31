package Email::Sender::Transport::Test;
# ABSTRACT: deliver mail in memory for testing
$Email::Sender::Transport::Test::VERSION = '1.300031';
use Moo;

use Email::Sender::Failure::Multi;
use Email::Sender::Success::Partial;
use MooX::Types::MooseLike::Base qw(ArrayRef Bool);

#pod =head1 DESCRIPTION
#pod
#pod This transport is meant for testing email deliveries in memory.  It will store
#pod a record of any delivery made so that they can be inspected afterward.
#pod
#pod =for Pod::Coverage recipient_failure delivery_failure
#pod
#pod By default, the Test transport will not allow partial success and will always
#pod succeed.  It can be made to fail predictably, however, if it is extended and
#pod its C<recipient_failure> or C<delivery_failure> methods are overridden.  These
#pod methods are called as follows:
#pod
#pod   $self->delivery_failure($email, $envelope);
#pod
#pod   $self->recipient_failure($to);
#pod
#pod If they return true, the sending will fail.  If the transport was created with
#pod a true C<allow_partial_success> attribute, recipient failures can cause partial
#pod success to be returned.
#pod
#pod For more flexible failure modes, you can override more aggressively or can use
#pod L<Email::Sender::Transport::Failable>.
#pod
#pod =attr deliveries
#pod
#pod =for Pod::Coverage clear_deliveries
#pod
#pod This attribute stores an arrayref of all the deliveries made via the transport.
#pod
#pod Each delivery is a hashref, in the following format:
#pod
#pod   {
#pod     email     => $email,
#pod     envelope  => $envelope,
#pod     successes => \@ok_rcpts,
#pod     failures  => \@failures,
#pod   }
#pod
#pod Both successful and failed deliveries are stored.
#pod
#pod A number of methods related to this attribute are provided:
#pod
#pod =for :list
#pod * delivery_count
#pod * clear_deliveries
#pod * shift_deliveries
#pod
#pod =cut

has allow_partial_success => (is => 'ro', isa => Bool, default => sub { 0 });

sub recipient_failure { }
sub delivery_failure  { }

has deliveries => (
  isa => ArrayRef,
  init_arg   => undef,
  default    => sub { [] },
  is         => 'ro',
  reader     => '_deliveries',
);

sub delivery_count { scalar @{ $_[0]->_deliveries } }
sub record_delivery { push @{ shift->_deliveries }, @_ }
sub deliveries { @{ $_[0]->_deliveries } }
sub shift_deliveries { shift @{ $_[0]->_deliveries } }
sub clear_deliveries { @{ $_[0]->_deliveries } = () }

sub send_email {
  my ($self, $email, $envelope) = @_;

  my @failures;
  my @ok_rcpts;

  if (my $failure = $self->delivery_failure($email, $envelope)) {
    $failure->throw;
  }

  for my $to (@{ $envelope->{to} }) {
    if (my $failure = $self->recipient_failure($to)) {
      push @failures, $failure;
    } else {
      push @ok_rcpts, $to;
    }
  }

  if (
    @failures
    and ((@ok_rcpts == 0) or (! $self->allow_partial_success))
  ) {
    $failures[0]->throw if @failures == 1 and @ok_rcpts == 0;

    my $message = sprintf '%s recipients were rejected',
      @ok_rcpts ? 'some' : 'all';

    Email::Sender::Failure::Multi->throw(
      message  => $message,
      failures => \@failures,
    );
  }

  $self->record_delivery({
    email     => $email,
    envelope  => $envelope,
    successes => \@ok_rcpts,
    failures  => \@failures,
  });

  # XXX: We must report partial success (failures) if applicable.
  return $self->success unless @failures;
  return Email::Sender::Success::Partial->new({
    failure => Email::Sender::Failure::Multi->new({
      message  => 'some recipients were rejected',
      failures => \@failures
    }),
  });
}

with 'Email::Sender::Transport';
no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::Test - deliver mail in memory for testing

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

This transport is meant for testing email deliveries in memory.  It will store
a record of any delivery made so that they can be inspected afterward.

=head1 ATTRIBUTES

=head2 deliveries

=for Pod::Coverage recipient_failure delivery_failure

By default, the Test transport will not allow partial success and will always
succeed.  It can be made to fail predictably, however, if it is extended and
its C<recipient_failure> or C<delivery_failure> methods are overridden.  These
methods are called as follows:

  $self->delivery_failure($email, $envelope);

  $self->recipient_failure($to);

If they return true, the sending will fail.  If the transport was created with
a true C<allow_partial_success> attribute, recipient failures can cause partial
success to be returned.

For more flexible failure modes, you can override more aggressively or can use
L<Email::Sender::Transport::Failable>.

=for Pod::Coverage clear_deliveries

This attribute stores an arrayref of all the deliveries made via the transport.

Each delivery is a hashref, in the following format:

  {
    email     => $email,
    envelope  => $envelope,
    successes => \@ok_rcpts,
    failures  => \@failures,
  }

Both successful and failed deliveries are stored.

A number of methods related to this attribute are provided:

=over 4

=item *

delivery_count

=item *

clear_deliveries

=item *

shift_deliveries

=back

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
