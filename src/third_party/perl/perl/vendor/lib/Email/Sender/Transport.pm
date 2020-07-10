package Email::Sender::Transport;
# ABSTRACT: a role for email transports
$Email::Sender::Transport::VERSION = '1.300031';
use Moo::Role;

#pod =head1 DESCRIPTION
#pod
#pod Email::Sender::Transport is a Moo role to aid in writing classes used to send
#pod mail.  For the most part, its behavior comes entirely from the role
#pod L<Email::Sender::Role::CommonSending>, which it includes. The important
#pod difference is that Transports are often intended to be used by
#pod L<Email::Sender::Simple>, and they provide two methods related to that purpose.
#pod
#pod =for Pod::Coverage is_simple allow_partial_success
#pod
#pod First, they provide an C<allow_partial_success> method which returns true or
#pod false to indicate whether the transport will ever signal partial success.
#pod
#pod Second, they provide an C<is_simple> method, which returns true if the
#pod transport is suitable for use with Email::Sender::Simple.  By default, this
#pod method returns the inverse of C<allow_partial_success>.
#pod
#pod It is B<imperative> that these methods be accurate to prevent
#pod Email::Sender::Simple users from sending partially successful transmissions.
#pod Partial success is a complex case that almost all users will wish to avoid at
#pod all times.
#pod
#pod =cut

with 'Email::Sender::Role::CommonSending';

sub is_simple {
  my ($self) = @_;
  return if $self->allow_partial_success;
  return 1;
}

sub allow_partial_success { 0 }

no Moo::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport - a role for email transports

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

Email::Sender::Transport is a Moo role to aid in writing classes used to send
mail.  For the most part, its behavior comes entirely from the role
L<Email::Sender::Role::CommonSending>, which it includes. The important
difference is that Transports are often intended to be used by
L<Email::Sender::Simple>, and they provide two methods related to that purpose.

=for Pod::Coverage is_simple allow_partial_success

First, they provide an C<allow_partial_success> method which returns true or
false to indicate whether the transport will ever signal partial success.

Second, they provide an C<is_simple> method, which returns true if the
transport is suitable for use with Email::Sender::Simple.  By default, this
method returns the inverse of C<allow_partial_success>.

It is B<imperative> that these methods be accurate to prevent
Email::Sender::Simple users from sending partially successful transmissions.
Partial success is a complex case that almost all users will wish to avoid at
all times.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
