package Email::Sender::Transport::Maildir;
# ABSTRACT: deliver mail to a maildir on disk
$Email::Sender::Transport::Maildir::VERSION = '1.300031';
use Moo;
with 'Email::Sender::Transport';

use Errno ();
use Fcntl;
use File::Path 2.06;
use File::Spec;

use Sys::Hostname;

use MooX::Types::MooseLike::Base qw(Bool);

#pod =head1 DESCRIPTION
#pod
#pod This transport delivers into a maildir.  The maildir's location may be given as
#pod the F<dir> argument to the constructor, and defaults to F<Maildir> in the
#pod current directory (at the time of transport initialization).
#pod
#pod If the directory does not exist, it will be created.
#pod
#pod By default, three headers will be added:
#pod
#pod  * X-Email-Sender-From - the envelope sender
#pod  * X-Email-Sender-To   - the envelope recipients (one header per rcpt)
#pod  * Lines               - the number of lines in the body
#pod
#pod These can be controlled with the C<add_lines_header> and
#pod C<add_envelope_headers> constructor arguments.
#pod
#pod The L<Email::Sender::Success> object returned on success has a C<filename>
#pod method that returns the filename to which the message was delivered.
#pod
#pod =cut

{
  package
    Email::Sender::Success::MaildirSuccess;
  use Moo;
  use MooX::Types::MooseLike::Base qw(Str);
  extends 'Email::Sender::Success';
  has filename => (
    is  => 'ro',
    isa => Str,
    required => 1,
  );
  no Moo;
}


my $HOSTNAME;
BEGIN { ($HOSTNAME = hostname) =~ s/\..*//; }
sub _hostname { $HOSTNAME }

my $MAILDIR_TIME    = 0;
my $MAILDIR_COUNTER = 0;

has [ qw(add_lines_header add_envelope_headers) ] => (
  is  => 'ro',
  isa => Bool,
  default => sub { 1 },
);

has dir => (
  is  => 'ro',
  required => 1,
  default  => sub { File::Spec->catdir(File::Spec->curdir, 'Maildir') },
);

sub send_email {
  my ($self, $email, $env) = @_;

  my $dupe = Email::Abstract->new(\do { $email->as_string });

  if ($self->add_envelope_headers) {
    $dupe->set_header('X-Email-Sender-From' =>
      (defined $env->{from} ? $env->{from} : '-'),
    );

    my @to = grep {; defined } @{ $env->{to} };
    $dupe->set_header('X-Email-Sender-To'   => (@to ? @to : '-'));
  }

  $self->_ensure_maildir_exists;

  $self->_add_lines_header($dupe) if $self->add_lines_header;
  $self->_update_time;

  my $fn = $self->_deliver_email($dupe);

  return Email::Sender::Success::MaildirSuccess->new({
    filename => $fn,
  });
}

sub _ensure_maildir_exists {
  my ($self) = @_;

  for my $dir (qw(cur tmp new)) {
    my $subdir = File::Spec->catdir($self->dir, $dir);
    next if -d $subdir;

    Email::Sender::Failure->throw("couldn't create $subdir: $!")
      unless File::Path::make_path($subdir) || -d $subdir;
  }
}

sub _add_lines_header {
  my ($class, $email) = @_;
  return if $email->get_header("Lines");
  my $lines = $email->get_body =~ tr/\n/\n/;
  $email->set_header("Lines", $lines);
}

sub _update_time {
  my $time = time;
  if ($MAILDIR_TIME != $time) {
    $MAILDIR_TIME    = $time;
    $MAILDIR_COUNTER = 0;
  } else {
    $MAILDIR_COUNTER++;
  }
}

sub _deliver_email {
  my ($self, $email) = @_;

  my ($tmp_filename, $tmp_fh) = $self->_delivery_fh;

  # if (eval { $email->can('stream_to') }) {
  #  eval { $mail->stream_to($fh); 1 } or return;
  #} else {
  my $string = $email->as_string;
  $string =~ s/\x0D\x0A/\x0A/g unless $^O eq 'MSWin32';
  print $tmp_fh $string
    or Email::Sender::Failure->throw("could not write to $tmp_filename: $!");

  close $tmp_fh
    or Email::Sender::Failure->throw("error closing $tmp_filename: $!");

  my $target_name = File::Spec->catfile($self->dir, 'new', $tmp_filename);

  my $ok = rename(
    File::Spec->catfile($self->dir, 'tmp', $tmp_filename),
    $target_name,
  );

  Email::Sender::Failure->throw("could not move $tmp_filename from tmp to new")
    unless $ok;

  return $target_name;
}

sub _delivery_fh {
  my ($self) = @_;

  my $hostname = $self->_hostname;

  my ($filename, $fh);
  until ($fh) {
    $filename = join q{.}, $MAILDIR_TIME, $$, ++$MAILDIR_COUNTER, $hostname;
    my $filespec = File::Spec->catfile($self->dir, 'tmp', $filename);
    sysopen $fh, $filespec, O_CREAT|O_EXCL|O_WRONLY;
    binmode $fh;
    Email::Sender::Failure->throw("cannot create $filespec for delivery: $!")
      unless $fh or $!{EEXIST};
  }

  return ($filename, $fh);
}

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::Maildir - deliver mail to a maildir on disk

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

This transport delivers into a maildir.  The maildir's location may be given as
the F<dir> argument to the constructor, and defaults to F<Maildir> in the
current directory (at the time of transport initialization).

If the directory does not exist, it will be created.

By default, three headers will be added:

 * X-Email-Sender-From - the envelope sender
 * X-Email-Sender-To   - the envelope recipients (one header per rcpt)
 * Lines               - the number of lines in the body

These can be controlled with the C<add_lines_header> and
C<add_envelope_headers> constructor arguments.

The L<Email::Sender::Success> object returned on success has a C<filename>
method that returns the filename to which the message was delivered.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
