package Digest::OMAC::Base;

use strict;
#use warnings;
use Carp;
use MIME::Base64;

use constant DEBUG => 0;
use constant UNPACK_CAN_GROUP => $] >= 5.008;

sub new {
	my ( $class, $key, $cipher, @args ) = @_;

	if ( ref $key ) {
		$cipher = $key;
		$key = undef;
	}

    $cipher ||= 'Crypt::Rijndael';

    my $self = bless {
        cipher => undef,
    }, $class;

    return $self->_init($key, $cipher, @args);
}

sub add {
	my ( $self, @msg ) = @_;
	my $msg = join('', grep { defined } $self->{saved_block}, @msg);

	$self->{ix} += length($msg);

	my $c = $self->{cipher};
    my $blocksize = $c->blocksize;

	my @blocks = UNPACK_CAN_GROUP
		? unpack("(a$blocksize)*", $msg)
		: ( $msg =~ /(.{1,$blocksize})/sg );

	return unless @blocks;

	if ( length($blocks[-1]) < $blocksize ) {
		$self->{saved_block} = pop @blocks;
	} else {
		$self->{saved_block} = '';
	}

	return unless @blocks;

	my $Y = $self->{Y}; # Y[i-1]
	my $unenc_y;

	foreach my $block ( @blocks ) {
		$unenc_y = $block ^ $Y;
		$Y = $c->encrypt( $unenc_y ); # Y[i] = E( M[1] xor Y[-1] )
	}

	$self->{unenc_Y} = $unenc_y;
	$self->{Y} = $Y;

	return;
}

sub digest {
	my $self = shift;

	my $c = $self->{cipher};
    my $blocksize = $c->blocksize;

	my $last_block = $self->{saved_block};

	my $X;

	if ( length($last_block) or !$self->{ix} ) {
		my $padded = pack("B*", substr( unpack("B*", $last_block) . "1" . ( '0' x ($blocksize * 8) ), 0, $blocksize * 8 ) );
		$X = $padded ^ $self->{Y} ^ $self->{Lu2};
	} else {
		$X = $self->{unenc_Y} ^ $self->{Lu};
	}

	$self->reset;

	return $c->encrypt( $X );
}
	
sub reset {
    my $self = shift;
    my $blocksize = $self->{cipher}->blocksize;
    $self->{Y} = "\x00" x $blocksize;
	$self->{saved_block} = '';
    return $self;
}


sub _init {
	my ( $self, $key, $cipher ) = @_;

	if ( ref $cipher ) {
		$self->{cipher} = $cipher;
	} else {
		eval "require $cipher; 1;"
			or croak "Couldn't load $cipher: $@";
		$self->{cipher} = $cipher->new($key);
	}

	$self->{saved_block} = '';

	my $c = $self->{cipher};

    my $blocksize = $c->blocksize;

	my $zero = "\x00" x $blocksize;

	$self->{Y} = $zero;
	
    my $L = $self->{cipher}->encrypt($zero);
	
    if (DEBUG) { printf STDERR qq{DEBUG >> L=%s\n}, unpack "H*", $L }

	$self->{Lu} = $self->_lu( $blocksize, $L );

    if (DEBUG) { printf STDERR qq{DEBUG >> Lu=%s\n}, unpack "H*", $self->{Lu}; }

	$self->{Lu2} = $self->_lu2( $blocksize, $L, $self->{Lu} ); # for OMAC2 this is actually Lu^-1, not Lu^2, but we still call it Lu2

    if (DEBUG) { printf STDERR qq{DEBUG >> Lu2=%s\n}, unpack "H*", $self->{Lu2}; }

    return $self;
}

sub _lu {
	my ( $self, $blocksize,  $L ) = @_;
	$self->_shift_lu( $L, $self->_lu_constant($blocksize) );
}

sub _shift_lu {
	my ( $self, $L, $constant ) = @_;

	# used to do Bit::Vector's shift_left but that's broken
	my ( $msb, $tail ) = unpack("a a*", unpack("B*",$L));

	my $Lt = pack("B*", $tail . "0");

	if ( $msb ) {
		return $Lt ^ $constant;
	} else {
		return $Lt;
	}
}

sub _lu_constant {
	my ( $self, $blocksize ) = @_;

	if ( $blocksize == 16 ) { # 128
		return ( ("\x00" x 15) . "\x87" );
	} elsif ( $blocksize == 8 ) { # 64
		return ( ("\x00" x 7 ) . "\x1b" );
	} else {
		die "Blocksize $blocksize is not supported by OMAC";
	}
}

sub _lu2 {
	die "lu2 needs to be defined by subclass";
}

# support methods
sub hexdigest {
    return unpack 'H*', $_[0]->digest;
}

sub b64digest {
    my $result = MIME::Base64::encode($_[0]->digest);
    $result =~ s/=+$//;
    return $result;
}

sub addfile {
    my $self = shift;
    my $handle = shift;
    my $n;
    my $buff = '';

    while (($n = read $handle, $buff, 4*1024)) {
        $self->add($buff);
    }
    unless (defined $n) {
        croak "read failed: $!";
    }
    return $self;
}

sub add_bits {
    my $self = shift;
    my $bits;
    my $nbits;

    if (scalar @_ == 1) {
        my $arg = shift;
        $bits = pack 'B*', $arg;
        $nbits = length $arg;
    }
    else {
        $bits = shift;
        $nbits = shift;
    }
    if (($nbits % 8) != 0) {
        croak 'Number of bits must be multiple of 8 for this algorithm';
    }
    return $self->add(substr $bits, 0, $nbits/8);
}

1;
__END__

=head1 NAME

Digest::OMAC::Base - The One-key CBC MAC message authentication code (base
class for OMAC hashes)

=head1 SYNOPSIS

  use base qw(Digest::OMAC::Base);

=head1 DESCRIPTION

This module is used internally by L<Digest::CMAC>/L<Digest::OMAC1> and
L<Digest::OMAC2> (which does different shifting than OMAC1 but is otherwise the
same).

