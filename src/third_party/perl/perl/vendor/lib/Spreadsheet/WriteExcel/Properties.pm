package Spreadsheet::WriteExcel::Properties;

###############################################################################
#
# Properties - A module for creating Excel property sets.
#
#
# Used in conjunction with Spreadsheet::WriteExcel
#
# Copyright 2000-2010, John McNamara.
#
# Documentation after __END__
#

use Exporter;
use strict;
use Carp;
use POSIX 'fmod';
use Time::Local 'timelocal';




use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
@ISA        = qw(Exporter);

$VERSION    = '2.40';

# Set up the exports.
my @all_functions = qw(
    create_summary_property_set
    create_doc_summary_property_set
    _pack_property_data
    _pack_VT_I2
    _pack_VT_LPSTR
    _pack_VT_FILETIME
);

my @pps_summaries = qw(
    create_summary_property_set
    create_doc_summary_property_set
);

@EXPORT         = ();
@EXPORT_OK      = (@all_functions);
%EXPORT_TAGS    = (testing          => \@all_functions,
                   property_sets    => \@pps_summaries,
                  );


###############################################################################
#
# create_summary_property_set().
#
# Create the SummaryInformation property set. This is mainly used for the
# Title, Subject, Author, Keywords, Comments, Last author keywords and the
# creation date.
#
sub create_summary_property_set {

    my @properties          = @{$_[0]};

    my $byte_order          = pack 'v',  0xFFFE;
    my $version             = pack 'v',  0x0000;
    my $system_id           = pack 'V',  0x00020105;
    my $class_id            = pack 'H*', '00000000000000000000000000000000';
    my $num_property_sets   = pack 'V',  0x0001;
    my $format_id           = pack 'H*', 'E0859FF2F94F6810AB9108002B27B3D9';
    my $offset              = pack 'V',  0x0030;
    my $num_property        = pack 'V',  scalar @properties;
    my $property_offsets    = '';

    # Create the property set data block and calculate the offsets into it.
    my ($property_data, $offsets) = _pack_property_data(\@properties);

    # Create the property type and offsets based on the previous calculation.
    for my $i (0 .. @properties -1) {
        $property_offsets .= pack('VV', $properties[$i]->[0], $offsets->[$i]);
    }

    # Size of $size (4 bytes) +  $num_property (4 bytes) + the data structures.
    my $size = 8 + length($property_offsets) + length($property_data);
       $size = pack 'V',  $size;


    return  $byte_order         .
            $version            .
            $system_id          .
            $class_id           .
            $num_property_sets  .
            $format_id          .
            $offset             .
            $size               .
            $num_property       .
            $property_offsets   .
            $property_data;
}


###############################################################################
#
# Create the DocSummaryInformation property set. This is mainly used for the
# Manager, Company and Category keywords.
#
# The DocSummary also contains a stream for user defined properties. However
# this is a little arcane and probably not worth the implementation effort.
#
sub create_doc_summary_property_set {

    my @properties          = @{$_[0]};

    my $byte_order          = pack 'v',  0xFFFE;
    my $version             = pack 'v',  0x0000;
    my $system_id           = pack 'V',  0x00020105;
    my $class_id            = pack 'H*', '00000000000000000000000000000000';
    my $num_property_sets   = pack 'V',  0x0002;

    my $format_id_0         = pack 'H*', '02D5CDD59C2E1B10939708002B2CF9AE';
    my $format_id_1         = pack 'H*', '05D5CDD59C2E1B10939708002B2CF9AE';
    my $offset_0            = pack 'V',  0x0044;
    my $num_property_0      = pack 'V',  scalar @properties;
    my $property_offsets_0  = '';

    # Create the property set data block and calculate the offsets into it.
    my ($property_data_0, $offsets) = _pack_property_data(\@properties);

    # Create the property type and offsets based on the previous calculation.
    for my $i (0 .. @properties -1) {
        $property_offsets_0 .= pack('VV', $properties[$i]->[0], $offsets->[$i]);
    }

    # Size of $size (4 bytes) +  $num_property (4 bytes) + the data structures.
    my $data_len = 8 + length($property_offsets_0) + length($property_data_0);
    my $size_0   = pack 'V',  $data_len;


    # The second property set offset is at the end of the first property set.
    my $offset_1 = pack 'V',  0x0044 + $data_len;

    # We will use a static property set stream rather than try to generate it.
    my $property_data_1 = pack 'H*', join '', qw (
        98 00 00 00 03 00 00 00 00 00 00 00 20 00 00 00
        01 00 00 00 36 00 00 00 02 00 00 00 3E 00 00 00
        01 00 00 00 02 00 00 00 0A 00 00 00 5F 50 49 44
        5F 47 55 49 44 00 02 00 00 00 E4 04 00 00 41 00
        00 00 4E 00 00 00 7B 00 31 00 36 00 43 00 34 00
        42 00 38 00 33 00 42 00 2D 00 39 00 36 00 35 00
        46 00 2D 00 34 00 42 00 32 00 31 00 2D 00 39 00
        30 00 33 00 44 00 2D 00 39 00 31 00 30 00 46 00
        41 00 44 00 46 00 41 00 37 00 30 00 31 00 42 00
        7D 00 00 00 00 00 00 00 2D 00 39 00 30 00 33 00
    );


    return  $byte_order         .
            $version            .
            $system_id          .
            $class_id           .
            $num_property_sets  .
            $format_id_0        .
            $offset_0           .
            $format_id_1        .
            $offset_1           .

            $size_0             .
            $num_property_0     .
            $property_offsets_0 .
            $property_data_0    .

            $property_data_1;
}


###############################################################################
#
# _pack_property_data().
#
# Create a packed property set structure. Strings are null terminated and
# padded to a 4 byte boundary. We also use this function to keep track of the
# property offsets within the data structure. These offsets are used by the
# calling functions. Currently we only need to handle 4 property types:
# VT_I2, VT_LPSTR, VT_FILETIME.
#
sub _pack_property_data {

    my @properties          = @{$_[0]};
    my $offset              = $_[1] || 0;
    my $packed_property     = '';
    my $data                = '';
    my @offsets;

    # Get the strings codepage from the first property.
    my $codepage = $properties[0]->[2];

    # The properties start after 8 bytes for size + num_properties + 8 bytes
    # for each property type/offset pair.
    $offset += 8 * (@properties + 1);

    for my $property (@properties) {
        push @offsets, $offset;

        my $property_type = $property->[1];

        if    ($property_type eq 'VT_I2') {
            $packed_property = _pack_VT_I2($property->[2]);
        }
        elsif ($property_type eq 'VT_LPSTR') {
            $packed_property = _pack_VT_LPSTR($property->[2], $codepage);
        }
        elsif ($property_type eq 'VT_FILETIME') {
            $packed_property = _pack_VT_FILETIME($property->[2]);
        }
        else {
            croak "Unknown property type: $property_type\n";
        }

        $offset += length $packed_property;
        $data   .= $packed_property;
    }

    return $data, \@offsets;
}


###############################################################################
#
# _pack_VT_I2().
#
# Pack an OLE property type: VT_I2, 16-bit signed integer.
#
sub _pack_VT_I2 {

    my $type    = 0x0002;
    my $value   = $_[0];

    my $data = pack 'VV', $type, $value;

    return $data;
}


###############################################################################
#
# _pack_VT_LPSTR().
#
# Pack an OLE property type: VT_LPSTR, String in the Codepage encoding.
# The strings are null terminated and padded to a 4 byte boundary.
#
sub _pack_VT_LPSTR {

    my $type        = 0x001E;
    my $string      = $_[0] . "\0";
    my $codepage    = $_[1];
    my $length;
    my $byte_string;

    if ($codepage == 0x04E4) {
        # Latin1
        $byte_string = $string;
        $length      = length $byte_string;
    }
    elsif ($codepage == 0xFDE9) {
        # UTF-8
        if ( $] > 5.008 ) {
            require Encode;
            if (Encode::is_utf8($string)) {
                $byte_string = Encode::encode_utf8($string);
            }
            else {
                $byte_string = $string;
            }
        }
        else {
            $byte_string = $string;
        }

        $length = length $byte_string;
    }
    else {
        croak "Unknown codepage: $codepage\n";
    }

    # Pack the data.
    my $data  = pack 'VV', $type, $length;
       $data .= $byte_string;

    # The packed data has to null padded to a 4 byte boundary.
    if (my $extra = $length % 4) {
        $data .= "\0" x (4 - $extra);
    }

    return $data;
}


###############################################################################
#
# _pack_VT_FILETIME().
#
# Pack an OLE property type: VT_FILETIME.
#
sub _pack_VT_FILETIME {

    my $type        = 0x0040;
    my $localtime   = $_[0];

    # Convert from localtime to seconds.
    my $seconds = Time::Local::timelocal(@{$localtime});

    # Add the number of seconds between the 1601 and 1970 epochs.
    $seconds += 11644473600;

    # The FILETIME seconds are in units of 100 nanoseconds.
    my $nanoseconds = $seconds * 1E7;

    # Pack the total nanoseconds into 64 bits.
    my $time_hi = int($nanoseconds / 2**32);
    my $time_lo = POSIX::fmod($nanoseconds, 2**32);

    my $data = pack 'VVV', $type, $time_lo, $time_hi;

    return $data;
}


1;


__END__

=encoding latin1

=head1 NAME

Properties - A module for creating Excel property sets.

=head1 SYNOPSIS

See the C<set_properties()> method in the Spreadsheet::WriteExcel documentation.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::WriteExcel.

=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
