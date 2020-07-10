package Email::Sender::Role::CommonSending;
# ABSTRACT: the common sending tasks most Email::Sender classes will need
$Email::Sender::Role::CommonSending::VERSION = '1.300031';
use Moo::Role;

use Carp ();
use Email::Abstract 3.006;
use Email::Sender::Success;
use Email::Sender::Failure::Temporary;
use Email::Sender::Failure::Permanent;
use Scalar::Util ();
use Try::Tiny;

#pod =head1 DESCRIPTION
#pod
#pod Email::Sender::Role::CommonSending provides a number of features that should
#pod ease writing new classes that perform the L<Email::Sender> role.  Instead of
#pod writing a C<send> method, implementors will need to write a smaller
#pod C<send_email> method, which will be passed an L<Email::Abstract> object and
#pod envelope containing C<from> and C<to> entries.  The C<to> entry will be
#pod guaranteed to be an array reference.
#pod
#pod A C<success> method will also be provided as a shortcut for calling:
#pod
#pod   Email::Sender::Success->new(...);
#pod
#pod A few other minor details are handled by CommonSending; for more information,
#pod consult the source.
#pod
#pod The methods documented here may be overridden to alter the behavior of the
#pod CommonSending role.
#pod
#pod =cut

with 'Email::Sender';

requires 'send_email';

sub send {
  my ($self, $message, $env, @rest) = @_;
  my $email    = $self->prepare_email($message);
  my $envelope = $self->prepare_envelope($env);

  try {
    return $self->send_email($email, $envelope, @rest);
  } catch {
    Carp::confess('unknown error') unless my $err = $_;

    if (
      try { $err->isa('Email::Sender::Failure') }
      and ! (my @tmp = $err->recipients)
    ) {
      $err->_set_recipients([ @{ $envelope->{to} } ]);
    }

    die $err;
  }
}

#pod =method prepare_email
#pod
#pod This method is passed a scalar and is expected to return an Email::Abstract
#pod object.  You probably shouldn't override it in most cases.
#pod
#pod =cut

sub prepare_email {
  my ($self, $msg) = @_;

  Carp::confess("no email passed in to sender") unless defined $msg;

  # We check blessed because if someone would pass in a large message, in some
  # perls calling isa on the string would create a package with the string as
  # the name.  If the message was (say) two megs, now you'd have a two meg hash
  # key in the stash.  Oops! -- rjbs, 2008-12-04
  return $msg if Scalar::Util::blessed($msg) and eval { $msg->isa('Email::Abstract') };

  return Email::Abstract->new($msg);
}

#pod =method prepare_envelope
#pod
#pod This method is passed a hashref and returns a new hashref that should be used
#pod as the envelope passed to the C<send_email> method.  This method is responsible
#pod for ensuring that the F<to> entry is an array.
#pod
#pod =cut

sub prepare_envelope {
  my ($self, $env) = @_;

  my %new_env;
  $new_env{to}   = ref $env->{to} ? $env->{to} : [ grep {defined} $env->{to} ];
  $new_env{from} = $env->{from};

  return \%new_env;
}

#pod =method success
#pod
#pod   ...
#pod   return $self->success;
#pod
#pod This method returns a new Email::Sender::Success object.  Arguments passed to
#pod this method are passed along to the Success's constructor.  This is provided as
#pod a convenience for returning success from subclasses' C<send_email> methods.
#pod
#pod =cut

sub success {
  my $self = shift;
  my $success = Email::Sender::Success->new(@_);
}

no Moo::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Role::CommonSending - the common sending tasks most Email::Sender classes will need

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

Email::Sender::Role::CommonSending provides a number of features that should
ease writing new classes that perform the L<Email::Sender> role.  Instead of
writing a C<send> method, implementors will need to write a smaller
C<send_email> method, which will be passed an L<Email::Abstract> object and
envelope containing C<from> and C<to> entries.  The C<to> entry will be
guaranteed to be an array reference.

A C<success> method will also be provided as a shortcut for calling:

  Email::Sender::Success->new(...);

A few other minor details are handled by CommonSending; for more information,
consult the source.

The methods documented here may be overridden to alter the behavior of the
CommonSending role.

=head1 METHODS

=head2 prepare_email

This method is passed a scalar and is expected to return an Email::Abstract
object.  You probably shouldn't override it in most cases.

=head2 prepare_envelope

This method is passed a hashref and returns a new hashref that should be used
as the envelope passed to the C<send_email> method.  This method is responsible
for ensuring that the F<to> entry is an array.

=head2 success

  ...
  return $self->success;

This method returns a new Email::Sender::Success object.  Arguments passed to
this method are passed along to the Success's constructor.  This is provided as
a convenience for returning success from subclasses' C<send_email> methods.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
