package Email::Sender::Transport::Failable;
# ABSTRACT: a wrapper to makes things fail predictably
$Email::Sender::Transport::Failable::VERSION = '1.300031';
use Moo;
extends 'Email::Sender::Transport::Wrapper';

use MooX::Types::MooseLike::Base qw(ArrayRef);

#pod =head1 DESCRIPTION
#pod
#pod This transport extends L<Email::Sender::Transport::Wrapper>, meaning that it
#pod must be created with a C<transport> attribute of another
#pod Email::Sender::Transport.  It will proxy all email sending to that transport,
#pod but only after first deciding if it should fail.
#pod
#pod It does this by calling each coderef in its C<failure_conditions> attribute,
#pod which must be an arrayref of code references.  Each coderef will be called and
#pod will be passed the Failable transport, the Email::Abstract object, the
#pod envelope, and a reference to an array containing the rest of the arguments to
#pod C<send>.
#pod
#pod If any coderef returns a true value, the value will be used to signal failure.
#pod
#pod =cut

has 'failure_conditions' => (
  isa => ArrayRef,
  default => sub { [] },
  is      => 'ro',
  reader  => '_failure_conditions',
);

sub failure_conditions { @{$_[0]->_failure_conditions} }
sub fail_if { push @{shift->_failure_conditions}, @_ }
sub clear_failure_conditions { @{$_[0]->{failure_conditions}} = () }

around send_email => sub {
  my ($orig, $self, $email, $env, @rest) = @_;

  for my $cond ($self->failure_conditions) {
    my $reason = $cond->($self, $email, $env, \@rest);
    next unless $reason;
    die (ref $reason ? $reason : Email::Sender::Failure->new($reason));
  }

  return $self->$orig($email, $env, @rest);
};

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::Failable - a wrapper to makes things fail predictably

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

This transport extends L<Email::Sender::Transport::Wrapper>, meaning that it
must be created with a C<transport> attribute of another
Email::Sender::Transport.  It will proxy all email sending to that transport,
but only after first deciding if it should fail.

It does this by calling each coderef in its C<failure_conditions> attribute,
which must be an arrayref of code references.  Each coderef will be called and
will be passed the Failable transport, the Email::Abstract object, the
envelope, and a reference to an array containing the rest of the arguments to
C<send>.

If any coderef returns a true value, the value will be used to signal failure.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
