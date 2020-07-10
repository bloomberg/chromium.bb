package File::Slurper;
$File::Slurper::VERSION = '0.012';
use strict;
use warnings;

use Carp 'croak';
use Exporter 5.57 'import';

use Encode 2.11 qw/FB_CROAK STOP_AT_PARTIAL/;
use PerlIO::encoding;

our @EXPORT_OK = qw/read_binary read_text read_lines write_binary write_text read_dir/;

sub read_binary {
	my $filename = shift;

	# This logic is a bit ugly, but gives a significant speed boost
	# because slurpy readline is not optimized for non-buffered usage
	open my $fh, '<:unix', $filename or croak "Couldn't open $filename: $!";
	if (my $size = -s $fh) {
		my $buf;
		my ($pos, $read) = 0;
		do {
			defined($read = read $fh, ${$buf}, $size - $pos, $pos) or croak "Couldn't read $filename: $!";
			$pos += $read;
		} while ($read && $pos < $size);
		return ${$buf};
	}
	else {
		return do { local $/; <$fh> };
	}
}

use constant {
	CRLF_DEFAULT => $^O eq 'MSWin32',
	HAS_UTF8_STRICT => scalar do { local $@; eval { require PerlIO::utf8_strict } },
};

sub _text_layers {
	my ($encoding, $crlf) = @_;
	$crlf = CRLF_DEFAULT if $crlf && $crlf eq 'auto';

	if (HAS_UTF8_STRICT && $encoding =~ /^utf-?8\b/i) {
		return $crlf ? ':unix:utf8_strict:crlf' : ':unix:utf8_strict';
	}
	else {
		# non-ascii compatible encodings such as UTF-16 need encoding before crlf
		return $crlf ? ":raw:encoding($encoding):crlf" : ":raw:encoding($encoding)";
	}
}

sub read_text {
	my ($filename, $encoding, $crlf) = @_;
	$encoding ||= 'utf-8';
	my $layer = _text_layers($encoding, $crlf);

	local $PerlIO::encoding::fallback = STOP_AT_PARTIAL | FB_CROAK;
	open my $fh, "<$layer", $filename or croak "Couldn't open $filename: $!";
	return do { local $/; <$fh> };
}

sub write_text {
	my ($filename, undef, $encoding, $crlf) = @_;
	$encoding ||= 'utf-8';
	my $layer = _text_layers($encoding, $crlf);

	local $PerlIO::encoding::fallback = STOP_AT_PARTIAL | FB_CROAK;
	open my $fh, ">$layer", $filename or croak "Couldn't open $filename: $!";
	print $fh $_[1] or croak "Couldn't write to $filename: $!";
	close $fh or croak "Couldn't write to $filename: $!";
	return;
}

sub write_binary {
	my $filename = $_[0];
	open my $fh, ">:raw", $filename or croak "Couldn't open $filename: $!";
	print $fh $_[1] or croak "Couldn't write to $filename: $!";
	close $fh or croak "Couldn't write to $filename: $!";
	return;
}

sub read_lines {
	my ($filename, $encoding, $crlf, $skip_chomp) = @_;
	$encoding ||= 'utf-8';
	my $layer = _text_layers($encoding, $crlf);

	local $PerlIO::encoding::fallback = STOP_AT_PARTIAL | FB_CROAK;
	open my $fh, "<$layer", $filename or croak "Couldn't open $filename: $!";
	return <$fh> if $skip_chomp;
	my @buf = <$fh>;
	close $fh;
	chomp @buf;
	return @buf;
}

sub read_dir {
	my ($dirname) = @_;
	opendir my ($dir), $dirname or croak "Could not open $dirname: $!";
	return grep { not m/ \A \.\.? \z /x } readdir $dir;
}

1;

# ABSTRACT: A simple, sane and efficient module to slurp a file

__END__

=pod

=encoding UTF-8

=head1 NAME

File::Slurper - A simple, sane and efficient module to slurp a file

=head1 VERSION

version 0.012

=head1 SYNOPSIS

 use File::Slurper 'read_text';
 my $content = read_text($filename);

=head1 DESCRIPTION

This module provides functions for fast and correct slurping and spewing. All functions are optionally exported. All functions throw exceptions on errors, write functions don't return any meaningful value.

=head1 FUNCTIONS

=head2 read_text($filename, $encoding, $crlf)

Reads file C<$filename> into a scalar and decodes it from C<$encoding> (which defaults to UTF-8). If C<$crlf> is true, crlf translation is performed. The default for this argument is off. The special value C<'auto'> will set it to a platform specific default value.

=head2 read_binary($filename)

Reads file C<$filename> into a scalar without any decoding or transformation.

=head2 read_lines($filename, $encoding, $crlf, $skip_chomp)

Reads file C<$filename> into a list/array line-by-line, after decoding from C<$encoding>, optional crlf translation and chomping.

=head2 write_text($filename, $content, $encoding, $crlf)

Writes C<$content> to file C<$filename>, encoding it to C<$encoding> (which defaults to UTF-8). It can also take a C<crlf> argument that works exactly as in read_text.

=head2 write_binary($filename, $content)

Writes C<$content> to file C<$filename> as binary data.

=head2 read_dir($dirname)

Open C<dirname> and return all entries except C<.> and C<..>.

=head1 RATIONALE

This module tries to make it as easy as possible to read and write files correctly and fast. The most correct way of doing this is not always obvious (e.g. L<#83126|https://rt.cpan.org/Public/Bug/Display.html?id=83126>), and just as often the most obvious correct way is not the fastest correct way. This module hides away all such complications behind an easy intuitive interface.

=head1 DEPENDENCIES

This module has an optional dependency on L<PerlIO::utf8_strict|PerlIO::utf8_strict>. Installing this will make UTF-8 encoded IO significantly faster, but should not otherwise affect the operation of this module. This may change into a dependency on the related Unicode::UTF8 in the future.

=head1 SEE ALSO

=over 4

=item * L<Path::Tiny|Path::Tiny>

A minimalistic abstraction handling not only IO but also paths.

=item * L<IO::All|IO::All>

An attempt to expose as many IO related features as possible via a single API.

=item * L<File::Slurp|File::Slurp>

This is a previous generation file slurping module. It has a number of issues, as described L<here|http://blogs.perl.org/users/leon_timmermans/2015/08/fileslurp-is-broken-and-wrong.html>.

=item * L<File::Slurp::Tiny|File::Slurp::Tiny>

This was my previous attempt at a better file slurping module. It's mostly (but not entirely) a drop-in replacement for File::Slurp, which is both a feature (easy conversion) and a bug (interface issues).

=back

=head1 TODO

=over 4

=item * C<open_text>/C<open_binary>?

=item * C<drain_handle>?

=back

=head1 AUTHOR

Leon Timmermans <leont@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2014 by Leon Timmermans.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
