package Digest::OMAC2;

use base qw(Digest::OMAC::Base);

use strict;
#use warnings;
use Carp;
use MIME::Base64;

# we still call it Lu2 even though it's actually no longer squared ;-)

sub _lu2 {
	my ( $self, $blocksize,  $L ) = @_;
	$self->_shift_lu2( $L, $self->_lu2_constant($blocksize) );
}

sub _shift_lu2 {
	my ( $self, $L, $constant ) = @_;

	# used to do Bit::Vector's shift_left but that's broken
	my $unpacked = unpack("B*",$L);
	my $lsb = chop $unpacked;
	my $Lt = pack("B*", "0" . $unpacked);

	if ( $lsb ) {
		return $Lt ^ $constant;
	} else {
		return $Lt;
	}
}

sub _lu2_constant {
	my ( $self, $blocksize ) = @_;

	if ( $blocksize == 16 ) { # 128
		return ("\x80" . ("\x00" x 14) . "\x43");
	} elsif ( $blocksize == 8 ) { # 64
		return ("\x80" . ("\x00" x 6) . "\x0d");
	} else {
		die "Blocksize $blocksize is not supported by OMAC";
	}
}

1;
__END__

=pod

=head1 NAME

Digest::OMAC2 - OMAC 2 implementation

=head1 SYNOPSIS

	use Digest::OMAC2;

	my $d = Digest::OMAC2->new( $key, $cipher );

=head1 DESCRIPTION

OMAC 2 is a variant of the CMAC/OMAC 1 algorithm. The initialization routines
are slightly different. OMAC2 actually precedes OMAC1, so
L<Digest::CMAC>/L<Digest::OMAC1> is the reccomended version. Supposedly OMAC1
was more rigorously analyzed.
