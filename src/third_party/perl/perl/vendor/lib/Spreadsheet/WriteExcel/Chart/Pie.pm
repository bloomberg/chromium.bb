package Spreadsheet::WriteExcel::Chart::Pie;

###############################################################################
#
# Pie - A writer class for Excel Pie charts.
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

    $self->{_vary_data_color} = 1;

    bless $self, $class;
    return $self;
}


###############################################################################
#
# _store_chart_type()
#
# Implementation of the abstract method from the specific chart class.
#
# Write the Pie chart BIFF record.
#
sub _store_chart_type {

    my $self = shift;

    my $record = 0x1019;    # Record identifier.
    my $length = 0x0006;    # Number of bytes to follow.
    my $angle  = 0x0000;    # Angle.
    my $donut  = 0x0000;    # Donut hole size.
    my $grbit  = 0x0002;    # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $angle;
    $data .= pack 'v', $donut;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axisparent_stream(). Overridden.
#
# Write the AXISPARENT chart substream.
#
# A Pie chart has no X or Y axis so we override this method to remove them.
#
sub _store_axisparent_stream {

    my $self = shift;

    $self->_store_axisparent( @{ $self->{_config}->{_axisparent} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_axisparent_pos} } );

    $self->_store_chartformat_stream();
    $self->_store_end();
}


1;


__END__


=head1 NAME

Pie - A writer class for Excel Pie charts.

=head1 SYNOPSIS

To create a simple Excel file with a Pie chart using Spreadsheet::WriteExcel:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new( 'chart.xls' );
    my $worksheet = $workbook->add_worksheet();

    my $chart     = $workbook->add_chart( type => 'pie' );

    # Configure the chart.
    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
    );

    # Add the worksheet data the chart refers to.
    my $data = [
        [ 'Category', 2, 3, 4, 5, 6, 7 ],
        [ 'Value',    1, 4, 5, 2, 1, 5 ],
    ];

    $worksheet->write( 'A1', $data );

    __END__

=head1 DESCRIPTION

This module implements Pie charts for L<Spreadsheet::WriteExcel>. The chart object is created via the Workbook C<add_chart()> method:

    my $chart = $workbook->add_chart( type => 'pie' );

Once the object is created it can be configured via the following methods that are common to all chart classes:

    $chart->add_series();
    $chart->set_title();

These methods are explained in detail in L<Spreadsheet::WriteExcel::Chart>. Class specific methods or settings, if any, are explained below.

=head1 Pie Chart Methods

There aren't currently any pie chart specific methods. See the TODO section of L<Spreadsheet::WriteExcel::Chart>.

A Pie chart doesn't have an X or Y axis so the following common chart methods are ignored.

    $chart->set_x_axis();
    $chart->set_y_axis();

=head1 EXAMPLE

Here is a complete example that demonstrates most of the available features when creating a chart.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_pie.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );

    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values' ];
    my $data = [
        [ 'Apple', 'Cherry', 'Pecan' ],
        [ 60,       30,       10     ],
    ];

    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );

    # Create a new chart object. In this case an embedded chart.
    my $chart = $workbook->add_chart( type => 'pie', embedded => 1 );

    # Configure the series.
    $chart->add_series(
        name       => 'Pie sales data',
        categories => '=Sheet1!$A$2:$A$4',
        values     => '=Sheet1!$B$2:$B$4',
    );

    # Add a title.
    $chart->set_title( name => 'Popular Pie Types' );


    # Insert the chart into the worksheet (with an offset).
    $worksheet->insert_chart( 'C2', $chart, 25, 10 );

    __END__


=begin html

<p>This will produce a chart that looks like this:</p>

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/pie1.jpg" width="527" height="320" alt="Chart example." /></center></p>

=end html


=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.

