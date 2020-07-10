package Crypt::DSA::GMP::Key::SSH2;
use strict;
use warnings;

BEGIN {
  $Crypt::DSA::GMP::Key::SSH2::AUTHORITY = 'cpan:DANAJ';
  $Crypt::DSA::GMP::Key::SSH2::VERSION = '0.01';
}

use base qw( Crypt::DSA::GMP::Key );

use MIME::Base64 qw( decode_base64 );
use Crypt::DSA::GMP::Key;

sub deserialize {
    my $key = shift;
    my %param = @_;

    chomp($param{Content});
    my $base64;

    if ($param{Content} =~ m:ssh-dss (.+)\s+\S+\s*$:s) {
      $base64 = $1;
    } elsif ($param{Content} =~ /---- BEGIN/) {
      my($head, $object, $content, $tail) = $param{Content} =~
        m:(---- BEGIN ([^\n\-]+) ----)\n(.+)(---- END .*? ----)$:s;
      my @lines = split /\n/, $content;
      my $escaped = 0;
      my @real;
      for my $l (@lines) {
          if (substr($l, -1) eq '\\') {
              $escaped++;
              next;
          }
          next if index($l, ':') != -1;
          if ($escaped) {
              $escaped--;
              next;
          }
          push @real, $l;
      }
      $base64 = join "\n", @real;
    }
    return unless defined $base64;
    my $content = decode_base64($base64);
    my $b = BufferWithInt->new_with_init($content);

    if ($b->get_int32 == 7 && $b->get_bytes(7) eq 'ssh-dss') {
      # This is the public key format created by OpenSSH
      $key->p( $b->get_mp_ssh2b );
      $key->q( $b->get_mp_ssh2b );
      $key->g( $b->get_mp_ssh2b );
      $key->pub_key( $b->get_mp_ssh2b );
      $key->priv_key(undef);
      return $key;
    }
    $b->reset_offset;

    # This all follows ssh-keygen.c: do_convert_private_ssh2_from_blob
    my $magic = $b->get_int32;
    return unless $magic == 0x3f6ff9eb;   # Private Key MAGIC

    my($ignore);
    $ignore = $b->get_int32;
    my $type = $b->get_str;
    my $cipher = $b->get_str;
    $ignore = $b->get_int32 for 1..3;

    return unless $cipher eq 'none';

    $key->p( $b->get_mp_ssh2 );
    $key->g( $b->get_mp_ssh2 );
    $key->q( $b->get_mp_ssh2 );
    $key->pub_key( $b->get_mp_ssh2 );
    $key->priv_key( $b->get_mp_ssh2 );

    #return unless $b->length == $b->offset;

    $key;
}

sub serialize {
    my $key = shift;
    my %param = @_;
    die "serialize is unimplemented";
}


package BufferWithInt;
use strict;

use Data::Buffer;
use Crypt::DSA::GMP::Util qw( bin2mp );
use base qw( Data::Buffer );

sub get_mp_ssh2 {
    my $buf = shift;
    my $bits = $buf->get_int32;
    my $off = $buf->{offset};
    my $bytes = int(($bits+7) / 8);
    my $int = bin2mp( $buf->bytes($off, $bytes) );
    $buf->{offset} += $bytes;
    $int;
}

sub get_mp_ssh2b {
    my $buf = shift;
    my $bytes = $buf->get_int32;
    my $off = $buf->{offset};
    my $int = bin2mp( $buf->bytes($off, $bytes) );
    $buf->{offset} += $bytes;
    $int;
}

1;
__END__

=head1 NAME

Crypt::DSA::GMP::Key::SSH2 - Read/write DSA SSH2 files

=head1 SYNOPSIS

    use Crypt::DSA::GMP::Key;
    my $key = Crypt::DSA::GMP::Key->new( Type => 'SSH2', ...);
    $key->write( Type => 'SSH2', ...);

=head1 DESCRIPTION

L<Crypt::DSA::GMP::Key::SSH2> provides an interface for reading
and writing DSA SSH2 files, using L<Data::Buffer>, which provides
functionality for SSH-compatible binary in/out buffers.

Currently encrypted key files are not supported.

You shouldn't use this module directly. As the SYNOPSIS above
suggests, this module should be considered a plugin for
L<Crypt::DSA::GMP::Key>, and all access to SSH2 files (reading DSA
keys from disk, etc.) should be done through that module.

Read the L<Crypt::DSA::GMP::Key> documentation for more details.

=head1 SUBCLASS METHODS

=head2 serialize

Returns the appropriate serialization blob of the key.

=head2 deserialize

Given an argument hash containing I<Content> and I<Password>, this
unpacks the serialized key into the self object.

=head1 TODO

This doesn't handle data produced by OpenSSH.  To see the data
from a DSA key in their format:

   cat file.dsa | grep -v -- ----- | tr -d '\n' | base64 -d | \
                  openssl asn1parse -inform DER

So we will need Convert::ASN1 to handle this.

=head1 AUTHOR & COPYRIGHTS

See L<Crypt::DSA::GMP> for author, copyright, and license information.

=cut
