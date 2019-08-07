package Compress::unLZMA;

use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ( 'all' => [ qw(
	uncompress uncompressfile
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our $VERSION = '0.04';

require XSLoader;
XSLoader::load('Compress::unLZMA', $VERSION);

sub compress {
	die "not implemented!\n";
}

sub compressfile {
	die "not implemented!\n";
}

sub uncompress {
	my ($data) = @_;

	return undef if (length($data) < 14);

	my $properties = substr($data, 0, 5);

	my $size = 0;
	for (my $ii = 0; $ii < 4; $ii++) {
		my $b = ord(substr($data, $ii + 5, 1));
		$size += $b << ($ii * 8);
	}

	for (my $ii = 0; $ii < 4; $ii++) {
		my $b = ord(substr($data, $ii + 9, 1));
		if ($b != 0) {
			return undef;
		}
	}

	if (ord(substr($properties, 0, 1)) >= (9 * 5 * 5)) {
		return undef;
	}

	if ( $size == 0 ) { # empty file: no data to uncompress
		return '';
	}

	$data = substr($data, 13);

	my $result = uncompressdata($data, length($data), $size, $properties);

	return $result;
}

1;

__END__

=head1 NAME

Compress::unLZMA - Interface to LZMA decompression library

=head1 SYNOPSIS

  use Compress::unLZMA qw(uncompress uncompressfile);
  my $data = uncompressfile('foo.lzma');
  my $data = uncompress($content);

=head1 DESCRIPTION

LZMA is default and general compression method of 7z format
in 7-Zip program. LZMA provides high compression ratio and very 
fast decompression.

This module is based on the LZMA SDK which provides a cross-platform 
implementation of the LZMA decompression algorithm in ANSI C.

From SDK lzma.txt file:

  LZMA decompression code was ported from original C++ sources to C.
  Also it was simplified and optimized for code size.
  But it is fully compatible with LZMA from 7-Zip.

=head1 METHODS

=over 4

=item $data = uncompress($content)

Uncompress $data. if successfull, it returns the uncompressed data.
Otherwise it returns undef and $@ contains the error message.

The source buffer can be either be a scalar or a scalar reference.

=item $data = uncompressfile($file)

Uncompress the file $file. if successfull, it returns the uncompressed data.
Otherwise it returns undef and $@ contains the error message.

=back

=head1 CAVEATS

This version only implements in-memory decompression (patches are welcomed).

There is no way to recognize a valid LZMA encoded file with the SDK.
So, in some cases, you can crash your script if you try to uncompress
a non valid LZMA encoded file.

=head1 REQUESTS & BUGS

Please report any requests, suggestions or bugs via the RT bug-tracking system 
at http://rt.cpan.org/ or email to bug-Compress-unLZMA\@rt.cpan.org. 

http://rt.cpan.org/NoAuth/Bugs.html?Dist=Compress-unLZMA is the RT queue for Compress::unLZMA.
Please check to see if your bug has already been reported. 

=head1 MAINTAINER

Adriano Ferreira, ferreira@cpan.org

=head1 COPYRIGHT

Copyright 2004

Fabien Potencier, fabpot@cpan.org

This software may be freely copied and distributed under the same
terms and conditions as Perl.

=head1 LZMA COPYRIGHT

LZMA SDK 4.01  Copyright (C) 1999-2004 Igor Pavlov  2004-02-15

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

LZMA SDK also can be available under a proprietary license for 
those who cannot use the GNU LGPL in their code. To request
such proprietary license or any additional consultations,
write to support@7-zip.org

=head1 SEE ALSO

perl(1), http://www.7-zip.org/.

=cut
