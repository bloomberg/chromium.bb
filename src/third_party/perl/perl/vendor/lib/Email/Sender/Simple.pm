package Email::Sender::Simple;
# ABSTRACT: the simple interface for sending mail with Sender
$Email::Sender::Simple::VERSION = '1.300031';
use Moo;
with 'Email::Sender::Role::CommonSending';

#pod =head1 SEE INSTEAD
#pod
#pod For now, the best documentation of this class is in
#pod L<Email::Sender::Manual::QuickStart>.
#pod
#pod =cut

use Sub::Exporter::Util ();
use Sub::Exporter -setup => {
  exports => {
    sendmail        => Sub::Exporter::Util::curry_class('send'),
    try_to_sendmail => Sub::Exporter::Util::curry_class('try_to_send'),
  },
};

use Email::Address;
use Email::Sender::Transport;
use Email::Sender::Util;
use Try::Tiny;

{
  my $DEFAULT_TRANSPORT;
  my $DEFAULT_FROM_ENV;

  sub _default_was_from_env {
    my ($class) = @_;
    $class->default_transport;
    return $DEFAULT_FROM_ENV;
  }

  sub transport_from_env {
    my ($class, $env_base) = @_;
    $env_base ||= 'EMAIL_SENDER_TRANSPORT';

    my $transport_class = $ENV{$env_base};
    return unless defined $transport_class and length $transport_class;

    my %arg;
    for my $key (grep { /^\Q$env_base\E_[_0-9A-Za-z]+$/ } keys %ENV) {
      (my $new_key = $key) =~ s/^\Q$env_base\E_//;
      $arg{lc $new_key} = $ENV{$key};
    }

    return Email::Sender::Util->easy_transport($transport_class, \%arg);
  }

  sub default_transport {
    return $DEFAULT_TRANSPORT if $DEFAULT_TRANSPORT;
    my ($class) = @_;

    my $transport = $class->transport_from_env;

    if ($transport) {
      $DEFAULT_FROM_ENV  = 1;
      $DEFAULT_TRANSPORT = $transport;
    } else {
      $DEFAULT_FROM_ENV  = 0;
      $DEFAULT_TRANSPORT = $class->build_default_transport;
    }

    return $DEFAULT_TRANSPORT;
  }

  sub build_default_transport {
    require Email::Sender::Transport::Sendmail;
    my $transport = eval { Email::Sender::Transport::Sendmail->new };

    return $transport if $transport;

    require Email::Sender::Transport::SMTP;
    Email::Sender::Transport::SMTP->new;
  }

  sub reset_default_transport {
    undef $DEFAULT_TRANSPORT;
    undef $DEFAULT_FROM_ENV;
  }
}

# Maybe this should be an around, but I'm just not excited about figuring out
# order at the moment.  It just has to work. -- rjbs, 2009-06-05
around prepare_envelope => sub {
  my ($orig, $class, $arg) = @_;
  $arg ||= {};
  my $env = $class->$orig($arg);

  $env = {
    %$arg,
    %$env,
  };

  return $env;
};

sub send_email {
  my ($class, $email, $arg) = @_;

  my $transport = $class->default_transport;

  if ($arg->{transport}) {
    $arg = { %$arg }; # So we can delete transport without ill effects.
    $transport = delete $arg->{transport} unless $class->_default_was_from_env;
  }

  Carp::confess("transport $transport not safe for use with Email::Sender::Simple")
    unless $transport->is_simple;

  my ($to, $from) = $class->_get_to_from($email, $arg);

  Email::Sender::Failure::Permanent->throw("no recipients") if ! @$to;
  Email::Sender::Failure::Permanent->throw("no sender") if ! defined $from;

  return $transport->send(
    $email,
    {
      to   => $to,
      from => $from,
    },
  );
}

sub try_to_send {
  my ($class, $email, $arg) = @_;

  try {
    return $class->send($email, $arg);
  } catch {
    my $error = $_ || 'unknown error';
    return if try { $error->isa('Email::Sender::Failure') };
    die $error;
  };
}

sub _get_to_from {
  my ($class, $email, $arg) = @_;

  my $to = $arg->{to};
  unless (@$to) {
    my @to_addrs =
      map  { $_->address               }
      grep { defined                   }
      map  { Email::Address->parse($_) }
      map  { $email->get_header($_)    }
      qw(to cc);
    $to = \@to_addrs;
  }

  my $from = $arg->{from};
  unless (defined $from) {
    ($from) =
      map  { $_->address               }
      grep { defined                   }
      map  { Email::Address->parse($_) }
      map  { $email->get_header($_)    }
      qw(from);
  }

  return ($to, $from);
}

no Moo;
"220 OK";

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Simple - the simple interface for sending mail with Sender

=head1 VERSION

version 1.300031

=head1 SEE INSTEAD

For now, the best documentation of this class is in
L<Email::Sender::Manual::QuickStart>.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
