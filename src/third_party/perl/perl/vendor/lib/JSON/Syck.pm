package JSON::Syck;
use strict;
use vars qw( $VERSION @EXPORT_OK @ISA );
use Exporter;
use YAML::Syck ();

BEGIN {
    $VERSION    = '0.43';
    @EXPORT_OK  = qw( Load Dump LoadFile DumpFile );
    @ISA        = 'Exporter';
    *Load       = \&YAML::Syck::LoadJSON;
    *Dump       = \&YAML::Syck::DumpJSON;
}

sub DumpFile {
    my $file = shift;
    if ( YAML::Syck::_is_glob($file) ) {
        print {$file} YAML::Syck::DumpJSON($_[0]);
    }
    else {
        open(my $fh, '>',  $file) or die "Cannot write to $file: $!";
        print {$fh} YAML::Syck::DumpJSON($_[0]);
        close $fh;
    }
}


sub LoadFile {
    my $file = shift;
    if ( YAML::Syck::_is_glob($file) ) {
        YAML::Syck::LoadJSON(do { local $/; <$file> });
    }
    else {
        if(!-e $file || -z $file) {
	    die("'$file' is non-existant or empty");
	}
        open(my $fh, '<', $file) or die "Cannot read from $file: $!";
        YAML::Syck::LoadJSON(do { local $/; <$fh> });
    }
}

$JSON::Syck::ImplicitTyping  = 1;
$JSON::Syck::Headless        = 1;
$JSON::Syck::ImplicitUnicode = 0;
$JSON::Syck::SingleQuote     = 0;

1;

__END__

=head1 NAME

JSON::Syck - JSON is YAML (but consider using L<JSON::XS> instead!)

=head1 SYNOPSIS

    use JSON::Syck; # no exports by default 

    my $data = JSON::Syck::Load($json);
    my $json = JSON::Syck::Dump($data);

    # $file can be an IO object, or a filename
    my $data = JSON::Syck::LoadFile($file);
    JSON::Syck::DumpFile($file, $data);

=head1 DESCRIPTION

JSON::Syck is a syck implementation of JSON parsing and generation. Because
JSON is YAML (L<http://redhanded.hobix.com/inspect/yamlIsJson.html>), using
syck gives you a fast and memory-efficient parser and dumper for JSON data
representation.

However, a newer module L<JSON::XS>, has since emerged.  It is more flexible,
efficient and robust, so please consider using it instead of this module.

=head1 DIFFERENCE WITH JSON

You might want to know the difference between the I<JSON> module and
this one.

Since JSON is a pure-perl module and JSON::Syck is based on libsyck,
JSON::Syck is supposed to be very fast and memory efficient. See
chansen's benchmark table at
L<http://idisk.mac.com/christian.hansen/Public/perl/serialize.pl>

JSON.pm comes with dozens of ways to do the same thing and lots of
options, while JSON::Syck doesn't. There's only C<Load> and C<Dump>.

Oh, and JSON::Syck doesn't use camelCase method names :-)

=head1 REFERENCES

=head2 SCALAR REFERENCE

For now, when you pass a scalar reference to JSON::Syck, it
dereferences to get the actual scalar value.

JSON::Syck raises an exception when you pass in circular references.

If you want to serialize self refernecing stuff, you should use
YAML which supports it.

=head2 SUBROUTINE REFERENCE

When you pass subroutine reference, JSON::Syck dumps it as null.

=head1 UTF-8 FLAGS

By default this module doesn't touch any of utf-8 flags set in
strings, and assumes UTF-8 bytes to be passed and emit.

However, when you set C<$JSON::Syck::ImplicitUnicode> to 1, this
module properly decodes UTF-8 binaries and sets UTF-8 flag everywhere,
as in:

  JSON (UTF-8 bytes)   => Perl (UTF-8 flagged)
  JSON (UTF-8 flagged) => Perl (UTF-8 flagged)
  Perl (UTF-8 bytes)   => JSON (UTF-8 flagged)
  Perl (UTF-8 flagged) => JSON (UTF-8 flagged)

Unfortunately, there's no implicit way to dump Perl UTF-8 flagged data
structure to utf-8 encoded JSON. To do this, simply use Encode module, e.g.:

  use Encode;
  use JSON::Syck qw(Dump);

  my $json = encode_utf8( Dump($data) );

Alternatively you can use Encode::JavaScript::UCS to encode Unicode
strings as in I<%uXXXX> form.

  use Encode;
  use Encode::JavaScript::UCS;
  use JSON::Syck qw(Dump);

  my $json_unicode_escaped = encode( 'JavaScript-UCS', Dump($data) );

=head1 QUOTING

According to the JSON specification, all JSON strings are to be double-quoted.
However, when embedding JavaScript in HTML attributes, it may be more
convenient to use single quotes.

Set C<$JSON::Syck::SingleQuote> to 1 will make both C<Dump> and C<Load> expect
single-quoted string literals.

=head1 SEE ALSO

L<JSON::XS>,

=head1 AUTHORS

Audrey Tang E<lt>cpan@audreyt.orgE<gt>

Tatsuhiko Miyagawa E<lt>miyagawa@gmail.comE<gt>

=head1 COPYRIGHT

Copyright 2005-2009 by Audrey Tang E<lt>cpan@audreyt.orgE<gt>.

This software is released under the MIT license cited below.

The F<libsyck> code bundled with this library is released by
"why the lucky stiff", under a BSD-style license.  See the F<COPYING>
file for details.

=head2 The "MIT" License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=cut
