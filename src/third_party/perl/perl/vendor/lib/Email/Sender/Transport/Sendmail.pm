package Email::Sender::Transport::Sendmail;
# ABSTRACT: send mail via sendmail(1)
$Email::Sender::Transport::Sendmail::VERSION = '1.300031';
use Moo;
with 'Email::Sender::Transport';

use MooX::Types::MooseLike::Base qw(Str);

#pod =head2 DESCRIPTION
#pod
#pod This transport sends mail by piping it to the F<sendmail> command.  If the
#pod location of the F<sendmail> command is not provided in the constructor (see
#pod below) then the library will look for an executable file called F<sendmail> in
#pod the path.
#pod
#pod To specify the location of sendmail:
#pod
#pod   my $sender = Email::Sender::Transport::Sendmail->new({ sendmail => $path });
#pod
#pod =cut

use File::Spec ();

has 'sendmail' => (
  is  => 'ro',
  isa => Str,
  required => 1,
  lazy     => 1,
  default  => sub {
    # This should not have to be lazy, but Moose has a bug(?) that prevents the
    # instance or partial-instance from being passed in to the default sub.
    # Laziness doesn't hurt much, though, because (ugh) of the BUILD below.
    # -- rjbs, 2008-12-04

    # return $ENV{PERL_SENDMAIL_PATH} if $ENV{PERL_SENDMAIL_PATH}; # ???
    return $_[0]->_find_sendmail('sendmail');
  },
);

sub BUILD {
  $_[0]->sendmail; # force population -- rjbs, 2009-06-08
}

sub _find_sendmail {
  my ($self, $program_name) = @_;
  $program_name ||= 'sendmail';

  my @path = File::Spec->path;

  if ($program_name eq 'sendmail') {
    # for 'real' sendmail we will look in common locations -- rjbs, 2009-07-12
    push @path, (
      File::Spec->catfile('', qw(usr sbin)),
      File::Spec->catfile('', qw(usr lib)),
    );
  }

  for my $dir (@path) {
    my $sendmail = File::Spec->catfile($dir, $program_name);
    return $sendmail if ($^O eq 'MSWin32') ? -f $sendmail : -x $sendmail;
  }

  Carp::confess("couldn't find a sendmail executable");
}

sub _sendmail_pipe {
  my ($self, $envelope) = @_;

  my $prog = $self->sendmail;

  my ($first, @args) = $^O eq 'MSWin32'
           ? qq(| "$prog" -i -f $envelope->{from} @{$envelope->{to}})
           : (q{|-}, $prog, '-i', '-f', $envelope->{from}, '--', @{$envelope->{to}});

  no warnings 'exec'; ## no critic
  my $pipe;
  Email::Sender::Failure->throw("couldn't open pipe to sendmail ($prog): $!")
    unless open($pipe, $first, @args);

  return $pipe;
}

sub send_email {
  my ($self, $email, $envelope) = @_;

  my $pipe = $self->_sendmail_pipe($envelope);

  my $string = $email->as_string;
  $string =~ s/\x0D\x0A/\x0A/g unless $^O eq 'MSWin32';

  print $pipe $string
    or Email::Sender::Failure->throw("couldn't send message to sendmail: $!");

  close $pipe
    or Email::Sender::Failure->throw("error when closing pipe to sendmail: $!");

  return $self->success;
}

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::Sendmail - send mail via sendmail(1)

=head1 VERSION

version 1.300031

=head2 DESCRIPTION

This transport sends mail by piping it to the F<sendmail> command.  If the
location of the F<sendmail> command is not provided in the constructor (see
below) then the library will look for an executable file called F<sendmail> in
the path.

To specify the location of sendmail:

  my $sender = Email::Sender::Transport::Sendmail->new({ sendmail => $path });

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
