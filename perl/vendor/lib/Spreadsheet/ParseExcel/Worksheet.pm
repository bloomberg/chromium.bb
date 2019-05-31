package Spreadsheet::ParseExcel::Worksheet;

###############################################################################
#
# Spreadsheet::ParseExcel::Worksheet - A class for Worksheets.
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
use Scalar::Util qw(weaken);

our $VERSION = '0.65';

###############################################################################
#
# new()
#
sub new {

    my ( $class, %properties ) = @_;

    my $self = \%properties;

    weaken $self->{_Book};

    $self->{Cells}       = undef;
    $self->{DefColWidth} = 8.43;

    return bless $self, $class;
}

###############################################################################
#
# get_cell( $row, $col )
#
# Returns the Cell object at row $row and column $col, if defined.
#
sub get_cell {

    my ( $self, $row, $col ) = @_;

    if (   !defined $row
        || !defined $col
        || !defined $self->{MaxRow}
        || !defined $self->{MaxCol} )
    {

        # Return undef if no arguments are given or if no cells are defined.
        return undef;
    }
    elsif ($row < $self->{MinRow}
        || $row > $self->{MaxRow}
        || $col < $self->{MinCol}
        || $col > $self->{MaxCol} )
    {

        # Return undef if outside allowable row/col range.
        return undef;
    }
    else {

        # Return the Cell object.
        return $self->{Cells}->[$row]->[$col];
    }
}

###############################################################################
#
# row_range()
#
# Returns a two-element list ($min, $max) containing the minimum and maximum
# defined rows in the worksheet.
#
# If there is no row defined $max is smaller than $min.
#
sub row_range {

    my $self = shift;

    my $min = $self->{MinRow} || 0;
    my $max = defined( $self->{MaxRow} ) ? $self->{MaxRow} : ( $min - 1 );

    return ( $min, $max );
}

###############################################################################
#
# col_range()
#
# Returns a two-element list ($min, $max) containing the minimum and maximum
# defined cols in the worksheet.
#
# If there is no column defined $max is smaller than $min.
#
sub col_range {

    my $self = shift;

    my $min = $self->{MinCol} || 0;
    my $max = defined( $self->{MaxCol} ) ? $self->{MaxCol} : ( $min - 1 );

    return ( $min, $max );
}

###############################################################################
#
# get_name()
#
# Returns the name of the worksheet.
#
sub get_name {

    my $self = shift;

    return $self->{Name};
}

###############################################################################
#
# sheet_num()
#
sub sheet_num {

    my $self = shift;

    return $self->{_SheetNo};
}

###############################################################################
#
# get_h_pagebreaks()
#
# Returns an array ref of row numbers where a horizontal page break occurs.
#
sub get_h_pagebreaks {

    my $self = shift;

    return $self->{HPageBreak};
}

###############################################################################
#
# get_v_pagebreaks()
#
# Returns an array ref of column numbers where a vertical page break occurs.
#
sub get_v_pagebreaks {

    my $self = shift;

    return $self->{VPageBreak};
}

###############################################################################
#
# get_merged_areas()
#
# Returns an array ref of cells that are merged.
#
sub get_merged_areas {

    my $self = shift;

    return $self->{MergedArea};
}

###############################################################################
#
# get_row_heights()
#
# Returns an array of row heights.
#
sub get_row_heights {

    my $self = shift;

    if ( wantarray() ) {
      return unless $self->{RowHeight};
      return @{ $self->{RowHeight} };
    }
    return $self->{RowHeight};
}

###############################################################################
#
# get_col_widths()
#
# Returns an array of column widths.
#
sub get_col_widths {

    my $self = shift;

    if ( wantarray() ) {
      return unless $self->{ColWidth};
      return @{ $self->{ColWidth} };
    }
    return $self->{ColWidth};
}

###############################################################################
#
# get_default_row_height()
#
# Returns the default row height for the worksheet. Generally 12.75.
#
sub get_default_row_height {

    my $self = shift;

    return $self->{DefRowHeight};
}

###############################################################################
#
# get_default_col_width()
#
# Returns the default column width for the worksheet. Generally 8.43.
#
sub get_default_col_width {

    my $self = shift;

    return $self->{DefColWidth};
}

###############################################################################
#
# _get_row_properties()
#
# Returns an array_ref of row properties.
# TODO. This is a placeholder for a future method.
#
sub _get_row_properties {

    my $self = shift;

    return $self->{RowProperties};
}

###############################################################################
#
# _get_col_properties()
#
# Returns an array_ref of column properties.
# TODO. This is a placeholder for a future method.
#
sub _get_col_properties {

    my $self = shift;

    return $self->{ColProperties};
}

###############################################################################
#
# get_header()
#
# Returns the worksheet header string.
#
sub get_header {

    my $self = shift;

    return $self->{Header};
}

###############################################################################
#
# get_footer()
#
# Returns the worksheet footer string.
#
sub get_footer {

    my $self = shift;

    return $self->{Footer};
}

###############################################################################
#
# get_margin_left()
#
# Returns the left margin of the worksheet in inches.
#
sub get_margin_left {

    my $self = shift;

    return $self->{LeftMargin};
}

###############################################################################
#
# get_margin_right()
#
# Returns the right margin of the worksheet in inches.
#
sub get_margin_right {

    my $self = shift;

    return $self->{RightMargin};
}

###############################################################################
#
# get_margin_top()
#
# Returns the top margin of the worksheet in inches.
#
sub get_margin_top {

    my $self = shift;

    return $self->{TopMargin};
}

###############################################################################
#
# get_margin_bottom()
#
# Returns the bottom margin of the worksheet in inches.
#
sub get_margin_bottom {

    my $self = shift;

    return $self->{BottomMargin};
}

###############################################################################
#
# get_margin_header()
#
# Returns the header margin of the worksheet in inches.
#
sub get_margin_header {

    my $self = shift;

    return $self->{HeaderMargin};
}

###############################################################################
#
# get_margin_footer()
#
# Returns the footer margin of the worksheet in inches.
#
sub get_margin_footer {

    my $self = shift;

    return $self->{FooterMargin};
}

###############################################################################
#
# get_paper()
#
# Returns the printer paper size.
#
sub get_paper {

    my $self = shift;

    return $self->{PaperSize};
}

###############################################################################
#
# get_start_page()
#
# Returns the page number that printing will start from.
#
sub get_start_page {

    my $self = shift;

    # Only return the page number if the "First page number" option is set.
    if ( $self->{UsePage} ) {
        return $self->{PageStart};
    }
    else {
        return 0;
    }
}

###############################################################################
#
# get_print_order()
#
# Returns the Worksheet page printing order.
#
sub get_print_order {

    my $self = shift;

    return $self->{LeftToRight};
}

###############################################################################
#
# get_print_scale()
#
# Returns the workbook scale for printing.
#
sub get_print_scale {

    my $self = shift;

    return $self->{Scale};
}

###############################################################################
#
# get_fit_to_pages()
#
# Returns the number of pages wide and high that the printed worksheet page
# will fit to.
#
sub get_fit_to_pages {

    my $self = shift;

    if ( !$self->{PageFit} ) {
        return ( 0, 0 );
    }
    else {
        return ( $self->{FitWidth}, $self->{FitHeight} );
    }
}

###############################################################################
#
# is_portrait()
#
# Returns true if the worksheet has been set for printing in portrait mode.
#
sub is_portrait {

    my $self = shift;

    return $self->{Landscape};
}

###############################################################################
#
# is_centered_horizontally()
#
# Returns true if the worksheet has been centered horizontally for printing.
#
sub is_centered_horizontally {

    my $self = shift;

    return $self->{HCenter};
}

###############################################################################
#
# is_centered_vertically()
#
# Returns true if the worksheet has been centered vertically for printing.
#
sub is_centered_vertically {

    my $self = shift;

    return $self->{HCenter};
}

###############################################################################
#
# is_print_gridlines()
#
# Returns true if the worksheet print "gridlines" option is turned on.
#
sub is_print_gridlines {

    my $self = shift;

    return $self->{PrintGrid};
}

###############################################################################
#
# is_print_row_col_headers()
#
# Returns true if the worksheet print "row and column headings" option is on.
#
sub is_print_row_col_headers {

    my $self = shift;

    return $self->{PrintHeaders};
}

###############################################################################
#
# is_print_black_and_white()
#
# Returns true if the worksheet print "black and white" option is turned on.
#
sub is_print_black_and_white {

    my $self = shift;

    return $self->{NoColor};
}

###############################################################################
#
# is_print_draft()
#
# Returns true if the worksheet print "draft" option is turned on.
#
sub is_print_draft {

    my $self = shift;

    return $self->{Draft};
}

###############################################################################
#
# is_print_comments()
#
# Returns true if the worksheet print "comments" option is turned on.
#
sub is_print_comments {

    my $self = shift;

    return $self->{Notes};
}

=head2 get_tab_color()

Return color index of tab, or undef if not set.

=cut

sub get_tab_color {
    my $worksheet = shift;

    return $worksheet->{TabColor};
}

=head2 is_sheet_hidden()

Return true if sheet is hidden

=cut

sub is_sheet_hidden {
    my $worksheet = shift;

    return $worksheet->{SheetHidden};
}

=head2 is_row_hidden($row)

In scalar context, return true if $row is hidden
In array context, return an array whose elements are true
if the corresponding row is hidden.

=cut

sub is_row_hidden {
    my $worksheet = shift;

    my ($row) = @_;

    unless ( $worksheet->{RowHidden} ) {
        return () if (wantarray);
        return 0;
    }

    return @{ $worksheet->{RowHidden} } if (wantarray);
    return $worksheet->{RowHidden}[$row];
}

=head2 is_col_hidden($col)

In scalar context, return true if $col is hidden
In array context, return an array whose elements are true
if the corresponding column is hidden.

=cut

sub is_col_hidden {
    my $worksheet = shift;

    my ($col) = @_;

    unless ( $worksheet->{ColHidden} ) {
        return () if (wantarray);
        return 0;
    }

    return @{ $worksheet->{ColHidden} } if (wantarray);
    return $worksheet->{ColHidden}[$col];
}

###############################################################################
#
# Mapping between legacy method names and new names.
#
{
    no warnings;    # Ignore warnings about variables used only once.
    *sheetNo  = *sheet_num;
    *Cell     = *get_cell;
    *RowRange = *row_range;
    *ColRange = *col_range;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::Worksheet - A class for Worksheets.

=head1 SYNOPSIS

See the documentation for L<Spreadsheet::ParseExcel>.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::ParseExcel. See the documentation for Spreadsheet::ParseExcel.

=head1 Methods

The C<Spreadsheet::ParseExcel::Worksheet> class encapsulates the properties of an Excel worksheet. It has the following methods:

    $worksheet->get_cell()
    $worksheet->row_range()
    $worksheet->col_range()
    $worksheet->get_name()
    $worksheet->get_h_pagebreaks()
    $worksheet->get_v_pagebreaks()
    $worksheet->get_merged_areas()
    $worksheet->get_row_heights()
    $worksheet->get_col_widths()
    $worksheet->get_default_row_height()
    $worksheet->get_default_col_width()
    $worksheet->get_header()
    $worksheet->get_footer()
    $worksheet->get_margin_left()
    $worksheet->get_margin_right()
    $worksheet->get_margin_top()
    $worksheet->get_margin_bottom()
    $worksheet->get_margin_header()
    $worksheet->get_margin_footer()
    $worksheet->get_paper()
    $worksheet->get_start_page()
    $worksheet->get_print_order()
    $worksheet->get_print_scale()
    $worksheet->get_fit_to_pages()
    $worksheet->is_portrait()
    $worksheet->is_centered_horizontally()
    $worksheet->is_centered_vertically()
    $worksheet->is_print_gridlines()
    $worksheet->is_print_row_col_headers()
    $worksheet->is_print_black_and_white()
    $worksheet->is_print_draft()
    $worksheet->is_print_comments()


=head2 get_cell($row, $col)

Return the L</Cell> object at row C<$row> and column C<$col> if it is defined. Otherwise returns undef.

    my $cell = $worksheet->get_cell($row, $col);

=head2 row_range()

Returns a two-element list C<($min, $max)> containing the minimum and maximum defined rows in the worksheet. If there is no row defined C<$max> is smaller than C<$min>.

    my ( $row_min, $row_max ) = $worksheet->row_range();

=head2 col_range()

Returns a two-element list C<($min, $max)> containing the minimum and maximum of defined columns in the worksheet. If there is no column defined C<$max> is smaller than C<$min>.

    my ( $col_min, $col_max ) = $worksheet->col_range();


=head2 get_name()

The C<get_name()> method returns the name of the worksheet.

    my $name = $worksheet->get_name();


=head2 get_h_pagebreaks()

The C<get_h_pagebreaks()> method returns an array ref of row numbers where a horizontal page break occurs.

    my $h_pagebreaks = $worksheet->get_h_pagebreaks();

Returns C<undef> if there are no pagebreaks.


=head2 get_v_pagebreaks()

The C<get_v_pagebreaks()> method returns an array ref of column numbers where a vertical page break occurs.

    my $v_pagebreaks = $worksheet->get_v_pagebreaks();

Returns C<undef> if there are no pagebreaks.


=head2 get_merged_areas()

The C<get_merged_areas()> method returns an array ref of cells that are merged.

    my $merged_areas = $worksheet->get_merged_areas();

Each merged area is represented as follows:

    [ $start_row, $start_col, $end_row, $end_col]

Returns C<undef> if there are no merged areas.


=head2 get_row_heights()

The C<get_row_heights()> method returns an array_ref of row heights in scalar context,
and an array in list context.

    my $row_heights = $worksheet->get_row_heights();

Returns C<undef> if the property isn't set.


=head2 get_col_widths()

The C<get_col_widths()> method returns an array_ref of column widths in scalar context,
and an array in list context.

    my $col_widths = $worksheet->get_col_widths();

Returns C<undef> if the property isn't set.


=head2 get_default_row_height()

The C<get_default_row_height()> method returns the default row height for the worksheet. Generally 12.75.

    my $default_row_height = $worksheet->get_default_row_height();


=head2 get_default_col_width()

The C<get_default_col_width()> method returns the default column width for the worksheet. Generally 8.43.

    my $default_col_width = $worksheet->get_default_col_width();


=head2 get_header()

The C<get_header()> method returns the worksheet header string. This string can contain control codes for alignment and font properties. Refer to the Excel on-line help on headers and footers or to the Spreadsheet::WriteExcel documentation for set_header().

    my $header = $worksheet->get_header();

Returns C<undef> if the property isn't set.


=head2 get_footer()

The C<get_footer()> method returns the worksheet footer string. This string can contain control codes for alignment and font properties. Refer to the Excel on-line help on headers and footers or to the Spreadsheet::WriteExcel documentation for set_header().

    my $footer = $worksheet->get_footer();

Returns C<undef> if the property isn't set.


=head2 get_margin_left()

The C<get_margin_left()> method returns the left margin of the worksheet in inches.

    my $margin_left = $worksheet->get_margin_left();

Returns C<undef> if the property isn't set.


=head2 get_margin_right()

The C<get_margin_right()> method returns the right margin of the worksheet in inches.

    my $margin_right = $worksheet->get_margin_right();

Returns C<undef> if the property isn't set.


=head2 get_margin_top()

The C<get_margin_top()> method returns the top margin of the worksheet in inches.

    my $margin_top = $worksheet->get_margin_top();

Returns C<undef> if the property isn't set.


=head2 get_margin_bottom()

The C<get_margin_bottom()> method returns the bottom margin of the worksheet in inches.

    my $margin_bottom = $worksheet->get_margin_bottom();

Returns C<undef> if the property isn't set.


=head2 get_margin_header()

The C<get_margin_header()> method returns the header margin of the worksheet in inches.

    my $margin_header = $worksheet->get_margin_header();

Returns a default value of 0.5 if not set.


=head2 get_margin_footer()

The C<get_margin_footer()> method returns the footer margin of the worksheet in inches.

    my $margin_footer = $worksheet->get_margin_footer();

Returns a default value of 0.5 if not set.


=head2 get_paper()

The C<get_paper()> method returns the printer paper size.

    my $paper = $worksheet->get_paper();

The value corresponds to the formats shown below:

    Index   Paper format            Paper size
    =====   ============            ==========
      0     Printer default         -
      1     Letter                  8 1/2 x 11 in
      2     Letter Small            8 1/2 x 11 in
      3     Tabloid                 11 x 17 in
      4     Ledger                  17 x 11 in
      5     Legal                   8 1/2 x 14 in
      6     Statement               5 1/2 x 8 1/2 in
      7     Executive               7 1/4 x 10 1/2 in
      8     A3                      297 x 420 mm
      9     A4                      210 x 297 mm
     10     A4 Small                210 x 297 mm
     11     A5                      148 x 210 mm
     12     B4                      250 x 354 mm
     13     B5                      182 x 257 mm
     14     Folio                   8 1/2 x 13 in
     15     Quarto                  215 x 275 mm
     16     -                       10x14 in
     17     -                       11x17 in
     18     Note                    8 1/2 x 11 in
     19     Envelope  9             3 7/8 x 8 7/8
     20     Envelope 10             4 1/8 x 9 1/2
     21     Envelope 11             4 1/2 x 10 3/8
     22     Envelope 12             4 3/4 x 11
     23     Envelope 14             5 x 11 1/2
     24     C size sheet            -
     25     D size sheet            -
     26     E size sheet            -
     27     Envelope DL             110 x 220 mm
     28     Envelope C3             324 x 458 mm
     29     Envelope C4             229 x 324 mm
     30     Envelope C5             162 x 229 mm
     31     Envelope C6             114 x 162 mm
     32     Envelope C65            114 x 229 mm
     33     Envelope B4             250 x 353 mm
     34     Envelope B5             176 x 250 mm
     35     Envelope B6             176 x 125 mm
     36     Envelope                110 x 230 mm
     37     Monarch                 3.875 x 7.5 in
     38     Envelope                3 5/8 x 6 1/2 in
     39     Fanfold                 14 7/8 x 11 in
     40     German Std Fanfold      8 1/2 x 12 in
     41     German Legal Fanfold    8 1/2 x 13 in
     256    User defined

The two most common paper sizes are C<1 = "US Letter"> and C<9 = A4>. Returns 9 by default.


=head2 get_start_page()

The C<get_start_page()> method returns the page number that printing will start from.

    my $start_page = $worksheet->get_start_page();

Returns 0 if the property isn't set.


=head2 get_print_order()

The C<get_print_order()> method returns 0 if the worksheet print "page order" is "Down then over" (the default) or 1 if it is "Over then down".

    my $print_order = $worksheet->get_print_order();


=head2 get_print_scale()

The C<get_print_scale()> method returns the workbook scale for printing. The print scale factor can be in the range 10 .. 400.

    my $print_scale = $worksheet->get_print_scale();

Returns 100 by default.


=head2 get_fit_to_pages()

The C<get_fit_to_pages()> method returns the number of pages wide and high that the printed worksheet page will fit to.

    my ($pages_wide, $pages_high) = $worksheet->get_fit_to_pages();

Returns (0, 0) if the property isn't set.


=head2 is_portrait()

The C<is_portrait()> method returns true if the worksheet has been set for printing in portrait mode.

    my $is_portrait = $worksheet->is_portrait();

Returns 0 if the worksheet has been set for printing in horizontal mode.


=head2 is_centered_horizontally()

The C<is_centered_horizontally()> method returns true if the worksheet has been centered horizontally for printing.

    my $is_centered_horizontally = $worksheet->is_centered_horizontally();

Returns 0 if the property isn't set.


=head2 is_centered_vertically()

The C<is_centered_vertically()> method returns true if the worksheet has been centered vertically for printing.

    my $is_centered_vertically = $worksheet->is_centered_vertically();

Returns 0 if the property isn't set.


=head2 is_print_gridlines()

The C<is_print_gridlines()> method returns true if the worksheet print "gridlines" option is turned on.

    my $is_print_gridlines = $worksheet->is_print_gridlines();

Returns 0 if the property isn't set.


=head2 is_print_row_col_headers()

The C<is_print_row_col_headers()> method returns true if the worksheet print "row and column headings" option is turned on.

    my $is_print_row_col_headers = $worksheet->is_print_row_col_headers();

Returns 0 if the property isn't set.


=head2 is_print_black_and_white()

The C<is_print_black_and_white()> method returns true if the worksheet print "black and white" option is turned on.

    my $is_print_black_and_white = $worksheet->is_print_black_and_white();

Returns 0 if the property isn't set.


=head2 is_print_draft()

The C<is_print_draft()> method returns true if the worksheet print "draft" option is turned on.

    my $is_print_draft = $worksheet->is_print_draft();

Returns 0 if the property isn't set.


=head2 is_print_comments()

The C<is_print_comments()> method returns true if the worksheet print "comments" option is turned on.

    my $is_print_comments = $worksheet->is_print_comments();

Returns 0 if the property isn't set.


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
