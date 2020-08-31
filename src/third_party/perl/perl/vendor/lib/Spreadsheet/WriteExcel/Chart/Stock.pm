package Spreadsheet::WriteExcel::Chart::Stock;

###############################################################################
#
# Stock - A writer class for Excel Stock charts.
#
# Used in conjunction with Spreadsheet::WriteExcel::Chart.
#
# See formatting note in Spreadsheet::WriteExcel::Chart.
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

require Exporter;

use strict;
use Spreadsheet::WriteExcel::Chart;


use vars qw($VERSION @ISA);
@ISA = qw(Spreadsheet::WriteExcel::Chart Exporter);

$VERSION = '2.40';

###############################################################################
#
# new()
#
#
sub new {

    my $class = shift;
    my $self  = Spreadsheet::WriteExcel::Chart->new( @_ );

    bless $self, $class;
    return $self;
}


###############################################################################
#
# _store_chart_type()
#
# Implementation of the abstract method from the specific chart class.
#
# Write the LINE chart BIFF record. A stock chart uses the same LINE record
# as a line chart but with additional DROPBAR and CHARTLINE records to define
# the stock style.
#
sub _store_chart_type {

    my $self = shift;

    my $record = 0x1018;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $grbit  = 0x0000;    # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $grbit;

    $self->_append( $header, $data );
}

###############################################################################
#
# _store_marker_dataformat_stream(). Overridden.
#
# This is an implementation of the parent abstract method to define
# properties of markers, linetypes, pie formats and other.
#
sub _store_marker_dataformat_stream {

    my $self = shift;

    $self->_store_dropbar();
    $self->_store_begin();
    $self->_store_lineformat( 0x00000000, 0x0000, 0xFFFF, 0x0001, 0x004F );
    $self->_store_areaformat( 0x00FFFFFF, 0x0000, 0x01, 0x01, 0x09, 0x08 );
    $self->_store_end();

    $self->_store_dropbar();
    $self->_store_begin();
    $self->_store_lineformat( 0x00000000, 0x0000, 0xFFFF, 0x0001, 0x004F );
    $self->_store_areaformat( 0x0000, 0x00FFFFFF, 0x01, 0x01, 0x08, 0x09 );
    $self->_store_end();

    $self->_store_chartline();
    $self->_store_lineformat( 0x00000000, 0x0000, 0xFFFF, 0x0000, 0x004F );


    $self->_store_dataformat( 0x0000, 0xFFFD, 0x0000 );
    $self->_store_begin();
    $self->_store_3dbarshape();
    $self->_store_lineformat( 0x00000000, 0x0005, 0xFFFF, 0x0000, 0x004F );
    $self->_store_areaformat( 0x00000000, 0x0000, 0x00, 0x01, 0x4D, 0x4D );
    $self->_store_pieformat();
    $self->_store_markerformat( 0x00, 0x00, 0x00, 0x00, 0x4D, 0x4D, 0x3C );
    $self->_store_end();

}


1;


__END__


=head1 NAME

Stock - A writer class for Excel Stock charts.

=head1 SYNOPSIS

To create a simple Excel file with a Stock chart using Spreadsheet::WriteExcel:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new( 'chart.xls' );
    my $worksheet = $workbook->add_worksheet();

    my $chart     = $workbook->add_chart( type => 'stock' );

    # Add a series for each Open-High-Low-Close.
    $chart->add_series( categories => '=Sheet1!$A$2:$A$6', values => '=Sheet1!$B$2:$B$6' );
    $chart->add_series( categories => '=Sheet1!$A$2:$A$6', values => '=Sheet1!$C$2:$C$6' );
    $chart->add_series( categories => '=Sheet1!$A$2:$A$6', values => '=Sheet1!$D$2:$D$6' );
    $chart->add_series( categories => '=Sheet1!$A$2:$A$6', values => '=Sheet1!$E$2:$E$6' );

    # Add the worksheet data the chart refers to.
    # ... See the full example below.

    __END__


=head1 DESCRIPTION

This module implements Stock charts for L<Spreadsheet::WriteExcel>. The chart object is created via the Workbook C<add_chart()> method:

    my $chart = $workbook->add_chart( type => 'stock' );

Once the object is created it can be configured via the following methods that are common to all chart classes:

    $chart->add_series();
    $chart->set_x_axis();
    $chart->set_y_axis();
    $chart->set_title();

These methods are explained in detail in L<Spreadsheet::WriteExcel::Chart>. Class specific methods or settings, if any, are explained below.

=head1 Stock Chart Methods

There aren't currently any stock chart specific methods. See the TODO section of L<Spreadsheet::WriteExcel::Chart>.

The default Stock chart is an Open-High-Low-Close chart. A series must be added for each of these data sources.

The default Stock chart is in black and white. User defined colours will be added at a later stage.

=head1 EXAMPLE

Here is a complete example that demonstrates most of the available features when creating a Stock chart.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook    = Spreadsheet::WriteExcel->new( 'chart_stock_ex.xls' );
    my $worksheet   = $workbook->add_worksheet();
    my $bold        = $workbook->add_format( bold => 1 );
    my $date_format = $workbook->add_format( num_format => 'dd/mm/yyyy' );

    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Date', 'Open', 'High', 'Low', 'Close' ];
    my @data = (
        [ '2009-08-23', 110.75, 113.48, 109.05, 109.40 ],
        [ '2009-08-24', 111.24, 111.60, 103.57, 104.87 ],
        [ '2009-08-25', 104.96, 108.00, 103.88, 106.00 ],
        [ '2009-08-26', 104.95, 107.95, 104.66, 107.91 ],
        [ '2009-08-27', 108.10, 108.62, 105.69, 106.15 ],
    );

    $worksheet->write( 'A1', $headings, $bold );

    my $row = 1;
    for my $data ( @data ) {
        $worksheet->write( $row, 0, $data->[0], $date_format );
        $worksheet->write( $row, 1, $data->[1] );
        $worksheet->write( $row, 2, $data->[2] );
        $worksheet->write( $row, 3, $data->[3] );
        $worksheet->write( $row, 4, $data->[4] );
        $row++;
    }

    # Create a new chart object. In this case an embedded chart.
    my $chart = $workbook->add_chart( type => 'stock', embedded => 1 );

    # Add a series for each of the Open-High-Low-Close columns.
    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$6',
        values     => '=Sheet1!$B$2:$B$6',
        name       => 'Open',
    );

    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$6',
        values     => '=Sheet1!$C$2:$C$6',
        name       => 'High',
    );

    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$6',
        values     => '=Sheet1!$D$2:$D$6',
        name       => 'Low',
    );

    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$6',
        values     => '=Sheet1!$E$2:$E$6',
        name       => 'Close',
    );

    # Add a chart title and some axis labels.
    $chart->set_title( name => 'Open-High-Low-Close', );
    $chart->set_x_axis( name => 'Date', );
    $chart->set_y_axis( name => 'Share price', );

    # Insert the chart into the worksheet (with an offset).
    $worksheet->insert_chart( 'F2', $chart, 25, 10 );

    __END__


=begin html

<p>This will produce a chart that looks like this:</p>

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stock1.jpg" width="527" height="320" alt="Chart example." /></center></p>

=end html


=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.

