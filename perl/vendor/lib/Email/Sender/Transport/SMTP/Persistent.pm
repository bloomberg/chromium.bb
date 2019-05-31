package Email::Sender::Transport::SMTP::Persistent;
# ABSTRACT: an SMTP client that stays online
$Email::Sender::Transport::SMTP::Persistent::VERSION = '1.300031';
use Moo;
extends 'Email::Sender::Transport::SMTP';

#pod =head1 DESCRIPTION
#pod
#pod The stock L<Email::Sender::Transport::SMTP> reconnects each time it sends a
#pod message.  This transport only reconnects when the existing connection fails.
#pod
#pod =cut

use Net::SMTP;

has _cached_client => (
  is => 'rw',
);

sub _smtp_client {
  my ($self) = @_;

  if (my $client = $self->_cached_client) {
    return $client if eval { $client->reset; $client->ok; };

    my $error = $@
             || 'error resetting cached SMTP connection: ' . $client->message;

    Carp::carp($error);
  }

  my $client = $self->SUPER::_smtp_client;

  $self->_cached_client($client);

  return $client;
}

sub _message_complete { }

#pod =method disconnect
#pod
#pod   $transport->disconnect;
#pod
#pod This method sends an SMTP QUIT command and destroys the SMTP client, if on
#pod exists and is connected.
#pod
#pod =cut

sub disconnect {
  my ($self) = @_;
  return unless $self->_cached_client;
  $self->_cached_client->quit;
  $self->_cached_client(undef);
}

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::SMTP::Persistent - an SMTP client that stays online

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

The stock L<Email::Sender::Transport::SMTP> reconnects each time it sends a
message.  This transport only reconnects when the existing connection fails.

=head1 METHODS

=head2 disconnect

  $transport->disconnect;

This method sends an SMTP QUIT command and destroys the SMTP client, if on
exists and is connected.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
