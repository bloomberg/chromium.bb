package Spreadsheet::WriteExcel::Examples;

###############################################################################
#
# Examples - Spreadsheet::WriteExcel examples.
#
# A documentation only module showing the examples that are
# included in the Spreadsheet::WriteExcel distribution. This
# file was generated automatically via the gen_examples_pod.pl
# program that is also included in the examples directory.
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

use strict;
use vars qw($VERSION);
$VERSION = '2.40';

1;

__END__

=pod

=head1 NAME

Examples - Spreadsheet::WriteExcel example programs.

=head1 DESCRIPTION

This is a documentation only module showing the examples that are
included in the L<Spreadsheet::WriteExcel> distribution.

This file was auto-generated via the gen_examples_pod.pl
program that is also included in the examples directory.

=head1 Example programs

The following is a list of the 85 example programs that are included in the Spreadsheet::WriteExcel distribution.

=over

=item * L<Example: a_simple.pl> A get started example with some basic features.

=item * L<Example: demo.pl> A demo of some of the available features.

=item * L<Example: regions.pl> A simple example of multiple worksheets.

=item * L<Example: stats.pl> Basic formulas and functions.

=item * L<Example: formats.pl> All the available formatting on several worksheets.

=item * L<Example: bug_report.pl> A template for submitting bug reports.

=item * L<Example: autofilter.pl> Examples of worksheet autofilters.

=item * L<Example: autofit.pl> Simulate Excel's autofit for column widths.

=item * L<Example: bigfile.pl> Write past the 7MB limit with OLE::Storage_Lite.

=item * L<Example: cgi.pl> A simple CGI program.

=item * L<Example: chart_area.pl> A demo of area style charts.

=item * L<Example: chart_bar.pl> A demo of bar (vertical histogram) style charts.

=item * L<Example: chart_column.pl> A demo of column (histogram) style charts.

=item * L<Example: chart_line.pl> A demo of line style charts.

=item * L<Example: chart_pie.pl> A demo of pie style charts.

=item * L<Example: chart_scatter.pl> A demo of scatter style charts.

=item * L<Example: chart_stock.pl> A demo of stock style charts.

=item * L<Example: chess.pl> An example of reusing formatting via properties.

=item * L<Example: colors.pl> A demo of the colour palette and named colours.

=item * L<Example: comments1.pl> Add comments to worksheet cells.

=item * L<Example: comments2.pl> Add comments with advanced options.

=item * L<Example: copyformat.pl> Example of copying a cell format.

=item * L<Example: data_validate.pl> An example of data validation and dropdown lists.

=item * L<Example: date_time.pl> Write dates and times with write_date_time().

=item * L<Example: defined_name.pl> Example of how to create defined names.

=item * L<Example: diag_border.pl> A simple example of diagonal cell borders.

=item * L<Example: easter_egg.pl> Expose the Excel97 flight simulator.

=item * L<Example: filehandle.pl> Examples of working with filehandles.

=item * L<Example: formula_result.pl> Formulas with user specified results.

=item * L<Example: headers.pl> Examples of worksheet headers and footers.

=item * L<Example: hide_sheet.pl> Simple example of hiding a worksheet.

=item * L<Example: hyperlink1.pl> Shows how to create web hyperlinks.

=item * L<Example: hyperlink2.pl> Examples of internal and external hyperlinks.

=item * L<Example: images.pl> Adding images to worksheets.

=item * L<Example: indent.pl> An example of cell indentation.

=item * L<Example: merge1.pl> A simple example of cell merging.

=item * L<Example: merge2.pl> A simple example of cell merging with formatting.

=item * L<Example: merge3.pl> Add hyperlinks to merged cells.

=item * L<Example: merge4.pl> An advanced example of merging with formatting.

=item * L<Example: merge5.pl> An advanced example of merging with formatting.

=item * L<Example: merge6.pl> An example of merging with Unicode strings.

=item * L<Example: mod_perl1.pl> A simple mod_perl 1 program.

=item * L<Example: mod_perl2.pl> A simple mod_perl 2 program.

=item * L<Example: outline.pl> An example of outlines and grouping.

=item * L<Example: outline_collapsed.pl> An example of collapsed outlines.

=item * L<Example: panes.pl> An examples of how to create panes.

=item * L<Example: properties.pl> Add document properties to a workbook.

=item * L<Example: protection.pl> Example of cell locking and formula hiding.

=item * L<Example: repeat.pl> Example of writing repeated formulas.

=item * L<Example: right_to_left.pl> Change default sheet direction to right to left.

=item * L<Example: row_wrap.pl> How to wrap data from one worksheet onto another.

=item * L<Example: sales.pl> An example of a simple sales spreadsheet.

=item * L<Example: sendmail.pl> Send an Excel email attachment using Mail::Sender.

=item * L<Example: stats_ext.pl> Same as stats.pl with external references.

=item * L<Example: stocks.pl> Demonstrates conditional formatting.

=item * L<Example: tab_colors.pl> Example of how to set worksheet tab colours.

=item * L<Example: textwrap.pl> Demonstrates text wrapping options.

=item * L<Example: win32ole.pl> A sample Win32::OLE example for comparison.

=item * L<Example: write_arrays.pl> Example of writing 1D or 2D arrays of data.

=item * L<Example: write_handler1.pl> Example of extending the write() method. Step 1.

=item * L<Example: write_handler2.pl> Example of extending the write() method. Step 2.

=item * L<Example: write_handler3.pl> Example of extending the write() method. Step 3.

=item * L<Example: write_handler4.pl> Example of extending the write() method. Step 4.

=item * L<Example: write_to_scalar.pl> Example of writing an Excel file to a Perl scalar.

=item * L<Example: unicode_utf16.pl> Simple example of using Unicode UTF16 strings.

=item * L<Example: unicode_utf16_japan.pl> Write Japanese Unicode strings using UTF-16.

=item * L<Example: unicode_cyrillic.pl> Write Russian Cyrillic strings using UTF-8.

=item * L<Example: unicode_list.pl> List the chars in a Unicode font.

=item * L<Example: unicode_2022_jp.pl> Japanese: ISO-2022-JP to utf8 in perl 5.8.

=item * L<Example: unicode_8859_11.pl> Thai:     ISO-8859_11 to utf8 in perl 5.8.

=item * L<Example: unicode_8859_7.pl> Greek:    ISO-8859_7  to utf8 in perl 5.8.

=item * L<Example: unicode_big5.pl> Chinese:  BIG5        to utf8 in perl 5.8.

=item * L<Example: unicode_cp1251.pl> Russian:  CP1251      to utf8 in perl 5.8.

=item * L<Example: unicode_cp1256.pl> Arabic:   CP1256      to utf8 in perl 5.8.

=item * L<Example: unicode_koi8r.pl> Russian:  KOI8-R      to utf8 in perl 5.8.

=item * L<Example: unicode_polish_utf8.pl> Polish :  UTF8        to utf8 in perl 5.8.

=item * L<Example: unicode_shift_jis.pl> Japanese: Shift JIS   to utf8 in perl 5.8.

=item * L<Example: csv2xls.pl> Program to convert a CSV file to an Excel file.

=item * L<Example: tab2xls.pl> Program to convert a tab separated file to xls.

=item * L<Example: datecalc1.pl> Convert Unix/Perl time to Excel time.

=item * L<Example: datecalc2.pl> Calculate an Excel date using Date::Calc.

=item * L<Example: lecxe.pl> Convert Excel to WriteExcel using Win32::OLE.

=item * L<Example: convertA1.pl> Helper functions for dealing with A1 notation.

=item * L<Example: function_locale.pl> Add non-English function names to Formula.pm.

=item * L<Example: writeA1.pl> Example of how to extend the module.

=back

=head2 Example: a_simple.pl



A simple example of how to use the Spreadsheet::WriteExcel module to write
some  text and numbers to an Excel binary file.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/a_simple.jpg" width="640" height="420" alt="Output from a_simple.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple example of how to use the Spreadsheet::WriteExcel module to write
    # some  text and numbers to an Excel binary file.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook called simple.xls and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('a_simple.xls');
    my $worksheet = $workbook->add_worksheet();
    
    # The general syntax is write($row, $column, $token). Note that row and
    # column are zero indexed
    #
    
    # Write some text
    $worksheet->write(0, 0,  "Hi Excel!");
    
    
    # Write some numbers
    $worksheet->write(2, 0,  3);          # Writes 3
    $worksheet->write(3, 0,  3.00000);    # Writes 3
    $worksheet->write(4, 0,  3.00001);    # Writes 3.00001
    $worksheet->write(5, 0,  3.14159);    # TeX revision no.?
    
    
    # Write some formulas
    $worksheet->write(7, 0,  '=A3 + A6');
    $worksheet->write(8, 0,  '=IF(A5>3,"Yes", "No")');
    
    
    # Write a hyperlink
    $worksheet->write(10, 0, 'http://www.perl.com/');
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/a_simple.pl>

=head2 Example: demo.pl



A simple demo of some of the features of Spreadsheet::WriteExcel.

This program is used to create the project screenshot for Freshmeat:
L<http://freshmeat.net/projects/writeexcel/>



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/demo.jpg" width="640" height="420" alt="Output from demo.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # A simple demo of some of the features of Spreadsheet::WriteExcel.
    #
    # This program is used to create the project screenshot for Freshmeat:
    # L<http://freshmeat.net/projects/writeexcel/>
    #
    # reverse('(c)'), October 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new("demo.xls");
    my $worksheet  = $workbook->add_worksheet('Demo');
    my $worksheet2 = $workbook->add_worksheet('Another sheet');
    my $worksheet3 = $workbook->add_worksheet('And another');
    
    my $bold       = $workbook->add_format(bold => 1);
    
    
    #######################################################################
    #
    # Write a general heading
    #
    $worksheet->set_column('A:A', 36, $bold);
    $worksheet->set_column('B:B', 20       );
    $worksheet->set_row   (0,     40       );
    
    my $heading  = $workbook->add_format(
                                            bold    => 1,
                                            color   => 'blue',
                                            size    => 16,
                                            merge   => 1,
                                            align  => 'vcenter',
                                            );
    
    my @headings = ('Features of Spreadsheet::WriteExcel', '');
    $worksheet->write_row('A1', \@headings, $heading);
    
    
    #######################################################################
    #
    # Some text examples
    #
    my $text_format  = $workbook->add_format(
                                                bold    => 1,
                                                italic  => 1,
                                                color   => 'red',
                                                size    => 18,
                                                font    =>'Lucida Calligraphy'
                                            );
    
    # A phrase in Cyrillic
    my $unicode = pack "H*", "042d0442043e002004440440043004370430002004".
                             "3d043000200440044304410441043a043e043c0021";
    
    
    $worksheet->write('A2', "Text");
    $worksheet->write('B2', "Hello Excel");
    $worksheet->write('A3', "Formatted text");
    $worksheet->write('B3', "Hello Excel", $text_format);
    $worksheet->write('A4', "Unicode text");
    $worksheet->write_utf16be_string('B4', $unicode);
    
    #######################################################################
    #
    # Some numeric examples
    #
    my $num1_format  = $workbook->add_format(num_format => '$#,##0.00');
    my $num2_format  = $workbook->add_format(num_format => ' d mmmm yyy');
    
    
    $worksheet->write('A5', "Numbers");
    $worksheet->write('B5', 1234.56);
    $worksheet->write('A6', "Formatted numbers");
    $worksheet->write('B6', 1234.56, $num1_format);
    $worksheet->write('A7', "Formatted numbers");
    $worksheet->write('B7', 37257, $num2_format);
    
    
    #######################################################################
    #
    # Formulae
    #
    $worksheet->set_selection('B8');
    $worksheet->write('A8', 'Formulas and functions, "=SIN(PI()/4)"');
    $worksheet->write('B8', '=SIN(PI()/4)');
    
    
    #######################################################################
    #
    # Hyperlinks
    #
    $worksheet->write('A9', "Hyperlinks");
    $worksheet->write('B9',  'http://www.perl.com/' );
    
    
    #######################################################################
    #
    # Images
    #
    $worksheet->write('A10', "Images");
    $worksheet->insert_image('B10', 'republic.png', 16, 8);
    
    
    #######################################################################
    #
    # Misc
    #
    $worksheet->write('A18', "Page/printer setup");
    $worksheet->write('A19', "Multiple worksheets");
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/demo.pl>

=head2 Example: regions.pl



An example of how to use the Spreadsheet:WriteExcel module to write a basic
Excel workbook with multiple worksheets.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/regions.jpg" width="640" height="420" alt="Output from regions.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # An example of how to use the Spreadsheet:WriteExcel module to write a basic
    # Excel workbook with multiple worksheets.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new Excel workbook
    my $workbook = Spreadsheet::WriteExcel->new("regions.xls");
    
    # Add some worksheets
    my $north = $workbook->add_worksheet("North");
    my $south = $workbook->add_worksheet("South");
    my $east  = $workbook->add_worksheet("East");
    my $west  = $workbook->add_worksheet("West");
    
    # Add a Format
    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_color('blue');
    
    # Add a caption to each worksheet
    foreach my $worksheet ($workbook->sheets()) {
        $worksheet->write(0, 0, "Sales", $format);
    }
    
    # Write some data
    $north->write(0, 1, 200000);
    $south->write(0, 1, 100000);
    $east->write (0, 1, 150000);
    $west->write (0, 1, 100000);
    
    # Set the active worksheet
    $south->activate();
    
    # Set the width of the first column
    $south->set_column(0, 0, 20);
    
    # Set the active cell
    $south->set_selection(0, 1);


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/regions.pl>

=head2 Example: stats.pl



A simple example of how to use functions with the Spreadsheet::WriteExcel
module.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stats.jpg" width="640" height="420" alt="Output from stats.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple example of how to use functions with the Spreadsheet::WriteExcel
    # module.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("stats.xls");
    my $worksheet = $workbook->add_worksheet('Test data');
    
    # Set the column width for columns 1
    $worksheet->set_column(0, 0, 20);
    
    
    # Create a format for the headings
    my $format = $workbook->add_format();
    $format->set_bold();
    
    
    # Write the sample data
    $worksheet->write(0, 0, 'Sample', $format);
    $worksheet->write(0, 1, 1);
    $worksheet->write(0, 2, 2);
    $worksheet->write(0, 3, 3);
    $worksheet->write(0, 4, 4);
    $worksheet->write(0, 5, 5);
    $worksheet->write(0, 6, 6);
    $worksheet->write(0, 7, 7);
    $worksheet->write(0, 8, 8);
    
    $worksheet->write(1, 0, 'Length', $format);
    $worksheet->write(1, 1, 25.4);
    $worksheet->write(1, 2, 25.4);
    $worksheet->write(1, 3, 24.8);
    $worksheet->write(1, 4, 25.0);
    $worksheet->write(1, 5, 25.3);
    $worksheet->write(1, 6, 24.9);
    $worksheet->write(1, 7, 25.2);
    $worksheet->write(1, 8, 24.8);
    
    # Write some statistical functions
    $worksheet->write(4,  0, 'Count', $format);
    $worksheet->write(4,  1, '=COUNT(B1:I1)');
    
    $worksheet->write(5,  0, 'Sum', $format);
    $worksheet->write(5,  1, '=SUM(B2:I2)');
    
    $worksheet->write(6,  0, 'Average', $format);
    $worksheet->write(6,  1, '=AVERAGE(B2:I2)');
    
    $worksheet->write(7,  0, 'Min', $format);
    $worksheet->write(7,  1, '=MIN(B2:I2)');
    
    $worksheet->write(8,  0, 'Max', $format);
    $worksheet->write(8,  1, '=MAX(B2:I2)');
    
    $worksheet->write(9,  0, 'Standard Deviation', $format);
    $worksheet->write(9,  1, '=STDEV(B2:I2)');
    
    $worksheet->write(10, 0, 'Kurtosis', $format);
    $worksheet->write(10, 1, '=KURT(B2:I2)');
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/stats.pl>

=head2 Example: formats.pl



Examples of formatting using the Spreadsheet::WriteExcel module.

This program demonstrates almost all possible formatting options. It is worth
running this program and viewing the output Excel file if you are interested
in the various formatting possibilities.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/formats.jpg" width="640" height="420" alt="Output from formats.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Examples of formatting using the Spreadsheet::WriteExcel module.
    #
    # This program demonstrates almost all possible formatting options. It is worth
    # running this program and viewing the output Excel file if you are interested
    # in the various formatting possibilities.
    #
    # reverse('(c)'), September 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook = Spreadsheet::WriteExcel->new('formats.xls');
    
    # Some common formats
    my $center  = $workbook->add_format(align => 'center');
    my $heading = $workbook->add_format(align => 'center', bold => 1);
    
    # The named colors
    my %colors = (
                    0x08, 'black',
                    0x0C, 'blue',
                    0x10, 'brown',
                    0x0F, 'cyan',
                    0x17, 'gray',
                    0x11, 'green',
                    0x0B, 'lime',
                    0x0E, 'magenta',
                    0x12, 'navy',
                    0x35, 'orange',
                    0x21, 'pink',
                    0x14, 'purple',
                    0x0A, 'red',
                    0x16, 'silver',
                    0x09, 'white',
                    0x0D, 'yellow',
                 );
    
    # Call these subroutines to demonstrate different formatting options
    intro();
    fonts();
    named_colors();
    standard_colors();
    numeric_formats();
    borders();
    patterns();
    alignment();
    misc();
    
    # Note: this is required
    $workbook->close();
    
    
    ######################################################################
    #
    # Intro.
    #
    sub intro {
    
        my $worksheet = $workbook->add_worksheet('Introduction');
    
        $worksheet->set_column(0, 0, 60);
    
        my $format = $workbook->add_format();
        $format->set_bold();
        $format->set_size(14);
        $format->set_color('blue');
        $format->set_align('center');
    
        my $format2 = $workbook->add_format();
        $format2->set_bold();
        $format2->set_color('blue');
    
        $worksheet->write(2, 0, 'This workbook demonstrates some of',  $format);
        $worksheet->write(3, 0, 'the formatting options provided by',  $format);
        $worksheet->write(4, 0, 'the Spreadsheet::WriteExcel module.', $format);
    
        $worksheet->write('A7',  'Sections:', $format2);
        $worksheet->write('A8',  "internal:Fonts!A1",             'Fonts'          );
        $worksheet->write('A9',  "internal:'Named colors'!A1",    'Named colors'   );
        $worksheet->write('A10', "internal:'Standard colors'!A1", 'Standard colors');
        $worksheet->write('A11', "internal:'Numeric formats'!A1", 'Numeric formats');
        $worksheet->write('A12', "internal:Borders!A1",           'Borders'        );
        $worksheet->write('A13', "internal:Patterns!A1",          'Patterns'       );
        $worksheet->write('A14', "internal:Alignment!A1",         'Alignment'      );
        $worksheet->write('A15', "internal:Miscellaneous!A1",     'Miscellaneous'  );
    
    }
    
    
    ######################################################################
    #
    # Demonstrate the named colors.
    #
    sub named_colors {
    
        my $worksheet = $workbook->add_worksheet('Named colors');
    
        $worksheet->set_column(0, 3, 15);
    
        $worksheet->write(0, 0, "Index", $heading);
        $worksheet->write(0, 1, "Index", $heading);
        $worksheet->write(0, 2, "Name",  $heading);
        $worksheet->write(0, 3, "Color", $heading);
    
        my $i = 1;
    
        while (my($index, $color) = each %colors) {
            my $format = $workbook->add_format(
                                                bg_color => $color,
                                                pattern  => 1,
                                                border   => 1
                                             );
    
            $worksheet->write($i+1, 0, $index,                    $center);
            $worksheet->write($i+1, 1, sprintf("0x%02X", $index), $center);
            $worksheet->write($i+1, 2, $color,                    $center);
            $worksheet->write($i+1, 3, '',                        $format);
            $i++;
        }
    }
    
    
    ######################################################################
    #
    # Demonstrate the standard Excel colors in the range 8..63.
    #
    sub standard_colors {
    
        my $worksheet = $workbook->add_worksheet('Standard colors');
    
        $worksheet->set_column(0, 3, 15);
    
        $worksheet->write(0, 0, "Index", $heading);
        $worksheet->write(0, 1, "Index", $heading);
        $worksheet->write(0, 2, "Color", $heading);
        $worksheet->write(0, 3, "Name",  $heading);
    
        for my $i (8..63) {
            my $format = $workbook->add_format(
                                                bg_color => $i,
                                                pattern  => 1,
                                                border   => 1
                                             );
    
            $worksheet->write(($i -7), 0, $i,                    $center);
            $worksheet->write(($i -7), 1, sprintf("0x%02X", $i), $center);
            $worksheet->write(($i -7), 2, '',                    $format);
    
            # Add the  color names
            if (exists $colors{$i}) {
                $worksheet->write(($i -7), 3, $colors{$i}, $center);
    
            }
        }
    }
    
    
    ######################################################################
    #
    # Demonstrate the standard numeric formats.
    #
    sub numeric_formats {
    
        my $worksheet = $workbook->add_worksheet('Numeric formats');
    
        $worksheet->set_column(0, 4, 15);
        $worksheet->set_column(5, 5, 45);
    
        $worksheet->write(0, 0, "Index",       $heading);
        $worksheet->write(0, 1, "Index",       $heading);
        $worksheet->write(0, 2, "Unformatted", $heading);
        $worksheet->write(0, 3, "Formatted",   $heading);
        $worksheet->write(0, 4, "Negative",    $heading);
        $worksheet->write(0, 5, "Format",      $heading);
    
        my @formats;
        push @formats, [ 0x00, 1234.567,   0,         'General' ];
        push @formats, [ 0x01, 1234.567,   0,         '0' ];
        push @formats, [ 0x02, 1234.567,   0,         '0.00' ];
        push @formats, [ 0x03, 1234.567,   0,         '#,##0' ];
        push @formats, [ 0x04, 1234.567,   0,         '#,##0.00' ];
        push @formats, [ 0x05, 1234.567,   -1234.567, '($#,##0_);($#,##0)' ];
        push @formats, [ 0x06, 1234.567,   -1234.567, '($#,##0_);[Red]($#,##0)' ];
        push @formats, [ 0x07, 1234.567,   -1234.567, '($#,##0.00_);($#,##0.00)' ];
        push @formats, [ 0x08, 1234.567,   -1234.567, '($#,##0.00_);[Red]($#,##0.00)' ];
        push @formats, [ 0x09, 0.567,      0,         '0%' ];
        push @formats, [ 0x0a, 0.567,      0,         '0.00%' ];
        push @formats, [ 0x0b, 1234.567,   0,         '0.00E+00' ];
        push @formats, [ 0x0c, 0.75,       0,         '# ?/?' ];
        push @formats, [ 0x0d, 0.3125,     0,         '# ??/??' ];
        push @formats, [ 0x0e, 36892.521,  0,         'm/d/yy' ];
        push @formats, [ 0x0f, 36892.521,  0,         'd-mmm-yy' ];
        push @formats, [ 0x10, 36892.521,  0,         'd-mmm' ];
        push @formats, [ 0x11, 36892.521,  0,         'mmm-yy' ];
        push @formats, [ 0x12, 36892.521,  0,         'h:mm AM/PM' ];
        push @formats, [ 0x13, 36892.521,  0,         'h:mm:ss AM/PM' ];
        push @formats, [ 0x14, 36892.521,  0,         'h:mm' ];
        push @formats, [ 0x15, 36892.521,  0,         'h:mm:ss' ];
        push @formats, [ 0x16, 36892.521,  0,         'm/d/yy h:mm' ];
        push @formats, [ 0x25, 1234.567,   -1234.567, '(#,##0_);(#,##0)' ];
        push @formats, [ 0x26, 1234.567,   -1234.567, '(#,##0_);[Red](#,##0)' ];
        push @formats, [ 0x27, 1234.567,   -1234.567, '(#,##0.00_);(#,##0.00)' ];
        push @formats, [ 0x28, 1234.567,   -1234.567, '(#,##0.00_);[Red](#,##0.00)' ];
        push @formats, [ 0x29, 1234.567,   -1234.567, '_(* #,##0_);_(* (#,##0);_(* "-"_);_(@_)' ];
        push @formats, [ 0x2a, 1234.567,   -1234.567, '_($* #,##0_);_($* (#,##0);_($* "-"_);_(@_)' ];
        push @formats, [ 0x2b, 1234.567,   -1234.567, '_(* #,##0.00_);_(* (#,##0.00);_(* "-"??_);_(@_)' ];
        push @formats, [ 0x2c, 1234.567,   -1234.567, '_($* #,##0.00_);_($* (#,##0.00);_($* "-"??_);_(@_)' ];
        push @formats, [ 0x2d, 36892.521,  0,         'mm:ss' ];
        push @formats, [ 0x2e, 3.0153,     0,         '[h]:mm:ss' ];
        push @formats, [ 0x2f, 36892.521,  0,         'mm:ss.0' ];
        push @formats, [ 0x30, 1234.567,   0,         '##0.0E+0' ];
        push @formats, [ 0x31, 1234.567,   0,         '@' ];
    
        my $i;
        foreach my $format (@formats){
            my $style = $workbook->add_format();
            $style->set_num_format($format->[0]);
    
            $i++;
            $worksheet->write($i, 0, $format->[0],                    $center);
            $worksheet->write($i, 1, sprintf("0x%02X", $format->[0]), $center);
            $worksheet->write($i, 2, $format->[1],                    $center);
            $worksheet->write($i, 3, $format->[1],                    $style);
    
            if ($format->[2]) {
                $worksheet->write($i, 4, $format->[2], $style);
            }
    
            $worksheet->write_string($i, 5, $format->[3]);
        }
    }
    
    
    ######################################################################
    #
    # Demonstrate the font options.
    #
    sub fonts {
    
        my $worksheet = $workbook->add_worksheet('Fonts');
    
        $worksheet->set_column(0, 0, 30);
        $worksheet->set_column(1, 1, 10);
    
        $worksheet->write(0, 0, "Font name",   $heading);
        $worksheet->write(0, 1, "Font size",   $heading);
    
        my @fonts;
        push @fonts, [ 10, 'Arial' ];
        push @fonts, [ 12, 'Arial' ];
        push @fonts, [ 14, 'Arial' ];
        push @fonts, [ 12, 'Arial Black' ];
        push @fonts, [ 12, 'Arial Narrow' ];
        push @fonts, [ 12, 'Century Schoolbook' ];
        push @fonts, [ 12, 'Courier' ];
        push @fonts, [ 12, 'Courier New' ];
        push @fonts, [ 12, 'Garamond' ];
        push @fonts, [ 12, 'Impact' ];
        push @fonts, [ 12, 'Lucida Handwriting'] ;
        push @fonts, [ 12, 'Times New Roman' ];
        push @fonts, [ 12, 'Symbol' ];
        push @fonts, [ 12, 'Wingdings' ];
        push @fonts, [ 12, 'A font that doesn\'t exist' ];
    
        my $i;
        foreach my $font (@fonts){
            my $format = $workbook->add_format();
    
            $format->set_size($font->[0]);
            $format->set_font($font->[1]);
    
            $i++;
            $worksheet->write($i, 0, $font->[1], $format);
            $worksheet->write($i, 1, $font->[0], $format);
        }
    
    }
    
    
    ######################################################################
    #
    # Demonstrate the standard Excel border styles.
    #
    sub borders {
    
        my $worksheet = $workbook->add_worksheet('Borders');
    
        $worksheet->set_column(0, 4, 10);
        $worksheet->set_column(5, 5, 40);
    
        $worksheet->write(0, 0, "Index", $heading);
        $worksheet->write(0, 1, "Index", $heading);
        $worksheet->write(0, 3, "Style", $heading);
        $worksheet->write(0, 5, "The style is highlighted in red for ", $heading);
        $worksheet->write(1, 5, "emphasis, the default color is black.", $heading);
    
        for my $i (0..13){
            my $format = $workbook->add_format();
            $format->set_border($i);
            $format->set_border_color('red');
            $format->set_align('center');
    
            $worksheet->write((2*($i+1)), 0, $i,                    $center);
            $worksheet->write((2*($i+1)), 1, sprintf("0x%02X", $i), $center);
    
            $worksheet->write((2*($i+1)), 3, "Border", $format);
        }
    
        $worksheet->write(30, 0, "Diag type", $heading);
        $worksheet->write(30, 1, "Index", $heading);
        $worksheet->write(30, 3, "Style", $heading);
        $worksheet->write(30, 5, "Diagonal Boder styles", $heading);
    
        for my $i (1..3){
            my $format = $workbook->add_format();
            $format->set_diag_type($i);
            $format->set_diag_border(1);
            $format->set_diag_color('red');
            $format->set_align('center');
    
            $worksheet->write((2*($i+15)), 0, $i,                     $center);
            $worksheet->write((2*($i+15)), 1, sprintf("0x%02X", $i),  $center);
    
            $worksheet->write((2*($i+15)), 3, "Border", $format);
        }
    }
    
    
    
    ######################################################################
    #
    # Demonstrate the standard Excel cell patterns.
    #
    sub patterns {
    
        my $worksheet = $workbook->add_worksheet('Patterns');
    
        $worksheet->set_column(0, 4, 10);
        $worksheet->set_column(5, 5, 50);
    
        $worksheet->write(0, 0, "Index", $heading);
        $worksheet->write(0, 1, "Index", $heading);
        $worksheet->write(0, 3, "Pattern", $heading);
    
        $worksheet->write(0, 5, "The background colour has been set to silver.", $heading);
        $worksheet->write(1, 5, "The foreground colour has been set to green.",  $heading);
    
        for my $i (0..18){
            my $format = $workbook->add_format();
    
            $format->set_pattern($i);
            $format->set_bg_color('silver');
            $format->set_fg_color('green');
            $format->set_align('center');
    
            $worksheet->write((2*($i+1)), 0, $i,                    $center);
            $worksheet->write((2*($i+1)), 1, sprintf("0x%02X", $i), $center);
    
            $worksheet->write((2*($i+1)), 3, "Pattern", $format);
    
            if ($i == 1) {
                $worksheet->write((2*($i+1)), 5, "This is solid colour, the most useful pattern.", $heading);
            }
        }
    }
    
    
    ######################################################################
    #
    # Demonstrate the standard Excel cell alignments.
    #
    sub alignment {
    
        my $worksheet = $workbook->add_worksheet('Alignment');
    
        $worksheet->set_column(0, 7, 12);
        $worksheet->set_row(0, 40);
        $worksheet->set_selection(7, 0);
    
        my $format01 = $workbook->add_format();
        my $format02 = $workbook->add_format();
        my $format03 = $workbook->add_format();
        my $format04 = $workbook->add_format();
        my $format05 = $workbook->add_format();
        my $format06 = $workbook->add_format();
        my $format07 = $workbook->add_format();
        my $format08 = $workbook->add_format();
        my $format09 = $workbook->add_format();
        my $format10 = $workbook->add_format();
        my $format11 = $workbook->add_format();
        my $format12 = $workbook->add_format();
        my $format13 = $workbook->add_format();
        my $format14 = $workbook->add_format();
        my $format15 = $workbook->add_format();
        my $format16 = $workbook->add_format();
        my $format17 = $workbook->add_format();
    
        $format02->set_align('top');
        $format03->set_align('bottom');
        $format04->set_align('vcenter');
        $format05->set_align('vjustify');
        $format06->set_text_wrap();
    
        $format07->set_align('left');
        $format08->set_align('right');
        $format09->set_align('center');
        $format10->set_align('fill');
        $format11->set_align('justify');
        $format12->set_merge();
    
        $format13->set_rotation(45);
        $format14->set_rotation(-45);
        $format15->set_rotation(270);
    
        $format16->set_shrink();
        $format17->set_indent(1);
    
        $worksheet->write(0, 0, 'Vertical',     $heading);
        $worksheet->write(0, 1, 'top',          $format02);
        $worksheet->write(0, 2, 'bottom',       $format03);
        $worksheet->write(0, 3, 'vcenter',      $format04);
        $worksheet->write(0, 4, 'vjustify',     $format05);
        $worksheet->write(0, 5, "text\nwrap",   $format06);
    
        $worksheet->write(2, 0, 'Horizontal',   $heading);
        $worksheet->write(2, 1, 'left',         $format07);
        $worksheet->write(2, 2, 'right',        $format08);
        $worksheet->write(2, 3, 'center',       $format09);
        $worksheet->write(2, 4, 'fill',         $format10);
        $worksheet->write(2, 5, 'justify',      $format11);
    
        $worksheet->write(3, 1, 'merge',        $format12);
        $worksheet->write(3, 2, '',             $format12);
    
        $worksheet->write(3, 3, 'Shrink ' x 3,  $format16);
        $worksheet->write(3, 4, 'Indent',       $format17);
    
    
        $worksheet->write(5, 0, 'Rotation',     $heading);
        $worksheet->write(5, 1, 'Rotate 45',    $format13);
        $worksheet->write(6, 1, 'Rotate -45',   $format14);
        $worksheet->write(7, 1, 'Rotate 270',   $format15);
    }
    
    
    ######################################################################
    #
    # Demonstrate other miscellaneous features.
    #
    sub misc {
    
        my $worksheet = $workbook->add_worksheet('Miscellaneous');
    
        $worksheet->set_column(2, 2, 25);
    
        my $format01 = $workbook->add_format();
        my $format02 = $workbook->add_format();
        my $format03 = $workbook->add_format();
        my $format04 = $workbook->add_format();
        my $format05 = $workbook->add_format();
        my $format06 = $workbook->add_format();
        my $format07 = $workbook->add_format();
    
        $format01->set_underline(0x01);
        $format02->set_underline(0x02);
        $format03->set_underline(0x21);
        $format04->set_underline(0x22);
        $format05->set_font_strikeout();
        $format06->set_font_outline();
        $format07->set_font_shadow();
    
        $worksheet->write(1,  2, 'Underline  0x01',          $format01);
        $worksheet->write(3,  2, 'Underline  0x02',          $format02);
        $worksheet->write(5,  2, 'Underline  0x21',          $format03);
        $worksheet->write(7,  2, 'Underline  0x22',          $format04);
        $worksheet->write(9,  2, 'Strikeout',                $format05);
        $worksheet->write(11, 2, 'Outline (Macintosh only)', $format06);
        $worksheet->write(13, 2, 'Shadow (Macintosh only)',  $format07);
    }
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/formats.pl>

=head2 Example: bug_report.pl



A template for submitting a bug report.

Run this program and read the output from the command line.



    #!/usr/bin/perl -w
    
    
    ###############################################################################
    #
    # A template for submitting a bug report.
    #
    # Run this program and read the output from the command line.
    #
    # reverse('(c)'), March 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    
    print << 'HINTS_1';
    
    REPORTING A BUG OR ASKING A QUESTION
    
        Feel free to report bugs or ask questions. However, to save time
        consider the following steps first:
    
        Read the documentation:
    
            The Spreadsheet::WriteExcel documentation has been refined in
            response to user questions. Therefore, if you have a question it is
            possible that someone else has asked it before you and that it is
            already addressed in the documentation. Since there is a lot of
            documentation to get through you should at least read the table of
            contents and search for keywords that you are interested in.
    
        Look at the example programs:
    
            There are over 70 example programs shipped with the standard
            Spreadsheet::WriteExcel distribution. Many of these were created in
            response to user questions. Try to identify an example program that
            corresponds to your query and adapt it to your needs.
    
    HINTS_1
    print "Press enter ..."; <STDIN>;
    
    print << 'HINTS_2';
    
        If you submit a bug report here are some pointers.
    
        1.  Put "WriteExcel:" at the beginning of the subject line. This helps
            to filter genuine messages from spam.
    
        2.  Describe the problems as clearly and as concisely as possible.
    
        3.  Send a sample program. It is often easier to describe a problem in
            code than in written prose.
    
        4.  The sample program should be as small as possible to demonstrate the
            problem. Don't copy and past large sections of your program. The
            program should also be self contained and working.
    
        A sample bug report is generated below. If you use this format then it
        will help to analyse your question and respond to it more quickly.
    
        Please don't send patches without contacting the author first.
    
    
    HINTS_2
    print "Press enter ..."; <STDIN>;
    
    
    print << 'EMAIL';
    
    =======================================================================
    
    To:      John McNamara <jmcnamara@cpan.org>
    Subject: WriteExcel: Problem with something.
    
    Hi John,
    
    I am using Spreadsheet::WriteExcel and I have encountered a problem. I
    want it to do SOMETHING but the module appears to do SOMETHING_ELSE.
    
    Here is some code that demonstrates the problem.
    
        #!/usr/bin/perl -w
    
        use strict;
        use Spreadsheet::WriteExcel;
    
        my $workbook  = Spreadsheet::WriteExcel->new("reload.xls");
        my $worksheet = $workbook->add_worksheet();
    
        $worksheet->write(0, 0, "Hi Excel!");
    
        __END__
    
    
    I tested using Excel XX (or Gnumeric or OpenOffice.org).
    
    My automatically generated system details are as follows:
    EMAIL
    
    
    print "\n    Perl version   : $]";
    print "\n    OS name        : $^O";
    print "\n    Module versions: (not all are required)\n";
    
    
    my @modules = qw(
                      Spreadsheet::WriteExcel
                      Spreadsheet::ParseExcel
                      OLE::Storage_Lite
                      Parse::RecDescent
                      File::Temp
                      Digest::MD4
                      Digest::Perl::MD4
                      Digest::MD5
                    );
    
    
    for my $module (@modules) {
        my $version;
        eval "require $module";
    
        if (not $@) {
            $version = $module->VERSION;
            $version = '(unknown)' if not defined $version;
        }
        else {
            $version = '(not installed)';
        }
    
        printf "%21s%-24s\t%s\n", "", $module, $version;
    }
    
    
    print << "BYE";
    Yours etc.,
    
    A. Person
    --
    
    BYE
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/bug_report.pl>

=head2 Example: autofilter.pl



An example of how to create autofilters with Spreadsheet::WriteExcel.

An autofilter is a way of adding drop down lists to the headers of a 2D range
of worksheet data. This is turn allow users to filter the data based on
simple criteria so that some data is shown and some is hidden.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/autofilter.jpg" width="640" height="420" alt="Output from autofilter.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # An example of how to create autofilters with Spreadsheet::WriteExcel.
    #
    # An autofilter is a way of adding drop down lists to the headers of a 2D range
    # of worksheet data. This is turn allow users to filter the data based on
    # simple criteria so that some data is shown and some is hidden.
    #
    # reverse('(c)'), September 2007, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new('autofilter.xls');
    
    die "Couldn't create new Excel file: $!.\n" unless defined $workbook;
    
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    my $worksheet3 = $workbook->add_worksheet();
    my $worksheet4 = $workbook->add_worksheet();
    my $worksheet5 = $workbook->add_worksheet();
    my $worksheet6 = $workbook->add_worksheet();
    
    my $bold       = $workbook->add_format(bold => 1);
    
    
    # Extract the data embedded at the end of this file.
    my @headings = split ' ', <DATA>;
    my @data;
    push @data, [split] while <DATA>;
    
    
    # Set up several sheets with the same data.
    for my $worksheet ($workbook->sheets()) {
        $worksheet->set_column('A:D', 12);
        $worksheet->set_row(0, 20, $bold);
        $worksheet->write('A1', \@headings);
    }
    
    
    ###############################################################################
    #
    # Example 1. Autofilter without conditions.
    #
    
    $worksheet1->autofilter('A1:D51');
    $worksheet1->write('A2', [[@data]]);
    
    
    ###############################################################################
    #
    #
    # Example 2. Autofilter with a filter condition in the first column.
    #
    
    # The range in this example is the same as above but in row-column notation.
    $worksheet2->autofilter(0, 0, 50, 3);
    
    # The placeholder "Region" in the filter is ignored and can be any string
    # that adds clarity to the expression.
    #
    $worksheet2->filter_column(0, 'Region eq East');
    
    #
    # Hide the rows that don't match the filter criteria.
    #
    my $row = 1;
    
    for my $row_data (@data) {
        my $region = $row_data->[0];
    
        if ($region eq 'East') {
            # Row is visible.
        }
        else {
            # Hide row.
            $worksheet2->set_row($row, undef, undef, 1);
        }
    
        $worksheet2->write($row++, 0, $row_data);
    }
    
    
    ###############################################################################
    #
    #
    # Example 3. Autofilter with a dual filter condition in one of the columns.
    #
    
    $worksheet3->autofilter('A1:D51');
    
    $worksheet3->filter_column('A', 'x eq East or x eq South');
    
    #
    # Hide the rows that don't match the filter criteria.
    #
    $row = 1;
    
    for my $row_data (@data) {
        my $region = $row_data->[0];
    
        if ($region eq 'East' or $region eq 'South') {
            # Row is visible.
        }
        else {
            # Hide row.
            $worksheet3->set_row($row, undef, undef, 1);
        }
    
        $worksheet3->write($row++, 0, $row_data);
    }
    
    
    ###############################################################################
    #
    #
    # Example 4. Autofilter with filter conditions in two columns.
    #
    
    $worksheet4->autofilter('A1:D51');
    
    $worksheet4->filter_column('A', 'x eq East');
    $worksheet4->filter_column('C', 'x > 3000 and x < 8000' );
    
    #
    # Hide the rows that don't match the filter criteria.
    #
    $row = 1;
    
    for my $row_data (@data) {
        my $region = $row_data->[0];
        my $volume = $row_data->[2];
    
        if ($region eq 'East' and
            $volume >  3000   and $volume < 8000
        )
        {
            # Row is visible.
        }
        else {
            # Hide row.
            $worksheet4->set_row($row, undef, undef, 1);
        }
    
        $worksheet4->write($row++, 0, $row_data);
    }
    
    
    ###############################################################################
    #
    #
    # Example 5. Autofilter with filter for blanks.
    #
    
    # Create a blank cell in our test data.
    $data[5]->[0] = '';
    
    
    $worksheet5->autofilter('A1:D51');
    $worksheet5->filter_column('A', 'x == Blanks');
    
    #
    # Hide the rows that don't match the filter criteria.
    #
    $row = 1;
    
    for my $row_data (@data) {
        my $region = $row_data->[0];
    
        if ($region eq '')
        {
            # Row is visible.
        }
        else {
            # Hide row.
            $worksheet5->set_row($row, undef, undef, 1);
        }
    
        $worksheet5->write($row++, 0, $row_data);
    }
    
    
    ###############################################################################
    #
    #
    # Example 6. Autofilter with filter for non-blanks.
    #
    
    
    $worksheet6->autofilter('A1:D51');
    $worksheet6->filter_column('A', 'x == NonBlanks');
    
    #
    # Hide the rows that don't match the filter criteria.
    #
    $row = 1;
    
    for my $row_data (@data) {
        my $region = $row_data->[0];
    
        if ($region ne '')
        {
            # Row is visible.
        }
        else {
            # Hide row.
            $worksheet6->set_row($row, undef, undef, 1);
        }
    
        $worksheet6->write($row++, 0, $row_data);
    }
    
    
    
    __DATA__
    Region    Item      Volume    Month
    East      Apple     9000      July
    East      Apple     5000      July
    South     Orange    9000      September
    North     Apple     2000      November
    West      Apple     9000      November
    South     Pear      7000      October
    North     Pear      9000      August
    West      Orange    1000      December
    West      Grape     1000      November
    South     Pear      10000     April
    West      Grape     6000      January
    South     Orange    3000      May
    North     Apple     3000      December
    South     Apple     7000      February
    West      Grape     1000      December
    East      Grape     8000      February
    South     Grape     10000     June
    West      Pear      7000      December
    South     Apple     2000      October
    East      Grape     7000      December
    North     Grape     6000      April
    East      Pear      8000      February
    North     Apple     7000      August
    North     Orange    7000      July
    North     Apple     6000      June
    South     Grape     8000      September
    West      Apple     3000      October
    South     Orange    10000     November
    West      Grape     4000      July
    North     Orange    5000      August
    East      Orange    1000      November
    East      Orange    4000      October
    North     Grape     5000      August
    East      Apple     1000      December
    South     Apple     10000     March
    East      Grape     7000      October
    West      Grape     1000      September
    East      Grape     10000     October
    South     Orange    8000      March
    North     Apple     4000      July
    South     Orange    5000      July
    West      Apple     4000      June
    East      Apple     5000      April
    North     Pear      3000      August
    East      Grape     9000      November
    North     Orange    8000      October
    East      Apple     10000     June
    South     Pear      1000      December
    North     Grape     10000     July
    East      Grape     6000      February


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/autofilter.pl>

=head2 Example: autofit.pl



Simulate Excel's autofit for column widths.

Excel provides a function called Autofit (Format->Columns->Autofit) that
adjusts column widths to match the length of the longest string in a column.
Excel calculates these widths at run time when it has access to information
about string lengths and font information. This function is *not* a feature
of the file format and thus cannot be implemented by Spreadsheet::WriteExcel.

However, we can make an attempt to simulate it by keeping track of the
longest string written to each column and then adjusting the column widths
prior to closing the file.

We keep track of the longest strings by adding a handler to the write()
function. See add_handler() in the S::WE docs for more information.

The main problem with trying to simulate Autofit lies in defining a
relationship between a string length and its width in a arbitrary font and
size. We use two approaches below. The first is a simple direct relationship
obtained by trial and error. The second is a slightly more sophisticated
method using an external module. For more complicated applications you will
probably have to work out your own methods.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/autofit.jpg" width="640" height="420" alt="Output from autofit.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # Simulate Excel's autofit for column widths.
    #
    # Excel provides a function called Autofit (Format->Columns->Autofit) that
    # adjusts column widths to match the length of the longest string in a column.
    # Excel calculates these widths at run time when it has access to information
    # about string lengths and font information. This function is *not* a feature
    # of the file format and thus cannot be implemented by Spreadsheet::WriteExcel.
    #
    # However, we can make an attempt to simulate it by keeping track of the
    # longest string written to each column and then adjusting the column widths
    # prior to closing the file.
    #
    # We keep track of the longest strings by adding a handler to the write()
    # function. See add_handler() in the S::WE docs for more information.
    #
    # The main problem with trying to simulate Autofit lies in defining a
    # relationship between a string length and its width in a arbitrary font and
    # size. We use two approaches below. The first is a simple direct relationship
    # obtained by trial and error. The second is a slightly more sophisticated
    # method using an external module. For more complicated applications you will
    # probably have to work out your own methods.
    #
    # reverse('(c)'), May 2006, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook    = Spreadsheet::WriteExcel->new('autofit.xls');
    my $worksheet   = $workbook->add_worksheet();
    
    
    ###############################################################################
    #
    # Add a handler to store the width of the longest string written to a column.
    # We use the stored width to simulate an autofit of the column widths.
    #
    # You should do this for every worksheet you want to autofit.
    #
    $worksheet->add_write_handler(qr[\w], \&store_string_widths);
    
    
    
    $worksheet->write('A1', 'Hello');
    $worksheet->write('B1', 'Hello World');
    $worksheet->write('D1', 'Hello');
    $worksheet->write('F1', 'This is a long string as an example.');
    
    # Run the autofit after you have finished writing strings to the workbook.
    autofit_columns($worksheet);
    
    
    
    ###############################################################################
    #
    # Functions used for Autofit.
    #
    ###############################################################################
    
    ###############################################################################
    #
    # Adjust the column widths to fit the longest string in the column.
    #
    sub autofit_columns {
    
        my $worksheet = shift;
        my $col       = 0;
    
        for my $width (@{$worksheet->{__col_widths}}) {
    
            $worksheet->set_column($col, $col, $width) if $width;
            $col++;
        }
    }
    
    
    ###############################################################################
    #
    # The following function is a callback that was added via add_write_handler()
    # above. It modifies the write() function so that it stores the maximum
    # unwrapped width of a string in a column.
    #
    sub store_string_widths {
    
        my $worksheet = shift;
        my $col       = $_[1];
        my $token     = $_[2];
    
        # Ignore some tokens that we aren't interested in.
        return if not defined $token;       # Ignore undefs.
        return if $token eq '';             # Ignore blank cells.
        return if ref $token eq 'ARRAY';    # Ignore array refs.
        return if $token =~ /^=/;           # Ignore formula
    
        # Ignore numbers
        return if $token =~ /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/;
    
        # Ignore various internal and external hyperlinks. In a real scenario
        # you may wish to track the length of the optional strings used with
        # urls.
        return if $token =~ m{^[fh]tt?ps?://};
        return if $token =~ m{^mailto:};
        return if $token =~ m{^(?:in|ex)ternal:};
    
    
        # We store the string width as data in the Worksheet object. We use
        # a double underscore key name to avoid conflicts with future names.
        #
        my $old_width    = $worksheet->{__col_widths}->[$col];
        my $string_width = string_width($token);
    
        if (not defined $old_width or $string_width > $old_width) {
            # You may wish to set a minimum column width as follows.
            #return undef if $string_width < 10;
    
            $worksheet->{__col_widths}->[$col] = $string_width;
        }
    
    
        # Return control to write();
        return undef;
    }
    
    
    ###############################################################################
    #
    # Very simple conversion between string length and string width for Arial 10.
    # See below for a more sophisticated method.
    #
    sub string_width {
    
        return 0.9 * length $_[0];
    }
    
    __END__
    
    
    
    ###############################################################################
    #
    # This function uses an external module to get a more accurate width for a
    # string. Note that in a real program you could "use" the module instead of
    # "require"-ing it and you could make the Font object global to avoid repeated
    # initialisation.
    #
    # Note also that the $pixel_width to $cell_width is specific to Arial. For
    # other fonts you should calculate appropriate relationships. A future version
    # of S::WE will provide a way of specifying column widths in pixels instead of
    # cell units in order to simplify this conversion.
    #
    sub string_width {
    
        require Font::TTFMetrics;
    
        my $arial        = Font::TTFMetrics->new('c:\windows\fonts\arial.ttf');
    
        my $font_size    = 10;
        my $dpi          = 96;
        my $units_per_em = $arial->get_units_per_em();
        my $font_width   = $arial->string_width($_[0]);
    
        # Convert to pixels as per TTFMetrics docs.
        my $pixel_width  = 6 + $font_width *$font_size *$dpi /(72 *$units_per_em);
    
        # Add extra pixels for border around text.
        $pixel_width  += 6;
    
        # Convert to cell width (for Arial) and for cell widths > 1.
        my $cell_width   = ($pixel_width -5) /7;
    
        return $cell_width;
    
    }
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/autofit.pl>

=head2 Example: bigfile.pl



Example of creating a Spreadsheet::WriteExcel that is larger than the
default 7MB limit.

This is exactly that same as any other Spreadsheet::WriteExcel program except
that is requires that the OLE::Storage module is installed.


=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/bigfile.jpg" width="640" height="420" alt="Output from bigfile.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of creating a Spreadsheet::WriteExcel that is larger than the
    # default 7MB limit.
    #
    # This is exactly that same as any other Spreadsheet::WriteExcel program except
    # that is requires that the OLE::Storage module is installed.
    #
    # reverse('(c)'), Jan 2007, John McNamara, jmcnamara@cpan.org
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new('bigfile.xls');
    my $worksheet = $workbook->add_worksheet();
    
    $worksheet->set_column(0, 50, 18);
    
    for my $col (0 .. 50) {
        for my $row (0 .. 6000) {
            $worksheet->write($row, $col, "Row: $row Col: $col");
        }
    }
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/bigfile.pl>

=head2 Example: cgi.pl



Example of how to use the Spreadsheet::WriteExcel module to send an Excel
file to a browser in a CGI program.

On Windows the hash-bang line should be something like:

    #!C:\Perl\bin\perl.exe

The "Content-Disposition" line will cause a prompt to be generated to save
the file. If you want to stream the file to the browser instead, comment out
that line as shown below.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel module to send an Excel
    # file to a browser in a CGI program.
    #
    # On Windows the hash-bang line should be something like:
    #
    #     #!C:\Perl\bin\perl.exe
    #
    # The "Content-Disposition" line will cause a prompt to be generated to save
    # the file. If you want to stream the file to the browser instead, comment out
    # that line as shown below.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Set the filename and send the content type
    my $filename ="cgitest.xls";
    
    print "Content-type: application/vnd.ms-excel\n";
    # The Content-Disposition will generate a prompt to save the file. If you want
    # to stream the file to the browser, comment out the following line.
    print "Content-Disposition: attachment; filename=$filename\n";
    print "\n";
    
    # Create a new workbook and add a worksheet. The special Perl filehandle - will
    # redirect the output to STDOUT
    #
    my $workbook  = Spreadsheet::WriteExcel->new(\*STDOUT);
    my $worksheet = $workbook->add_worksheet();
    
    
    # Set the column width for column 1
    $worksheet->set_column(0, 0, 20);
    
    
    # Create a format
    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_size(15);
    $format->set_color('blue');
    
    
    # Write to the workbook
    $worksheet->write(0, 0, "Hi Excel!", $format);
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/cgi.pl>

=head2 Example: chart_area.pl



A simple demo of Area charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_area.jpg" width="640" height="420" alt="Output from chart_area.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Area charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_area.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );
    
    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values 1', 'Values 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];
    
    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'area' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$7' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories (X axis)
    #            and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'area' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with added title and axes labels.
    #
    my $chart3 = $workbook->add_chart( type => 'area' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart3->set_title( name => 'Results of sample analysis' );
    $chart3->set_x_axis( name => 'Sample number' );
    $chart3->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart but with an added series and with a
    #            user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'area' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add another series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );
    
    # Add some labels.
    $chart4->set_title( name => 'Results of sample analysis' );
    $chart4->set_x_axis( name => 'Sample number' );
    $chart4->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'area', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart5->set_title( name => 'Results of sample analysis' );
    $chart5->set_x_axis( name => 'Sample number' );
    $chart5->set_y_axis( name => 'Sample length (cm)' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'E2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_area.pl>

=head2 Example: chart_bar.pl



A simple demo of Bar charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_bar.jpg" width="640" height="420" alt="Output from chart_bar.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Bar charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_bar.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );
    
    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values 1', 'Values 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];
    
    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'bar' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$7' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories (X axis)
    #            and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'bar' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with added title and axes labels.
    #
    my $chart3 = $workbook->add_chart( type => 'bar' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart3->set_title( name => 'Results of sample analysis' );
    $chart3->set_x_axis( name => 'Sample number' );
    $chart3->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart but with an added series and with a
    #            user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'bar' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add another series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );
    
    # Add some labels.
    $chart4->set_title( name => 'Results of sample analysis' );
    $chart4->set_x_axis( name => 'Sample number' );
    $chart4->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'bar', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart5->set_title( name => 'Results of sample analysis' );
    $chart5->set_x_axis( name => 'Sample number' );
    $chart5->set_y_axis( name => 'Sample length (cm)' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'E2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_bar.pl>

=head2 Example: chart_column.pl



A simple demo of Column charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_column.jpg" width="640" height="420" alt="Output from chart_column.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Column charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_column.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );
    
    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values 1', 'Values 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];
    
    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'column' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$7' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories (X axis)
    #            and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'column' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with added title and axes labels.
    #
    my $chart3 = $workbook->add_chart( type => 'column' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart3->set_title( name => 'Results of sample analysis' );
    $chart3->set_x_axis( name => 'Sample number' );
    $chart3->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart but with an added series and with a
    #            user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'column' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add another series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );
    
    # Add some labels.
    $chart4->set_title( name => 'Results of sample analysis' );
    $chart4->set_x_axis( name => 'Sample number' );
    $chart4->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'column', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart5->set_title( name => 'Results of sample analysis' );
    $chart5->set_x_axis( name => 'Sample number' );
    $chart5->set_y_axis( name => 'Sample length (cm)' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'E2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_column.pl>

=head2 Example: chart_line.pl



A simple demo of Line charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_line.jpg" width="640" height="420" alt="Output from chart_line.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Line charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_line.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );
    
    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values 1', 'Values 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];
    
    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'line' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$7' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories (X axis)
    #            and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'line' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with added title and axes labels.
    #
    my $chart3 = $workbook->add_chart( type => 'line' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart3->set_title( name => 'Results of sample analysis' );
    $chart3->set_x_axis( name => 'Sample number' );
    $chart3->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart but with an added series and with a
    #            user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'line' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add another series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );
    
    # Add some labels.
    $chart4->set_title( name => 'Results of sample analysis' );
    $chart4->set_x_axis( name => 'Sample number' );
    $chart4->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'line', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart5->set_title( name => 'Results of sample analysis' );
    $chart5->set_x_axis( name => 'Sample number' );
    $chart5->set_y_axis( name => 'Sample length (cm)' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'E2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_line.pl>

=head2 Example: chart_pie.pl



A simple demo of Pie charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_pie.jpg" width="640" height="420" alt="Output from chart_pie.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Pie charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
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
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'pie' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$4' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'pie' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$4',
        values     => '=Sheet1!$B$2:$B$4',
        name       => 'Pie sales data',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with an added title.
    #
    my $chart3 = $workbook->add_chart( type => 'pie' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$4',
        values     => '=Sheet1!$B$2:$B$4',
        name       => 'Pie sales data',
    );
    
    # Add a title.
    $chart3->set_title( name => 'Popular Pie Types' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart with a user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'pie' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$4',
        values     => '=Sheet1!$B$2:$B$4',
        name       => 'Pie sales data',
    );
    
    # The other chart_*.pl examples add a second series in example 4 but additional
    # series aren't plotted in a pie chart.
    
    # Add a title.
    $chart4->set_title( name => 'Popular Pie Types' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'pie', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$4',
        values     => '=Sheet1!$B$2:$B$4',
        name       => 'Pie sales data',
    );
    
    # Add a title.
    $chart5->set_title( name => 'Popular Pie Types' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'D2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_pie.pl>

=head2 Example: chart_scatter.pl



A simple demo of Scatter charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_scatter.jpg" width="640" height="420" alt="Output from chart_scatter.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Scatter charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), December 2009, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_scatter.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );
    
    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Category', 'Values 1', 'Values 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];
    
    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );
    
    
    ###############################################################################
    #
    # Example 1. A minimal chart.
    #
    my $chart1 = $workbook->add_chart( type => 'scatter' );
    
    # Add values only. Use the default categories.
    $chart1->add_series( values => '=Sheet1!$B$2:$B$7' );
    
    
    ###############################################################################
    #
    # Example 2. A minimal chart with user specified categories (X axis)
    #            and a series name.
    #
    my $chart2 = $workbook->add_chart( type => 'scatter' );
    
    # Configure the series.
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    
    ###############################################################################
    #
    # Example 3. Same as previous chart but with added title and axes labels.
    #
    my $chart3 = $workbook->add_chart( type => 'scatter' );
    
    # Configure the series.
    $chart3->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart3->set_title( name => 'Results of sample analysis' );
    $chart3->set_x_axis( name => 'Sample number' );
    $chart3->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 4. Same as previous chart but with an added series and with a
    #            user specified chart sheet name.
    #
    my $chart4 = $workbook->add_chart( name => 'Results Chart', type => 'scatter' );
    
    # Configure the series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add another series.
    $chart4->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );
    
    # Add some labels.
    $chart4->set_title( name => 'Results of sample analysis' );
    $chart4->set_x_axis( name => 'Sample number' );
    $chart4->set_y_axis( name => 'Sample length (cm)' );
    
    
    ###############################################################################
    #
    # Example 5. Same as Example 3 but as an embedded chart.
    #
    my $chart5 = $workbook->add_chart( type => 'scatter', embedded => 1 );
    
    # Configure the series.
    $chart5->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );
    
    # Add some labels.
    $chart5->set_title( name => 'Results of sample analysis' );
    $chart5->set_x_axis( name => 'Sample number' );
    $chart5->set_y_axis( name => 'Sample length (cm)' );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'E2', $chart5 );
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_scatter.pl>

=head2 Example: chart_stock.pl



A simple demo of Stock charts in Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chart_stock.jpg" width="640" height="420" alt="Output from chart_stock.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # A simple demo of Stock charts in Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), January 2010, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_stock.xls' );
    my $worksheet = $workbook->add_worksheet();
    
    
    ###############################################################################
    #
    # Set up the data worksheet that the charts will refer to. We read the example
    # data from the __DATA__ section at the end of the file. This simulates
    # reading the data from a database or other source.
    #
    # The default Excel Stock chart is an Open-High-Low-Close chart. Therefore
    # we will need data for each of those series.
    #
    # The layout of the __DATA__ section is similar to the layout of the worksheet.
    #
    
    # Add some formats.
    my $bold        = $workbook->add_format( bold       => 1 );
    my $date_format = $workbook->add_format( num_format => 'dd/mm/yyyy' );
    
    # Increase the width of the column used for date to make it clearer.
    $worksheet->set_column( 'A:A', 12 );
    
    # Read the data from the __DATA__ section at the end. In a real example this
    # would probably be a database query.
    my @stock_data;
    
    while ( <DATA> ) {
        next unless /\S/;    # Skip blank lines.
        next if /^#/;        # Skip comments.
    
        push @stock_data, [split];
    }
    
    # Write the data to the worksheet.
    my $row = 0;
    my $col = 0;
    
    my $headers = shift @stock_data;
    $worksheet->write( $row++, $col, $headers, $bold );
    
    for my $stock_data ( @stock_data ) {
    
        my @data = @$stock_data;
        my $date = shift @data;
    
        $worksheet->write( $row, $col, $date, $date_format );
        $worksheet->write( $row, $col + 1, \@data );
    
        $row++;
    }
    
    
    ###############################################################################
    #
    # Example 1. A default Open-High-Low-Close chart with series names, axes labels
    #            and a title.
    #
    
    my $chart1 = $workbook->add_chart( type => 'stock' );
    
    # Add a series for each of the Open-High-Low-Close columns. The categories are
    # the dates in the first column.
    
    $chart1->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$B$2:$B$10',
        name       => 'Open',
    );
    
    $chart1->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$C$2:$C$10',
        name       => 'High',
    );
    
    $chart1->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$D$2:$D$10',
        name       => 'Low',
    );
    
    $chart1->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$E$2:$E$10',
        name       => 'Close',
    );
    
    # Add a chart title and axes labels.
    $chart1->set_title( name => 'Open-High-Low-Close', );
    $chart1->set_x_axis( name => 'Date', );
    $chart1->set_y_axis( name => 'Share price', );
    
    ###############################################################################
    #
    # Example 2. Same as the previous as an embedded chart.
    #
    
    my $chart2 = $workbook->add_chart( type => 'stock', embedded => 1 );
    
    # Add a series for each of the Open-High-Low-Close columns. The categories are
    # the dates in the first column.
    
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$B$2:$B$10',
        name       => 'Open',
    );
    
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$C$2:$C$10',
        name       => 'High',
    );
    
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$D$2:$D$10',
        name       => 'Low',
    );
    
    $chart2->add_series(
        categories => '=Sheet1!$A$2:$A$10',
        values     => '=Sheet1!$E$2:$E$10',
        name       => 'Close',
    );
    
    # Add a chart title and axes labels.
    $chart2->set_title( name => 'Open-High-Low-Close', );
    $chart2->set_x_axis( name => 'Date', );
    $chart2->set_y_axis( name => 'Share price', );
    
    # Insert the chart into the main worksheet.
    $worksheet->insert_chart( 'G2', $chart2 );
    
    
    __DATA__
    # Some sample stock data used for charting.
    Date        Open    High    Low     Close
    2009-08-19  100.00  104.06  95.96   100.34
    2009-08-20  101.01  109.08  100.50  108.31
    2009-08-23  110.75  113.48  109.05  109.40
    2009-08-24  111.24  111.60  103.57  104.87
    2009-08-25  104.96  108.00  103.88  106.00
    2009-08-26  104.95  107.95  104.66  107.91
    2009-08-27  108.10  108.62  105.69  106.15
    2009-08-30  105.28  105.49  102.01  102.01
    2009-08-31  102.30  103.71  102.16  102.37


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chart_stock.pl>

=head2 Example: chess.pl



Example of formatting using the Spreadsheet::WriteExcel module via
property hashes.

Setting format properties via hashes of values is useful when you have
to deal with a large number of similar formats. Consider for example a
chess board pattern with black squares, white unformatted squares and
a border.

This relatively simple example requires 14 separate Format
objects although there are only 5 different properties: black
background, top border, bottom border, left border and right border.

Using property hashes it is possible to define these 5 sets of
properties and then add them together to create the 14 Format
configurations.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/chess.jpg" width="640" height="420" alt="Output from chess.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ########################################################################
    #
    # Example of formatting using the Spreadsheet::WriteExcel module via
    # property hashes.
    #
    # Setting format properties via hashes of values is useful when you have
    # to deal with a large number of similar formats. Consider for example a
    # chess board pattern with black squares, white unformatted squares and
    # a border.
    #
    # This relatively simple example requires 14 separate Format
    # objects although there are only 5 different properties: black
    # background, top border, bottom border, left border and right border.
    #
    # Using property hashes it is possible to define these 5 sets of
    # properties and then add them together to create the 14 Format
    # configurations.
    #
    # reverse('(c)'), July 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("chess.xls");
    my $worksheet = $workbook->add_worksheet();
    
    
    # Some row and column formatting
    $worksheet->set_column('B:I', 10);
    
    for my $i (1..8) {
        $worksheet->set_row($i, 50);
    }
    
    
    # Define the property hashes
    #
    my %black = (
                    'fg_color'  => 'black',
                    'pattern'   => 1,
                );
    
    my %top     = ( 'top'    => 6 );
    my %bottom  = ( 'bottom' => 6 );
    my %left    = ( 'left'   => 6 );
    my %right   = ( 'right'  => 6 );
    
    
    # Define the formats
    #
    my $format01 = $workbook->add_format(%top,    %left          );
    my $format02 = $workbook->add_format(%top,    %black         );
    my $format03 = $workbook->add_format(%top,                   );
    my $format04 = $workbook->add_format(%top,    %right, %black );
    
    my $format05 = $workbook->add_format(%left                   );
    my $format06 = $workbook->add_format(%black                  );
    my $format07 = $workbook->add_format(                        );
    my $format08 = $workbook->add_format(%right,  %black         );
    my $format09 = $workbook->add_format(%right                  );
    my $format10 = $workbook->add_format(%left,   %black         );
    
    my $format11 = $workbook->add_format(%bottom, %left,  %black );
    my $format12 = $workbook->add_format(%bottom                 );
    my $format13 = $workbook->add_format(%bottom, %black         );
    my $format14 = $workbook->add_format(%bottom, %right         );
    
    
    # Draw the pattern
    $worksheet->write('B2', '', $format01);
    $worksheet->write('C2', '', $format02);
    $worksheet->write('D2', '', $format03);
    $worksheet->write('E2', '', $format02);
    $worksheet->write('F2', '', $format03);
    $worksheet->write('G2', '', $format02);
    $worksheet->write('H2', '', $format03);
    $worksheet->write('I2', '', $format04);
    
    $worksheet->write('B3', '', $format10);
    $worksheet->write('C3', '', $format07);
    $worksheet->write('D3', '', $format06);
    $worksheet->write('E3', '', $format07);
    $worksheet->write('F3', '', $format06);
    $worksheet->write('G3', '', $format07);
    $worksheet->write('H3', '', $format06);
    $worksheet->write('I3', '', $format09);
    
    $worksheet->write('B4', '', $format05);
    $worksheet->write('C4', '', $format06);
    $worksheet->write('D4', '', $format07);
    $worksheet->write('E4', '', $format06);
    $worksheet->write('F4', '', $format07);
    $worksheet->write('G4', '', $format06);
    $worksheet->write('H4', '', $format07);
    $worksheet->write('I4', '', $format08);
    
    $worksheet->write('B5', '', $format10);
    $worksheet->write('C5', '', $format07);
    $worksheet->write('D5', '', $format06);
    $worksheet->write('E5', '', $format07);
    $worksheet->write('F5', '', $format06);
    $worksheet->write('G5', '', $format07);
    $worksheet->write('H5', '', $format06);
    $worksheet->write('I5', '', $format09);
    
    $worksheet->write('B6', '', $format05);
    $worksheet->write('C6', '', $format06);
    $worksheet->write('D6', '', $format07);
    $worksheet->write('E6', '', $format06);
    $worksheet->write('F6', '', $format07);
    $worksheet->write('G6', '', $format06);
    $worksheet->write('H6', '', $format07);
    $worksheet->write('I6', '', $format08);
    
    $worksheet->write('B7', '', $format10);
    $worksheet->write('C7', '', $format07);
    $worksheet->write('D7', '', $format06);
    $worksheet->write('E7', '', $format07);
    $worksheet->write('F7', '', $format06);
    $worksheet->write('G7', '', $format07);
    $worksheet->write('H7', '', $format06);
    $worksheet->write('I7', '', $format09);
    
    $worksheet->write('B8', '', $format05);
    $worksheet->write('C8', '', $format06);
    $worksheet->write('D8', '', $format07);
    $worksheet->write('E8', '', $format06);
    $worksheet->write('F8', '', $format07);
    $worksheet->write('G8', '', $format06);
    $worksheet->write('H8', '', $format07);
    $worksheet->write('I8', '', $format08);
    
    $worksheet->write('B9', '', $format11);
    $worksheet->write('C9', '', $format12);
    $worksheet->write('D9', '', $format13);
    $worksheet->write('E9', '', $format12);
    $worksheet->write('F9', '', $format13);
    $worksheet->write('G9', '', $format12);
    $worksheet->write('H9', '', $format13);
    $worksheet->write('I9', '', $format14);
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/chess.pl>

=head2 Example: colors.pl



Demonstrates Spreadsheet::WriteExcel's named colors and the Excel color
palette.

The set_custom_color() Worksheet method can be used to override one of the
built-in palette values with a more suitable colour. See the main docs.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/colors.jpg" width="640" height="420" alt="Output from colors.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ################################################################################
    #
    # Demonstrates Spreadsheet::WriteExcel's named colors and the Excel color
    # palette.
    #
    # The set_custom_color() Worksheet method can be used to override one of the
    # built-in palette values with a more suitable colour. See the main docs.
    #
    # reverse('(c)'), March 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook = Spreadsheet::WriteExcel->new("colors.xls");
    
    # Some common formats
    my $center  = $workbook->add_format(align => 'center');
    my $heading = $workbook->add_format(align => 'center', bold => 1);
    
    
    ######################################################################
    #
    # Demonstrate the named colors.
    #
    
    my %colors = (
                    0x08, 'black',
                    0x0C, 'blue',
                    0x10, 'brown',
                    0x0F, 'cyan',
                    0x17, 'gray',
                    0x11, 'green',
                    0x0B, 'lime',
                    0x0E, 'magenta',
                    0x12, 'navy',
                    0x35, 'orange',
                    0x21, 'pink',
                    0x14, 'purple',
                    0x0A, 'red',
                    0x16, 'silver',
                    0x09, 'white',
                    0x0D, 'yellow',
                 );
    
    my $worksheet1 = $workbook->add_worksheet('Named colors');
    
    $worksheet1->set_column(0, 3, 15);
    
    $worksheet1->write(0, 0, "Index", $heading);
    $worksheet1->write(0, 1, "Index", $heading);
    $worksheet1->write(0, 2, "Name",  $heading);
    $worksheet1->write(0, 3, "Color", $heading);
    
    my $i = 1;
    
    while (my($index, $color) = each %colors) {
        my $format = $workbook->add_format(
                                            fg_color => $color,
                                            pattern  => 1,
                                            border   => 1
                                         );
    
        $worksheet1->write($i+1, 0, $index,                    $center);
        $worksheet1->write($i+1, 1, sprintf("0x%02X", $index), $center);
        $worksheet1->write($i+1, 2, $color,                    $center);
        $worksheet1->write($i+1, 3, '',                        $format);
        $i++;
    }
    
    
    ######################################################################
    #
    # Demonstrate the standard Excel colors in the range 8..63.
    #
    
    my $worksheet2 = $workbook->add_worksheet('Standard colors');
    
    $worksheet2->set_column(0, 3, 15);
    
    $worksheet2->write(0, 0, "Index", $heading);
    $worksheet2->write(0, 1, "Index", $heading);
    $worksheet2->write(0, 2, "Color", $heading);
    $worksheet2->write(0, 3, "Name",  $heading);
    
    for my $i (8..63) {
        my $format = $workbook->add_format(
                                            fg_color => $i,
                                            pattern  => 1,
                                            border   => 1
                                         );
    
        $worksheet2->write(($i -7), 0, $i,                    $center);
        $worksheet2->write(($i -7), 1, sprintf("0x%02X", $i), $center);
        $worksheet2->write(($i -7), 2, '',                    $format);
    
        # Add the  color names
        if (exists $colors{$i}) {
            $worksheet2->write(($i -7), 3, $colors{$i}, $center);
    
        }
    }
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/colors.pl>

=head2 Example: comments1.pl



This example demonstrates writing cell comments.

A cell comment is indicated in Excel by a small red triangle in the upper
right-hand corner of the cell.

For more advanced comment options see comments2.pl.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/comments1.jpg" width="640" height="420" alt="Output from comments1.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This example demonstrates writing cell comments.
    #
    # A cell comment is indicated in Excel by a small red triangle in the upper
    # right-hand corner of the cell.
    #
    # For more advanced comment options see comments2.pl.
    #
    # reverse('(c)'), November 2005, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("comments1.xls");
    my $worksheet = $workbook->add_worksheet();
    
    
    
    $worksheet->write        ('A1', 'Hello'            );
    $worksheet->write_comment('A1', 'This is a comment');
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/comments1.pl>

=head2 Example: comments2.pl



This example demonstrates writing cell comments.

A cell comment is indicated in Excel by a small red triangle in the upper
right-hand corner of the cell.

Each of the worksheets demonstrates different features of cell comments.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/comments2.jpg" width="640" height="420" alt="Output from comments2.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This example demonstrates writing cell comments.
    #
    # A cell comment is indicated in Excel by a small red triangle in the upper
    # right-hand corner of the cell.
    #
    # Each of the worksheets demonstrates different features of cell comments.
    #
    # reverse('(c)'), November 2005, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new("comments2.xls");
    my $text_wrap  = $workbook->add_format(text_wrap => 1, valign => 'top');
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    my $worksheet3 = $workbook->add_worksheet();
    my $worksheet4 = $workbook->add_worksheet();
    my $worksheet5 = $workbook->add_worksheet();
    my $worksheet6 = $workbook->add_worksheet();
    my $worksheet7 = $workbook->add_worksheet();
    my $worksheet8 = $workbook->add_worksheet();
    
    
    # Variables that we will use in each example.
    my $cell_text = '';
    my $comment   = '';
    
    
    
    
    ###############################################################################
    #
    # Example 1. Demonstrates a simple cell comment without formatting and Unicode
    #            comments encoded as UTF-16 and as UTF-8.
    #
    
    # Set up some formatting.
    $worksheet1->set_column('C:C', 25);
    $worksheet1->set_row(2, 50);
    $worksheet1->set_row(5, 50);
    
    
    # Simple ascii string.
    $cell_text = 'Hold the mouse over this cell to see the comment.';
    
    $comment   = 'This is a comment.';
    
    $worksheet1->write        ('C3', $cell_text, $text_wrap);
    $worksheet1->write_comment('C3', $comment);
    
    
    # UTF-16 string.
    $cell_text = 'This is a UTF-16 comment.';
    
    $comment   = pack "n", 0x263a;
    
    $worksheet1->write        ('C6', $cell_text, $text_wrap);
    $worksheet1->write_comment('C6', $comment, encoding => 1);
    
    
    # UTF-8 string in perl 5.8.
    if ($] >= 5.008) {
    
        $worksheet1->set_row(8, 50);
        $cell_text = 'This is a UTF-8 string.';
        $comment   = chr 0x263a;
    
        $worksheet1->write        ('C9', $cell_text, $text_wrap);
        $worksheet1->write_comment('C9', $comment);
    }
    
    
    
    ###############################################################################
    #
    # Example 2. Demonstrates visible and hidden comments.
    #
    
    # Set up some formatting.
    $worksheet2->set_column('C:C', 25);
    $worksheet2->set_row(2, 50);
    $worksheet2->set_row(5, 50);
    
    
    $cell_text = 'This cell comment is visible.';
    
    $comment   = 'Hello.';
    
    $worksheet2->write        ('C3', $cell_text, $text_wrap);
    $worksheet2->write_comment('C3', $comment, visible => 1);
    
    
    $cell_text = "This cell comment isn't visible (the default).";
    
    $comment   = 'Hello.';
    
    $worksheet2->write        ('C6', $cell_text, $text_wrap);
    $worksheet2->write_comment('C6', $comment);
    
    
    
    
    ###############################################################################
    #
    # Example 3. Demonstrates visible and hidden comments set at the worksheet
    #            level.
    #
    
    # Set up some formatting.
    $worksheet3->set_column('C:C', 25);
    $worksheet3->set_row(2, 50);
    $worksheet3->set_row(5, 50);
    $worksheet3->set_row(8, 50);
    
    # Make all comments on the worksheet visible.
    $worksheet3->show_comments();
    
    $cell_text = 'This cell comment is visible, explicitly.';
    
    $comment   = 'Hello.';
    
    $worksheet3->write        ('C3', $cell_text, $text_wrap);
    $worksheet3->write_comment('C3', $comment, visible => 1);
    
    
    $cell_text = 'This cell comment is also visible because '.
                 'we used show_comments().';
    
    $comment   = 'Hello.';
    
    $worksheet3->write        ('C6', $cell_text, $text_wrap);
    $worksheet3->write_comment('C6', $comment);
    
    
    $cell_text = 'However, we can still override it locally.';
    
    $comment   = 'Hello.';
    
    $worksheet3->write        ('C9', $cell_text, $text_wrap);
    $worksheet3->write_comment('C9', $comment, visible => 0);
    
    
    
    
    ###############################################################################
    #
    # Example 4. Demonstrates changes to the comment box dimensions.
    #
    
    # Set up some formatting.
    $worksheet4->set_column('C:C', 25);
    $worksheet4->set_row(2,  50);
    $worksheet4->set_row(5,  50);
    $worksheet4->set_row(8,  50);
    $worksheet4->set_row(15, 50);
    
    $worksheet4->show_comments();
    
    $cell_text = 'This cell comment is default size.';
    
    $comment   = 'Hello.';
    
    $worksheet4->write        ('C3', $cell_text, $text_wrap);
    $worksheet4->write_comment('C3', $comment);
    
    
    $cell_text = 'This cell comment is twice as wide.';
    
    $comment   = 'Hello.';
    
    $worksheet4->write        ('C6', $cell_text, $text_wrap);
    $worksheet4->write_comment('C6', $comment, x_scale => 2);
    
    
    $cell_text = 'This cell comment is twice as high.';
    
    $comment   = 'Hello.';
    
    $worksheet4->write        ('C9', $cell_text, $text_wrap);
    $worksheet4->write_comment('C9', $comment, y_scale => 2);
    
    
    $cell_text = 'This cell comment is scaled in both directions.';
    
    $comment   = 'Hello.';
    
    $worksheet4->write        ('C16', $cell_text, $text_wrap);
    $worksheet4->write_comment('C16', $comment, x_scale => 1.2, y_scale => 0.8);
    
    
    $cell_text = 'This cell comment has width and height specified in pixels.';
    
    $comment   = 'Hello.';
    
    $worksheet4->write        ('C19', $cell_text, $text_wrap);
    $worksheet4->write_comment('C19', $comment, width => 200, height => 20);
    
    
    
    ###############################################################################
    #
    # Example 5. Demonstrates changes to the cell comment position.
    #
    
    $worksheet5->set_column('C:C', 25);
    $worksheet5->set_row(2, 50);
    $worksheet5->set_row(5, 50);
    $worksheet5->set_row(8, 50);
    $worksheet5->set_row(11, 50);
    
    $worksheet5->show_comments();
    
    $cell_text = 'This cell comment is in the default position.';
    
    $comment   = 'Hello.';
    
    $worksheet5->write        ('C3', $cell_text, $text_wrap);
    $worksheet5->write_comment('C3', $comment);
    
    
    $cell_text = 'This cell comment has been moved to another cell.';
    
    $comment   = 'Hello.';
    
    $worksheet5->write        ('C6', $cell_text, $text_wrap);
    $worksheet5->write_comment('C6', $comment, start_cell => 'E4');
    
    
    $cell_text = 'This cell comment has been moved to another cell.';
    
    $comment   = 'Hello.';
    
    $worksheet5->write        ('C9', $cell_text, $text_wrap);
    $worksheet5->write_comment('C9', $comment, start_row => 8, start_col => 4);
    
    
    $cell_text = 'This cell comment has been shifted within its default cell.';
    
    $comment   = 'Hello.';
    
    $worksheet5->write        ('C12', $cell_text, $text_wrap);
    $worksheet5->write_comment('C12', $comment, x_offset => 30, y_offset => 12);
    
    
    
    ###############################################################################
    #
    # Example 6. Demonstrates changes to the comment background colour.
    #
    
    $worksheet6->set_column('C:C', 25);
    $worksheet6->set_row(2, 50);
    $worksheet6->set_row(5, 50);
    $worksheet6->set_row(8, 50);
    
    $worksheet6->show_comments();
    
    $cell_text = 'This cell comment has a different colour.';
    
    $comment   = 'Hello.';
    
    $worksheet6->write        ('C3', $cell_text, $text_wrap);
    $worksheet6->write_comment('C3', $comment, color => 'green');
    
    
    $cell_text = 'This cell comment has the default colour.';
    
    $comment   = 'Hello.';
    
    $worksheet6->write        ('C6', $cell_text, $text_wrap);
    $worksheet6->write_comment('C6', $comment);
    
    
    $cell_text = 'This cell comment has a different colour.';
    
    $comment   = 'Hello.';
    
    $worksheet6->write        ('C9', $cell_text, $text_wrap);
    $worksheet6->write_comment('C9', $comment, color => 0x35);
    
    
    
    
    ###############################################################################
    #
    # Example 7. Demonstrates how to set the cell comment author.
    #
    
    $worksheet7->set_column('C:C', 30);
    $worksheet7->set_row(2,  50);
    $worksheet7->set_row(5,  50);
    $worksheet7->set_row(8,  50);
    $worksheet7->set_row(11, 50);
    
    my $author = '';
    my $cell   = 'C3';
    
    $cell_text = "Move the mouse over this cell and you will see 'Cell commented ".
                 "by $author' (blank) in the status bar at the bottom";
    
    $comment   = 'Hello.';
    
    $worksheet7->write        ($cell, $cell_text, $text_wrap);
    $worksheet7->write_comment($cell, $comment);
    
    
    $author    = 'Perl';
    $cell      = 'C6';
    $cell_text = "Move the mouse over this cell and you will see 'Cell commented ".
                 "by $author' in the status bar at the bottom";
    
    $comment   = 'Hello.';
    
    $worksheet7->write        ($cell, $cell_text, $text_wrap);
    $worksheet7->write_comment($cell, $comment, author => $author);
    
    
    $author    = pack "n", 0x20AC; # UTF-16 Euro
    $cell      = 'C9';
    $cell_text = "Move the mouse over this cell and you will see 'Cell commented ".
                 "by Euro' in the status bar at the bottom";
    
    $comment   = 'Hello.';
    
    $worksheet7->write        ($cell, $cell_text, $text_wrap);
    $worksheet7->write_comment($cell, $comment, author          => $author,
                                                author_encoding => 1      );
    
    # UTF-8 string in perl 5.8.
    if ($] >= 5.008) {
        $author    = chr 0x20AC;
        $cell      = 'C12';
        $cell_text = "Move the mouse over this cell and you will see 'Cell commented ".
                     "by $author' in the status bar at the bottom";
        $comment   = 'Hello.';
    
        $worksheet7->write        ($cell, $cell_text, $text_wrap);
        $worksheet7->write_comment($cell, $comment, author => $author);
    
    }
    
    
    ###############################################################################
    #
    # Example 8. Demonstrates the need to explicitly set the row height.
    #
    
    # Set up some formatting.
    $worksheet8->set_column('C:C', 25);
    $worksheet8->set_row(2, 80);
    
    $worksheet8->show_comments();
    
    
    $cell_text = 'The height of this row has been adjusted explicitly using ' .
                 'set_row(). The size of the comment box is adjusted '         .
                 'accordingly by WriteExcel.';
    
    $comment   = 'Hello.';
    
    $worksheet8->write        ('C3', $cell_text, $text_wrap);
    $worksheet8->write_comment('C3', $comment);
    
    
    $cell_text = 'The height of this row has been adjusted by Excel due to the '  .
                 'text wrap property being set. Unfortunately this means that '   .
                 'the height of the row is unknown to WriteExcel at run time '    .
                 "and thus the comment box is stretched as well.\n\n"             .
                 'Use set_row() to specify the row height explicitly to avoid '   .
                 'this problem.';
    
    $comment   = 'Hello.';
    
    $worksheet8->write        ('C6', $cell_text, $text_wrap);
    $worksheet8->write_comment('C6', $comment);
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/comments2.pl>

=head2 Example: copyformat.pl



Example of how to use the format copying method with Spreadsheet::WriteExcel.

This feature isn't required very often.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/copyformat.jpg" width="640" height="420" alt="Output from copyformat.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the format copying method with Spreadsheet::WriteExcel.
    #
    # This feature isn't required very often.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create workbook1
    my $workbook1       = Spreadsheet::WriteExcel->new("workbook1.xls");
    my $worksheet1      = $workbook1->add_worksheet();
    my $format1a        = $workbook1->add_format();
    my $format1b        = $workbook1->add_format();
    
    # Create workbook2
    my $workbook2       = Spreadsheet::WriteExcel->new("workbook2.xls");
    my $worksheet2      = $workbook2->add_worksheet();
    my $format2a        = $workbook2->add_format();
    my $format2b        = $workbook2->add_format();
    
    
    # Create a global format object that isn't tied to a workbook
    my $global_format   = Spreadsheet::WriteExcel::Format->new();
    
    # Set the formatting
    $global_format->set_color('blue');
    $global_format->set_bold();
    $global_format->set_italic();
    
    # Create another example format
    $format1b->set_color('red');
    
    # Copy the global format properties to the worksheet formats
    $format1a->copy($global_format);
    $format2a->copy($global_format);
    
    # Copy a format from worksheet1 to worksheet2
    $format2b->copy($format1b);
    
    # Write some output
    $worksheet1->write(0, 0, "Ciao", $format1a);
    $worksheet1->write(1, 0, "Ciao", $format1b);
    
    $worksheet2->write(0, 0, "Hello", $format2a);
    $worksheet2->write(1, 0, "Hello", $format2b);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/copyformat.pl>

=head2 Example: data_validate.pl



Example of how to add data validation and dropdown lists to a
Spreadsheet::WriteExcel file.

Data validation is a feature of Excel which allows you to restrict the data
that a users enters in a cell and to display help and warning messages. It
also allows you to restrict input to values in a drop down list.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/data_validate.jpg" width="640" height="420" alt="Output from data_validate.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to add data validation and dropdown lists to a
    # Spreadsheet::WriteExcel file.
    #
    # Data validation is a feature of Excel which allows you to restrict the data
    # that a users enters in a cell and to display help and warning messages. It
    # also allows you to restrict input to values in a drop down list.
    #
    # reverse('(c)'), August 2008, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new('data_validate.xls');
    my $worksheet = $workbook->add_worksheet();
    
    # Add a format for the header cells.
    my $header_format = $workbook->add_format(
                                                border      => 1,
                                                bg_color    => 43,
                                                bold        => 1,
                                                text_wrap   => 1,
                                                valign      => 'vcenter',
                                                indent      => 1,
                                             );
    
    # Set up layout of the worksheet.
    $worksheet->set_column('A:A', 64);
    $worksheet->set_column('B:B', 15);
    $worksheet->set_column('D:D', 15);
    $worksheet->set_row(0, 36);
    $worksheet->set_selection('B3');
    
    
    # Write the header cells and some data that will be used in the examples.
    my $row = 0;
    my $txt;
    my $heading1 = 'Some examples of data validation in Spreadsheet::WriteExcel';
    my $heading2 = 'Enter values in this column';
    my $heading3 = 'Sample Data';
    
    $worksheet->write('A1', $heading1, $header_format);
    $worksheet->write('B1', $heading2, $header_format);
    $worksheet->write('D1', $heading3, $header_format);
    
    $worksheet->write('D3', ['Integers',   1, 10]);
    $worksheet->write('D4', ['List data', 'open', 'high', 'close']);
    $worksheet->write('D5', ['Formula',   '=AND(F5=50,G5=60)', 50, 60]);
    
    
    #
    # Example 1. Limiting input to an integer in a fixed range.
    #
    $txt = 'Enter an integer between 1 and 10';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'integer',
            criteria        => 'between',
            minimum         => 1,
            maximum         => 10,
        });
    
    
    #
    # Example 2. Limiting input to an integer outside a fixed range.
    #
    $txt = 'Enter an integer that is not between 1 and 10 (using cell references)';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'integer',
            criteria        => 'not between',
            minimum         => '=E3',
            maximum         => '=F3',
        });
    
    
    #
    # Example 3. Limiting input to an integer greater than a fixed value.
    #
    $txt = 'Enter an integer greater than 0';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'integer',
            criteria        => '>',
            value           => 0,
        });
    
    
    #
    # Example 4. Limiting input to an integer less than a fixed value.
    #
    $txt = 'Enter an integer less than 10';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'integer',
            criteria        => '<',
            value           => 10,
        });
    
    
    #
    # Example 5. Limiting input to a decimal in a fixed range.
    #
    $txt = 'Enter a decimal between 0.1 and 0.5';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'decimal',
            criteria        => 'between',
            minimum         => 0.1,
            maximum         => 0.5,
        });
    
    
    #
    # Example 6. Limiting input to a value in a dropdown list.
    #
    $txt = 'Select a value from a drop down list';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'list',
            source          => ['open', 'high', 'close'],
        });
    
    
    #
    # Example 6. Limiting input to a value in a dropdown list.
    #
    $txt = 'Select a value from a drop down list (using a cell range)';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'list',
            source          => '=E4:G4',
        });
    
    
    #
    # Example 7. Limiting input to a date in a fixed range.
    #
    $txt = 'Enter a date between 1/1/2008 and 12/12/2008';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'date',
            criteria        => 'between',
            minimum         => '2008-01-01T',
            maximum         => '2008-12-12T',
        });
    
    
    #
    # Example 8. Limiting input to a time in a fixed range.
    #
    $txt = 'Enter a time between 6:00 and 12:00';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'time',
            criteria        => 'between',
            minimum         => 'T06:00',
            maximum         => 'T12:00',
        });
    
    
    #
    # Example 9. Limiting input to a string greater than a fixed length.
    #
    $txt = 'Enter a string longer than 3 characters';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'length',
            criteria        => '>',
            value           => 3,
        });
    
    
    #
    # Example 10. Limiting input based on a formula.
    #
    $txt = 'Enter a value if the following is true "=AND(F5=50,G5=60)"';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate        => 'custom',
            value           => '=AND(F5=50,G5=60)',
        });
    
    
    #
    # Example 11. Displaying and modify data validation messages.
    #
    $txt = 'Displays a message when you select the cell';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate      => 'integer',
            criteria      => 'between',
            minimum       => 1,
            maximum       => 100,
            input_title   => 'Enter an integer:',
            input_message => 'between 1 and 100',
        });
    
    
    #
    # Example 12. Displaying and modify data validation messages.
    #
    $txt = 'Display a custom error message when integer isn\'t between 1 and 100';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate      => 'integer',
            criteria      => 'between',
            minimum       => 1,
            maximum       => 100,
            input_title   => 'Enter an integer:',
            input_message => 'between 1 and 100',
            error_title   => 'Input value is not valid!',
            error_message => 'It should be an integer between 1 and 100',
        });
    
    
    #
    # Example 13. Displaying and modify data validation messages.
    #
    $txt = 'Display a custom information message when integer isn\'t between 1 and 100';
    $row += 2;
    
    $worksheet->write($row, 0, $txt);
    $worksheet->data_validation($row, 1,
        {
            validate      => 'integer',
            criteria      => 'between',
            minimum       => 1,
            maximum       => 100,
            input_title   => 'Enter an integer:',
            input_message => 'between 1 and 100',
            error_title   => 'Input value is not valid!',
            error_message => 'It should be an integer between 1 and 100',
            error_type    => 'information',
        });
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/data_validate.pl>

=head2 Example: date_time.pl



Spreadsheet::WriteExcel example of writing dates and times using the
write_date_time() Worksheet method.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/date_time.jpg" width="640" height="420" alt="Output from date_time.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Spreadsheet::WriteExcel example of writing dates and times using the
    # write_date_time() Worksheet method.
    #
    # reverse('(c)'), August 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("date_time.xls");
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format(bold => 1);
    my $row       = 0;
    
    
    # Expand the first column so that the date is visible.
    $worksheet->set_column("A:B", 30);
    
    
    # Write the column headers
    $worksheet->write('A1', 'Formatted date', $bold);
    $worksheet->write('B1', 'Format',         $bold);
    
    
    # Examples date and time formats. In the output file compare how changing
    # the format codes change the appearance of the date.
    #
    my @date_formats = (
        'dd/mm/yy',
        'mm/dd/yy',
        '',
        'd mm yy',
        'dd mm yy',
        '',
        'dd m yy',
        'dd mm yy',
        'dd mmm yy',
        'dd mmmm yy',
        '',
        'dd mm y',
        'dd mm yyy',
        'dd mm yyyy',
        '',
        'd mmmm yyyy',
        '',
        'dd/mm/yy',
        'dd/mm/yy hh:mm',
        'dd/mm/yy hh:mm:ss',
        'dd/mm/yy hh:mm:ss.000',
        '',
        'hh:mm',
        'hh:mm:ss',
        'hh:mm:ss.000',
    );
    
    
    # Write the same date and time using each of the above formats. The empty
    # string formats create a blank line to make the example clearer.
    #
    for my $date_format (@date_formats) {
        $row++;
        next if $date_format eq '';
    
        # Create a format for the date or time.
        my $format =  $workbook->add_format(
                                            num_format => $date_format,
                                            align      => 'left'
                                           );
    
        # Write the same date using different formats.
        $worksheet->write_date_time($row, 0, '2004-08-01T12:30:45.123', $format);
        $worksheet->write          ($row, 1, $date_format);
    }
    
    
    # The following is an example of an invalid date. It is written as a string
    # instead of a number. This is also Excel's default behaviour.
    #
    $row += 2;
    $worksheet->write_date_time($row, 0, '2004-13-01T12:30:45.123');
    $worksheet->write          ($row, 1, 'Invalid date. Written as string.', $bold);
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/date_time.pl>

=head2 Example: defined_name.pl



Example of how to create defined names in a Spreadsheet::WriteExcel file.

This method is used to defined a name that can be used to represent a value,
a single cell or a range of cells in a workbook.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/defined_name.jpg" width="640" height="420" alt="Output from defined_name.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to create defined names in a Spreadsheet::WriteExcel file.
    #
    # This method is used to defined a name that can be used to represent a value,
    # a single cell or a range of cells in a workbook.
    #
    # reverse('(c)'), September 2008, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new('defined_name.xls');
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    
    
    $workbook->define_name('Exchange_rate', '=0.96');
    $workbook->define_name('Sales',         '=Sheet1!$G$1:$H$10');
    $workbook->define_name('Sheet2!Sales',  '=Sheet2!$G$1:$G$10');
    
    
    for my $worksheet ($workbook->sheets()) {
        $worksheet->set_column('A:A', 45);
        $worksheet->write('A2', 'This worksheet contains some defined names,');
        $worksheet->write('A3', 'See the Insert -> Name -> Define dialog.');
    
    }
    
    
    $worksheet1->write('A4', '=Exchange_rate');
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/defined_name.pl>

=head2 Example: diag_border.pl



A simple formatting example that demonstrates how to add a diagonal cell
border with Spreadsheet::WriteExcel



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/diag_border.jpg" width="640" height="420" alt="Output from diag_border.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple formatting example that demonstrates how to add a diagonal cell
    # border with Spreadsheet::WriteExcel
    #
    # reverse('(c)'), May 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new('diag_border.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    my $format1   = $workbook->add_format(diag_type       => '1');
    
    my $format2   = $workbook->add_format(diag_type       => '2');
    
    my $format3   = $workbook->add_format(diag_type       => '3');
    
    my $format4   = $workbook->add_format(
                                          diag_type       => '3',
                                          diag_border     => '7',
                                          diag_color      => 'red',
                                         );
    
    
    $worksheet->write('B3',  'Text', $format1);
    $worksheet->write('B6',  'Text', $format2);
    $worksheet->write('B9',  'Text', $format3);
    $worksheet->write('B12', 'Text', $format4);
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/diag_border.pl>

=head2 Example: easter_egg.pl



This uses the Win32::OLE module to expose the Flight Simulator easter egg
in Excel 97 SR2.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This uses the Win32::OLE module to expose the Flight Simulator easter egg
    # in Excel 97 SR2.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Win32::OLE;
    
    my $application = Win32::OLE->new("Excel.Application");
    my $workbook    = $application->Workbooks->Add;
    my $worksheet   = $workbook->Worksheets(1);
    
    $application->{Visible} = 1;
    
    $worksheet->Range("L97:X97")->Select;
    $worksheet->Range("M97")->Activate;
    
    my $message =  "Hold down Shift and Ctrl and click the ".
                   "Chart Wizard icon on the toolbar.\n\n".
                   "Use the mouse motion and buttons to control ".
                   "movement. Try to find the monolith. ".
                   "Close this dialog first.";
    
    $application->InputBox($message);


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/easter_egg.pl>

=head2 Example: filehandle.pl



Example of using Spreadsheet::WriteExcel to write Excel files to
different filehandles.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/filehandle.jpg" width="640" height="420" alt="Output from filehandle.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of using Spreadsheet::WriteExcel to write Excel files to
    # different filehandles.
    #
    # reverse('(c)'), April 2003, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    use IO::Scalar;
    
    
    
    
    ###############################################################################
    #
    # Example 1. This demonstrates the standard way of creating an Excel file by
    # specifying a file name.
    #
    
    my $workbook1  = Spreadsheet::WriteExcel->new('fh_01.xls');
    my $worksheet1 = $workbook1->add_worksheet();
    
    $worksheet1->write(0, 0,  "Hi Excel!");
    
    
    
    
    ###############################################################################
    #
    # Example 2. Write an Excel file to an existing filehandle.
    #
    
    open    TEST, "> fh_02.xls" or die "Couldn't open file: $!";
    binmode TEST; # Always do this regardless of whether the platform requires it.
    
    my $workbook2  = Spreadsheet::WriteExcel->new(\*TEST);
    my $worksheet2 = $workbook2->add_worksheet();
    
    $worksheet2->write(0, 0,  "Hi Excel!");
    
    
    
    
    ###############################################################################
    #
    # Example 3. Write an Excel file to an existing OO style filehandle.
    #
    
    my $fh = FileHandle->new("> fh_03.xls")
             or die "Couldn't open file: $!";
    
    binmode($fh);
    
    my $workbook3  = Spreadsheet::WriteExcel->new($fh);
    my $worksheet3 = $workbook3->add_worksheet();
    
    $worksheet3->write(0, 0,  "Hi Excel!");
    
    
    
    
    ###############################################################################
    #
    # Example 4. Write an Excel file to a string via IO::Scalar. Please refer to
    # the IO::Scalar documentation for further details.
    #
    
    my $xls_str;
    
    tie *XLS, 'IO::Scalar', \$xls_str;
    
    my $workbook4  = Spreadsheet::WriteExcel->new(\*XLS);
    my $worksheet4 = $workbook4->add_worksheet();
    
    $worksheet4->write(0, 0, "Hi Excel 4");
    $workbook4->close(); # This is required before we use the scalar
    
    
    # The Excel file is now in $xls_str. As a demonstration, print it to a file.
    open    TMP, "> fh_04.xls" or die "Couldn't open file: $!";
    binmode TMP;
    print   TMP  $xls_str;
    close   TMP;
    
    
    
    
    ###############################################################################
    #
    # Example 5. Write an Excel file to a string via IO::Scalar's newer interface.
    # Please refer to the IO::Scalar documentation for further details.
    #
    my $xls_str2;
    
    my $fh5 = IO::Scalar->new(\$xls_str2);
    
    
    my $workbook5  = Spreadsheet::WriteExcel->new($fh5);
    my $worksheet5 = $workbook5->add_worksheet();
    
    $worksheet5->write(0, 0, "Hi Excel 5");
    $workbook5->close(); # This is required before we use the scalar
    
    # The Excel file is now in $xls_str. As a demonstration, print it to a file.
    open    TMP, "> fh_05.xls" or die "Couldn't open file: $!";
    binmode TMP;
    print   TMP  $xls_str2;
    close   TMP;
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/filehandle.pl>

=head2 Example: formula_result.pl



Example of how to write Spreadsheet::WriteExcel formulas with a user
specified result.

This is generally only required when writing a spreadsheet for an
application other than Excel where the formula isn't evaluated.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/formula_result.jpg" width="640" height="420" alt="Output from formula_result.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to write Spreadsheet::WriteExcel formulas with a user
    # specified result.
    #
    # This is generally only required when writing a spreadsheet for an
    # application other than Excel where the formula isn't evaluated.
    #
    # reverse('(c)'), August 2005, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new('formula_result.xls');
    my $worksheet = $workbook->add_worksheet();
    my $format    = $workbook->add_format(color => 'blue');
    
    
    $worksheet->write('A1', '=1+2');
    $worksheet->write('A2', '=1+2',                     $format, 4);
    $worksheet->write('A3', '="ABC"',                   undef,   'DEF');
    $worksheet->write('A4', '=IF(A1 > 1, TRUE, FALSE)', undef,   'TRUE');
    $worksheet->write('A5', '=1/0',                     undef,   '#DIV/0!');
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/formula_result.pl>

=head2 Example: headers.pl



This program shows several examples of how to set up headers and
footers with Spreadsheet::WriteExcel.

The control characters used in the header/footer strings are:

    Control             Category            Description
    =======             ========            ===========
    &L                  Justification       Left
    &C                                      Center
    &R                                      Right

    &P                  Information         Page number
    &N                                      Total number of pages
    &D                                      Date
    &T                                      Time
    &F                                      File name
    &A                                      Worksheet name

    &fontsize           Font                Font size
    &"font,style"                           Font name and style
    &U                                      Single underline
    &E                                      Double underline
    &S                                      Strikethrough
    &X                                      Superscript
    &Y                                      Subscript

    &&                  Miscellaneous       Literal ampersand &

See the main Spreadsheet::WriteExcel documentation for more information.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/headers.jpg" width="640" height="420" alt="Output from headers.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ######################################################################
    #
    # This program shows several examples of how to set up headers and
    # footers with Spreadsheet::WriteExcel.
    #
    # The control characters used in the header/footer strings are:
    #
    #     Control             Category            Description
    #     =======             ========            ===========
    #     &L                  Justification       Left
    #     &C                                      Center
    #     &R                                      Right
    #
    #     &P                  Information         Page number
    #     &N                                      Total number of pages
    #     &D                                      Date
    #     &T                                      Time
    #     &F                                      File name
    #     &A                                      Worksheet name
    #
    #     &fontsize           Font                Font size
    #     &"font,style"                           Font name and style
    #     &U                                      Single underline
    #     &E                                      Double underline
    #     &S                                      Strikethrough
    #     &X                                      Superscript
    #     &Y                                      Subscript
    #
    #     &&                  Miscellaneous       Literal ampersand &
    #
    # See the main Spreadsheet::WriteExcel documentation for more information.
    #
    # reverse('(c)'), March 2002, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("headers.xls");
    my $preview   = "Select Print Preview to see the header and footer";
    
    
    ######################################################################
    #
    # A simple example to start
    #
    my $worksheet1  = $workbook->add_worksheet('Simple');
    
    my $header1     = '&CHere is some centred text.';
    
    my $footer1     = '&LHere is some left aligned text.';
    
    
    $worksheet1->set_header($header1);
    $worksheet1->set_footer($footer1);
    
    $worksheet1->set_column('A:A', 50);
    $worksheet1->write('A1', $preview);
    
    
    
    
    ######################################################################
    #
    # This is an example of some of the header/footer variables.
    #
    my $worksheet2  = $workbook->add_worksheet('Variables');
    
    my $header2     = '&LPage &P of &N'.
                      '&CFilename: &F' .
                      '&RSheetname: &A';
    
    my $footer2     = '&LCurrent date: &D'.
                      '&RCurrent time: &T';
    
    
    
    $worksheet2->set_header($header2);
    $worksheet2->set_footer($footer2);
    
    
    $worksheet2->set_column('A:A', 50);
    $worksheet2->write('A1', $preview);
    $worksheet2->write('A21', "Next sheet");
    $worksheet2->set_h_pagebreaks(20);
    
    
    
    ######################################################################
    #
    # This example shows how to use more than one font
    #
    my $worksheet3 = $workbook->add_worksheet('Mixed fonts');
    
    my $header3    = '&C' .
                     '&"Courier New,Bold"Hello ' .
                     '&"Arial,Italic"World';
    
    my $footer3    = '&C' .
                     '&"Symbol"e' .
                     '&"Arial" = mc&X2';
    
    $worksheet3->set_header($header3);
    $worksheet3->set_footer($footer3);
    
    $worksheet3->set_column('A:A', 50);
    $worksheet3->write('A1', $preview);
    
    
    
    
    ######################################################################
    #
    # Example of line wrapping
    #
    my $worksheet4 = $workbook->add_worksheet('Word wrap');
    
    my $header4    = "&CHeading 1\nHeading 2\nHeading 3";
    
    $worksheet4->set_header($header4);
    
    $worksheet4->set_column('A:A', 50);
    $worksheet4->write('A1', $preview);
    
    
    
    
    ######################################################################
    #
    # Example of inserting a literal ampersand &
    #
    my $worksheet5 = $workbook->add_worksheet('Ampersand');
    
    my $header5    = "&CCuriouser && Curiouser - Attorneys at Law";
    
    $worksheet5->set_header($header5);
    
    $worksheet5->set_column('A:A', 50);
    $worksheet5->write('A1', $preview);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/headers.pl>

=head2 Example: hide_sheet.pl



Example of how to hide a worksheet with Spreadsheet::WriteExcel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/hide_sheet.jpg" width="640" height="420" alt="Output from hide_sheet.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to hide a worksheet with Spreadsheet::WriteExcel.
    #
    # reverse('(c)'), April 2005, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new('hidden.xls');
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    my $worksheet3 = $workbook->add_worksheet();
    
    # Sheet2 won't be visible until it is unhidden in Excel.
    $worksheet2->hide();
    
    $worksheet1->write(0, 0, 'Sheet2 is hidden');
    $worksheet2->write(0, 0, 'How did you find me?');
    $worksheet3->write(0, 0, 'Sheet2 is hidden');
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/hide_sheet.pl>

=head2 Example: hyperlink1.pl



Example of how to use the WriteExcel module to write hyperlinks.

See also hyperlink2.pl for worksheet URL examples.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/hyperlink1.jpg" width="640" height="420" alt="Output from hyperlink1.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the WriteExcel module to write hyperlinks.
    #
    # See also hyperlink2.pl for worksheet URL examples.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("hyperlink.xls");
    my $worksheet = $workbook->add_worksheet('Hyperlinks');
    
    # Format the first column
    $worksheet->set_column('A:A', 30);
    $worksheet->set_selection('B1');
    
    
    # Add a sample format
    my $format = $workbook->add_format();
    $format->set_size(12);
    $format->set_bold();
    $format->set_color('red');
    $format->set_underline();
    
    
    # Write some hyperlinks
    $worksheet->write('A1', 'http://www.perl.com/'                );
    $worksheet->write('A3', 'http://www.perl.com/', 'Perl home'   );
    $worksheet->write('A5', 'http://www.perl.com/', undef, $format);
    $worksheet->write('A7', 'mailto:jmcnamara@cpan.org', 'Mail me');
    
    # Write a URL that isn't a hyperlink
    $worksheet->write_string('A9', 'http://www.perl.com/');
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/hyperlink1.pl>

=head2 Example: hyperlink2.pl



Example of how to use the WriteExcel module to write internal and internal
hyperlinks.

If you wish to run this program and follow the hyperlinks you should create
the following directory structure:

    C:\ -- Temp --+-- Europe
                  |
                  \-- Asia


See also hyperlink1.pl for web URL examples.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the WriteExcel module to write internal and internal
    # hyperlinks.
    #
    # If you wish to run this program and follow the hyperlinks you should create
    # the following directory structure:
    #
    #     C:\ -- Temp --+-- Europe
    #                   |
    #                   \-- Asia
    #
    #
    # See also hyperlink1.pl for web URL examples.
    #
    # reverse('(c)'), February 2002, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create three workbooks:
    #   C:\Temp\Europe\Ireland.xls
    #   C:\Temp\Europe\Italy.xls
    #   C:\Temp\Asia\China.xls
    #
    my $ireland   = Spreadsheet::WriteExcel->new('C:\Temp\Europe\Ireland.xls');
    my $ire_links = $ireland->add_worksheet('Links');
    my $ire_sales = $ireland->add_worksheet('Sales');
    my $ire_data  = $ireland->add_worksheet('Product Data');
    
    my $italy     = Spreadsheet::WriteExcel->new('C:\Temp\Europe\Italy.xls');
    my $ita_links = $italy->add_worksheet('Links');
    my $ita_sales = $italy->add_worksheet('Sales');
    my $ita_data  = $italy->add_worksheet('Product Data');
    
    my $china     = Spreadsheet::WriteExcel->new('C:\Temp\Asia\China.xls');
    my $cha_links = $china->add_worksheet('Links');
    my $cha_sales = $china->add_worksheet('Sales');
    my $cha_data  = $china->add_worksheet('Product Data');
    
    # Add a format
    my $format = $ireland->add_format(color => 'green', bold => 1);
    $ire_links->set_column('A:B', 25);
    
    
    ###############################################################################
    #
    # Examples of internal links
    #
    $ire_links->write('A1', 'Internal links', $format);
    
    # Internal link
    $ire_links->write('A2', 'internal:Sales!A2');
    
    # Internal link to a range
    $ire_links->write('A3', 'internal:Sales!A3:D3');
    
    # Internal link with an alternative string
    $ire_links->write('A4', 'internal:Sales!A4', 'Link');
    
    # Internal link with a format
    $ire_links->write('A5', 'internal:Sales!A5', $format);
    
    # Internal link with an alternative string and format
    $ire_links->write('A6', 'internal:Sales!A6', 'Link', $format);
    
    # Internal link (spaces in worksheet name)
    $ire_links->write('A7', q{internal:'Product Data'!A7});
    
    
    ###############################################################################
    #
    # Examples of external links
    #
    $ire_links->write('B1', 'External links', $format);
    
    # External link to a local file
    $ire_links->write('B2', 'external:Italy.xls');
    
    # External link to a local file with worksheet
    $ire_links->write('B3', 'external:Italy.xls#Sales!B3');
    
    # External link to a local file with worksheet and alternative string
    $ire_links->write('B4', 'external:Italy.xls#Sales!B4', 'Link');
    
    # External link to a local file with worksheet and format
    $ire_links->write('B5', 'external:Italy.xls#Sales!B5', $format);
    
    # External link to a remote file, absolute path
    $ire_links->write('B6', 'external:c:/Temp/Asia/China.xls');
    
    # External link to a remote file, relative path
    $ire_links->write('B7', 'external:../Asia/China.xls');
    
    # External link to a remote file with worksheet
    $ire_links->write('B8', 'external:c:/Temp/Asia/China.xls#Sales!B8');
    
    # External link to a remote file with worksheet (with spaces in the name)
    $ire_links->write('B9', q{external:c:/Temp/Asia/China.xls#'Product Data'!B9});
    
    
    ###############################################################################
    #
    # Some utility links to return to the main sheet
    #
    $ire_sales->write('A2', 'internal:Links!A2', 'Back');
    $ire_sales->write('A3', 'internal:Links!A3', 'Back');
    $ire_sales->write('A4', 'internal:Links!A4', 'Back');
    $ire_sales->write('A5', 'internal:Links!A5', 'Back');
    $ire_sales->write('A6', 'internal:Links!A6', 'Back');
    $ire_data-> write('A7', 'internal:Links!A7', 'Back');
    
    $ita_links->write('A1', 'external:Ireland.xls#Links!B2', 'Back');
    $ita_sales->write('B3', 'external:Ireland.xls#Links!B3', 'Back');
    $ita_sales->write('B4', 'external:Ireland.xls#Links!B4', 'Back');
    $ita_sales->write('B5', 'external:Ireland.xls#Links!B5', 'Back');
    $cha_links->write('A1', 'external:../Europe/Ireland.xls#Links!B6', 'Back');
    $cha_sales->write('B8', 'external:../Europe/Ireland.xls#Links!B8', 'Back');
    $cha_data-> write('B9', 'external:../Europe/Ireland.xls#Links!B9', 'Back');
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/hyperlink2.pl>

=head2 Example: images.pl



Example of how to insert images into an Excel worksheet using the
Spreadsheet::WriteExcel insert_image() method.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/images.jpg" width="640" height="420" alt="Output from images.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to insert images into an Excel worksheet using the
    # Spreadsheet::WriteExcel insert_image() method.
    #
    # reverse('(c)'), October 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook called simple.xls and add a worksheet
    my $workbook   = Spreadsheet::WriteExcel->new("images.xls");
    my $worksheet1 = $workbook->add_worksheet('Image 1');
    my $worksheet2 = $workbook->add_worksheet('Image 2');
    my $worksheet3 = $workbook->add_worksheet('Image 3');
    my $worksheet4 = $workbook->add_worksheet('Image 4');
    
    # Insert a basic image
    $worksheet1->write('A10', "Image inserted into worksheet.");
    $worksheet1->insert_image('A1', 'republic.png');
    
    
    # Insert an image with an offset
    $worksheet2->write('A10', "Image inserted with an offset.");
    $worksheet2->insert_image('A1', 'republic.png', 32, 10);
    
    # Insert a scaled image
    $worksheet3->write('A10', "Image scaled: width x 2, height x 0.8.");
    $worksheet3->insert_image('A1', 'republic.png', 0, 0, 2, 0.8);
    
    # Insert an image over varied column and row sizes.
    $worksheet4->set_column('A:A', 5);
    $worksheet4->set_column('B:B', undef, undef, 1); # Hidden
    $worksheet4->set_column('C:D', 10);
    $worksheet4->set_row(0, 30);
    $worksheet4->set_row(3, 5);
    
    $worksheet4->write('A10', "Image inserted over scaled rows and columns.");
    $worksheet4->insert_image('A1', 'republic.png');
    
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/images.pl>

=head2 Example: indent.pl



A simple formatting example using Spreadsheet::WriteExcel.

This program demonstrates the indentation cell format.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/indent.jpg" width="640" height="420" alt="Output from indent.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple formatting example using Spreadsheet::WriteExcel.
    #
    # This program demonstrates the indentation cell format.
    #
    # reverse('(c)'), May 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new('indent.xls');
    
    my $worksheet = $workbook->add_worksheet();
    my $indent1   = $workbook->add_format(indent => 1);
    my $indent2   = $workbook->add_format(indent => 2);
    
    $worksheet->set_column('A:A', 40);
    
    
    $worksheet->write('A1', "This text is indented 1 level",  $indent1);
    $worksheet->write('A2', "This text is indented 2 levels", $indent2);
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/indent.pl>

=head2 Example: merge1.pl



Simple example of merging cells using the Spreadsheet::WriteExcel module.

This example merges three cells using the "Centre Across Selection"
alignment which was the Excel 5 method of achieving a merge. For a more
modern approach use the merge_range() worksheet method instead.
See the merge3.pl - merge6.pl programs.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge1.jpg" width="640" height="420" alt="Output from merge1.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Simple example of merging cells using the Spreadsheet::WriteExcel module.
    #
    # This example merges three cells using the "Centre Across Selection"
    # alignment which was the Excel 5 method of achieving a merge. For a more
    # modern approach use the merge_range() worksheet method instead.
    # See the merge3.pl - merge6.pl programs.
    #
    # reverse('(c)'), August 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("merge1.xls");
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_column('B:D', 20);
    $worksheet->set_row(2, 30);
    
    
    # Create a merge format
    my $format = $workbook->add_format(center_across => 1);
    
    
    # Only one cell should contain text, the others should be blank.
    $worksheet->write      (2, 1, "Center across selection", $format);
    $worksheet->write_blank(2, 2,                 $format);
    $worksheet->write_blank(2, 3,                 $format);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge1.pl>

=head2 Example: merge2.pl



Simple example of merging cells using the Spreadsheet::WriteExcel module

This example merges three cells using the "Centre Across Selection"
alignment which was the Excel 5 method of achieving a merge. For a more
modern approach use the merge_range() worksheet method instead.
See the merge3.pl - merge6.pl programs.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge2.jpg" width="640" height="420" alt="Output from merge2.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Simple example of merging cells using the Spreadsheet::WriteExcel module
    #
    # This example merges three cells using the "Centre Across Selection"
    # alignment which was the Excel 5 method of achieving a merge. For a more
    # modern approach use the merge_range() worksheet method instead.
    # See the merge3.pl - merge6.pl programs.
    #
    # reverse('(c)'), August 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("merge2.xls");
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_column(1, 2, 30);
    $worksheet->set_row(2, 40);
    
    
    # Create a merged format
    my $format = $workbook->add_format(
                                            center_across   => 1,
                                            bold            => 1,
                                            size            => 15,
                                            pattern         => 1,
                                            border          => 6,
                                            color           => 'white',
                                            fg_color        => 'green',
                                            border_color    => 'yellow',
                                            align           => 'vcenter',
                                      );
    
    
    # Only one cell should contain text, the others should be blank.
    $worksheet->write      (2, 1, "Center across selection", $format);
    $worksheet->write_blank(2, 2,                            $format);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge2.pl>

=head2 Example: merge3.pl



Example of how to use Spreadsheet::WriteExcel to write a hyperlink in a
merged cell. There are two options write_url_range() with a standard merge
format or merge_range().



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge3.jpg" width="640" height="420" alt="Output from merge3.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use Spreadsheet::WriteExcel to write a hyperlink in a
    # merged cell. There are two options write_url_range() with a standard merge
    # format or merge_range().
    #
    # reverse('(c)'), September 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('merge3.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_row($_, 30) for (1, 3, 6, 7);
    $worksheet->set_column('B:D', 20);
    
    
    ###############################################################################
    #
    # Example 1: Merge cells containing a hyperlink using write_url_range()
    # and the standard Excel 5+ merge property.
    #
    my $format1 = $workbook->add_format(
                                        center_across   => 1,
                                        border          => 1,
                                        underline       => 1,
                                        color           => 'blue',
                                     );
    
    # Write the cells to be merged
    $worksheet->write_url_range('B2:D2', 'http://www.perl.com', $format1);
    $worksheet->write_blank('C2', $format1);
    $worksheet->write_blank('D2', $format1);
    
    
    
    ###############################################################################
    #
    # Example 2: Merge cells containing a hyperlink using merge_range().
    #
    my $format2 = $workbook->add_format(
                                        border      => 1,
                                        underline   => 1,
                                        color       => 'blue',
                                        align       => 'center',
                                        valign      => 'vcenter',
                                      );
    
    # Merge 3 cells
    $worksheet->merge_range('B4:D4', 'http://www.perl.com', $format2);
    
    
    # Merge 3 cells over two rows
    $worksheet->merge_range('B7:D8', 'http://www.perl.com', $format2);
    
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge3.pl>

=head2 Example: merge4.pl



Example of how to use the Spreadsheet::WriteExcel merge_range() workbook
method with complex formatting.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge4.jpg" width="640" height="420" alt="Output from merge4.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel merge_range() workbook
    # method with complex formatting.
    #
    # reverse('(c)'), September 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('merge4.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_row($_, 30) for (1..11);
    $worksheet->set_column('B:D', 20);
    
    
    ###############################################################################
    #
    # Example 1: Text centered vertically and horizontally
    #
    my $format1 = $workbook->add_format(
                                        border  => 6,
                                        bold    => 1,
                                        color   => 'red',
                                        valign  => 'vcenter',
                                        align   => 'center',
                                       );
    
    
    
    $worksheet->merge_range('B2:D3', 'Vertical and horizontal', $format1);
    
    
    ###############################################################################
    #
    # Example 2: Text aligned to the top and left
    #
    my $format2 = $workbook->add_format(
                                        border  => 6,
                                        bold    => 1,
                                        color   => 'red',
                                        valign  => 'top',
                                        align   => 'left',
                                      );
    
    
    
    $worksheet->merge_range('B5:D6', 'Aligned to the top and left', $format2);
    
    
    ###############################################################################
    #
    # Example 3:  Text aligned to the bottom and right
    #
    my $format3 = $workbook->add_format(
                                        border  => 6,
                                        bold    => 1,
                                        color   => 'red',
                                        valign  => 'bottom',
                                        align   => 'right',
                                      );
    
    
    
    $worksheet->merge_range('B8:D9', 'Aligned to the bottom and right', $format3);
    
    
    ###############################################################################
    #
    # Example 4:  Text justified (i.e. wrapped) in the cell
    #
    my $format4 = $workbook->add_format(
                                        border  => 6,
                                        bold    => 1,
                                        color   => 'red',
                                        valign  => 'top',
                                        align   => 'justify',
                                      );
    
    
    
    $worksheet->merge_range('B11:D12', 'Justified: '.'so on and ' x18, $format4);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge4.pl>

=head2 Example: merge5.pl



Example of how to use the Spreadsheet::WriteExcel merge_cells() workbook
method with complex formatting and rotation.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge5.jpg" width="640" height="420" alt="Output from merge5.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel merge_cells() workbook
    # method with complex formatting and rotation.
    #
    #
    # reverse('(c)'), September 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('merge5.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_row($_, 36)         for (3..8);
    $worksheet->set_column($_, $_ , 15) for (1,3,5);
    
    
    ###############################################################################
    #
    # Rotation 1, letters run from top to bottom
    #
    my $format1 = $workbook->add_format(
                                        border      => 6,
                                        bold        => 1,
                                        color       => 'red',
                                        valign      => 'vcentre',
                                        align       => 'centre',
                                        rotation    => 270,
                                      );
    
    
    $worksheet->merge_range('B4:B9', 'Rotation 270', $format1);
    
    
    ###############################################################################
    #
    # Rotation 2, 90 deg anticlockwise
    #
    my $format2 = $workbook->add_format(
                                        border      => 6,
                                        bold        => 1,
                                        color       => 'red',
                                        valign      => 'vcentre',
                                        align       => 'centre',
                                        rotation    => 90,
                                      );
    
    
    $worksheet->merge_range('D4:D9', 'Rotation 90 deg', $format2);
    
    
    
    ###############################################################################
    #
    # Rotation 3, 90 deg clockwise
    #
    my $format3 = $workbook->add_format(
                                        border      => 6,
                                        bold        => 1,
                                        color       => 'red',
                                        valign      => 'vcentre',
                                        align       => 'centre',
                                        rotation    => -90,
                                      );
    
    
    $worksheet->merge_range('F4:F9', 'Rotation -90 deg', $format3);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge5.pl>

=head2 Example: merge6.pl



Example of how to use the Spreadsheet::WriteExcel merge_cells() workbook
method with Unicode strings.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/merge6.jpg" width="640" height="420" alt="Output from merge6.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel merge_cells() workbook
    # method with Unicode strings.
    #
    #
    # reverse('(c)'), December 2005, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('merge6.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Increase the cell size of the merged cells to highlight the formatting.
    $worksheet->set_row($_, 36) for 2..9;
    $worksheet->set_column('B:D', 25);
    
    
    # Format for the merged cells.
    my $format = $workbook->add_format(
                                        border      => 6,
                                        bold        => 1,
                                        color       => 'red',
                                        size        => 20,
                                        valign      => 'vcentre',
                                        align       => 'left',
                                        indent      => 1,
                                      );
    
    
    
    
    ###############################################################################
    #
    # Write an Ascii string.
    #
    
    $worksheet->merge_range('B3:D4', 'ASCII: A simple string', $format);
    
    
    
    
    ###############################################################################
    #
    # Write a UTF-16 Unicode string.
    #
    
    # A phrase in Cyrillic encoded as UTF-16BE.
    my $utf16_str = pack "H*", '005500540046002d00310036003a0020'.
                               '042d0442043e002004440440043004370430002004'.
                               '3d043000200440044304410441043a043e043c0021';
    
    # Note the extra parameter at the end to indicate UTF-16 encoding.
    $worksheet->merge_range('B6:D7', $utf16_str, $format, 1);
    
    
    
    
    ###############################################################################
    #
    # Write a UTF-8 Unicode string.
    #
    
    if ($] >= 5.008) {
        my $smiley = chr 0x263a;
        $worksheet->merge_range('B9:D10', "UTF-8: A Unicode smiley $smiley",
                                           $format);
    }
    else {
        $worksheet->merge_range('B9:D10', "UTF-8: Requires Perl 5.8", $format);
    }
    
    
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/merge6.pl>

=head2 Example: mod_perl1.pl



Example of how to use the Spreadsheet::WriteExcel module to send an Excel
file to a browser using mod_perl 1 and Apache

This module ties *XLS directly to Apache, and with the correct
content-disposition/types it will prompt the user to save
the file, or open it at this location.

This script is a modification of the Spreadsheet::WriteExcel cgi.pl example.

Change the name of this file to Cgi.pm.
Change the package location to where ever you locate this package.
In the example below it is located in the WriteExcel directory.

Your httpd.conf entry for this module, should you choose to use it
as a stand alone app, should look similar to the following:

    <Location /spreadsheet-test>
      SetHandler perl-script
      PerlHandler Spreadsheet::WriteExcel::Cgi
      PerlSendHeader On
    </Location>

The PerlHandler name above and the package name below *have* to match.

    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel module to send an Excel
    # file to a browser using mod_perl 1 and Apache
    #
    # This module ties *XLS directly to Apache, and with the correct
    # content-disposition/types it will prompt the user to save
    # the file, or open it at this location.
    #
    # This script is a modification of the Spreadsheet::WriteExcel cgi.pl example.
    #
    # Change the name of this file to Cgi.pm.
    # Change the package location to where ever you locate this package.
    # In the example below it is located in the WriteExcel directory.
    #
    # Your httpd.conf entry for this module, should you choose to use it
    # as a stand alone app, should look similar to the following:
    #
    #     <Location /spreadsheet-test>
    #       SetHandler perl-script
    #       PerlHandler Spreadsheet::WriteExcel::Cgi
    #       PerlSendHeader On
    #     </Location>
    #
    # The PerlHandler name above and the package name below *have* to match.
    
    # Apr 2001, Thomas Sullivan, webmaster@860.org
    # Feb 2001, John McNamara, jmcnamara@cpan.org
    
    package Spreadsheet::WriteExcel::Cgi;
    
    ##########################################
    # Pragma Definitions
    ##########################################
    use strict;
    
    ##########################################
    # Required Modules
    ##########################################
    use Apache::Constants qw(:common);
    use Apache::Request;
    use Apache::URI; # This may not be needed
    use Spreadsheet::WriteExcel;
    
    ##########################################
    # Main App Body
    ##########################################
    sub handler {
        # New apache object
        # Should you decide to use it.
        my $r = Apache::Request->new(shift);
    
        # Set the filename and send the content type
        # This will appear when they save the spreadsheet
        my $filename ="cgitest.xls";
    
        ####################################################
        ## Send the content type headers
        ####################################################
        print "Content-disposition: attachment;filename=$filename\n";
        print "Content-type: application/vnd.ms-excel\n\n";
    
        ####################################################
        # Tie a filehandle to Apache's STDOUT.
        # Create a new workbook and add a worksheet.
        ####################################################
        tie *XLS => 'Apache';
        binmode(*XLS);
    
        my $workbook  = Spreadsheet::WriteExcel->new(\*XLS);
        my $worksheet = $workbook->add_worksheet();
    
    
        # Set the column width for column 1
        $worksheet->set_column(0, 0, 20);
    
    
        # Create a format
        my $format = $workbook->add_format();
        $format->set_bold();
        $format->set_size(15);
        $format->set_color('blue');
    
    
        # Write to the workbook
        $worksheet->write(0, 0, "Hi Excel!", $format);
    
        # You must close the workbook for Content-disposition
        $workbook->close();
    }
    
    1;


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/mod_perl1.pl>

=head2 Example: mod_perl2.pl



Example of how to use the Spreadsheet::WriteExcel module to send an Excel
file to a browser using mod_perl 2 and Apache.

This module ties *XLS directly to Apache, and with the correct
content-disposition/types it will prompt the user to save
the file, or open it at this location.

This script is a modification of the Spreadsheet::WriteExcel cgi.pl example.

Change the name of this file to MP2Test.pm.
Change the package location to where ever you locate this package.
In the example below it is located in the WriteExcel directory.

Your httpd.conf entry for this module, should you choose to use it
as a stand alone app, should look similar to the following:

    PerlModule Apache2::RequestRec
    PerlModule APR::Table
    PerlModule Apache2::RequestIO

    <Location /spreadsheet-test>
       SetHandler perl-script
       PerlResponseHandler Spreadsheet::WriteExcel::MP2Test
    </Location>

The PerlResponseHandler must match the package name below.

    ###############################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel module to send an Excel
    # file to a browser using mod_perl 2 and Apache.
    #
    # This module ties *XLS directly to Apache, and with the correct
    # content-disposition/types it will prompt the user to save
    # the file, or open it at this location.
    #
    # This script is a modification of the Spreadsheet::WriteExcel cgi.pl example.
    #
    # Change the name of this file to MP2Test.pm.
    # Change the package location to where ever you locate this package.
    # In the example below it is located in the WriteExcel directory.
    #
    # Your httpd.conf entry for this module, should you choose to use it
    # as a stand alone app, should look similar to the following:
    #
    #     PerlModule Apache2::RequestRec
    #     PerlModule APR::Table
    #     PerlModule Apache2::RequestIO
    #
    #     <Location /spreadsheet-test>
    #        SetHandler perl-script
    #        PerlResponseHandler Spreadsheet::WriteExcel::MP2Test
    #     </Location>
    #
    # The PerlResponseHandler must match the package name below.
    
    # Jun 2004, Matisse Enzer, matisse@matisse.net  (mod_perl 2 version)
    # Apr 2001, Thomas Sullivan, webmaster@860.org
    # Feb 2001, John McNamara, jmcnamara@cpan.org
    
    package Spreadsheet::WriteExcel::MP2Test;
    
    ##########################################
    # Pragma Definitions
    ##########################################
    use strict;
    
    ##########################################
    # Required Modules
    ##########################################
    use Apache2::Const -compile => qw( :common );
    use Spreadsheet::WriteExcel;
    
    ##########################################
    # Main App Body
    ##########################################
    sub handler {
        my($r) = @_;  # Apache request object is passed to handler in mod_perl 2
    
        # Set the filename and send the content type
        # This will appear when they save the spreadsheet
        my $filename ="mod_perl2_test.xls";
    
        ####################################################
        ## Send the content type headers the mod_perl 2 way
        ####################################################
        $r->headers_out->{'Content-Disposition'} = "attachment;filename=$filename";
        $r->content_type('application/vnd.ms-excel');
    
        ####################################################
        # Tie a filehandle to Apache's STDOUT.
        # Create a new workbook and add a worksheet.
        ####################################################
        tie *XLS => $r;  # The mod_perl 2 way. Tie to the Apache::RequestRec object
        binmode(*XLS);
    
        my $workbook  = Spreadsheet::WriteExcel->new(\*XLS);
        my $worksheet = $workbook->add_worksheet();
    
    
        # Set the column width for column 1
        $worksheet->set_column(0, 0, 20);
    
    
        # Create a format
        my $format = $workbook->add_format();
        $format->set_bold();
        $format->set_size(15);
        $format->set_color('blue');
    
    
        # Write to the workbook
        $worksheet->write(0, 0, 'Hi Excel! from ' . $r->hostname , $format);
    
        # You must close the workbook for Content-disposition
        $workbook->close();
        return Apache2::Const::OK;
    }
    
    1;


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/mod_perl2.pl>

=head2 Example: outline.pl



Example of how use Spreadsheet::WriteExcel to generate Excel outlines and
grouping.


Excel allows you to group rows or columns so that they can be hidden or
displayed with a single mouse click. This feature is referred to as outlines.

Outlines can reduce complex data down to a few salient sub-totals or 
summaries.

This feature is best viewed in Excel but the following is an ASCII
representation of what a worksheet with three outlines might look like.
Rows 3-4 and rows 7-8 are grouped at level 2. Rows 2-9 are grouped at
level 1. The lines at the left hand side are called outline level bars.


            ------------------------------------------
     1 2 3 |   |   A   |   B   |   C   |   D   |  ...
            ------------------------------------------
      _    | 1 |   A   |       |       |       |  ...
     |  _  | 2 |   B   |       |       |       |  ...
     | |   | 3 |  (C)  |       |       |       |  ...
     | |   | 4 |  (D)  |       |       |       |  ...
     | -   | 5 |   E   |       |       |       |  ...
     |  _  | 6 |   F   |       |       |       |  ...
     | |   | 7 |  (G)  |       |       |       |  ...
     | |   | 8 |  (H)  |       |       |       |  ...
     | -   | 9 |   I   |       |       |       |  ...
     -     | . |  ...  |  ...  |  ...  |  ...  |  ...


Clicking the minus sign on each of the level 2 outlines will collapse and
hide the data as shown in the next figure. The minus sign changes to a plus
sign to indicate that the data in the outline is hidden.

            ------------------------------------------
     1 2 3 |   |   A   |   B   |   C   |   D   |  ...
            ------------------------------------------
      _    | 1 |   A   |       |       |       |  ...
     |     | 2 |   B   |       |       |       |  ...
     | +   | 5 |   E   |       |       |       |  ...
     |     | 6 |   F   |       |       |       |  ...
     | +   | 9 |   I   |       |       |       |  ...
     -     | . |  ...  |  ...  |  ...  |  ...  |  ...


Clicking on the minus sign on the level 1 outline will collapse the remaining
rows as follows:

            ------------------------------------------
     1 2 3 |   |   A   |   B   |   C   |   D   |  ...
            ------------------------------------------
           | 1 |   A   |       |       |       |  ...
     +     | . |  ...  |  ...  |  ...  |  ...  |  ...

See the main Spreadsheet::WriteExcel documentation for more information.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/outline.jpg" width="640" height="420" alt="Output from outline.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how use Spreadsheet::WriteExcel to generate Excel outlines and
    # grouping.
    #
    #
    # Excel allows you to group rows or columns so that they can be hidden or
    # displayed with a single mouse click. This feature is referred to as outlines.
    #
    # Outlines can reduce complex data down to a few salient sub-totals or 
    # summaries.
    #
    # This feature is best viewed in Excel but the following is an ASCII
    # representation of what a worksheet with three outlines might look like.
    # Rows 3-4 and rows 7-8 are grouped at level 2. Rows 2-9 are grouped at
    # level 1. The lines at the left hand side are called outline level bars.
    #
    #
    #             ------------------------------------------
    #      1 2 3 |   |   A   |   B   |   C   |   D   |  ...
    #             ------------------------------------------
    #       _    | 1 |   A   |       |       |       |  ...
    #      |  _  | 2 |   B   |       |       |       |  ...
    #      | |   | 3 |  (C)  |       |       |       |  ...
    #      | |   | 4 |  (D)  |       |       |       |  ...
    #      | -   | 5 |   E   |       |       |       |  ...
    #      |  _  | 6 |   F   |       |       |       |  ...
    #      | |   | 7 |  (G)  |       |       |       |  ...
    #      | |   | 8 |  (H)  |       |       |       |  ...
    #      | -   | 9 |   I   |       |       |       |  ...
    #      -     | . |  ...  |  ...  |  ...  |  ...  |  ...
    #
    #
    # Clicking the minus sign on each of the level 2 outlines will collapse and
    # hide the data as shown in the next figure. The minus sign changes to a plus
    # sign to indicate that the data in the outline is hidden.
    #
    #             ------------------------------------------
    #      1 2 3 |   |   A   |   B   |   C   |   D   |  ...
    #             ------------------------------------------
    #       _    | 1 |   A   |       |       |       |  ...
    #      |     | 2 |   B   |       |       |       |  ...
    #      | +   | 5 |   E   |       |       |       |  ...
    #      |     | 6 |   F   |       |       |       |  ...
    #      | +   | 9 |   I   |       |       |       |  ...
    #      -     | . |  ...  |  ...  |  ...  |  ...  |  ...
    #
    #
    # Clicking on the minus sign on the level 1 outline will collapse the remaining
    # rows as follows:
    #
    #             ------------------------------------------
    #      1 2 3 |   |   A   |   B   |   C   |   D   |  ...
    #             ------------------------------------------
    #            | 1 |   A   |       |       |       |  ...
    #      +     | . |  ...  |  ...  |  ...  |  ...  |  ...
    #
    # See the main Spreadsheet::WriteExcel documentation for more information.
    #
    # reverse('(c)'), April 2003, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add some worksheets
    my $workbook   = Spreadsheet::WriteExcel->new('outline.xls');
    my $worksheet1 = $workbook->add_worksheet('Outlined Rows');
    my $worksheet2 = $workbook->add_worksheet('Collapsed Rows');
    my $worksheet3 = $workbook->add_worksheet('Outline Columns');
    my $worksheet4 = $workbook->add_worksheet('Outline levels');
    
    # Add a general format
    my $bold = $workbook->add_format(bold => 1);
    
    
    
    ###############################################################################
    #
    # Example 1: Create a worksheet with outlined rows. It also includes SUBTOTAL()
    # functions so that it looks like the type of automatic outlines that are
    # generated when you use the Excel Data->SubTotals menu item.
    #
    
    
    # For outlines the important parameters are $hidden and $level. Rows with the
    # same $level are grouped together. The group will be collapsed if $hidden is
    # non-zero. $height and $XF are assigned default values if they are undef.
    #
    # The syntax is: set_row($row, $height, $XF, $hidden, $level, $collapsed)
    #
    $worksheet1->set_row(1,  undef, undef, 0, 2);
    $worksheet1->set_row(2,  undef, undef, 0, 2);
    $worksheet1->set_row(3,  undef, undef, 0, 2);
    $worksheet1->set_row(4,  undef, undef, 0, 2);
    $worksheet1->set_row(5,  undef, undef, 0, 1);
    
    $worksheet1->set_row(6,  undef, undef, 0, 2);
    $worksheet1->set_row(7,  undef, undef, 0, 2);
    $worksheet1->set_row(8,  undef, undef, 0, 2);
    $worksheet1->set_row(9,  undef, undef, 0, 2);
    $worksheet1->set_row(10, undef, undef, 0, 1);
    
    
    # Add a column format for clarity
    $worksheet1->set_column('A:A', 20);
    
    # Add the data, labels and formulas
    $worksheet1->write('A1',  'Region', $bold);
    $worksheet1->write('A2',  'North');
    $worksheet1->write('A3',  'North');
    $worksheet1->write('A4',  'North');
    $worksheet1->write('A5',  'North');
    $worksheet1->write('A6',  'North Total', $bold);
    
    $worksheet1->write('B1',  'Sales',  $bold);
    $worksheet1->write('B2',  1000);
    $worksheet1->write('B3',  1200);
    $worksheet1->write('B4',  900);
    $worksheet1->write('B5',  1200);
    $worksheet1->write('B6',  '=SUBTOTAL(9,B2:B5)', $bold);
    
    $worksheet1->write('A7',  'South');
    $worksheet1->write('A8',  'South');
    $worksheet1->write('A9',  'South');
    $worksheet1->write('A10', 'South');
    $worksheet1->write('A11', 'South Total', $bold);
    
    $worksheet1->write('B7',  400);
    $worksheet1->write('B8',  600);
    $worksheet1->write('B9',  500);
    $worksheet1->write('B10', 600);
    $worksheet1->write('B11', '=SUBTOTAL(9,B7:B10)', $bold);
    
    $worksheet1->write('A12', 'Grand Total', $bold);
    $worksheet1->write('B12', '=SUBTOTAL(9,B2:B10)', $bold);
    
    
    ###############################################################################
    #
    # Example 2: Create a worksheet with outlined rows. This is the same as the
    # previous example except that the rows are collapsed.
    # Note: We need to indicate the row that contains the collapsed symbol '+'
    # with the optional parameter, $collapsed.
    
    # The group will be collapsed if $hidden is non-zero.
    # The syntax is: set_row($row, $height, $XF, $hidden, $level, $collapsed)
    #
    $worksheet2->set_row(1,  undef, undef, 1, 2);
    $worksheet2->set_row(2,  undef, undef, 1, 2);
    $worksheet2->set_row(3,  undef, undef, 1, 2);
    $worksheet2->set_row(4,  undef, undef, 1, 2);
    $worksheet2->set_row(5,  undef, undef, 1, 1);
    
    $worksheet2->set_row(6,  undef, undef, 1, 2);
    $worksheet2->set_row(7,  undef, undef, 1, 2);
    $worksheet2->set_row(8,  undef, undef, 1, 2);
    $worksheet2->set_row(9,  undef, undef, 1, 2);
    $worksheet2->set_row(10, undef, undef, 1, 1);
    $worksheet2->set_row(11, undef, undef, 0, 0, 1);
    
    
    # Add a column format for clarity
    $worksheet2->set_column('A:A', 20);
    
    # Add the data, labels and formulas
    $worksheet2->write('A1',  'Region', $bold);
    $worksheet2->write('A2',  'North');
    $worksheet2->write('A3',  'North');
    $worksheet2->write('A4',  'North');
    $worksheet2->write('A5',  'North');
    $worksheet2->write('A6',  'North Total', $bold);
    
    $worksheet2->write('B1',  'Sales',  $bold);
    $worksheet2->write('B2',  1000);
    $worksheet2->write('B3',  1200);
    $worksheet2->write('B4',  900);
    $worksheet2->write('B5',  1200);
    $worksheet2->write('B6',  '=SUBTOTAL(9,B2:B5)', $bold);
    
    $worksheet2->write('A7',  'South');
    $worksheet2->write('A8',  'South');
    $worksheet2->write('A9',  'South');
    $worksheet2->write('A10', 'South');
    $worksheet2->write('A11', 'South Total', $bold);
    
    $worksheet2->write('B7',  400);
    $worksheet2->write('B8',  600);
    $worksheet2->write('B9',  500);
    $worksheet2->write('B10', 600);
    $worksheet2->write('B11', '=SUBTOTAL(9,B7:B10)', $bold);
    
    $worksheet2->write('A12', 'Grand Total', $bold);
    $worksheet2->write('B12', '=SUBTOTAL(9,B2:B10)', $bold);
    
    
    
    ###############################################################################
    #
    # Example 3: Create a worksheet with outlined columns.
    #
    my $data = [
                ['Month', 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',' Total'],
                ['North', 50,    20,    15,    25,    65,    80,    ,'=SUM(B2:G2)'],
                ['South', 10,    20,    30,    50,    50,    50,    ,'=SUM(B3:G3)'],
                ['East',  45,    75,    50,    15,    75,    100,   ,'=SUM(B4:G4)'],
                ['West',  15,    15,    55,    35,    20,    50,    ,'=SUM(B5:G6)'],
               ];
    
    # Add bold format to the first row
    $worksheet3->set_row(0, undef, $bold);
    
    # Syntax: set_column($col1, $col2, $width, $XF, $hidden, $level, $collapsed)
    $worksheet3->set_column('A:A', 10, $bold      );
    $worksheet3->set_column('B:G', 5,  undef, 0, 1);
    $worksheet3->set_column('H:H', 10);
    
    # Write the data and a formula
    $worksheet3->write_col('A1', $data);
    $worksheet3->write('H6', '=SUM(H2:H5)', $bold);
    
    
    
    ###############################################################################
    #
    # Example 4: Show all possible outline levels.
    #
    my $levels = ["Level 1", "Level 2", "Level 3", "Level 4",
                  "Level 5", "Level 6", "Level 7", "Level 6",
                  "Level 5", "Level 4", "Level 3", "Level 2", "Level 1"];
    
    
    $worksheet4->write_col('A1', $levels);
    
    $worksheet4->set_row(0,  undef, undef, undef, 1);
    $worksheet4->set_row(1,  undef, undef, undef, 2);
    $worksheet4->set_row(2,  undef, undef, undef, 3);
    $worksheet4->set_row(3,  undef, undef, undef, 4);
    $worksheet4->set_row(4,  undef, undef, undef, 5);
    $worksheet4->set_row(5,  undef, undef, undef, 6);
    $worksheet4->set_row(6,  undef, undef, undef, 7);
    $worksheet4->set_row(7,  undef, undef, undef, 6);
    $worksheet4->set_row(8,  undef, undef, undef, 5);
    $worksheet4->set_row(9,  undef, undef, undef, 4);
    $worksheet4->set_row(10, undef, undef, undef, 3);
    $worksheet4->set_row(11, undef, undef, undef, 2);
    $worksheet4->set_row(12, undef, undef, undef, 1);
    
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/outline.pl>

=head2 Example: outline_collapsed.pl



Example of how use Spreadsheet::WriteExcel to generate Excel outlines and
grouping.

These example focus mainly on collapsed outlines. See also the
outlines.pl example program for more general examples.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/outline_collapsed.jpg" width="640" height="420" alt="Output from outline_collapsed.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how use Spreadsheet::WriteExcel to generate Excel outlines and
    # grouping.
    #
    # These example focus mainly on collapsed outlines. See also the
    # outlines.pl example program for more general examples.
    #
    # reverse('(c)'), March 2008, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add some worksheets
    my $workbook   = Spreadsheet::WriteExcel->new('outline_collapsed.xls');
    my $worksheet1 = $workbook->add_worksheet('Outlined Rows');
    my $worksheet2 = $workbook->add_worksheet('Collapsed Rows 1');
    my $worksheet3 = $workbook->add_worksheet('Collapsed Rows 2');
    my $worksheet4 = $workbook->add_worksheet('Collapsed Rows 3');
    my $worksheet5 = $workbook->add_worksheet('Outline Columns');
    my $worksheet6 = $workbook->add_worksheet('Collapsed Columns');
    
    
    # Add a general format
    my $bold = $workbook->add_format(bold => 1);
    
    
    #
    # This function will generate the same data and sub-totals on each worksheet.
    #
    sub create_sub_totals {
    
        my $worksheet = $_[0];
    
        # Add a column format for clarity
        $worksheet->set_column('A:A', 20);
    
        # Add the data, labels and formulas
        $worksheet->write('A1',  'Region', $bold);
        $worksheet->write('A2',  'North');
        $worksheet->write('A3',  'North');
        $worksheet->write('A4',  'North');
        $worksheet->write('A5',  'North');
        $worksheet->write('A6',  'North Total', $bold);
    
        $worksheet->write('B1',  'Sales',  $bold);
        $worksheet->write('B2',  1000);
        $worksheet->write('B3',  1200);
        $worksheet->write('B4',  900);
        $worksheet->write('B5',  1200);
        $worksheet->write('B6',  '=SUBTOTAL(9,B2:B5)', $bold);
    
        $worksheet->write('A7',  'South');
        $worksheet->write('A8',  'South');
        $worksheet->write('A9',  'South');
        $worksheet->write('A10', 'South');
        $worksheet->write('A11', 'South Total', $bold);
    
        $worksheet->write('B7',  400);
        $worksheet->write('B8',  600);
        $worksheet->write('B9',  500);
        $worksheet->write('B10', 600);
        $worksheet->write('B11', '=SUBTOTAL(9,B7:B10)', $bold);
    
        $worksheet->write('A12', 'Grand Total', $bold);
        $worksheet->write('B12', '=SUBTOTAL(9,B2:B10)', $bold);
    
    }
    
    
    ###############################################################################
    #
    # Example 1: Create a worksheet with outlined rows. It also includes SUBTOTAL()
    # functions so that it looks like the type of automatic outlines that are
    # generated when you use the Excel Data->SubTotals menu item.
    #
    
    # The syntax is: set_row($row, $height, $XF, $hidden, $level, $collapsed)
    $worksheet1->set_row(1,  undef, undef, 0, 2);
    $worksheet1->set_row(2,  undef, undef, 0, 2);
    $worksheet1->set_row(3,  undef, undef, 0, 2);
    $worksheet1->set_row(4,  undef, undef, 0, 2);
    $worksheet1->set_row(5,  undef, undef, 0, 1);
    
    $worksheet1->set_row(6,  undef, undef, 0, 2);
    $worksheet1->set_row(7,  undef, undef, 0, 2);
    $worksheet1->set_row(8,  undef, undef, 0, 2);
    $worksheet1->set_row(9,  undef, undef, 0, 2);
    $worksheet1->set_row(10, undef, undef, 0, 1);
    
    # Write the sub-total data that is common to the row examples.
    create_sub_totals($worksheet1);
    
    
    ###############################################################################
    #
    # Example 2: Create a worksheet with collapsed outlined rows.
    # This is the same as the example 1  except that the all rows are collapsed.
    # Note: We need to indicate the row that contains the collapsed symbol '+' with
    # the optional parameter, $collapsed.
    
    $worksheet2->set_row(1,  undef, undef, 1, 2);
    $worksheet2->set_row(2,  undef, undef, 1, 2);
    $worksheet2->set_row(3,  undef, undef, 1, 2);
    $worksheet2->set_row(4,  undef, undef, 1, 2);
    $worksheet2->set_row(5,  undef, undef, 1, 1);
    
    $worksheet2->set_row(6,  undef, undef, 1, 2);
    $worksheet2->set_row(7,  undef, undef, 1, 2);
    $worksheet2->set_row(8,  undef, undef, 1, 2);
    $worksheet2->set_row(9,  undef, undef, 1, 2);
    $worksheet2->set_row(10, undef, undef, 1, 1);
    
    $worksheet2->set_row(11, undef, undef, 0, 0, 1);
    
    # Write the sub-total data that is common to the row examples.
    create_sub_totals($worksheet2);
    
    
    ###############################################################################
    #
    # Example 3: Create a worksheet with collapsed outlined rows.
    # Same as the example 1  except that the two sub-totals are collapsed.
    
    $worksheet3->set_row(1,  undef, undef, 1, 2);
    $worksheet3->set_row(2,  undef, undef, 1, 2);
    $worksheet3->set_row(3,  undef, undef, 1, 2);
    $worksheet3->set_row(4,  undef, undef, 1, 2);
    $worksheet3->set_row(5,  undef, undef, 0, 1, 1);
    
    $worksheet3->set_row(6,  undef, undef, 1, 2);
    $worksheet3->set_row(7,  undef, undef, 1, 2);
    $worksheet3->set_row(8,  undef, undef, 1, 2);
    $worksheet3->set_row(9,  undef, undef, 1, 2);
    $worksheet3->set_row(10, undef, undef, 0, 1, 1);
    
    
    # Write the sub-total data that is common to the row examples.
    create_sub_totals($worksheet3);
    
    
    ###############################################################################
    #
    # Example 4: Create a worksheet with outlined rows.
    # Same as the example 1  except that the two sub-totals are collapsed.
    
    $worksheet4->set_row(1,  undef, undef, 1, 2);
    $worksheet4->set_row(2,  undef, undef, 1, 2);
    $worksheet4->set_row(3,  undef, undef, 1, 2);
    $worksheet4->set_row(4,  undef, undef, 1, 2);
    $worksheet4->set_row(5,  undef, undef, 1, 1, 1);
    
    $worksheet4->set_row(6,  undef, undef, 1, 2);
    $worksheet4->set_row(7,  undef, undef, 1, 2);
    $worksheet4->set_row(8,  undef, undef, 1, 2);
    $worksheet4->set_row(9,  undef, undef, 1, 2);
    $worksheet4->set_row(10, undef, undef, 1, 1, 1);
    
    $worksheet4->set_row(11, undef, undef, 0, 0, 1);
    
    # Write the sub-total data that is common to the row examples.
    create_sub_totals($worksheet4);
    
    
    
    ###############################################################################
    #
    # Example 5: Create a worksheet with outlined columns.
    #
    my $data = [
                ['Month', 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',' Total'],
                ['North', 50,    20,    15,    25,    65,    80,    ,'=SUM(B2:G2)'],
                ['South', 10,    20,    30,    50,    50,    50,    ,'=SUM(B3:G3)'],
                ['East',  45,    75,    50,    15,    75,    100,   ,'=SUM(B4:G4)'],
                ['West',  15,    15,    55,    35,    20,    50,    ,'=SUM(B5:G6)'],
               ];
    
    # Add bold format to the first row
    $worksheet5->set_row(0, undef, $bold);
    
    # Syntax: set_column($col1, $col2, $width, $XF, $hidden, $level, $collapsed)
    $worksheet5->set_column('A:A', 10, $bold      );
    $worksheet5->set_column('B:G', 5,  undef, 0, 1);
    $worksheet5->set_column('H:H', 10             );
    
    # Write the data and a formula
    $worksheet5->write_col('A1', $data);
    $worksheet5->write('H6', '=SUM(H2:H5)', $bold);
    
    
    ###############################################################################
    #
    # Example 6: Create a worksheet with collapsed outlined columns.
    # This is the same as the previous example except collapsed columns.
    
    # Add bold format to the first row
    $worksheet6->set_row(0, undef, $bold);
    
    # Syntax: set_column($col1, $col2, $width, $XF, $hidden, $level, $collapsed)
    $worksheet6->set_column('A:A', 10, $bold         );
    $worksheet6->set_column('B:G', 5,  undef, 1, 1   );
    $worksheet6->set_column('H:H', 10, undef, 0, 0, 1);
    
    # Write the data and a formula
    $worksheet6->write_col('A1', $data);
    $worksheet6->write('H6', '=SUM(H2:H5)', $bold);
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/outline_collapsed.pl>

=head2 Example: panes.pl



Example of using the WriteExcel module to create worksheet panes.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/panes.jpg" width="640" height="420" alt="Output from panes.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of using the WriteExcel module to create worksheet panes.
    #
    # reverse('(c)'), May 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("panes.xls");
    
    my $worksheet1 = $workbook->add_worksheet('Panes 1');
    my $worksheet2 = $workbook->add_worksheet('Panes 2');
    my $worksheet3 = $workbook->add_worksheet('Panes 3');
    my $worksheet4 = $workbook->add_worksheet('Panes 4');
    
    # Freeze panes
    $worksheet1->freeze_panes(1, 0); # 1 row
    
    $worksheet2->freeze_panes(0, 1); # 1 column
    $worksheet3->freeze_panes(1, 1); # 1 row and column
    
    # Split panes.
    # The divisions must be specified in terms of row and column dimensions.
    # The default row height is 12.75 and the default column width is 8.43
    #
    $worksheet4->split_panes(12.75, 8.43, 1, 1); # 1 row and column
    
    
    #######################################################################
    #
    # Set up some formatting and text to highlight the panes
    #
    
    my $header = $workbook->add_format();
    $header->set_color('white');
    $header->set_align('center');
    $header->set_align('vcenter');
    $header->set_pattern();
    $header->set_fg_color('green');
    
    my $center = $workbook->add_format();
    $center->set_align('center');
    
    
    #######################################################################
    #
    # Sheet 1
    #
    
    $worksheet1->set_column('A:I', 16);
    $worksheet1->set_row(0, 20);
    $worksheet1->set_selection('C3');
    
    for my $i (0..8){
        $worksheet1->write(0, $i, 'Scroll down', $header);
    }
    
    for my $i (1..100){
        for my $j (0..8){
            $worksheet1->write($i, $j, $i+1, $center);
        }
    }
    
    
    #######################################################################
    #
    # Sheet 2
    #
    
    $worksheet2->set_column('A:A', 16);
    $worksheet2->set_selection('C3');
    
    for my $i (0..49){
        $worksheet2->set_row($i, 15);
        $worksheet2->write($i, 0, 'Scroll right', $header);
    }
    
    for my $i (0..49){
        for my $j (1..25){
            $worksheet2->write($i, $j, $j, $center);
        }
    }
    
    
    #######################################################################
    #
    # Sheet 3
    #
    
    $worksheet3->set_column('A:Z', 16);
    $worksheet3->set_selection('C3');
    
    for my $i (1..25){
        $worksheet3->write(0, $i, 'Scroll down',  $header);
    }
    
    for my $i (1..49){
        $worksheet3->write($i, 0, 'Scroll right', $header);
    }
    
    for my $i (1..49){
        for my $j (1..25){
            $worksheet3->write($i, $j, $j, $center);
        }
    }
    
    
    #######################################################################
    #
    # Sheet 4
    #
    
    $worksheet4->set_selection('C3');
    
    for my $i (1..25){
        $worksheet4->write(0, $i, 'Scroll', $center);
    }
    
    for my $i (1..49){
        $worksheet4->write($i, 0, 'Scroll', $center);
    }
    
    for my $i (1..49){
        for my $j (1..25){
            $worksheet4->write($i, $j, $j, $center);
        }
    }
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/panes.pl>

=head2 Example: properties.pl



An example of adding document properties to a Spreadsheet::WriteExcel file.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/properties.jpg" width="640" height="420" alt="Output from properties.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # An example of adding document properties to a Spreadsheet::WriteExcel file.
    #
    # reverse('(c)'), August 2008, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new('properties.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    $workbook->set_properties(
        title    => 'This is an example spreadsheet',
        subject  => 'With document properties',
        author   => 'John McNamara',
        manager  => 'Dr. Heinz Doofenshmirtz ',
        company  => 'of Wolves',
        category => 'Example spreadsheets',
        keywords => 'Sample, Example, Properties',
        comments => 'Created with Perl and Spreadsheet::WriteExcel',
    );
    
    
    $worksheet->set_column('A:A', 50);
    $worksheet->write('A1', 'Select File->Properties to see the file properties');
    
    
    __END__


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/properties.pl>

=head2 Example: protection.pl



Example of cell locking and formula hiding in an Excel worksheet via
the Spreadsheet::WriteExcel module.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/protection.jpg" width="640" height="420" alt="Output from protection.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ########################################################################
    #
    # Example of cell locking and formula hiding in an Excel worksheet via
    # the Spreadsheet::WriteExcel module.
    #
    # reverse('(c)'), August 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("protection.xls");
    my $worksheet = $workbook->add_worksheet();
    
    # Create some format objects
    my $locked    = $workbook->add_format(locked => 1);
    my $unlocked  = $workbook->add_format(locked => 0);
    my $hidden    = $workbook->add_format(hidden => 1);
    
    # Format the columns
    $worksheet->set_column('A:A', 42);
    $worksheet->set_selection('B3:B3');
    
    # Protect the worksheet
    $worksheet->protect();
    
    # Examples of cell locking and hiding
    $worksheet->write('A1', 'Cell B1 is locked. It cannot be edited.');
    $worksheet->write('B1', '=1+2', $locked);
    
    $worksheet->write('A2', 'Cell B2 is unlocked. It can be edited.');
    $worksheet->write('B2', '=1+2', $unlocked);
    
    $worksheet->write('A3', "Cell B3 is hidden. The formula isn't visible.");
    $worksheet->write('B3', '=1+2', $hidden);
    
    $worksheet->write('A5', 'Use Menu->Tools->Protection->Unprotect Sheet');
    $worksheet->write('A6', 'to remove the worksheet protection.   ');
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/protection.pl>

=head2 Example: repeat.pl



Example of writing repeated formulas.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/repeat.jpg" width="640" height="420" alt="Output from repeat.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ######################################################################
    #
    # Example of writing repeated formulas.
    #
    # reverse('(c)'), August 2002, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new("repeat.xls");
    my $worksheet = $workbook->add_worksheet();
    
    
    my $limit = 1000;
    
    # Write a column of numbers
    for my $row (0..$limit) {
        $worksheet->write($row, 0,  $row);
    }
    
    
    # Store a formula
    my $formula = $worksheet->store_formula('=A1*5+4');
    
    
    # Write a column of formulas based on the stored formula
    for my $row (0..$limit) {
        $worksheet->repeat_formula($row, 1, $formula, undef,
                                            qr/^A1$/, 'A'.($row+1));
    }
    
    
    # Direct formula writing. As a speed comparison uncomment the
    # following and run the program again
    
    #for my $row (0..$limit) {
    #    $worksheet->write_formula($row, 2, '=A'.($row+1).'*5+4');
    #}
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/repeat.pl>

=head2 Example: right_to_left.pl



Example of how to change the default worksheet direction from
left-to-right to right-to-left as required by some eastern verions
of Excel.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/right_to_left.jpg" width="640" height="420" alt="Output from right_to_left.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to change the default worksheet direction from
    # left-to-right to right-to-left as required by some eastern verions
    # of Excel.
    #
    # reverse('(c)'), January 2006, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook   = Spreadsheet::WriteExcel->new("right_to_left.xls");
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    
    $worksheet2->right_to_left();
    
    $worksheet1->write(0, 0, 'Hello'); #  A1, B1, C1, ...
    $worksheet2->write(0, 0, 'Hello'); # ..., C1, B1, A1
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/right_to_left.pl>

=head2 Example: row_wrap.pl



Demonstrates how to wrap data from one worksheet onto another.

Excel has a row limit of 65536 rows. Sometimes the amount of row data to be
written to a file is greater than this limit. In this case it is a useful
technique to wrap the data from one worksheet onto the next so that we get
something like the following:

  Sheet1  Row     1  -  65536
  Sheet2  Row 65537  - 131072
  Sheet3  Row 131073 - ...

In order to achieve this we use a single worksheet reference and
reinitialise it to point to a new worksheet when required.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/row_wrap.jpg" width="640" height="420" alt="Output from row_wrap.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # Demonstrates how to wrap data from one worksheet onto another.
    #
    # Excel has a row limit of 65536 rows. Sometimes the amount of row data to be
    # written to a file is greater than this limit. In this case it is a useful
    # technique to wrap the data from one worksheet onto the next so that we get
    # something like the following:
    #
    #   Sheet1  Row     1  -  65536
    #   Sheet2  Row 65537  - 131072
    #   Sheet3  Row 131073 - ...
    #
    # In order to achieve this we use a single worksheet reference and
    # reinitialise it to point to a new worksheet when required.
    #
    # reverse('(c)'), May 2006, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    my $workbook  = Spreadsheet::WriteExcel->new('row_wrap.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Worksheet formatting.
    $worksheet->set_column('A:A', 20);
    
    
    # For the sake of this example we will use a small row limit. In order to use
    # the entire row range set the $row_limit to 65536.
    my $row_limit = 10;
    my $row       = 0;
    
    for my $count (1 .. 2 * $row_limit +10) {
    
        # When we hit the row limit we redirect the output
        # to a new worksheet and reset the row number.
        if ($row == $row_limit) {
            $worksheet = $workbook->add_worksheet();
            $row = 0;
    
            # Repeat any worksheet formatting.
            $worksheet->set_column('A:A', 20);
        }
    
        $worksheet->write($row, 0,  "This is row $count");
        $row++;
    }
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/row_wrap.pl>

=head2 Example: sales.pl



Example of a sales worksheet to demonstrate several different features.
Also uses functions from the L<Spreadsheet::WriteExcel::Utility> module.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/sales.jpg" width="640" height="420" alt="Output from sales.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of a sales worksheet to demonstrate several different features.
    # Also uses functions from the L<Spreadsheet::WriteExcel::Utility> module.
    #
    # reverse('(c)'), October 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    use Spreadsheet::WriteExcel::Utility;
    
    # Create a new workbook and add a worksheet
    my $workbook        = Spreadsheet::WriteExcel->new("sales.xls");
    my $worksheet       = $workbook->add_worksheet('May Sales');
    
    
    # Set up some formats
    my %heading         =   (
                                bold        => 1,
                                pattern     => 1,
                                fg_color    => 19,
                                border      => 1,
                                align       => 'center',
                            );
    
    my %total           =   (
                            bold        => 1,
                            top         => 1,
                            num_format  => '$#,##0.00'
                            );
    
    my $heading         = $workbook->add_format(%heading);
    my $total_format    = $workbook->add_format(%total);
    my $price_format    = $workbook->add_format(num_format => '$#,##0.00');
    my $date_format     = $workbook->add_format(num_format => 'mmm d yyy');
    
    
    # Write the main headings
    $worksheet->freeze_panes(1); # Freeze the first row
    $worksheet->write('A1', 'Item',     $heading);
    $worksheet->write('B1', 'Quantity', $heading);
    $worksheet->write('C1', 'Price',    $heading);
    $worksheet->write('D1', 'Total',    $heading);
    $worksheet->write('E1', 'Date',     $heading);
    
    # Set the column widths
    $worksheet->set_column('A:A', 25);
    $worksheet->set_column('B:B', 10);
    $worksheet->set_column('C:E', 16);
    
    
    # Extract the sales data from the __DATA__ section at the end of the file.
    # In reality this information would probably come from a database
    my @sales;
    
    foreach my $line (<DATA>) {
        chomp $line;
        next if $line eq '';
        # Simple-minded processing of CSV data. Refer to the Text::CSV_XS
        # and Text::xSV modules for a more complete CSV handling.
        my @items = split /,/, $line;
        push @sales, \@items;
    }
    
    
    # Write out the items from each row
    my $row = 1;
    foreach my $sale (@sales) {
    
        $worksheet->write($row, 0, @$sale[0]);
        $worksheet->write($row, 1, @$sale[1]);
        $worksheet->write($row, 2, @$sale[2], $price_format);
    
        # Create a formula like '=B2*C2'
        my $formula =   '='
                        . xl_rowcol_to_cell($row, 1)
                        . "*"
                        . xl_rowcol_to_cell($row, 2);
    
        $worksheet->write($row, 3, $formula, $price_format);
    
        # Parse the date
        my $date = xl_decode_date_US(@$sale[3]);
        $worksheet->write($row, 4, $date, $date_format);
        $row++;
    }
    
    # Create a formula to sum the totals, like '=SUM(D2:D6)'
    my $total = '=SUM(D2:'
                . xl_rowcol_to_cell($row-1, 3)
                . ")";
    
    $worksheet->write($row, 3, $total, $total_format);
    
    
    
    __DATA__
    586 card,20,125.50,5/12/01
    Flat Screen Monitor,1,1300.00,5/12/01
    64 MB dimms,45,49.99,5/13/01
    15 GB HD,12,300.00,5/13/01
    Speakers (pair),5,15.50,5/14/01
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/sales.pl>

=head2 Example: sendmail.pl



Example of how to use Mail::Sender to send a Spreadsheet::WriteExcel Excel
file as an attachment.

The main thing is to ensure that you close() the Worbook before you send it.

See the L<Mail::Sender> module for further details.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use Mail::Sender to send a Spreadsheet::WriteExcel Excel
    # file as an attachment.
    #
    # The main thing is to ensure that you close() the Worbook before you send it.
    #
    # See the L<Mail::Sender> module for further details.
    #
    # reverse('(c)'), August 2002, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    use Mail::Sender;
    
    # Create an Excel file
    my $workbook  = Spreadsheet::WriteExcel->new("sendmail.xls");
    my $worksheet = $workbook->add_worksheet;
    
    $worksheet->write('A1', "Hello World!");
    
    $workbook->close(); # Must close before sending
    
    
    
    # Send the file.  Change all variables to suit
    my $sender = new Mail::Sender
    {
        smtp => '123.123.123.123',
        from => 'Someone'
    };
    
    $sender->MailFile(
    {
        to      => 'another@mail.com',
        subject => 'Excel file',
        msg     => "Here is the data.\n",
        file    => 'mail.xls',
    });
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/sendmail.pl>

=head2 Example: stats_ext.pl



Example of formatting using the Spreadsheet::WriteExcel module

This is a simple example of how to use functions that reference cells in
other worksheets within the same workbook.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stats_ext.jpg" width="640" height="420" alt="Output from stats_ext.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of formatting using the Spreadsheet::WriteExcel module
    #
    # This is a simple example of how to use functions that reference cells in
    # other worksheets within the same workbook.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("stats_ext.xls");
    my $worksheet1 = $workbook->add_worksheet('Test results');
    my $worksheet2 = $workbook->add_worksheet('Data');
    
    # Set the column width for columns 1
    $worksheet1->set_column('A:A', 20);
    
    
    # Create a format for the headings
    my $heading = $workbook->add_format();
    $heading->set_bold();
    
    # Create a numerical format
    my $numformat = $workbook->add_format();
    $numformat->set_num_format('0.00');
    
    
    
    
    # Write some statistical functions
    $worksheet1->write('A1', 'Count', $heading);
    $worksheet1->write('B1', '=COUNT(Data!B2:B9)');
    
    $worksheet1->write('A2', 'Sum', $heading);
    $worksheet1->write('B2', '=SUM(Data!B2:B9)');
    
    $worksheet1->write('A3', 'Average', $heading);
    $worksheet1->write('B3', '=AVERAGE(Data!B2:B9)');
    
    $worksheet1->write('A4', 'Min', $heading);
    $worksheet1->write('B4', '=MIN(Data!B2:B9)');
    
    $worksheet1->write('A5', 'Max', $heading);
    $worksheet1->write('B5', '=MAX(Data!B2:B9)');
    
    $worksheet1->write('A6', 'Standard Deviation', $heading);
    $worksheet1->write('B6', '=STDEV(Data!B2:B9)');
    
    $worksheet1->write('A7', 'Kurtosis', $heading);
    $worksheet1->write('B7', '=KURT(Data!B2:B9)');
    
    
    # Write the sample data
    $worksheet2->write('A1', 'Sample', $heading);
    $worksheet2->write('A2', 1);
    $worksheet2->write('A3', 2);
    $worksheet2->write('A4', 3);
    $worksheet2->write('A5', 4);
    $worksheet2->write('A6', 5);
    $worksheet2->write('A7', 6);
    $worksheet2->write('A8', 7);
    $worksheet2->write('A9', 8);
    
    $worksheet2->write('B1', 'Length', $heading);
    $worksheet2->write('B2', 25.4, $numformat);
    $worksheet2->write('B3', 25.4, $numformat);
    $worksheet2->write('B4', 24.8, $numformat);
    $worksheet2->write('B5', 25.0, $numformat);
    $worksheet2->write('B6', 25.3, $numformat);
    $worksheet2->write('B7', 24.9, $numformat);
    $worksheet2->write('B8', 25.2, $numformat);
    $worksheet2->write('B9', 24.8, $numformat);


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/stats_ext.pl>

=head2 Example: stocks.pl



Example of formatting using the Spreadsheet::WriteExcel module

This example shows how to use a conditional numerical format
with colours to indicate if a share price has gone up or down.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stocks.jpg" width="640" height="420" alt="Output from stocks.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of formatting using the Spreadsheet::WriteExcel module
    #
    # This example shows how to use a conditional numerical format
    # with colours to indicate if a share price has gone up or down.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("stocks.xls");
    my $worksheet = $workbook->add_worksheet();
    
    # Set the column width for columns 1, 2, 3 and 4
    $worksheet->set_column(0, 3, 15);
    
    
    # Create a format for the column headings
    my $header = $workbook->add_format();
    $header->set_bold();
    $header->set_size(12);
    $header->set_color('blue');
    
    
    # Create a format for the stock price
    my $f_price = $workbook->add_format();
    $f_price->set_align('left');
    $f_price->set_num_format('$0.00');
    
    
    # Create a format for the stock volume
    my $f_volume = $workbook->add_format();
    $f_volume->set_align('left');
    $f_volume->set_num_format('#,##0');
    
    
    # Create a format for the price change. This is an example of a conditional
    # format. The number is formatted as a percentage. If it is positive it is
    # formatted in green, if it is negative it is formatted in red and if it is
    # zero it is formatted as the default font colour (in this case black).
    # Note: the [Green] format produces an unappealing lime green. Try
    # [Color 10] instead for a dark green.
    #
    my $f_change = $workbook->add_format();
    $f_change->set_align('left');
    $f_change->set_num_format('[Green]0.0%;[Red]-0.0%;0.0%');
    
    
    # Write out the data
    $worksheet->write(0, 0, 'Company', $header);
    $worksheet->write(0, 1, 'Price',   $header);
    $worksheet->write(0, 2, 'Volume',  $header);
    $worksheet->write(0, 3, 'Change',  $header);
    
    $worksheet->write(1, 0, 'Damage Inc.'     );
    $worksheet->write(1, 1, 30.25,     $f_price);  # $30.25
    $worksheet->write(1, 2, 1234567,   $f_volume); # 1,234,567
    $worksheet->write(1, 3, 0.085,     $f_change); # 8.5% in green
    
    $worksheet->write(2, 0, 'Dump Corp.'      );
    $worksheet->write(2, 1, 1.56,      $f_price);  # $1.56
    $worksheet->write(2, 2, 7564,      $f_volume); # 7,564
    $worksheet->write(2, 3, -0.015,    $f_change); # -1.5% in red
    
    $worksheet->write(3, 0, 'Rev Ltd.'        );
    $worksheet->write(3, 1, 0.13,      $f_price);  # $0.13
    $worksheet->write(3, 2, 321,       $f_volume); # 321
    $worksheet->write(3, 3, 0,         $f_change); # 0 in the font color (black)
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/stocks.pl>

=head2 Example: tab_colors.pl



Example of how to set Excel worksheet tab colours.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/tab_colors.jpg" width="640" height="420" alt="Output from tab_colors.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to set Excel worksheet tab colours.
    #
    # reverse('(c)'), May 2006, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook   = Spreadsheet::WriteExcel->new('tab_colors.xls');
    
    my $worksheet1 = $workbook->add_worksheet();
    my $worksheet2 = $workbook->add_worksheet();
    my $worksheet3 = $workbook->add_worksheet();
    my $worksheet4 = $workbook->add_worksheet();
    
    # Worksheet1 will have the default tab colour.
    $worksheet2->set_tab_color('red');
    $worksheet3->set_tab_color('green');
    $worksheet4->set_tab_color(0x35); # Orange


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/tab_colors.pl>

=head2 Example: textwrap.pl



Example of formatting using the Spreadsheet::WriteExcel module

This example shows how to wrap text in a cell. There are two alternatives,
vertical justification and text wrap.

With vertical justification the text is wrapped automatically to fit the
column width. With text wrap you must specify a newline with an embedded \n.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/textwrap.jpg" width="640" height="420" alt="Output from textwrap.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of formatting using the Spreadsheet::WriteExcel module
    #
    # This example shows how to wrap text in a cell. There are two alternatives,
    # vertical justification and text wrap.
    #
    # With vertical justification the text is wrapped automatically to fit the
    # column width. With text wrap you must specify a newline with an embedded \n.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("textwrap.xls");
    my $worksheet = $workbook->add_worksheet();
    
    # Set the column width for columns 1, 2 and 3
    $worksheet->set_column(1, 1, 24);
    $worksheet->set_column(2, 2, 34);
    $worksheet->set_column(3, 3, 34);
    
    # Set the row height for rows 1, 4, and 6. The height of row 2 will adjust
    # automatically to fit the text.
    #
    $worksheet->set_row(0, 30);
    $worksheet->set_row(3, 40);
    $worksheet->set_row(5, 80);
    
    
    # No newlines
    my $str1  = "For whatever we lose (like a you or a me) ";
    $str1    .= "it's always ourselves we find in the sea";
    
    # Embedded newlines
    my $str2  = "For whatever we lose\n(like a you or a me)\n";
       $str2 .= "it's always ourselves\nwe find in the sea";
    
    
    # Create a format for the column headings
    my $header = $workbook->add_format();
    $header->set_bold();
    $header->set_font("Courier New");
    $header->set_align('center');
    $header->set_align('vcenter');
    
    # Create a "vertical justification" format
    my $format1 = $workbook->add_format();
    $format1->set_align('vjustify');
    
    # Create a "text wrap" format
    my $format2 = $workbook->add_format();
    $format2->set_text_wrap();
    
    # Write the headers
    $worksheet->write(0, 1, "set_align('vjustify')", $header);
    $worksheet->write(0, 2, "set_align('vjustify')", $header);
    $worksheet->write(0, 3, "set_text_wrap()", $header);
    
    # Write some examples
    $worksheet->write(1, 1, $str1, $format1);
    $worksheet->write(1, 2, $str1, $format1);
    $worksheet->write(1, 3, $str2, $format2);
    
    $worksheet->write(3, 1, $str1, $format1);
    $worksheet->write(3, 2, $str1, $format1);
    $worksheet->write(3, 3, $str2, $format2);
    
    $worksheet->write(5, 1, $str1, $format1);
    $worksheet->write(5, 2, $str1, $format1);
    $worksheet->write(5, 3, $str2, $format2);
    
    
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/textwrap.pl>

=head2 Example: win32ole.pl



This is a simple example of how to create an Excel file using the
Win32::OLE module for the sake of comparison.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This is a simple example of how to create an Excel file using the
    # Win32::OLE module for the sake of comparison.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Cwd;
    use Win32::OLE;
    use Win32::OLE::Const 'Microsoft Excel';
    
    
    my $application = Win32::OLE->new("Excel.Application");
    my $workbook    = $application->Workbooks->Add;
    my $worksheet   = $workbook->Worksheets(1);
    
    $worksheet->Cells(1,1)->{Value} = "Hello World";
    $worksheet->Cells(2,1)->{Value} = "One";
    $worksheet->Cells(3,1)->{Value} = "Two";
    $worksheet->Cells(4,1)->{Value} =  3;
    $worksheet->Cells(5,1)->{Value} =  4.0000001;
    
    # Add some formatting
    $worksheet->Cells(1,1)->Font->{Bold}       = "True";
    $worksheet->Cells(1,1)->Font->{Size}       = 16;
    $worksheet->Cells(1,1)->Font->{ColorIndex} = 3;
    $worksheet->Columns("A:A")->{ColumnWidth}  = 25;
    
    # Write a hyperlink
    my $range = $worksheet->Range("A7:A7");
    $worksheet->Hyperlinks->Add({ Anchor => $range, Address => "http://www.perl.com/"});
    
    # Get current directory using Cwd.pm
    my $dir = cwd();
    
    $workbook->SaveAs({
                        FileName   => $dir . '/win32ole.xls',
                        FileFormat => xlNormal,
                      });
    $workbook->Close;


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/win32ole.pl>

=head2 Example: write_arrays.pl



Example of how to use the Spreadsheet::WriteExcel module to
write 1D and 2D arrays of data.

To find out more about array references refer(!!) to the perlref and
perlreftut manpages. To find out more about 2D arrays or "list of
lists" refer to the perllol manpage.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/write_arrays.jpg" width="640" height="420" alt="Output from write_arrays.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    #######################################################################
    #
    # Example of how to use the Spreadsheet::WriteExcel module to
    # write 1D and 2D arrays of data.
    #
    # To find out more about array references refer(!!) to the perlref and
    # perlreftut manpages. To find out more about 2D arrays or "list of
    # lists" refer to the perllol manpage.
    #
    # reverse('(c)'), March 2002, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook   = Spreadsheet::WriteExcel->new("write_arrays.xls");
    my $worksheet1 = $workbook->add_worksheet('Example 1');
    my $worksheet2 = $workbook->add_worksheet('Example 2');
    my $worksheet3 = $workbook->add_worksheet('Example 3');
    my $worksheet4 = $workbook->add_worksheet('Example 4');
    my $worksheet5 = $workbook->add_worksheet('Example 5');
    my $worksheet6 = $workbook->add_worksheet('Example 6');
    my $worksheet7 = $workbook->add_worksheet('Example 7');
    my $worksheet8 = $workbook->add_worksheet('Example 8');
    
    my $format     = $workbook->add_format(color => 'red', bold => 1);
    
    
    # Data arrays used in the following examples.
    # undef values are written as blank cells (with format if specified).
    #
    my @array   =   ( 'one', 'two', undef, 'four' );
    
    my @array2d =   (
                        ['maggie', 'milly', 'molly', 'may'  ],
                        [13,       14,      15,      16     ],
                        ['shell',  'star',  'crab',  'stone'],
                    );
    
    
    # 1. Write a row of data using an array reference.
    $worksheet1->write('A1', \@array);
    
    # 2. Same as 1. above using an anonymous array ref.
    $worksheet2->write('A1', [ @array ]);
    
    # 3. Write a row of data using an explicit write_row() method call.
    #    This is the same as calling write() in Ex. 1 above.
    #
    $worksheet3->write_row('A1', \@array);
    
    # 4. Write a column of data using the write_col() method call.
    $worksheet4->write_col('A1', \@array);
    
    # 5. Write a column of data using a ref to an array ref, i.e. a 2D array.
    $worksheet5->write('A1', [ \@array ]);
    
    # 6. Write a 2D array in col-row order.
    $worksheet6->write('A1', \@array2d);
    
    # 7. Write a 2D array in row-col order.
    $worksheet7->write_col('A1', \@array2d);
    
    # 8. Write a row of data with formatting. The blank cell is also formatted.
    $worksheet8->write('A1', \@array, $format);
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_arrays.pl>

=head2 Example: write_handler1.pl



Example of how to add a user defined data handler to the Spreadsheet::
WriteExcel write() method.

The following example shows how to add a handler for a 7 digit ID number.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/write_handler1.jpg" width="640" height="420" alt="Output from write_handler1.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to add a user defined data handler to the Spreadsheet::
    # WriteExcel write() method.
    #
    # The following example shows how to add a handler for a 7 digit ID number.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook    = Spreadsheet::WriteExcel->new("write_handler1.xls");
    my $worksheet   = $workbook->add_worksheet();
    
    
    ###############################################################################
    #
    # Add a handler for 7 digit id numbers. This is useful when you want a string
    # such as 0000001 written as a string instead of a number and thus preserve
    # the leading zeroes.
    #
    # Note: you can get the same effect using the keep_leading_zeros() method but
    # this serves as a simple example.
    #
    $worksheet->add_write_handler(qr[^\d{7}$], \&write_my_id);
    
    
    ###############################################################################
    #
    # The following function processes the data when a match is found.
    #
    sub write_my_id {
    
        my $worksheet = shift;
    
        return $worksheet->write_string(@_);
    }
    
    
    # This format maintains the cell as text even if it is edited.
    my $id_format   = $workbook->add_format(num_format => '@');
    
    
    # Write some numbers in the user defined format
    $worksheet->write('A1', '0000000', $id_format);
    $worksheet->write('A2', '0000001', $id_format);
    $worksheet->write('A3', '0004000', $id_format);
    $worksheet->write('A4', '1234567', $id_format);
    
    # Write some numbers that don't match the defined format
    $worksheet->write('A6', '000000',  $id_format);
    $worksheet->write('A7', '000001',  $id_format);
    $worksheet->write('A8', '004000',  $id_format);
    $worksheet->write('A9', '123456',  $id_format);
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_handler1.pl>

=head2 Example: write_handler2.pl



Example of how to add a user defined data handler to the Spreadsheet::
WriteExcel write() method.

The following example shows how to add a handler for a 7 digit ID number.
It adds an additional constraint to the write_handler1.pl in that it only
filters data that isn't in the third column.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/write_handler2.jpg" width="640" height="420" alt="Output from write_handler2.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to add a user defined data handler to the Spreadsheet::
    # WriteExcel write() method.
    #
    # The following example shows how to add a handler for a 7 digit ID number.
    # It adds an additional constraint to the write_handler1.pl in that it only
    # filters data that isn't in the third column.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook    = Spreadsheet::WriteExcel->new("write_handler2.xls");
    my $worksheet   = $workbook->add_worksheet();
    
    
    ###############################################################################
    #
    # Add a handler for 7 digit id numbers. This is useful when you want a string
    # such as 0000001 written as a string instead of a number and thus preserve
    # the leading zeroes.
    #
    # Note: you can get the same effect using the keep_leading_zeros() method but
    # this serves as a simple example.
    #
    $worksheet->add_write_handler(qr[^\d{7}$], \&write_my_id);
    
    
    ###############################################################################
    #
    # The following function processes the data when a match is found. The handler
    # is set up so that it only filters data if it is in the third column.
    #
    sub write_my_id {
    
        my $worksheet = shift;
        my $col       = $_[1];
    
        # col is zero based
        if ($col != 2) {
            return $worksheet->write_string(@_);
        }
        else {
            # Reject the match and return control to write()
            return undef;
        }
    
    }
    
    
    # This format maintains the cell as text even if it is edited.
    my $id_format   = $workbook->add_format(num_format => '@');
    
    
    # Write some numbers in the user defined format
    $worksheet->write('A1', '0000000', $id_format);
    $worksheet->write('B1', '0000001', $id_format);
    $worksheet->write('C1', '0000002', $id_format);
    $worksheet->write('D1', '0000003', $id_format);
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_handler2.pl>

=head2 Example: write_handler3.pl



Example of how to add a user defined data handler to the Spreadsheet::
WriteExcel write() method.

The following example shows how to add a handler for dates in a specific
format.

See write_handler4.pl for a more rigorous example with error handling.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/write_handler3.jpg" width="640" height="420" alt="Output from write_handler3.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to add a user defined data handler to the Spreadsheet::
    # WriteExcel write() method.
    #
    # The following example shows how to add a handler for dates in a specific
    # format.
    #
    # See write_handler4.pl for a more rigorous example with error handling.
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook    = Spreadsheet::WriteExcel->new("write_handler3.xls");
    my $worksheet   = $workbook->add_worksheet();
    my $date_format = $workbook->add_format(num_format => 'dd/mm/yy');
    
    
    ###############################################################################
    #
    # Add a handler to match dates in the following format: d/m/yyyy
    #
    # The day and month can be single or double digits.
    #
    $worksheet->add_write_handler(qr[^\d{1,2}/\d{1,2}/\d{4}$], \&write_my_date);
    
    
    ###############################################################################
    #
    # The following function processes the data when a match is found.
    # See write_handler4.pl for a more rigorous example with error handling.
    #
    sub write_my_date {
    
        my $worksheet = shift;
        my @args      = @_;
    
        my $token     = $args[2];
           $token     =~ qr[^(\d{1,2})/(\d{1,2})/(\d{4})$];
    
        # Change to the date format required by write_date_time().
        my $date = sprintf "%4d-%02d-%02dT", $3, $2, $1;
    
        $args[2] = $date;
    
        return $worksheet->write_date_time(@args);
    }
    
    
    # Write some dates in the user defined format
    $worksheet->write('A1', '22/12/2004', $date_format);
    $worksheet->write('A2', '1/1/1995',   $date_format);
    $worksheet->write('A3', '01/01/1995', $date_format);
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_handler3.pl>

=head2 Example: write_handler4.pl



Example of how to add a user defined data handler to the Spreadsheet::
WriteExcel write() method.

The following example shows how to add a handler for dates in a specific
format.

This is a more rigorous version of write_handler3.pl.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/write_handler4.jpg" width="640" height="420" alt="Output from write_handler4.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to add a user defined data handler to the Spreadsheet::
    # WriteExcel write() method.
    #
    # The following example shows how to add a handler for dates in a specific
    # format.
    #
    # This is a more rigorous version of write_handler3.pl.
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook    = Spreadsheet::WriteExcel->new("write_handler4.xls");
    my $worksheet   = $workbook->add_worksheet();
    my $date_format = $workbook->add_format(num_format => 'dd/mm/yy');
    
    
    ###############################################################################
    #
    # Add a handler to match dates in the following formats: d/m/yy, d/m/yyyy
    #
    # The day and month can be single or double digits and the year can be  2 or 4
    # digits.
    #
    $worksheet->add_write_handler(qr[^\d{1,2}/\d{1,2}/\d{2,4}$], \&write_my_date);
    
    
    ###############################################################################
    #
    # The following function processes the data when a match is found.
    #
    sub write_my_date {
    
        my $worksheet = shift;
        my @args      = @_;
    
        my $token     = $args[2];
    
        if ($token =~  qr[^(\d{1,2})/(\d{1,2})/(\d{2,4})$]) {
    
            my $day  = $1;
            my $mon  = $2;
            my $year = $3;
    
            # Use a window for 2 digit dates. This will keep some ragged Perl
            # programmer employed in thirty years time. :-)
            if (length $year == 2) {
                if ($year < 50) {
                    $year += 2000;
                }
                else {
                    $year += 1900;
                }
            }
    
            my $date = sprintf "%4d-%02d-%02dT", $year, $mon, $day;
    
            # Convert the ISO ISO8601 style string to an Excel date
            $date = $worksheet->convert_date_time($date);
    
            if (defined $date) {
                # Date was valid
                $args[2] = $date;
                return $worksheet->write_number(@args);
            }
            else {
                # Not a valid date therefore write as a string
                return $worksheet->write_string(@args);
            }
        }
        else {
            # Shouldn't happen if the same match is used in the re and sub.
            return undef;
        }
    }
    
    
    # Write some dates in the user defined format
    $worksheet->write('A1', '22/12/2004', $date_format);
    $worksheet->write('A2', '22/12/04',   $date_format);
    $worksheet->write('A3', '2/12/04',    $date_format);
    $worksheet->write('A4', '2/5/04',     $date_format);
    $worksheet->write('A5', '2/5/95',     $date_format);
    $worksheet->write('A6', '2/5/1995',   $date_format);
    
    # Some erroneous dates
    $worksheet->write('A8', '2/5/1895',   $date_format); # Date out of Excel range
    $worksheet->write('A9', '29/2/2003',  $date_format); # Invalid leap day
    $worksheet->write('A10','50/50/50',   $date_format); # Matches but isn't a date
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_handler4.pl>

=head2 Example: write_to_scalar.pl



An example of writing an Excel file to a Perl scalar using Spreadsheet::
WriteExcel and the new features of perl 5.8.

For an examples of how to write to a scalar in versions prior to perl 5.8
see the filehandle.pl program and IO:Scalar.



    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # An example of writing an Excel file to a Perl scalar using Spreadsheet::
    # WriteExcel and the new features of perl 5.8.
    #
    # For an examples of how to write to a scalar in versions prior to perl 5.8
    # see the filehandle.pl program and IO:Scalar.
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    require 5.008;
    
    
    # Use perl 5.8's feature of using a scalar as a filehandle.
    my   $fh;
    my   $str = '';
    open $fh, '>', \$str or die "Failed to open filehandle: $!";
    
    
    # Or replace the previous three lines with this:
    # open my $fh, '>', \my $str or die "Failed to open filehandle: $!";
    
    
    # Spreadsheet::WriteExce accepts filehandle as well as file names.
    my $workbook  = Spreadsheet::WriteExcel->new($fh);
    my $worksheet = $workbook->add_worksheet();
    
    $worksheet->write(0, 0,  "Hi Excel!");
    
    $workbook->close();
    
    
    # The Excel file in now in $str. Remember to binmode() the output
    # filehandle before printing it.
    binmode STDOUT;
    print $str;
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/write_to_scalar.pl>

=head2 Example: unicode_utf16.pl



A simple example of writing some Unicode text with Spreadsheet::WriteExcel.

This example shows UTF16 encoding. With perl 5.8 it is also possible to use
utf8 without modification.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_utf16.jpg" width="640" height="420" alt="Output from unicode_utf16.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of writing some Unicode text with Spreadsheet::WriteExcel.
    #
    # This example shows UTF16 encoding. With perl 5.8 it is also possible to use
    # utf8 without modification.
    #
    # reverse('(c)'), May 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new('unicode_utf16.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Write the Unicode smiley face (with increased font for legibility)
    my $smiley    = pack "n", 0x263a;
    my $big_font  = $workbook->add_format(size => 40);
    
    $worksheet->write_utf16be_string('A3', $smiley, $big_font);
    
    
    # Write a phrase in Cyrillic
    my $uni_str = pack "H*", "042d0442043e002004440440043004370430002004".
                             "3d043000200440044304410441043a043e043c0021";
    
    $worksheet->write_utf16be_string('A5', $uni_str);
    
    
    $worksheet->write_utf16be_string('A7', pack "H*", "0074006500730074");
    
    
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_utf16.pl>

=head2 Example: unicode_utf16_japan.pl



A simple example of writing some Unicode text with Spreadsheet::WriteExcel.

This creates an Excel file with the word Nippon in 3 character sets.

This example shows UTF16 encoding. With perl 5.8 it is also possible to use
utf8 without modification.

See also the unicode_2022_jp.pl and unicode_shift_jis.pl examples.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_utf16_japan.jpg" width="640" height="420" alt="Output from unicode_utf16_japan.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of writing some Unicode text with Spreadsheet::WriteExcel.
    #
    # This creates an Excel file with the word Nippon in 3 character sets.
    #
    # This example shows UTF16 encoding. With perl 5.8 it is also possible to use
    # utf8 without modification.
    #
    # See also the unicode_2022_jp.pl and unicode_shift_jis.pl examples.
    #
    # reverse('(c)'), May 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new('unicode_utf16_japan.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Set a Unicode font.
    my $uni_font  = $workbook->add_format(font => 'Arial Unicode MS');
    
    
    # Create some UTF-16BE Unicode text.
    my $kanji     = pack 'n*', 0x65e5, 0x672c;
    my $katakana  = pack 'n*', 0xff86, 0xff8e, 0xff9d;
    my $hiragana  = pack 'n*', 0x306b, 0x307b, 0x3093;
    
    
    
    $worksheet->write_utf16be_string('A1', $kanji,    $uni_font);
    $worksheet->write_utf16be_string('A2', $katakana, $uni_font);
    $worksheet->write_utf16be_string('A3', $hiragana, $uni_font);
    
    
    $worksheet->write('B1', 'Kanji');
    $worksheet->write('B2', 'Katakana');
    $worksheet->write('B3', 'Hiragana');
    
    
    __END__
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_utf16_japan.pl>

=head2 Example: unicode_cyrillic.pl



A simple example of writing some Russian cyrillic text using
Spreadsheet::WriteExcel and perl 5.8.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_cyrillic.jpg" width="640" height="420" alt="Output from unicode_cyrillic.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of writing some Russian cyrillic text using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # reverse('(c)'), March 2005, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    # In this example we generate utf8 strings from character data but in a
    # real application we would expect them to come from an external source.
    #
    
    
    # Create a Russian worksheet name in utf8.
    my $sheet   = pack "U*", 0x0421, 0x0442, 0x0440, 0x0430, 0x043D, 0x0438,
                             0x0446, 0x0430;
    
    
    # Create a Russian string.
    my $str     = pack "U*", 0x0417, 0x0434, 0x0440, 0x0430, 0x0432, 0x0441,
                             0x0442, 0x0432, 0x0443, 0x0439, 0x0020, 0x041C,
                             0x0438, 0x0440, 0x0021;
    
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_cyrillic.xls");
    my $worksheet = $workbook->add_worksheet($sheet . '1');
    
       $worksheet->set_column('A:A', 18);
       $worksheet->write('A1', $str);
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_cyrillic.pl>

=head2 Example: unicode_list.pl



A simple example using Spreadsheet::WriteExcel to display all available
Unicode characters in a font.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_list.jpg" width="640" height="420" alt="Output from unicode_list.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example using Spreadsheet::WriteExcel to display all available
    # Unicode characters in a font.
    #
    # reverse('(c)'), May 2004, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new('unicode_list.xls');
    my $worksheet = $workbook->add_worksheet();
    
    
    # Set a Unicode font.
    my $uni_font  = $workbook->add_format(font => 'Arial Unicode MS');
    
    # Ascii font for labels.
    my $courier   = $workbook->add_format(font => 'Courier New');
    
    
    my $char = 0;
    
    # Loop through all 32768 UTF-16BE characters.
    #
    for my $row (0 .. 2 ** 12 -1) {
        for my $col (0 .. 31) {
    
            last if $char == 0xffff;
    
            if ($col % 2 == 0){
                $worksheet->write_string($row, $col,
                                               sprintf('0x%04X', $char), $courier);
            }
            else {
                $worksheet->write_utf16be_string($row, $col,
                                                pack('n', $char++), $uni_font);
            }
        }
    }
    
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_list.pl>

=head2 Example: unicode_2022_jp.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Japanese from a file with ISO-2022-JP
encoded text.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_2022_jp.jpg" width="640" height="420" alt="Output from unicode_2022_jp.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Japanese from a file with ISO-2022-JP
    # encoded text.
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_2022_jp.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_2022_jp.txt';
    
    open FH, '<:encoding(iso-2022-jp)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_2022_jp.pl>

=head2 Example: unicode_8859_11.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Thai from a file with ISO-8859-11 encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_8859_11.jpg" width="640" height="420" alt="Output from unicode_8859_11.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Thai from a file with ISO-8859-11 encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_8859_11.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_8859_11.txt';
    
    open FH, '<:encoding(iso-8859-11)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_8859_11.pl>

=head2 Example: unicode_8859_7.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Greek from a file with ISO-8859-7 encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_8859_7.jpg" width="640" height="420" alt="Output from unicode_8859_7.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Greek from a file with ISO-8859-7 encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_8859_7.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_8859_7.txt';
    
    open FH, '<:encoding(iso-8859-7)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_8859_7.pl>

=head2 Example: unicode_big5.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Chinese from a file with BIG5 encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_big5.jpg" width="640" height="420" alt="Output from unicode_big5.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Chinese from a file with BIG5 encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_big5.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 80);
    
    
    my $file = 'unicode_big5.txt';
    
    open FH, '<:encoding(big5)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_big5.pl>

=head2 Example: unicode_cp1251.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Russian from a file with CP1251 encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_cp1251.jpg" width="640" height="420" alt="Output from unicode_cp1251.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Russian from a file with CP1251 encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_cp1251.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_cp1251.txt';
    
    open FH, '<:encoding(cp1251)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_cp1251.pl>

=head2 Example: unicode_cp1256.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Arabic text from a CP-1256 encoded file.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_cp1256.jpg" width="640" height="420" alt="Output from unicode_cp1256.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Arabic text from a CP-1256 encoded file.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_cp1256.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_cp1256.txt';
    
    open FH, '<:encoding(cp1256)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_cp1256.pl>

=head2 Example: unicode_koi8r.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Russian from a file with KOI8-R encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_koi8r.jpg" width="640" height="420" alt="Output from unicode_koi8r.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Russian from a file with KOI8-R encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_koi8r.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_koi8r.txt';
    
    open FH, '<:encoding(koi8-r)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_koi8r.pl>

=head2 Example: unicode_polish_utf8.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Polish from a file with UTF8 encoded text.




=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_polish_utf8.jpg" width="640" height="420" alt="Output from unicode_polish_utf8.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Polish from a file with UTF8 encoded text.
    #
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_polish_utf8.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_polish_utf8.txt';
    
    open FH, '<:encoding(utf8)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_polish_utf8.pl>

=head2 Example: unicode_shift_jis.pl



A simple example of converting some Unicode text to an Excel file using
Spreadsheet::WriteExcel and perl 5.8.

This example generates some Japenese text from a file with Shift-JIS
encoded text.



=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/unicode_shift_jis.jpg" width="640" height="420" alt="Output from unicode_shift_jis.pl" /></center></p>

=end html

Source code for this example:

    #!/usr/bin/perl -w
    
    ##############################################################################
    #
    # A simple example of converting some Unicode text to an Excel file using
    # Spreadsheet::WriteExcel and perl 5.8.
    #
    # This example generates some Japenese text from a file with Shift-JIS
    # encoded text.
    #
    # reverse('(c)'), September 2004, John McNamara, jmcnamara@cpan.org
    #
    
    
    
    # Perl 5.8 or later is required for proper utf8 handling. For older perl
    # versions you should use UTF16 and the write_utf16be_string() method.
    # See the write_utf16be_string section of the Spreadsheet::WriteExcel docs.
    #
    require 5.008;
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    my $workbook  = Spreadsheet::WriteExcel->new("unicode_shift_jis.xls");
    my $worksheet = $workbook->add_worksheet();
       $worksheet->set_column('A:A', 50);
    
    
    my $file = 'unicode_shift_jis.txt';
    
    open FH, '<:encoding(shiftjis)', $file  or die "Couldn't open $file: $!\n";
    
    my $row = 0;
    
    while (<FH>) {
        next if /^#/; # Ignore the comments in the sample file.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }
    
    
    __END__
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/unicode_shift_jis.pl>

=head2 Example: csv2xls.pl



Example of how to use the WriteExcel module

Simple program to convert a CSV comma-separated value file to an Excel file.
This is more or less an non-op since Excel can read CSV files.
The program uses Text::CSV_XS to parse the CSV.

Usage: csv2xls.pl file.csv newfile.xls


NOTE: This is only a simple conversion utility for illustrative purposes.
For converting a CSV or Tab separated or any other type of delimited
text file to Excel I recommend the more rigorous csv2xls program that is
part of H.Merijn Brand's Text::CSV_XS module distro.

See the examples/csv2xls link here:
    L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the WriteExcel module
    #
    # Simple program to convert a CSV comma-separated value file to an Excel file.
    # This is more or less an non-op since Excel can read CSV files.
    # The program uses Text::CSV_XS to parse the CSV.
    #
    # Usage: csv2xls.pl file.csv newfile.xls
    #
    #
    # NOTE: This is only a simple conversion utility for illustrative purposes.
    # For converting a CSV or Tab separated or any other type of delimited
    # text file to Excel I recommend the more rigorous csv2xls program that is
    # part of H.Merijn Brand's Text::CSV_XS module distro.
    #
    # See the examples/csv2xls link here:
    #     L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    use Text::CSV_XS;
    
    # Check for valid number of arguments
    if (($#ARGV < 1) || ($#ARGV > 2)) {
       die("Usage: csv2xls csvfile.txt newfile.xls\n");
    };
    
    # Open the Comma Separated Variable file
    open (CSVFILE, $ARGV[0]) or die "$ARGV[0]: $!";
    
    # Create a new Excel workbook
    my $workbook  = Spreadsheet::WriteExcel->new($ARGV[1]);
    my $worksheet = $workbook->add_worksheet();
    
    # Create a new CSV parsing object
    my $csv = Text::CSV_XS->new;
    
    # Row and column are zero indexed
    my $row = 0;
    
    
    while (<CSVFILE>) {
        if ($csv->parse($_)) {
            my @Fld = $csv->fields;
    
            my $col = 0;
            foreach my $token (@Fld) {
                $worksheet->write($row, $col, $token);
                $col++;
            }
            $row++;
        }
        else {
            my $err = $csv->error_input;
            print "Text::CSV_XS parse() failed on argument: ", $err, "\n";
        }
    }


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/csv2xls.pl>

=head2 Example: tab2xls.pl



Example of how to use the WriteExcel module

The following converts a tab separated file into an Excel file

Usage: tab2xls.pl tabfile.txt newfile.xls


NOTE: This is only a simple conversion utility for illustrative purposes.
For converting a CSV or Tab separated or any other type of delimited
text file to Excel I recommend the more rigorous csv2xls program that is
part of H.Merijn Brand's Text::CSV_XS module distro.

See the examples/csv2xls link here:
    L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to use the WriteExcel module
    #
    # The following converts a tab separated file into an Excel file
    #
    # Usage: tab2xls.pl tabfile.txt newfile.xls
    #
    #
    # NOTE: This is only a simple conversion utility for illustrative purposes.
    # For converting a CSV or Tab separated or any other type of delimited
    # text file to Excel I recommend the more rigorous csv2xls program that is
    # part of H.Merijn Brand's Text::CSV_XS module distro.
    #
    # See the examples/csv2xls link here:
    #     L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    # Check for valid number of arguments
    if (($#ARGV < 1) || ($#ARGV > 2)) {
        die("Usage: tab2xls tabfile.txt newfile.xls\n");
    };
    
    
    # Open the tab delimited file
    open (TABFILE, $ARGV[0]) or die "$ARGV[0]: $!";
    
    
    # Create a new Excel workbook
    my $workbook  = Spreadsheet::WriteExcel->new($ARGV[1]);
    my $worksheet = $workbook->add_worksheet();
    
    # Row and column are zero indexed
    my $row = 0;
    
    while (<TABFILE>) {
        chomp;
        # Split on single tab
        my @Fld = split('\t', $_);
    
        my $col = 0;
        foreach my $token (@Fld) {
            $worksheet->write($row, $col, $token);
            $col++;
        }
        $row++;
    }


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/tab2xls.pl>

=head2 Example: datecalc1.pl



NOTE: An easier way of writing dates and times is to use the newer
      write_date_time() Worksheet method. See the date_time.pl example.



Demonstration of writing date/time cells to Excel spreadsheets,
using UNIX/Perl time as source of date/time.



UNIX/Perl time is the time since the Epoch (00:00:00 GMT, 1 Jan 1970)
measured in seconds.

An Excel file can use exactly one of two different date/time systems.
In these systems, a floating point number represents the number of days
(and fractional parts of the day) since a start point. The floating point
number is referred to as a 'serial'.

The two systems ('1900' and '1904') use different starting points:

 '1900'; '1.00' is 1 Jan 1900 BUT 1900 is erroneously regarded as
         a leap year - see:
           http://support.microsoft.com/support/kb/articles/Q181/3/70.asp
         for the excuse^H^H^H^H^H^Hreason.
 '1904'; '1.00' is 2 Jan 1904.

The '1904' system is the default for Apple Macs. Windows versions of
Excel have the option to use the '1904' system.

Note that Visual Basic's "DateSerial" function does NOT erroneously
regard 1900 as a leap year, and thus its serials do not agree with
the 1900 serials of Excel for dates before 1 Mar 1900.

Note that StarOffice (at least at version 5.2) does NOT erroneously
regard 1900 as a leap year, and thus its serials do not agree with
the 1900 serials of Excel for dates before 1 Mar 1900.


    #!/usr/bin/perl -w
    
    
    ######################################################################
    #
    # NOTE: An easier way of writing dates and times is to use the newer
    #       write_date_time() Worksheet method. See the date_time.pl example.
    #
    ######################################################################
    #
    # Demonstration of writing date/time cells to Excel spreadsheets,
    # using UNIX/Perl time as source of date/time.
    #
    ######################################################################
    #
    # UNIX/Perl time is the time since the Epoch (00:00:00 GMT, 1 Jan 1970)
    # measured in seconds.
    #
    # An Excel file can use exactly one of two different date/time systems.
    # In these systems, a floating point number represents the number of days
    # (and fractional parts of the day) since a start point. The floating point
    # number is referred to as a 'serial'.
    #
    # The two systems ('1900' and '1904') use different starting points:
    #
    #  '1900'; '1.00' is 1 Jan 1900 BUT 1900 is erroneously regarded as
    #          a leap year - see:
    #            http://support.microsoft.com/support/kb/articles/Q181/3/70.asp
    #          for the excuse^H^H^H^H^H^Hreason.
    #  '1904'; '1.00' is 2 Jan 1904.
    #
    # The '1904' system is the default for Apple Macs. Windows versions of
    # Excel have the option to use the '1904' system.
    #
    # Note that Visual Basic's "DateSerial" function does NOT erroneously
    # regard 1900 as a leap year, and thus its serials do not agree with
    # the 1900 serials of Excel for dates before 1 Mar 1900.
    #
    # Note that StarOffice (at least at version 5.2) does NOT erroneously
    # regard 1900 as a leap year, and thus its serials do not agree with
    # the 1900 serials of Excel for dates before 1 Mar 1900.
    #
    
    # Copyright 2000, Andrew Benham, adsb@bigfoot.com
    #
    
    ######################################################################
    #
    # Calculation description
    # =======================
    #
    # 1900 system
    # -----------
    # Unix time is '0' at 00:00:00 GMT 1 Jan 1970, i.e. 70 years after 1 Jan 1900.
    # Of those 70 years, 17 (1904,08,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68)
    # were leap years with an extra day.
    # Thus there were 17 + 70*365 days = 25567 days between 1 Jan 1900 and
    # 1 Jan 1970.
    # In the 1900 system, '1' is 1 Jan 1900, but as 1900 was not a leap year
    # 1 Jan 1900 should really be '2', so 1 Jan 1970 is '25569'.
    #
    # 1904 system
    # -----------
    # Unix time is '0' at 00:00:00 GMT 1 Jan 1970, i.e. 66 years after 1 Jan 1904.
    # Of those 66 years, 17 (1904,08,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68)
    # were leap years with an extra day.
    # Thus there were 17 + 66*365 days = 24107 days between 1 Jan 1904 and
    # 1 Jan 1970.
    # In the 1904 system, 2 Jan 1904 being '1', 1 Jan 1970 is '24107'.
    #
    ######################################################################
    #
    # Copyright (c) 2000, Andrew Benham.
    # This program is free software. It may be used, redistributed and/or
    # modified under the same terms as Perl itself.
    #
    # Andrew Benham, adsb@bigfoot.com
    # London, United Kingdom
    # 11 Nov 2000
    #
    ######################################################################
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    use Time::Local;
    
    use vars qw/$DATE_SYSTEM/;
    
    # Use 1900 date system on all platforms other than Apple Mac (for which
    # use 1904 date system).
    $DATE_SYSTEM = ($^O eq 'MacOS') ? 1 : 0;
    
    my $workbook = Spreadsheet::WriteExcel->new("dates.xls");
    my $worksheet = $workbook->add_worksheet();
    
    my $format_date =  $workbook->add_format();
    $format_date->set_num_format('d mmmm yyy');
    
    $worksheet->set_column(0,1,21);
    
    $worksheet->write_string (0,0,"The epoch (GMT)");
    $worksheet->write_number (0,1,&calc_serial(0,1),0x16);
    
    $worksheet->write_string (1,0,"The epoch (localtime)");
    $worksheet->write_number (1,1,&calc_serial(0,0),0x16);
    
    $worksheet->write_string (2,0,"Today");
    $worksheet->write_number (2,1,&calc_serial(),$format_date);
    
    my $christmas2000 = timelocal(0,0,0,25,11,100);
    $worksheet->write_string (3,0,"Christmas 2000");
    $worksheet->write_number (3,1,&calc_serial($christmas2000),$format_date);
    
    $workbook->close();
    
    #-----------------------------------------------------------
    # calc_serial()
    #
    # Called with (up to) 2 parameters.
    #   1.  Unix timestamp.  If omitted, uses current time.
    #   2.  GMT flag. Set to '1' to return serial in GMT.
    #       If omitted, returns serial in appropriate timezone.
    #
    # Returns date/time serial according to $DATE_SYSTEM selected
    #-----------------------------------------------------------
    sub calc_serial {
    	my $time = (defined $_[0]) ? $_[0] : time();
    	my $gmtflag = (defined $_[1]) ? $_[1] : 0;
    
    	# Divide timestamp by number of seconds in a day.
    	# This gives a date serial with '0' on 1 Jan 1970.
    	my $serial = $time / 86400;
    
    	# Adjust the date serial by the offset appropriate to the
    	# currently selected system (1900/1904).
    	if ($DATE_SYSTEM == 0) {	# use 1900 system
    		$serial += 25569;
    	} else {			# use 1904 system
    		$serial += 24107;
    	}
    
    	unless ($gmtflag) {
    		# Now have a 'raw' serial with the right offset. But this
    		# gives a serial in GMT, which is false unless the timezone
    		# is GMT. We need to adjust the serial by the appropriate
    		# timezone offset.
    		# Calculate the appropriate timezone offset by seeing what
    		# the differences between localtime and gmtime for the given
    		# time are.
    
    		my @gmtime = gmtime($time);
    		my @ltime  = localtime($time);
    
    		# For the first 7 elements of the two arrays, adjust the
    		# date serial where the elements differ.
    		for (0 .. 6) {
    			my $diff = $ltime[$_] - $gmtime[$_];
    			if ($diff) {
    				$serial += _adjustment($diff,$_);
    			}
    		}
    	}
    
    	# Perpetuate the error that 1900 was a leap year by decrementing
    	# the serial if we're using the 1900 system and the date is prior to
    	# 1 Mar 1900. This has the effect of making serial value '60'
    	# 29 Feb 1900.
    
    	# This fix only has any effect if UNIX/Perl time on the platform
    	# can represent 1900. Many can't.
    
    	unless ($DATE_SYSTEM) {
    		$serial-- if ($serial < 61);	# '61' is 1 Mar 1900
    	}
    	return $serial;
    }
    
    sub _adjustment {
    	# Based on the difference in the localtime/gmtime array elements
    	# number, return the adjustment required to the serial.
    
    	# We only look at some elements of the localtime/gmtime arrays:
    	#    seconds    unlikely to be different as all known timezones
    	#               have an offset of integral multiples of 15 minutes,
    	#		but it's easy to do.
    	#    minutes    will be different for timezone offsets which are
    	#		not an exact number of hours.
    	#    hours	very likely to be different.
    	#    weekday	will differ when localtime/gmtime difference
    	#		straddles midnight.
    	#
    	# Assume that difference between localtime and gmtime is less than
    	# 5 days, then don't have to do maths for day of month, month number,
    	# year number, etc...
    
    	my ($delta,$element) = @_;
    	my $adjust = 0;
    
    	if ($element == 0) {		# Seconds
    		$adjust = $delta/86400;		# 60 * 60 * 24
    	} elsif ($element == 1) {	# Minutes
    		$adjust = $delta/1440;		# 60 * 24
    	} elsif ($element == 2) {	# Hours
    		$adjust = $delta/24;		# 24
    	} elsif ($element == 6) {	# Day of week number
    		# Catch difference straddling Sat/Sun in either direction
    		$delta += 7 if ($delta < -4);
    		$delta -= 7 if ($delta > 4);
    
    		$adjust = $delta;
    	}
    	return $adjust;
    }
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/datecalc1.pl>

=head2 Example: datecalc2.pl



Example of how to using the Date::Calc module to calculate Excel dates.

NOTE: An easier way of writing dates and times is to use the newer
      write_date_time() Worksheet method. See the date_time.pl example.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # Example of how to using the Date::Calc module to calculate Excel dates.
    #
    # NOTE: An easier way of writing dates and times is to use the newer
    #       write_date_time() Worksheet method. See the date_time.pl example.
    #
    # reverse('(c)'), June 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    use Date::Calc qw(Delta_DHMS); # You may need to install this module.
    
    
    # Create a new workbook and add a worksheet
    my $workbook = Spreadsheet::WriteExcel->new("excel_date2.xls");
    my $worksheet = $workbook->add_worksheet();
    
    # Expand the first column so that the date is visible.
    $worksheet->set_column("A:A", 25);
    
    
    # Add a format for the date
    my $format =  $workbook->add_format();
    $format->set_num_format('d mmmm yyy HH:MM:SS');
    
    
    my $date;
    
    # Write some dates and times
    $date =  excel_date(1900, 1, 1);
    $worksheet->write("A1", $date, $format);
    
    $date =  excel_date(2000, 1, 1);
    $worksheet->write("A2", $date, $format);
    
    $date =  excel_date(2000, 4, 17, 14, 33, 15);
    $worksheet->write("A3", $date, $format);
    
    
    ###############################################################################
    #
    # excel_date($years, $months, $days, $hours, $minutes, $seconds)
    #
    # Create an Excel date in the 1900 format. All of the arguments are optional
    # but you should at least add $years.
    #
    # Corrects for Excel's missing leap day in 1900. See excel_time1.pl for an
    # explanation.
    #
    sub excel_date {
    
        my $years   = $_[0] || 1900;
        my $months  = $_[1] || 1;
        my $days    = $_[2] || 1;
        my $hours   = $_[3] || 0;
        my $minutes = $_[4] || 0;
        my $seconds = $_[5] || 0;
    
        my @date = ($years, $months, $days, $hours, $minutes, $seconds);
        my @epoch = (1899, 12, 31, 0, 0, 0);
    
        ($days, $hours, $minutes, $seconds) = Delta_DHMS(@epoch, @date);
    
        my $date = $days + ($hours*3600 +$minutes*60 +$seconds)/(24*60*60);
    
        # Add a day for Excel's missing leap day in 1900
        $date++ if ($date > 59);
    
        return $date;
    }
    
    ###############################################################################
    #
    # excel_date($years, $months, $days, $hours, $minutes, $seconds)
    #
    # Create an Excel date in the 1904 format. All of the arguments are optional
    # but you should at least add $years.
    #
    # You will also need to call $workbook->set_1904() for this format to be valid.
    #
    sub excel_date_1904 {
    
        my $years   = $_[0] || 1900;
        my $months  = $_[1] || 1;
        my $days    = $_[2] || 1;
        my $hours   = $_[3] || 0;
        my $minutes = $_[4] || 0;
        my $seconds = $_[5] || 0;
    
        my @date = ($years, $months, $days, $hours, $minutes, $seconds);
        my @epoch = (1904, 1, 1, 0, 0, 0);
    
        ($days, $hours, $minutes, $seconds) = Delta_DHMS(@epoch, @date);
    
        my $date = $days + ($hours*3600 +$minutes*60 +$seconds)/(24*60*60);
    
        return $date;
    }
    
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/datecalc2.pl>

=head2 Example: lecxe.pl


Utility program to convert an Excel file into a Spreadsheet::WriteExcel
program using Win32::OLE


    #!/usr/bin/perl -w
    
    #
    # Utility program to convert an Excel file into a Spreadsheet::WriteExcel
    # program using Win32::OLE
    #
    
    #
    # lecxe program
    # by t0mas@netlords.net
    #
    # Version  0.01a    Initial release (alpha)
    
    
    # Modules
    use strict;
    use Win32::OLE;
    use Win32::OLE::Const;
    use Getopt::Std;
    
    
    # Vars
    use vars qw(%opts);
    
    
    # Get options
    getopts('i:o:v',\%opts);
    
    
    # Not enough options
    exit &usage unless ($opts{i} && $opts{o});
    
    
    # Create Excel object
    my $Excel = new Win32::OLE("Excel.Application","Quit") or
            die "Can't start excel: $!";
    
    
    # Get constants
    my $ExcelConst=Win32::OLE::Const->Load("Microsoft Excel");
    
    
    # Show Excel
    $Excel->{Visible} = 1 if ($opts{v});
    
    
    # Open infile
    my $Workbook = $Excel->Workbooks->Open({Filename=>$opts{i}});
    
    
    # Open outfile
    open (OUTFILE,">$opts{o}") or die "Can't open outfile $opts{o}: $!";
    
    
    # Print header for outfile
    print OUTFILE <<'EOH';
    #!/usr/bin/perl -w
    
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    
    use vars qw($workbook %worksheets %formats);
    
    
    $workbook = Spreadsheet::WriteExcel->new("_change_me_.xls");
    
    
    EOH
    
    
    # Loop all sheets
    foreach my $sheetnum (1..$Excel->Workbooks(1)->Worksheets->Count) {
    
    
            # Format sheet
            my $name=$Excel->Workbooks(1)->Worksheets($sheetnum)->Name;
            print "Sheet $name\n" if ($opts{v});
            print OUTFILE "# Sheet $name\n";
            print OUTFILE "\$worksheets{'$name'} = \$workbook->add_worksheet('$name');\n";
    
    
            # Get usedrange of cells in worksheet
            my $usedrange=$Excel->Workbooks(1)->Worksheets($sheetnum)->UsedRange;
    
    
            # Loop all columns in used range
            foreach my $j (1..$usedrange->Columns->Count){
    
    
                    # Format column
                    print "Col $j\n" if ($opts{v});
                    my ($colwidth);
                    $colwidth=$usedrange->Columns($j)->ColumnWidth;
                    print OUTFILE "# Column $j\n";
                    print OUTFILE "\$worksheets{'$name'}->set_column(".($j-1).",".($j-1).
                            ", $colwidth);\n";
    
    
                    # Loop all rows in used range
                    foreach my $i (1..$usedrange->Rows->Count){
    
    
                            # Format row
                            print "Row $i\n" if ($opts{v});
                            print OUTFILE "# Row $i\n";
                            do {
                                    my ($rowheight);
                                    $rowheight=$usedrange->Rows($i)->RowHeight;
                                    print OUTFILE "\$worksheets{'$name'}->set_row(".($i-1).
                                            ", $rowheight);\n";
                            } if ($j==1);
    
    
                            # Start creating cell format
                            my $fname="\$formats{'".$name.'R'.$i.'C'.$j."'}";
                            my $format="$fname=\$workbook->add_format();\n";
                            my $print_format=0;
    
                            # Check for borders
                            my @bfnames=qw(left right top bottom);
                            foreach my $k (1..$usedrange->Cells($i,$j)->Borders->Count) {
                                    my $lstyle=$usedrange->Cells($i,$j)->Borders($k)->LineStyle;
                                    if ($lstyle > 0) {
                                            $format.=$fname."->set_".$bfnames[$k-1]."($lstyle);\n";
                                            $print_format=1;
                                    }
                            }
    
    
                            # Check for font
                            my ($fontattr,$prop,$func,%fontsets,$fontColor);
                            %fontsets=(Name=>'set_font',
                                                    Size=>'set_size');
                            while (($prop,$func) = each %fontsets) {
                                    $fontattr=$usedrange->Cells($i,$j)->Font->$prop;
                                    if ($fontattr ne "") {
                                            $format.=$fname."->$func('$fontattr');\n";
                                            $print_format=1;
                                    }
    
    
                            }
                            %fontsets=(Bold=>'set_bold(1)',
                                                    Italic=>'set_italic(1)',
                                                    Underline=>'set_underline(1)',
                                                    Strikethrough=>'set_strikeout(1)',
                                                    Superscript=>'set_script(1)',
                                                    Subscript=>'set_script(2)',
                                                    OutlineFont=>'set_outline(1)',
                                                    Shadow=>'set_shadow(1)');
                            while (($prop,$func) = each %fontsets) {
                                    $fontattr=$usedrange->Cells($i,$j)->Font->$prop;
                                    if ($fontattr==1) {
                                            $format.=$fname."->$func;\n" ;
    
                                            $print_format=1;
                                    }
                            }
                            $fontColor=$usedrange->Cells($i,$j)->Font->ColorIndex();
                            if ($fontColor>0&&$fontColor!=$ExcelConst->{xlColorIndexAutomatic}) {
                                    $format.=$fname."->set_color(".($fontColor+7).");\n" ;
                                    $print_format=1;
                            }
    
    
    
                            # Check text alignment, merging and wrapping
                            my ($halign,$valign,$merge,$wrap);
                            $halign=$usedrange->Cells($i,$j)->HorizontalAlignment;
                            my %hAligns=($ExcelConst->{xlHAlignCenter}=>"'center'",
                                    $ExcelConst->{xlHAlignJustify}=>"'justify'",
                                    $ExcelConst->{xlHAlignLeft}=>"'left'",
                                    $ExcelConst->{xlHAlignRight}=>"'right'",
                                    $ExcelConst->{xlHAlignFill}=>"'fill'",
                                    $ExcelConst->{xlHAlignCenterAcrossSelection}=>"'merge'");
                            if ($halign!=$ExcelConst->{xlHAlignGeneral}) {
                                    $format.=$fname."->set_align($hAligns{$halign});\n";
                                    $print_format=1;
                            }
                            $valign=$usedrange->Cells($i,$j)->VerticalAlignment;
                            my %vAligns=($ExcelConst->{xlVAlignBottom}=>"'bottom'",
                                    $ExcelConst->{xlVAlignCenter}=>"'vcenter'",
                                    $ExcelConst->{xlVAlignJustify}=>"'vjustify'",
                                    $ExcelConst->{xlVAlignTop}=>"'top'");
                            if ($valign) {
                                    $format.=$fname."->set_align($vAligns{$valign});\n";
                                    $print_format=1;
                            }
                            $merge=$usedrange->Cells($i,$j)->MergeCells;
                            if ($merge==1) {
                                    $format.=$fname."->set_merge();\n";
    
                                    $print_format=1;
                            }
                            $wrap=$usedrange->Cells($i,$j)->WrapText;
                            if ($wrap==1) {
                                    $format.=$fname."->set_text_wrap(1);\n";
    
                                    $print_format=1;
                            }
    
    
                            # Check patterns
                            my ($pattern,%pats);
                            %pats=(-4142=>0,-4125=>2,-4126=>3,-4124=>4,-4128=>5,-4166=>6,
                                            -4121=>7,-4162=>8);
                            $pattern=$usedrange->Cells($i,$j)->Interior->Pattern;
                            if ($pattern&&$pattern!=$ExcelConst->{xlPatternAutomatic}) {
                                    $pattern=$pats{$pattern} if ($pattern<0 && defined $pats{$pattern});
                                    $format.=$fname."->set_pattern($pattern);\n";
    
                                    # Colors fg/bg
                                    my ($cIndex);
                                    $cIndex=$usedrange->Cells($i,$j)->Interior->PatternColorIndex;
                                    if ($cIndex>0&&$cIndex!=$ExcelConst->{xlColorIndexAutomatic}) {
                                            $format.=$fname."->set_bg_color(".($cIndex+7).");\n";
                                    }
                                    $cIndex=$usedrange->Cells($i,$j)->Interior->ColorIndex;
                                    if ($cIndex>0&&$cIndex!=$ExcelConst->{xlColorIndexAutomatic}) {
                                            $format.=$fname."->set_fg_color(".($cIndex+7).");\n";
                                    }
                                    $print_format=1;
                            }
    
    
                            # Check for number format
                            my ($num_format);
                            $num_format=$usedrange->Cells($i,$j)->NumberFormat;
                            if ($num_format ne "") {
                                    $format.=$fname."->set_num_format('$num_format');\n";
                                    $print_format=1;
                            }
    
    
                            # Check for contents (text or formula)
                            my ($contents);
                            $contents=$usedrange->Cells($i,$j)->Formula;
                            $contents=$usedrange->Cells($i,$j)->Text if ($contents eq "");
    
    
                            # Print cell
                            if ($contents ne "" or $print_format) {
                                    print OUTFILE "# Cell($i,$j)\n";
                                    print OUTFILE $format if ($print_format);
                                    print OUTFILE "\$worksheets{'$name'}->write(".($i-1).",".($j-1).
                                            ",'$contents'";
                                    print OUTFILE ",$fname" if ($print_format);
                                    print OUTFILE ");\n";
                            }
                    }
            }
    }
    
    
    # Famous last words...
    print OUTFILE "\$workbook->close();\n";
    
    
    # Close outfile
    close (OUTFILE) or die "Can't close outfile $opts{o}: $!";
    
    
    ####################################################################
    sub usage {
            printf STDERR "usage: $0 [options]\n".
                    "\tOptions:\n".
                    "\t\t-v       \tverbose mode\n" .
                    "\t\t-i <name>\tname of input file\n" .
                    "\t\t-o <name>\tname of output file\n";
    }
    
    
    ####################################################################
    sub END {
            # Quit excel
            do {
                    $Excel->{DisplayAlerts} = 0;
                    $Excel->Quit;
            } if (defined $Excel);
    }
    
    
    __END__
    
    
    =head1 NAME
    
    
    lecxe - A Excel file to Spreadsheet::WriteExcel code converter
    
    
    =head1 DESCRIPTION
    
    
    This program takes an MS Excel workbook file as input and from
    that file, produces an output file with Perl code that uses the
    Spreadsheet::WriteExcel module to reproduce the original
    file.
    
    
    =head1 STUFF
    
    
    Additional hands-on editing of the output file might be neccecary
    as:
    
    
    * This program always names the file produced by output script
      _change_me_.xls
    
    
    * Users of international Excel versions will have som work to do
      on list separators and numeric punctation characters.
    
    
    =head1 SEE ALSO
    
    
    L<Win32::OLE>, L<Win32::OLE::Variant>, L<Spreadsheet::WriteExcel>
    
    
    =head1 BUGS
    
    
    * Picks wrong color on cells sometimes.
    
    
    * Probably a few other...
    
    
    =head1 DISCLAIMER
    
    
    I do not guarantee B<ANYTHING> with this program. If you use it you
    are doing so B<AT YOUR OWN RISK>! I may or may not support this
    depending on my time schedule...
    
    
    =head1 AUTHOR
    
    
    t0mas@netlords.net
    
    
    =head1 COPYRIGHT
    
    
    Copyright 2001, t0mas@netlords.net
    
    
    This package is free software; you can redistribute it and/or
    modify it under the same terms as Perl itself.


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/lecxe.pl>

=head2 Example: convertA1.pl



This program contains helper functions to deal with the Excel A1 cell
reference  notation.

These functions have been superseded by L<Spreadsheet::WriteExcel::Utility>.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This program contains helper functions to deal with the Excel A1 cell
    # reference  notation.
    #
    # These functions have been superseded by L<Spreadsheet::WriteExcel::Utility>.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    
    print "\n";
    print "Cell B7   is equivalent to (";
    print join " ", cell_to_rowcol('B7');
    print ") in row column notation.\n";
    
    print "Cell \$B7  is equivalent to (";
    print join " ", cell_to_rowcol('$B7');
    print ") in row column notation.\n";
    
    print "Cell B\$7  is equivalent to (";
    print join " ", cell_to_rowcol('B$7');
    print ") in row column notation.\n";
    
    print "Cell \$B\$7 is equivalent to (";
    print join " ", cell_to_rowcol('$B$7');
    print ") in row column notation.\n\n";
    
    print "Row and column (1999, 29)       are equivalent to ";
    print rowcol_to_cell(1999, 29),   ".\n";
    
    print "Row and column (1999, 29, 0, 1) are equivalent to ";
    print rowcol_to_cell(1999, 29, 0, 1),   ".\n\n";
    
    print "The base cell is:     Z7\n";
    print "Increment the row:    ", inc_cell_row('Z7'), "\n";
    print "Decrement the row:    ", dec_cell_row('Z7'), "\n";
    print "Increment the column: ", inc_cell_col('Z7'), "\n";
    print "Decrement the column: ", dec_cell_col('Z7'), "\n\n";
    
    
    ###############################################################################
    #
    # rowcol_to_cell($row, $col, $row_absolute, $col_absolute)
    #
    # Convert a zero based row and column reference to a A1 reference. For example
    # (0, 2) to C1. $row_absolute, $col_absolute are optional. They are boolean
    # values used to indicate if the row or column value is absolute, i.e. if it is
    # prefixed by a $ sign: eg. (0, 2, 0, 1) converts to $C1.
    #
    # Returns: a cell reference string.
    #
    sub rowcol_to_cell {
    
        my $row     = $_[0];
        my $col     = $_[1];
        my $row_abs = $_[2] || 0;
        my $col_abs = $_[3] || 0;
    
    
        if ($row_abs) {
            $row_abs = '$'
        }
        else {
            $row_abs = ''
        }
    
        if ($col_abs) {
            $col_abs = '$'
        }
        else {
            $col_abs = ''
        }
    
    
        my $int  = int ($col / 26);
        my $frac = $col % 26 +1;
    
        my $chr1 ='';
        my $chr2 ='';
    
    
        if ($frac != 0) {
            $chr2 = chr (ord('A') + $frac -1);
        }
    
        if ($int > 0) {
            $chr1 = chr (ord('A') + $int  -1);
        }
    
        $row++;     # Zero index to 1-index
    
        return $col_abs . $chr1 . $chr2 . $row_abs. $row;
    }
    
    
    ###############################################################################
    #
    # cell_to_rowcol($cell_ref)
    #
    # Convert an Excel cell reference in A1 notation to a zero based row and column
    # reference; converts C1 to (0, 2, 0, 0).
    #
    # Returns: row, column, row_is_absolute, column_is_absolute
    #
    #
    sub cell_to_rowcol {
    
        my $cell = shift;
    
        $cell =~ /(\$?)([A-I]?[A-Z])(\$?)(\d+)/;
    
        my $col_abs = $1 eq "" ? 0 : 1;
        my $col     = $2;
        my $row_abs = $3 eq "" ? 0 : 1;
        my $row     = $4;
    
        # Convert base26 column string to number
        # All your Base are belong to us.
        my @chars  = split //, $col;
        my $expn   = 0;
        $col       = 0;
    
        while (@chars) {
            my $char = pop(@chars); # LS char first
            $col += (ord($char) -ord('A') +1) * (26**$expn);
            $expn++;
        }
    
        # Convert 1-index to zero-index
        $row--;
        $col--;
    
        return $row, $col, $row_abs, $col_abs;
    }
    
    
    ###############################################################################
    #
    # inc_cell_row($cell_ref)
    #
    # Increments the row number of an Excel cell reference in A1 notation.
    # For example C3 to C4
    #
    # Returns: a cell reference string.
    #
    sub inc_cell_row {
    
        my $cell = shift;
        my ($row, $col, $row_abs, $col_abs) = cell_to_rowcol($cell);
    
        $row++;
    
        return rowcol_to_cell($row, $col, $row_abs, $col_abs);
    }
    
    
    ###############################################################################
    #
    # dec_cell_row($cell_ref)
    #
    # Decrements the row number of an Excel cell reference in A1 notation.
    # For example C4 to C3
    #
    # Returns: a cell reference string.
    #
    sub dec_cell_row {
    
        my $cell = shift;
        my ($row, $col, $row_abs, $col_abs) = cell_to_rowcol($cell);
    
        $row--;
    
        return rowcol_to_cell($row, $col, $row_abs, $col_abs);
    }
    
    
    ###############################################################################
    #
    # inc_cell_col($cell_ref)
    #
    # Increments the column number of an Excel cell reference in A1 notation.
    # For example C3 to D3
    #
    # Returns: a cell reference string.
    #
    sub inc_cell_col {
    
        my $cell = shift;
        my ($row, $col, $row_abs, $col_abs) = cell_to_rowcol($cell);
    
        $col++;
    
        return rowcol_to_cell($row, $col, $row_abs, $col_abs);
    }
    
    
    ###############################################################################
    #
    # dec_cell_col($cell_ref)
    #
    # Decrements the column number of an Excel cell reference in A1 notation.
    # For example D3 to C3
    #
    # Returns: a cell reference string.
    #
    sub dec_cell_col {
    
        my $cell = shift;
        my ($row, $col, $row_abs, $col_abs) = cell_to_rowcol($cell);
    
        $col--;
    
        return rowcol_to_cell($row, $col, $row_abs, $col_abs);
    }
    


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/convertA1.pl>


=head2 Example: writeA1.pl



This is an example of how to extend the Spreadsheet::WriteExcel module.

Code is appended to the Spreadsheet::WriteExcel::Worksheet module by reusing
the package name. The new code provides a write() method that allows you to
use Excels A1 style cell references.  This is not particularly useful but it
serves as an example of how the module can be extended without modifying the
code directly.



    #!/usr/bin/perl -w
    
    ###############################################################################
    #
    # This is an example of how to extend the Spreadsheet::WriteExcel module.
    #
    # Code is appended to the Spreadsheet::WriteExcel::Worksheet module by reusing
    # the package name. The new code provides a write() method that allows you to
    # use Excels A1 style cell references.  This is not particularly useful but it
    # serves as an example of how the module can be extended without modifying the
    # code directly.
    #
    # reverse('(c)'), March 2001, John McNamara, jmcnamara@cpan.org
    #
    
    use strict;
    use Spreadsheet::WriteExcel;
    
    # Create a new workbook called simple.xls and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new("writeA1.xls");
    my $worksheet = $workbook->add_worksheet();
    
    # Write numbers or text
    $worksheet->write  (0, 0, "Hello");
    $worksheet->writeA1("A3", "A3"   );
    $worksheet->writeA1("A5", 1.2345 );
    
    
    ###############################################################################
    #
    # The following will be appended to the Spreadsheet::WriteExcel::Worksheet
    # package.
    #
    
    package Spreadsheet::WriteExcel::Worksheet;
    
    ###############################################################################
    #
    # writeA1($cell, $token, $format)
    #
    # Convert $cell from Excel A1 notation to $row, $col notation and
    # call write() on $token.
    #
    # Returns: return value of called subroutine or -4 for invalid cell
    # reference.
    #
    sub writeA1 {
        my $self = shift;
        my $cell = shift;
        my $col;
        my $row;
    
        if ($cell =~ /([A-z]+)(\d+)/) {
           ($row, $col) = _convertA1($2, $1);
           $self->write($row, $col, @_);
        } else {
            return -4;
        }
    }
    
    ###############################################################################
    #
    # _convertA1($row, $col)
    #
    # Convert Excel A1 notation to $row, $col notation. Convert base26 column
    # string to a number.
    #
    sub _convertA1 {
        my $row    = $_[0];
        my $col    = $_[1]; # String in AA notation
    
        my @chars  = split //, $col;
        my $expn   = 0;
        $col       = 0;
    
        while (@chars) {
            my $char = uc(pop(@chars)); # LS char first
            $col += (ord($char) -ord('A') +1) * (26**$expn);
            $expn++;
        }
    
        # Convert 1 index to 0 index
        $row--;
        $col--;
    
        return($row, $col);
    }


Download this example: L<http://cpansearch.perl.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.40/examples/writeA1.pl>

=head1 AUTHOR

John McNamara jmcnamara@cpan.org

Contributed examples contain the original author's name.

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.

=cut
