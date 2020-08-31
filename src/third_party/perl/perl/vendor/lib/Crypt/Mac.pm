package Crypt::Mac;

use strict;
use warnings;
our $VERSION = '0.063';

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;

sub addfile {
  my ($self, $file) = @_;

  my $handle;
  if (ref(\$file) eq 'SCALAR') {
    open($handle, "<", $file) || die "FATAL: cannot open '$file': $!";
    binmode($handle);
  }
  else {
    $handle = $file
  }
  die "FATAL: invalid handle" unless defined $handle;

  my $n;
  my $buf = "";
  local $SIG{__DIE__} = \&CryptX::_croak;
  while (($n = read($handle, $buf, 32*1024))) {
    $self->add($buf);
  }
  die "FATAL: read failed: $!" unless defined $n;

  return $self;
}

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::Mac - [internal only]

=cut
