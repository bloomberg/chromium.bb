package Spreadsheet::ParseExcel::Cell;

###############################################################################
#
# Spreadsheet::ParseExcel::Cell - A class for Cell data and formatting.
#
# Used in conjunction with Spreadsheet::ParseExcel.
#
# Copyright (c) 2014      Douglas Wilson
# Copyright (c) 2009-2013 John McNamara
# Copyright (c) 2006-2008 Gabor Szabo
# Copyright (c) 2000-2006 Kawai Takanori
#
# perltidy with standard settings.
#
# Documentation after __END__
#

use strict;
use warnings;

our $VERSION = '0.65';

###############################################################################
#
# new()
#
# Constructor.
#
sub new {
    my ( $package, %properties ) = @_;
    my $self = \%properties;

    bless $self, $package;
}

###############################################################################
#
# value()
#
# Returns the formatted value of the cell.
#
sub value {

    my $self = shift;

    return $self->{_Value};
}

###############################################################################
#
# unformatted()
#
# Returns the unformatted value of the cell.
#
sub unformatted {

    my $self = shift;

    return $self->{Val};
}

###############################################################################
#
# get_format()
#
# Returns the Format object for the cell.
#
sub get_format {

    my $self = shift;

    return $self->{Format};
}

###############################################################################
#
# type()
#
# Returns the type of cell such as Text, Numeric or Date.
#
sub type {

    my $self = shift;

    return $self->{Type};
}

###############################################################################
#
# encoding()
#
# Returns the character encoding of the cell.
#
sub encoding {

    my $self = shift;

    if ( !defined $self->{Code} ) {
        return 1;
    }
    elsif ( $self->{Code} eq 'ucs2' ) {
        return 2;
    }
    elsif ( $self->{Code} eq '_native_' ) {
        return 3;
    }
    else {
        return 0;
    }

    return $self->{Code};
}

###############################################################################
#
# is_merged()
#
# Returns true if the cell is merged.
#
sub is_merged {

    my $self = shift;

    return $self->{Merged};
}

###############################################################################
#
# get_rich_text()
#
# Returns an array ref of font information about each string block in a "rich",
# i.e. multi-format, string.
#
sub get_rich_text {

    my $self = shift;

    return $self->{Rich};
}

###############################################################################
#
# get_hyperlink {
#
# Returns an array ref of hyperlink information if the cell contains a hyperlink.
# Returns undef otherwise
#
# [0] : Description of link (You may want $cell->value, as it will have rich text)
# [1] : URL - the link expressed as a URL. N.B. relative URLs will be defaulted to
#       the directory of the input file, if the input file name is known. Otherwise
#       %REL% will be inserted as a place-holder.  Depending on your application,
#       you should either remove %REL% or replace it with the appropriate path.
# [2] : Target frame (or undef if none)

sub get_hyperlink {
    my $self = shift;

    return $self->{Hyperlink} if exists $self->{Hyperlink};
    return undef;
}

# 
###############################################################################
#
# Mapping between legacy method names and new names.
#
{
    no warnings;    # Ignore warnings about variables used only once.
    *Value = \&value;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::Cell - A class for Cell data and formatting.

=head1 SYNOPSIS

See the documentation for Spreadsheet::ParseExcel.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::ParseExcel. See the documentation for Spreadsheet::ParseExcel.

=head1 Methods

The following Cell methods are available:

    $cell->value()
    $cell->unformatted()
    $cell->get_format()
    $cell->type()
    $cell->encoding()
    $cell->is_merged()
    $cell->get_rich_text()
    $cell->get_hyperlink()


=head2 value()

The C<value()> method returns the formatted value of the cell.

    my $value = $cell->value();

Formatted in this sense refers to the numeric format of the cell value. For example a number such as 40177 might be formatted as 40,117, 40117.000 or even as the date 2009/12/30.

If the cell doesn't contain a numeric format then the formatted and unformatted cell values are the same, see the C<unformatted()> method below.

For a defined C<$cell> the C<value()> method will always return a value.

In the case of a cell with formatting but no numeric or string contents the method will return the empty string C<''>.


=head2 unformatted()

The C<unformatted()> method returns the unformatted value of the cell.

    my $unformatted = $cell->unformatted();

Returns the cell value without a numeric format. See the C<value()> method above.


=head2 get_format()

The C<get_format()> method returns the L<Spreadsheet::ParseExcel::Format> object for the cell.

    my $format = $cell->get_format();

If a user defined format hasn't been applied to the cell then the default cell format is returned.


=head2 type()

The C<type()> method returns the type of cell such as Text, Numeric or Date. If the type was detected as Numeric, and the Cell Format matches C<m{^[dmy][-\\/dmy]*$}i>, it will be treated as a Date type.

    my $type = $cell->type();

See also L<Dates and Time in Excel>.


=head2 encoding()

The C<encoding()> method returns the character encoding of the cell.

    my $encoding = $cell->encoding();

This method is only of interest to developers. In general Spreadsheet::ParseExcel will return all character strings in UTF-8 regardless of the encoding used by Excel.

The C<encoding()> method returns one of the following values:

=over

=item * 0: Unknown format. This shouldn't happen. In the default case the format should be 1.

=item * 1: 8bit ASCII or single byte UTF-16. This indicates that the characters are encoded in a single byte. In Excel 95 and earlier This usually meant ASCII or an international variant. In Excel 97 it refers to a compressed UTF-16 character string where all of the high order bytes are 0 and are omitted to save space.

=item * 2: UTF-16BE.

=item * 3: Native encoding. In Excel 95 and earlier this encoding was used to represent multi-byte character encodings such as SJIS.

=back


=head2 is_merged()

The C<is_merged()> method returns true if the cell is merged.

    my $is_merged = $cell->is_merged();

Returns C<undef> if the property isn't set.


=head2 get_rich_text()

The C<get_rich_text()> method returns an array ref of font information about each string block in a "rich", i.e. multi-format, string.

    my $rich_text = $cell->get_rich_text();

The return value is an arrayref of arrayrefs in the form:

    [
        [ $start_position, $font_object ],
         ...,
    ]

Returns undef if the property isn't set.

=head2 get_hyperlink()

If a cell contains a hyperlink, the C<get_hyperlink()> method returns an array ref of information about it.

A cell can contain at most one hyperlink.  If it does, it contains no other value.

Otherwise, it returns undef;

The array contains:

=over

=item * 0: Description (what's displayed); undef if not present

=item * 1: Link, converted to an appropriate URL - Note: Relative links are based on the input file.  %REL% is used if the input file is unknown (e.g. a file handle or scalar)

=item * 2: Target - target frame (or undef if none)

=back

=head1 Dates and Time in Excel

Dates and times in Excel are represented by real numbers, for example "Jan 1 2001 12:30 PM" is represented by the number 36892.521.

The integer part of the number stores the number of days since the epoch and the fractional part stores the percentage of the day.

A date or time in Excel is just like any other number. The way in which it is displayed is controlled by the number format:

    Number format               $cell->value()            $cell->unformatted()
    =============               ==============            ==============
    'dd/mm/yy'                  '28/02/08'                39506.5
    'mm/dd/yy'                  '02/28/08'                39506.5
    'd-m-yyyy'                  '28-2-2008'               39506.5
    'dd/mm/yy hh:mm'            '28/02/08 12:00'          39506.5
    'd mmm yyyy'                '28 Feb 2008'             39506.5
    'mmm d yyyy hh:mm AM/PM'    'Feb 28 2008 12:00 PM'    39506.5


The L<Spreadsheet::ParseExcel::Utility> module contains a function called C<ExcelLocaltime> which will convert between an unformatted Excel date/time number and a C<localtime()> like array.

For date conversions using the CPAN C<DateTime> framework see L<DateTime::Format::Excel> http://search.cpan.org/search?dist=DateTime-Format-Excel


=head1 AUTHOR

Current maintainer 0.60+: Douglas Wilson dougw@cpan.org

Maintainer 0.40-0.59: John McNamara jmcnamara@cpan.org

Maintainer 0.27-0.33: Gabor Szabo szabgab@cpan.org

Original author: Kawai Takanori kwitknr@cpan.org

=head1 COPYRIGHT

Copyright (c) 2014 Douglas Wilson

Copyright (c) 2009-2013 John McNamara

Copyright (c) 2006-2008 Gabor Szabo

Copyright (c) 2000-2006 Kawai Takanori

All rights reserved.

You may distribute under the terms of either the GNU General Public License or the Artistic License, as specified in the Perl README file.

=cut
