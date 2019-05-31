package Math::Prime::Util::Entropy;
use strict;
use warnings;
use Carp qw/carp croak confess/;

BEGIN {
  $Math::Prime::Util::Entropy::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::Entropy::VERSION = '0.73';
}

sub _read_file {
  my($file, $nbytes) = @_;
  use Fcntl;
  my($s, $buffer, $nread) = ('', '', 0);
  return unless -r $file;
  sysopen(my $fh, $file, O_RDONLY);
  binmode $fh;
  while ($nread < $nbytes) {
    my $thisread = sysread $fh, $buffer, $nbytes-$nread;
    last unless defined $thisread && $thisread > 0;
    $s .= $buffer;
    $nread += length($buffer);
  }
  return unless $nbytes == length($s);
  return $s;
}

sub _try_urandom {
  if (-r "/dev/urandom") {
    return ('urandom', sub { _read_file("/dev/urandom",shift); }, 0, 1);
  }
  if (-r "/dev/random") {
    return ('random', sub { _read_file("/dev/random",shift); }, 1, 1);
  }
  return;
}

sub _try_win32 {
    return unless $^O eq 'MSWin32';
    eval { require Win32; require Win32::API; require Win32::API::Type; 1; }
        or return;
    use constant CRYPT_SILENT      => 0x40;       # Never display a UI.
    use constant PROV_RSA_FULL     => 1;          # Which service provider.
    use constant VERIFY_CONTEXT    => 0xF0000000; # Don't need existing keepairs
    use constant W2K_MAJOR_VERSION => 5;          # Windows 2000
    use constant W2K_MINOR_VERSION => 0;
    my ($major, $minor) = (Win32::GetOSVersion())[1, 2];
    return if $major < W2K_MAJOR_VERSION;

    if ($major == W2K_MAJOR_VERSION && $minor == W2K_MINOR_VERSION) {
        # We are Windows 2000.  Use the older CryptGenRandom interface.
        my $crypt_acquire_context_a =
            Win32::API->new('advapi32','CryptAcquireContextA','PPPNN','I');
        return unless defined $crypt_acquire_context_a;
        my $context = chr(0) x Win32::API::Type->sizeof('PULONG');
        my $result = $crypt_acquire_context_a->Call(
             $context, 0, 0, PROV_RSA_FULL, CRYPT_SILENT | VERIFY_CONTEXT );
        return unless $result;
        my $pack_type = Win32::API::Type::packing('PULONG');
        $context = unpack $pack_type, $context;
        my $crypt_gen_random =
            Win32::API->new( 'advapi32', 'CryptGenRandom', 'NNP', 'I' );
        return unless defined $crypt_gen_random;
        return ('CryptGenRandom',
            sub {
                my $nbytes = shift;
                my $buffer = chr(0) x $nbytes;
                my $result = $crypt_gen_random->Call($context, $nbytes, $buffer);
                croak "CryptGenRandom failed: $^E" unless $result;
                return $buffer;
            }, 0, 1);  # Assume non-blocking and strong
    } else {
        my $rtlgenrand = Win32::API->new( 'advapi32', <<'_RTLGENRANDOM_PROTO_');
INT SystemFunction036(
  PVOID RandomBuffer,
  ULONG RandomBufferLength
)
_RTLGENRANDOM_PROTO_
        return unless defined $rtlgenrand;
        return ('RtlGenRand',
            sub {
                my $nbytes = shift;
                my $buffer = chr(0) x $nbytes;
                my $result = $rtlgenrand->Call($buffer, $nbytes);
                croak "RtlGenRand failed: $^E" unless $result;
                return $buffer;
            }, 0, 1);  # Assume non-blocking and strong
    }
    return;
}

sub _try_crypt_prng {
  return unless eval { require Crypt::PRNG; 1; };
  return ('Crypt::PRNG', sub { Crypt::PRNG::random_bytes(shift) }, 0, 1);
}

sub _try_crypt_random_seed {
  return unless eval { require Crypt::Random::Seed; 1; };
  return ('Crypt::Random::Seed', sub { my $source = Crypt::Random::Seed->new(NonBlocking=>1); return unless $source; $source->random_bytes(shift) }, 0, 1);
}

my $_method;

sub entropy_bytes {
  my $nbytes = shift;
  my @methodlist = ( \&_try_win32,                 # All we have for Windows
                     \&_try_urandom,               # Best if available
                     \&_try_crypt_random_seed,     # More sources, fallbacks
                     \&_try_crypt_prng,            # Good CSPRNG, worse seeding
                   );

  if (!defined $_method) {
    foreach my $m (@methodlist) {
      my ($name, $rsub, $isblocking, $isstrong) = $m->();
      if (defined $name) {
        $_method = $rsub;
        last;
      }
    }
  }
  return unless defined $_method;
  $_method->($nbytes);
}

1;

__END__


# ABSTRACT:  Get a good random seed

=pod

=encoding utf8

=head1 NAME

Math::Prime::Util::Entropy - Get a good random seed


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

=head1 DESCRIPTION

Provides a single method to get a good seed if possible.  This is a streamlined
version of L<Crypt::Random::Seed>, with ideas from L<Bytes::Random::Secure::Tiny>.

=head2 entropy_bytes

Takes a number of bytes C<n> and returns either undef (no good seed available) or
a binary string with good entropy.

We try in order:

   - the Win32 Crypto API
   - /dev/urandom
   - /dev/random
   - L<Crypt::Random::Seed>
   - L<Crypt::PRNG>

=head1 SEE ALSO

L<Math::Prime::Util>
L<Crypt::Random::Seed>
L<Bytes::Random::Secure>
L<Bytes::Random::Secure::Tiny>
L<Crypt::PRNG>

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2017 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
