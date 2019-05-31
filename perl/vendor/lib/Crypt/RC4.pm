#--------------------------------------------------------------------#
# Crypt::RC4
#       Date Written:   07-Jun-2000 04:15:55 PM
#       Last Modified:  13-Dec-2001 03:33:49 PM 
#       Author:         Kurt Kincaid (sifukurt@yahoo.com)
#       Copyright (c) 2001, Kurt Kincaid
#           All Rights Reserved.
#
#       This is free software and may be modified and/or
#       redistributed under the same terms as Perl itself.
#--------------------------------------------------------------------#

package Crypt::RC4;

use strict;
use vars qw( $VERSION @ISA @EXPORT $MAX_CHUNK_SIZE );

$MAX_CHUNK_SIZE = 1024 unless $MAX_CHUNK_SIZE;

require Exporter;

@ISA     = qw(Exporter);
@EXPORT  = qw(RC4);
$VERSION = '2.02';

sub new {
    my ( $class, $key )  = @_;
    my $self = bless {}, $class;
    $self->{state} = Setup( $key );
    $self->{x} = 0;
    $self->{y} = 0;
    $self;
}

sub RC4 {
    my $self;
    my( @state, $x, $y );
    if ( ref $_[0] ) {
        $self = shift;
    @state = @{ $self->{state} };
    $x = $self->{x};
    $y = $self->{y};
    } else {
        @state = Setup( shift );
    $x = $y = 0;
    }
    my $message = shift;
    my $num_pieces = do {
    my $num = length($message) / $MAX_CHUNK_SIZE;
    my $int = int $num;
    $int == $num ? $int : $int+1;
    };
    for my $piece ( 0..$num_pieces - 1 ) {
    my @message = unpack "C*", substr($message, $piece * $MAX_CHUNK_SIZE, $MAX_CHUNK_SIZE);
    for ( @message ) {
        $x = 0 if ++$x > 255;
        $y -= 256 if ($y += $state[$x]) > 255;
        @state[$x, $y] = @state[$y, $x];
        $_ ^= $state[( $state[$x] + $state[$y] ) % 256];
    }
    substr($message, $piece * $MAX_CHUNK_SIZE, $MAX_CHUNK_SIZE) = pack "C*", @message;
    }
    if ($self) {
    $self->{state} = \@state;
    $self->{x} = $x;
    $self->{y} = $y;
    }
    $message;
}

sub Setup {
    my @k = unpack( 'C*', shift );
    my @state = 0..255;
    my $y = 0;
    for my $x (0..255) {
    $y = ( $k[$x % @k] + $state[$x] + $y ) % 256;
    @state[$x, $y] = @state[$y, $x];
    }
    wantarray ? @state : \@state;
}


1;
__END__

=head1 NAME

Crypt::RC4 - Perl implementation of the RC4 encryption algorithm

=head1 SYNOPSIS

# Functional Style
  use Crypt::RC4;
  $encrypted = RC4( $passphrase, $plaintext );
  $decrypt = RC4( $passphrase, $encrypted );
  
# OO Style
  use Crypt::RC4;
  $ref = Crypt::RC4->new( $passphrase );
  $encrypted = $ref->RC4( $plaintext );

  $ref2 = Crypt::RC4->new( $passphrase );
  $decrypted = $ref2->RC4( $encrypted );

# process an entire file, one line at a time
# (Warning: Encrypted file leaks line lengths.)
  $ref3 = Crypt::RC4->new( $passphrase );
  while (<FILE>) {
      chomp;
      print $ref3->RC4($_), "\n";
  }

=head1 DESCRIPTION

A simple implementation of the RC4 algorithm, developed by RSA Security, Inc. Here is the description
from RSA's website:

RC4 is a stream cipher designed by Rivest for RSA Data Security (now RSA Security). It is a variable
key-size stream cipher with byte-oriented operations. The algorithm is based on the use of a random
permutation. Analysis shows that the period of the cipher is overwhelmingly likely to be greater than
10100. Eight to sixteen machine operations are required per output byte, and the cipher can be
expected to run very quickly in software. Independent analysts have scrutinized the algorithm and it
is considered secure.

Based substantially on the "RC4 in 3 lines of perl" found at http://www.cypherspace.org

A major bug in v1.0 was fixed by David Hook (dgh@wumpus.com.au).  Thanks, David.

=head1 AUTHOR

Kurt Kincaid (sifukurt@yahoo.com)
Ronald Rivest for RSA Security, Inc.

=head1 BUGS

Disclaimer: Strictly speaking, this module uses the "alleged" RC4
algorithm. The Algorithm known as "RC4" is a trademark of RSA Security
Inc., and this document makes no claims one way or another that this
is the correct algorithm, and further, make no claims about the
quality of the source code nor any licensing requirements for
commercial use.

There's nothing preventing you from using this module in an insecure
way which leaks information. For example, encrypting multilple
messages with the same passphrase may allow an attacker to decode all of
them with little effort, even though they'll appear to be secured. If
serious crypto is your goal, be careful. Be very careful.

It's a pure-Perl implementation, so that rating of "Eight
to sixteen machine operations" is good for nothing but a good laugh.
If encryption and decryption are a bottleneck for you, please re-write
this module to use native code wherever practical.

=head1 LICENSE

This is free software and may be modified and/or
redistributed under the same terms as Perl itself.

=head1 SEE ALSO

L<perl>, L<http://www.cypherspace.org>, L<http://www.rsasecurity.com>, 
L<http://www.achtung.com/crypto/rc4.html>, 
L<http://www.columbia.edu/~ariel/ssleay/rrc4.html>

=cut
