use strict;
use warnings;
package Email::Sender::Util;
# ABSTRACT: random stuff that makes Email::Sender go
$Email::Sender::Util::VERSION = '1.300031';
use Email::Address;
use Email::Sender::Failure;
use Email::Sender::Failure::Permanent;
use Email::Sender::Failure::Temporary;
use List::Util 1.45 ();
use Module::Runtime qw(require_module);

# This code will be used by Email::Sender::Simple. -- rjbs, 2008-12-04
sub _recipients_from_email {
  my ($self, $email) = @_;

  my @to = List::Util::uniq(
           map { $_->address }
           map { Email::Address->parse($_) }
           map { $email->get_header($_) }
           qw(to cc bcc));

  return \@to;
}

sub _sender_from_email {
  my ($self, $email) = @_;

  my ($sender) = map { $_->address }
                 map { Email::Address->parse($_) }
                 scalar $email->get_header('from');

  return $sender;
}

# It's probably reasonable to make this code publicker at some point, but for
# now I don't want to deal with making a sane set of args. -- rjbs, 2008-12-09
sub _failure {
  my ($self, $error, $smtp, @rest) = @_;

  my ($code, $message);
  if ($smtp) {
    $code = $smtp->code;
    $message = $smtp->message;
    $message = ! defined $message ? "(no SMTP error message)"
             : ! length  $message ? "(empty SMTP error message)"
             :                       $message;

    $message = defined $error && length $error
             ? "$error: $message"
             : $message;
  } else {
    $message = $error;
    $message = "(no error given)" unless defined $message;
    $message = "(empty error string)" unless length $message;
  }

  my $error_class = ! $code       ? 'Email::Sender::Failure'
                  : $code =~ /^4/ ? 'Email::Sender::Failure::Temporary'
                  : $code =~ /^5/ ? 'Email::Sender::Failure::Permanent'
                  :                 'Email::Sender::Failure';

  $error_class->new({
    message => $message,
    code    => $code,
    @rest,
  });
}

#pod =method easy_transport
#pod
#pod   my $transport = Email::Sender::Util->easy_transport($class => \%arg);
#pod
#pod This takes the name of a transport class and a set of args to new.  It returns
#pod an Email::Sender::Transport object of that class.
#pod
#pod C<$class> is rewritten to C<Email::Sender::Transport::$class> unless it starts
#pod with an equals sign (C<=>) or contains a colon.  The equals sign, if present,
#pod will be removed.
#pod
#pod =cut

sub _rewrite_class {
  my $transport_class = $_[1];
  if ($transport_class !~ s/^=// and $transport_class !~ m{:}) {
    $transport_class = "Email::Sender::Transport::$transport_class";
  }

  return $transport_class;
}

sub easy_transport {
  my ($self, $transport_class, $arg) = @_;

  $transport_class = $self->_rewrite_class($transport_class);

  require_module($transport_class);
  return $transport_class->new($arg);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Util - random stuff that makes Email::Sender go

=head1 VERSION

version 1.300031

=head1 METHODS

=head2 easy_transport

  my $transport = Email::Sender::Util->easy_transport($class => \%arg);

This takes the name of a transport class and a set of args to new.  It returns
an Email::Sender::Transport object of that class.

C<$class> is rewritten to C<Email::Sender::Transport::$class> unless it starts
with an equals sign (C<=>) or contains a colon.  The equals sign, if present,
will be removed.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
