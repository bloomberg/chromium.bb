package Spreadsheet::WriteExcel;

###############################################################################
#
# WriteExcel.
#
# Spreadsheet::WriteExcel - Write to a cross-platform Excel binary file.
#
# Copyright 2000-2012, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

use Exporter;

use strict;
use Spreadsheet::WriteExcel::Workbook;



use vars qw($VERSION @ISA);
@ISA = qw(Spreadsheet::WriteExcel::Workbook Exporter);

$VERSION = '2.40'; # A farewell to arms.



###############################################################################
#
# new()
#
# Constructor. Wrapper for a Workbook object.
# uses: Spreadsheet::WriteExcel::BIFFwriter
#       Spreadsheet::WriteExcel::Chart
#       Spreadsheet::WriteExcel::OLEwriter
#       Spreadsheet::WriteExcel::Workbook
#       Spreadsheet::WriteExcel::Worksheet
#       Spreadsheet::WriteExcel::Format
#       Spreadsheet::WriteExcel::Formula
#       Spreadsheet::WriteExcel::Properties
#
sub new {

    my $class = shift;
    my $self  = Spreadsheet::WriteExcel::Workbook->new(@_);

    # Check for file creation failures before re-blessing
    bless  $self, $class if defined $self;

    return $self;
}


1;


__END__



=head1 NAME

Spreadsheet::WriteExcel - Write to a cross-platform Excel binary file.

=head1 VERSION

This document refers to version 2.40 of Spreadsheet::WriteExcel, released November 6, 2013.




=head1 SYNOPSIS

To write a string, a formatted string, a number and a formula to the first worksheet in an Excel workbook called perl.xls:

    use Spreadsheet::WriteExcel;

    # Create a new Excel workbook
    my $workbook = Spreadsheet::WriteExcel->new('perl.xls');

    # Add a worksheet
    $worksheet = $workbook->add_worksheet();

    #  Add and define a format
    $format = $workbook->add_format(); # Add a format
    $format->set_bold();
    $format->set_color('red');
    $format->set_align('center');

    # Write a formatted and unformatted string, row and column notation.
    $col = $row = 0;
    $worksheet->write($row, $col, 'Hi Excel!', $format);
    $worksheet->write(1,    $col, 'Hi Excel!');

    # Write a number and a formula using A1 notation
    $worksheet->write('A3', 1.2345);
    $worksheet->write('A4', '=SIN(PI()/4)');




=head1 DESCRIPTION

The Spreadsheet::WriteExcel Perl module can be used to create a cross-platform Excel binary file. Multiple worksheets can be added to a workbook and formatting can be applied to cells. Text, numbers, formulas, hyperlinks, images and charts can be written to the cells.

The file produced by this module is compatible with Excel 97, 2000, 2002, 2003 and 2007.

The module will work on the majority of Windows, UNIX and Mac platforms. Generated files are also compatible with the Linux/UNIX spreadsheet applications Gnumeric and OpenOffice.org.

This module cannot be used to write to an existing Excel file (See L</MODIFYING AND REWRITING EXCEL FILES>).

B<Note: This module is in maintenance only mode> and in future will only be updated with bug fixes. The newer, more feature rich and API compatible L<Excel::Writer::XLSX> module is recommended instead. See, L</Migrating to Excel::Writer::XLSX>.



=head1 QUICK START

Spreadsheet::WriteExcel tries to provide an interface to as many of Excel's features as possible. As a result there is a lot of documentation to accompany the interface and it can be difficult at first glance to see what it important and what is not. So for those of you who prefer to assemble Ikea furniture first and then read the instructions, here are three easy steps:

1. Create a new Excel I<workbook> (i.e. file) using C<new()>.

2. Add a I<worksheet> to the new workbook using C<add_worksheet()>.

3. Write to the worksheet using C<write()>.

Like this:

    use Spreadsheet::WriteExcel;                             # Step 0

    my $workbook = Spreadsheet::WriteExcel->new('perl.xls'); # Step 1
    $worksheet   = $workbook->add_worksheet();               # Step 2
    $worksheet->write('A1', 'Hi Excel!');                    # Step 3

This will create an Excel file called C<perl.xls> with a single worksheet and the text C<'Hi Excel!'> in the relevant cell. And that's it. Okay, so there is actually a zeroth step as well, but C<use module> goes without saying. There are also more than 80 examples that come with the distribution and which you can use to get you started. See L</EXAMPLES>.

Those of you who read the instructions first and assemble the furniture afterwards will know how to proceed. ;-)




=head1 WORKBOOK METHODS

The Spreadsheet::WriteExcel module provides an object oriented interface to a new Excel workbook. The following methods are available through a new workbook.

    new()
    add_worksheet()
    add_format()
    add_chart()
    add_chart_ext()
    close()
    compatibility_mode()
    set_properties()
    define_name()
    set_tempdir()
    set_custom_color()
    sheets()
    set_1904()
    set_codepage()

If you are unfamiliar with object oriented interfaces or the way that they are implemented in Perl have a look at C<perlobj> and C<perltoot> in the main Perl documentation.




=head2 new()

A new Excel workbook is created using the C<new()> constructor which accepts either a filename or a filehandle as a parameter. The following example creates a new Excel file based on a filename:

    my $workbook  = Spreadsheet::WriteExcel->new('filename.xls');
    my $worksheet = $workbook->add_worksheet();
    $worksheet->write(0, 0, 'Hi Excel!');

Here are some other examples of using C<new()> with filenames:

    my $workbook1 = Spreadsheet::WriteExcel->new($filename);
    my $workbook2 = Spreadsheet::WriteExcel->new('/tmp/filename.xls');
    my $workbook3 = Spreadsheet::WriteExcel->new("c:\\tmp\\filename.xls");
    my $workbook4 = Spreadsheet::WriteExcel->new('c:\tmp\filename.xls');

The last two examples demonstrates how to create a file on DOS or Windows where it is necessary to either escape the directory separator C<\> or to use single quotes to ensure that it isn't interpolated. For more information  see C<perlfaq5: Why can't I use "C:\temp\foo" in DOS paths?>.

The C<new()> constructor returns a Spreadsheet::WriteExcel object that you can use to add worksheets and store data. It should be noted that although C<my> is not specifically required it defines the scope of the new workbook variable and, in the majority of cases, ensures that the workbook is closed properly without explicitly calling the C<close()> method.

If the file cannot be created, due to file permissions or some other reason,  C<new> will return C<undef>. Therefore, it is good practice to check the return value of C<new> before proceeding. As usual the Perl variable C<$!> will be set if there is a file creation error. You will also see one of the warning messages detailed in L</DIAGNOSTICS>:

    my $workbook  = Spreadsheet::WriteExcel->new('protected.xls');
    die "Problems creating new Excel file: $!" unless defined $workbook;

You can also pass a valid filehandle to the C<new()> constructor. For example in a CGI program you could do something like this:

    binmode(STDOUT);
    my $workbook  = Spreadsheet::WriteExcel->new(\*STDOUT);

The requirement for C<binmode()> is explained below.

See also, the C<cgi.pl> program in the C<examples> directory of the distro.

However, this special case will not work in C<mod_perl> programs where you will have to do something like the following:

    # mod_perl 1
    ...
    tie *XLS, 'Apache';
    binmode(XLS);
    my $workbook  = Spreadsheet::WriteExcel->new(\*XLS);
    ...

    # mod_perl 2
    ...
    tie *XLS => $r;  # Tie to the Apache::RequestRec object
    binmode(*XLS);
    my $workbook  = Spreadsheet::WriteExcel->new(\*XLS);
    ...

See also, the C<mod_perl1.pl> and C<mod_perl2.pl> programs in the C<examples> directory of the distro.

Filehandles can also be useful if you want to stream an Excel file over a socket or if you want to store an Excel file in a scalar.

For example here is a way to write an Excel file to a scalar with C<perl 5.8>:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    # Requires perl 5.8 or later
    open my $fh, '>', \my $str or die "Failed to open filehandle: $!";

    my $workbook  = Spreadsheet::WriteExcel->new($fh);
    my $worksheet = $workbook->add_worksheet();

    $worksheet->write(0, 0,  'Hi Excel!');

    $workbook->close();

    # The Excel file in now in $str. Remember to binmode() the output
    # filehandle before printing it.
    binmode STDOUT;
    print $str;

See also the C<write_to_scalar.pl> and C<filehandle.pl> programs in the C<examples> directory of the distro.

B<Note about the requirement for> C<binmode()>. An Excel file is comprised of binary data. Therefore, if you are using a filehandle you should ensure that you C<binmode()> it prior to passing it to C<new()>.You should do this regardless of whether you are on a Windows platform or not. This applies especially to users of perl 5.8 on systems where C<UTF-8> is likely to be in operation such as RedHat Linux 9. If your program, either intentionally or not, writes C<UTF-8> data to a filehandle that is passed to C<new()> it will corrupt the Excel file that is created.

You don't have to worry about C<binmode()> if you are using filenames instead of filehandles. Spreadsheet::WriteExcel performs the C<binmode()> internally when it converts the filename to a filehandle. For more information about C<binmode()> see C<perlfunc> and C<perlopentut> in the main Perl documentation.





=head2 add_worksheet($sheetname, $utf_16_be)

At least one worksheet should be added to a new workbook. A worksheet is used to write data into cells:

    $worksheet1 = $workbook->add_worksheet();           # Sheet1
    $worksheet2 = $workbook->add_worksheet('Foglio2');  # Foglio2
    $worksheet3 = $workbook->add_worksheet('Data');     # Data
    $worksheet4 = $workbook->add_worksheet();           # Sheet4

If C<$sheetname> is not specified the default Excel convention will be followed, i.e. Sheet1, Sheet2, etc. The C<$utf_16_be> parameter is optional, see below.

The worksheet name must be a valid Excel worksheet name, i.e. it cannot contain any of the following characters, C<[ ] : * ? / \> and it must be less than 32 characters. In addition, you cannot use the same, case insensitive, C<$sheetname> for more than one worksheet.

On systems with C<perl 5.8> and later the C<add_worksheet()> method will also handle strings in C<UTF-8> format.

    $worksheet = $workbook->add_worksheet("\x{263a}"); # Smiley

On earlier Perl systems your can specify C<UTF-16BE> worksheet names using an additional optional parameter:

    my $name = pack 'n', 0x263a;
    $worksheet = $workbook->add_worksheet($name, 1);   # Smiley




=head2 add_format(%properties)

The C<add_format()> method can be used to create new Format objects which are used to apply formatting to a cell. You can either define the properties at creation time via a hash of property values or later via method calls.

    $format1 = $workbook->add_format(%props); # Set properties at creation
    $format2 = $workbook->add_format();       # Set properties later

See the L</CELL FORMATTING> section for more details about Format properties and how to set them.




=head2 add_chart(%properties)

This method is use to create a new chart either as a standalone worksheet (the default) or as an embeddable object that can be inserted into a worksheet via the C<insert_chart()> Worksheet method.

    my $chart = $workbook->add_chart( type => 'column' );

The properties that can be set are:

    type     (required)
    name     (optional)
    embedded (optional)

=over

=item * C<type>

This is a required parameter. It defines the type of chart that will be created.

    my $chart = $workbook->add_chart( type => 'line' );

The available types are:

    area
    bar
    column
    line
    pie
    scatter
    stock

=item * C<name>

Set the name for the chart sheet. The name property is optional and if it isn't supplied will default to C<Chart1 .. n>. The name must be a valid Excel worksheet name. See C<add_worksheet()> for more details on valid sheet names. The C<name> property can be omitted for embedded charts.

    my $chart = $workbook->add_chart( type => 'line', name => 'Results Chart' );

=item * C<embedded>

Specifies that the Chart object will be inserted in a worksheet via the C<insert_chart()> Worksheet method. It is an error to try insert a Chart that doesn't have this flag set.

    my $chart = $workbook->add_chart( type => 'line', embedded => 1 );

    # Configure the chart.
    ...

    # Insert the chart into the a worksheet.
    $worksheet->insert_chart( 'E2', $chart );

=back

See L<Spreadsheet::WriteExcel::Chart> for details on how to configure the chart object once it is created. See also the C<chart_*.pl> programs in the examples directory of the distro.




=head2 add_chart_ext($chart_data, $chartname)

This method is use to include externally generated charts in a Spreadsheet::WriteExcel file.

    my $chart = $workbook->add_chart_ext('chart01.bin', 'Chart1');

This feature is semi-deprecated in favour of the "native" charts created using C<add_chart()>. Read C<external_charts.txt> (or C<.pod>) in the external_charts directory of the distro for a full explanation.




=head2 close()

In general your Excel file will be closed automatically when your program ends or when the Workbook object goes out of scope, however the C<close()> method can be used to explicitly close an Excel file.

    $workbook->close();

An explicit C<close()> is required if the file must be closed prior to performing some external action on it such as copying it, reading its size or attaching it to an email.

In addition, C<close()> may be required to prevent perl's garbage collector from disposing of the Workbook, Worksheet and Format objects in the wrong order. Situations where this can occur are:

=over 4

=item *

If C<my()> was not used to declare the scope of a workbook variable created using C<new()>.

=item *

If the C<new()>, C<add_worksheet()> or C<add_format()> methods are called in subroutines.

=back

The reason for this is that Spreadsheet::WriteExcel relies on Perl's C<DESTROY> mechanism to trigger destructor methods in a specific sequence. This may not happen in cases where the Workbook, Worksheet and Format variables are not lexically scoped or where they have different lexical scopes.

In general, if you create a file with a size of 0 bytes or you fail to create a file you need to call C<close()>.

The return value of C<close()> is the same as that returned by perl when it closes the file created by C<new()>. This allows you to handle error conditions in the usual way:

    $workbook->close() or die "Error closing file: $!";




=head2 compatibility_mode()

This method is used to improve compatibility with third party applications that read Excel files.

    $workbook->compatibility_mode();

An Excel file is comprised of binary records that describe properties of a spreadsheet. Excel is reasonably liberal about this and, outside of a core subset, it doesn't require every possible record to be present when it reads a file. This is also true of Gnumeric and OpenOffice.Org Calc.

Spreadsheet::WriteExcel takes advantage of this fact to omit some records in order to minimise the amount of data stored in memory and to simplify and speed up the writing of files. However, some third party applications that read Excel files often expect certain records to be present. In "compatibility mode" Spreadsheet::WriteExcel writes these records and tries to be as close to an Excel generated file as possible.

Applications that require C<compatibility_mode()> are Apache POI, Apple Numbers, and Quickoffice on Nokia, Palm and other devices. You should also use C<compatibility_mode()> if your Excel file will be used as an external data source by another Excel file.

If you encounter other situations that require C<compatibility_mode()>, please let me know.

It should be noted that C<compatibility_mode()> requires additional data to be stored in memory and additional processing. This incurs a memory and speed penalty and may not be suitable for very large files (>20MB).

You must call C<compatibility_mode()> before calling C<add_worksheet()>.




=head2 set_properties()

The C<set_properties> method can be used to set the document properties of the Excel file created by C<Spreadsheet::WriteExcel>. These properties are visible when you use the C<< File->Properties >> menu option in Excel and are also available to external applications that read or index windows files.

The properties should be passed as a hash of values as follows:

    $workbook->set_properties(
        title    => 'This is an example spreadsheet',
        author   => 'John McNamara',
        comments => 'Created with Perl and Spreadsheet::WriteExcel',
    );

The properties that can be set are:

    title
    subject
    author
    manager
    company
    category
    keywords
    comments

User defined properties are not supported due to effort required.

In perl 5.8+ you can also pass UTF-8 strings as properties. See L</UNICODE IN EXCEL>.

    my $smiley = chr 0x263A;

    $workbook->set_properties(
        subject => "Happy now? $smiley",
    );

With older versions of perl you can use a module to convert a non-ASCII string to a binary representation of UTF-8 and then pass an additional C<utf8> flag to C<set_properties()>:

    my $smiley = pack 'H*', 'E298BA';

    $workbook->set_properties(
        subject => "Happy now? $smiley",
        utf8    => 1,
    );

Usually Spreadsheet::WriteExcel allows you to use UTF-16 with pre 5.8 versions of perl. However, document properties don't support UTF-16 for these type of strings.

In order to promote the usefulness of Perl and the Spreadsheet::WriteExcel module consider adding a comment such as the following when using document properties:

    $workbook->set_properties(
        ...,
        comments => 'Created with Perl and Spreadsheet::WriteExcel',
        ...,
    );

This feature requires that the C<OLE::Storage_Lite> module is installed (which is usually the case for a standard Spreadsheet::WriteExcel installation). However, this also means that the resulting OLE document may B<possibly> be buggy for files less than 7MB since it hasn't been as rigorously tested in that domain. As a result of this C<set_properties> is currently incompatible with Gnumeric for files less than 7MB. This is being investigated. If you encounter any problems with this features let me know.

For convenience it is possible to pass either a hash or hash ref of arguments to this method.

See also the C<properties.pl> program in the examples directory of the distro.




=head2 define_name()

This method is used to defined a name that can be used to represent a value, a single cell or a range of cells in a workbook.

    $workbook->define_name('Exchange_rate', '=0.96');
    $workbook->define_name('Sales',         '=Sheet1!$G$1:$H$10');
    $workbook->define_name('Sheet2!Sales',  '=Sheet2!$G$1:$G$10');

See the defined_name.pl program in the examples dir of the distro.

Note: This currently a beta feature. More documentation and examples will be added.




=head2 set_tempdir()

For speed and efficiency C<Spreadsheet::WriteExcel> stores worksheet data in temporary files prior to assembling the final workbook.

If Spreadsheet::WriteExcel is unable to create these temporary files it will store the required data in memory. This can be slow for large files.

The problem occurs mainly with IIS on Windows although it could feasibly occur on Unix systems as well. The problem generally occurs because the default temp file directory is defined as C<C:/> or some other directory that IIS doesn't provide write access to.

To check if this might be a problem on a particular system you can run a simple test program with C<-w> or C<use warnings>. This will generate a warning if the module cannot create the required temporary files:

    #!/usr/bin/perl -w

    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new('test.xls');
    my $worksheet = $workbook->add_worksheet();

To avoid this problem the C<set_tempdir()> method can be used to specify a directory that is accessible for the creation of temporary files.

The C<File::Temp> module is used to create the temporary files. File::Temp uses C<File::Spec> to determine an appropriate location for these files such as C</tmp> or C<c:\windows\temp>. You can find out which directory is used on your system as follows:

    perl -MFile::Spec -le "print File::Spec->tmpdir"

Even if the default temporary file directory is accessible you may wish to specify an alternative location for security or maintenance reasons:

    $workbook->set_tempdir('/tmp/writeexcel');
    $workbook->set_tempdir('c:\windows\temp\writeexcel');

The directory for the temporary file must exist, C<set_tempdir()> will not create a new directory.

One disadvantage of using the C<set_tempdir()> method is that on some Windows systems it will limit you to approximately 800 concurrent tempfiles. This means that a single program running on one of these systems will be limited to creating a total of 800 workbook and worksheet objects. You can run multiple, non-concurrent programs to work around this if necessary.




=head2 set_custom_color($index, $red, $green, $blue)

The C<set_custom_color()> method can be used to override one of the built-in palette values with a more suitable colour.

The value for C<$index> should be in the range 8..63, see L</COLOURS IN EXCEL>.

The default named colours use the following indices:

     8   =>   black
     9   =>   white
    10   =>   red
    11   =>   lime
    12   =>   blue
    13   =>   yellow
    14   =>   magenta
    15   =>   cyan
    16   =>   brown
    17   =>   green
    18   =>   navy
    20   =>   purple
    22   =>   silver
    23   =>   gray
    33   =>   pink
    53   =>   orange

A new colour is set using its RGB (red green blue) components. The C<$red>, C<$green> and C<$blue> values must be in the range 0..255. You can determine the required values in Excel using the C<Tools-E<gt>Options-E<gt>Colors-E<gt>Modify> dialog.

The C<set_custom_color()> workbook method can also be used with a HTML style C<#rrggbb> hex value:

    $workbook->set_custom_color(40, 255,  102,  0   ); # Orange
    $workbook->set_custom_color(40, 0xFF, 0x66, 0x00); # Same thing
    $workbook->set_custom_color(40, '#FF6600'       ); # Same thing

    my $font = $workbook->add_format(color => 40); # Use the modified colour

The return value from C<set_custom_color()> is the index of the colour that was changed:

    my $ferrari = $workbook->set_custom_color(40, 216, 12, 12);

    my $format  = $workbook->add_format(
                                        bg_color => $ferrari,
                                        pattern  => 1,
                                        border   => 1
                                      );




=head2 sheets(0, 1, ...)

The C<sheets()> method returns a list, or a sliced list, of the worksheets in a workbook.

If no arguments are passed the method returns a list of all the worksheets in the workbook. This is useful if you want to repeat an operation on each worksheet:

    foreach $worksheet ($workbook->sheets()) {
       print $worksheet->get_name();
    }


You can also specify a slice list to return one or more worksheet objects:

    $worksheet = $workbook->sheets(0);
    $worksheet->write('A1', 'Hello');


Or since return value from C<sheets()> is a reference to a worksheet object you can write the above example as:

    $workbook->sheets(0)->write('A1', 'Hello');


The following example returns the first and last worksheet in a workbook:

    foreach $worksheet ($workbook->sheets(0, -1)) {
       # Do something
    }


Array slices are explained in the perldata manpage.




=head2 set_1904()

Excel stores dates as real numbers where the integer part stores the number of days since the epoch and the fractional part stores the percentage of the day. The epoch can be either 1900 or 1904. Excel for Windows uses 1900 and Excel for Macintosh uses 1904. However, Excel on either platform will convert automatically between one system and the other.

Spreadsheet::WriteExcel stores dates in the 1900 format by default. If you wish to change this you can call the C<set_1904()> workbook method. You can query the current value by calling the C<get_1904()> workbook method. This returns 0 for 1900 and 1 for 1904.

See also L</DATES AND TIME IN EXCEL> for more information about working with Excel's date system.

In general you probably won't need to use C<set_1904()>.




=head2 set_codepage($codepage)

The default code page or character set used by Spreadsheet::WriteExcel is ANSI. This is also the default used by Excel for Windows. Occasionally however it may be necessary to change the code page via the C<set_codepage()> method.

Changing the code page may be required if your are using Spreadsheet::WriteExcel on the Macintosh and you are using characters outside the ASCII 128 character set:

    $workbook->set_codepage(1); # ANSI, MS Windows
    $workbook->set_codepage(2); # Apple Macintosh

The C<set_codepage()> method is rarely required.




=head1 WORKSHEET METHODS

A new worksheet is created by calling the C<add_worksheet()> method from a workbook object:

    $worksheet1 = $workbook->add_worksheet();
    $worksheet2 = $workbook->add_worksheet();

The following methods are available through a new worksheet:

    write()
    write_number()
    write_string()
    write_utf16be_string()
    write_utf16le_string()
    keep_leading_zeros()
    write_blank()
    write_row()
    write_col()
    write_date_time()
    write_url()
    write_url_range()
    write_formula()
    store_formula()
    repeat_formula()
    write_comment()
    show_comments()
    add_write_handler()
    insert_image()
    insert_chart()
    data_validation()
    get_name()
    activate()
    select()
    hide()
    set_first_sheet()
    protect()
    set_selection()
    set_row()
    set_column()
    outline_settings()
    freeze_panes()
    split_panes()
    merge_range()
    set_zoom()
    right_to_left()
    hide_zero()
    set_tab_color()
    autofilter()




=head2 Cell notation

Spreadsheet::WriteExcel supports two forms of notation to designate the position of cells: Row-column notation and A1 notation.

Row-column notation uses a zero based index for both row and column while A1 notation uses the standard Excel alphanumeric sequence of column letter and 1-based row. For example:

    (0, 0)      # The top left cell in row-column notation.
    ('A1')      # The top left cell in A1 notation.

    (1999, 29)  # Row-column notation.
    ('AD2000')  # The same cell in A1 notation.

Row-column notation is useful if you are referring to cells programmatically:

    for my $i (0 .. 9) {
        $worksheet->write($i, 0, 'Hello'); # Cells A1 to A10
    }

A1 notation is useful for setting up a worksheet manually and for working with formulas:

    $worksheet->write('H1', 200);
    $worksheet->write('H2', '=H1+1');

In formulas and applicable methods you can also use the C<A:A> column notation:

    $worksheet->write('A1', '=SUM(B:B)');

The C<Spreadsheet::WriteExcel::Utility> module that is included in the distro contains helper functions for dealing with A1 notation, for example:

    use Spreadsheet::WriteExcel::Utility;

    ($row, $col)    = xl_cell_to_rowcol('C2');  # (1, 2)
    $str            = xl_rowcol_to_cell(1, 2);  # C2

For simplicity, the parameter lists for the worksheet method calls in the following sections are given in terms of row-column notation. In all cases it is also possible to use A1 notation.

Note: in Excel it is also possible to use a R1C1 notation. This is not supported by Spreadsheet::WriteExcel.




=head2 write($row, $column, $token, $format)

Excel makes a distinction between data types such as strings, numbers, blanks, formulas and hyperlinks. To simplify the process of writing data the C<write()> method acts as a general alias for several more specific methods:

    write_string()
    write_number()
    write_blank()
    write_formula()
    write_url()
    write_row()
    write_col()

The general rule is that if the data looks like a I<something> then a I<something> is written. Here are some examples in both row-column and A1 notation:

                                                      # Same as:
    $worksheet->write(0, 0, 'Hello'                ); # write_string()
    $worksheet->write(1, 0, 'One'                  ); # write_string()
    $worksheet->write(2, 0,  2                     ); # write_number()
    $worksheet->write(3, 0,  3.00001               ); # write_number()
    $worksheet->write(4, 0,  ""                    ); # write_blank()
    $worksheet->write(5, 0,  ''                    ); # write_blank()
    $worksheet->write(6, 0,  undef                 ); # write_blank()
    $worksheet->write(7, 0                         ); # write_blank()
    $worksheet->write(8, 0,  'http://www.perl.com/'); # write_url()
    $worksheet->write('A9',  'ftp://ftp.cpan.org/' ); # write_url()
    $worksheet->write('A10', 'internal:Sheet1!A1'  ); # write_url()
    $worksheet->write('A11', 'external:c:\foo.xls' ); # write_url()
    $worksheet->write('A12', '=A3 + 3*A4'          ); # write_formula()
    $worksheet->write('A13', '=SIN(PI()/4)'        ); # write_formula()
    $worksheet->write('A14', \@array               ); # write_row()
    $worksheet->write('A15', [\@array]             ); # write_col()

    # And if the keep_leading_zeros property is set:
    $worksheet->write('A16', '2'                   ); # write_number()
    $worksheet->write('A17', '02'                  ); # write_string()
    $worksheet->write('A18', '00002'               ); # write_string()


The "looks like" rule is defined by regular expressions:

C<write_number()> if C<$token> is a number based on the following regex: C<$token =~ /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/>.

C<write_string()> if C<keep_leading_zeros()> is set and C<$token> is an integer with leading zeros based on the following regex: C<$token =~ /^0\d+$/>.

C<write_blank()> if C<$token> is undef or a blank string: C<undef>, C<""> or C<''>.

C<write_url()> if C<$token> is a http, https, ftp or mailto URL based on the following regexes: C<$token =~ m|^[fh]tt?ps?://|> or  C<$token =~ m|^mailto:|>.

C<write_url()> if C<$token> is an internal or external sheet reference based on the following regex: C<$token =~ m[^(in|ex)ternal:]>.

C<write_formula()> if the first character of C<$token> is C<"=">.

C<write_row()> if C<$token> is an array ref.

C<write_col()> if C<$token> is an array ref of array refs.

C<write_string()> if none of the previous conditions apply.

The C<$format> parameter is optional. It should be a valid Format object, see L</CELL FORMATTING>:

    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_color('red');
    $format->set_align('center');

    $worksheet->write(4, 0, 'Hello', $format); # Formatted string

The write() method will ignore empty strings or C<undef> tokens unless a format is also supplied. As such you needn't worry about special handling for empty or C<undef> values in your data. See also the C<write_blank()> method.

One problem with the C<write()> method is that occasionally data looks like a number but you don't want it treated as a number. For example, zip codes or ID numbers often start with a leading zero. If you write this data as a number then the leading zero(s) will be stripped. You can change this default behaviour by using the C<keep_leading_zeros()> method. While this property is in place any integers with leading zeros will be treated as strings and the zeros will be preserved. See the C<keep_leading_zeros()> section for a full discussion of this issue.

You can also add your own data handlers to the C<write()> method using C<add_write_handler()>.

On systems with C<perl 5.8> and later the C<write()> method will also handle Unicode strings in C<UTF-8> format.

The C<write> methods return:

    0 for success.
   -1 for insufficient number of arguments.
   -2 for row or column out of bounds.
   -3 for string too long.




=head2 write_number($row, $column, $number, $format)

Write an integer or a float to the cell specified by C<$row> and C<$column>:

    $worksheet->write_number(0, 0,  123456);
    $worksheet->write_number('A2',  2.3451);

See the note about L</Cell notation>. The C<$format> parameter is optional.

In general it is sufficient to use the C<write()> method.




=head2 write_string($row, $column, $string, $format)

Write a string to the cell specified by C<$row> and C<$column>:

    $worksheet->write_string(0, 0, 'Your text here' );
    $worksheet->write_string('A2', 'or here' );

The maximum string size is 32767 characters. However the maximum string segment that Excel can display in a cell is 1000. All 32767 characters can be displayed in the formula bar.

The C<$format> parameter is optional.

On systems with C<perl 5.8> and later the C<write()> method will also handle strings in C<UTF-8> format. With older perls you can also write Unicode in C<UTF16> format via the C<write_utf16be_string()> method. See also the C<unicode_*.pl> programs in the examples directory of the distro.

In general it is sufficient to use the C<write()> method. However, you may sometimes wish to use the C<write_string()> method to write data that looks like a number but that you don't want treated as a number. For example, zip codes or phone numbers:

    # Write as a plain string
    $worksheet->write_string('A1', '01209');

However, if the user edits this string Excel may convert it back to a number. To get around this you can use the Excel text format C<@>:

    # Format as a string. Doesn't change to a number when edited
    my $format1 = $workbook->add_format(num_format => '@');
    $worksheet->write_string('A2', '01209', $format1);

See also the note about L</Cell notation>.




=head2 write_utf16be_string($row, $column, $string, $format)

This method is used to write C<UTF-16BE> strings to a cell in Excel. It is functionally the same as the C<write_string()> method except that the string should be in C<UTF-16BE> Unicode format. It is generally easier, when using Spreadsheet::WriteExcel, to write unicode strings in C<UTF-8> format, see L</UNICODE IN EXCEL>. The C<write_utf16be_string()> method is mainly of use in versions of perl prior to 5.8.


The following is a simple example showing how to write some Unicode strings in C<UTF-16BE> format:

    #!/usr/bin/perl -w


    use strict;
    use Spreadsheet::WriteExcel;
    use Unicode::Map();

    my $workbook  = Spreadsheet::WriteExcel->new('utf_16_be.xls');
    my $worksheet = $workbook->add_worksheet();

    # Increase the column width for clarity
    $worksheet->set_column('A:A', 25);


    # Write a Unicode character
    #
    my $smiley = pack 'n', 0x263a;

    # Increase the font size for legibility.
    my $big_font = $workbook->add_format(size => 72);

    $worksheet->write_utf16be_string('A3', $smiley, $big_font);



    # Write a phrase in Cyrillic using a hex-encoded string
    #
    my $str = pack 'H*', '042d0442043e0020044404400430043704300020043d' .
                         '043000200440044304410441043a043e043c0021';

    $worksheet->write_utf16be_string('A5', $str);



    # Map a string to UTF-16BE using an external module.
    #
    my $map   = Unicode::Map->new('ISO-8859-1');
    my $utf16 = $map->to_unicode('Hello world!');

    $worksheet->write_utf16be_string('A7', $utf16);

You can convert ASCII encodings to the required C<UTF-16BE> format using one of the many Unicode modules on CPAN. For example C<Unicode::Map> and C<Unicode::String>: L<http://search.cpan.org/author/MSCHWARTZ/Unicode-Map/Map.pm> and L<http://search.cpan.org/author/GAAS/Unicode-String/String.pm>.

For a full list of the Perl Unicode modules see: L<http://search.cpan.org/search?query=unicode&mode=all>.

C<UTF-16BE> is the format most often returned by C<Perl> modules that generate C<UTF-16>. To write C<UTF-16> strings in little-endian format use the C<write_utf16be_string_le()> method below.

The C<write_utf16be_string()> method was previously called C<write_unicode()>. That, overly general, name is still supported but deprecated.

See also the C<unicode_*.pl> programs in the examples directory of the distro.




=head2 write_utf16le_string($row, $column, $string, $format)

This method is the same as C<write_utf16be()> except that the string should be 16-bit characters in little-endian format. This is generally referred to as C<UTF-16LE>. See L</UNICODE IN EXCEL>.

C<UTF-16> data can be changed from little-endian to big-endian format (and vice-versa) as follows:

    $utf16be = pack 'n*', unpack 'v*', $utf16le;




=head2 keep_leading_zeros()

This method changes the default handling of integers with leading zeros when using the C<write()> method.

The C<write()> method uses regular expressions to determine what type of data to write to an Excel worksheet. If the data looks like a number it writes a number using C<write_number()>. One problem with this approach is that occasionally data looks like a number but you don't want it treated as a number.

Zip codes and ID numbers, for example, often start with a leading zero. If you write this data as a number then the leading zero(s) will be stripped. This is the also the default behaviour when you enter data manually in Excel.

To get around this you can use one of three options. Write a formatted number, write the number as a string or use the C<keep_leading_zeros()> method to change the default behaviour of C<write()>:

    # Implicitly write a number, the leading zero is removed: 1209
    $worksheet->write('A1', '01209');

    # Write a zero padded number using a format: 01209
    my $format1 = $workbook->add_format(num_format => '00000');
    $worksheet->write('A2', '01209', $format1);

    # Write explicitly as a string: 01209
    $worksheet->write_string('A3', '01209');

    # Write implicitly as a string: 01209
    $worksheet->keep_leading_zeros();
    $worksheet->write('A4', '01209');


The above code would generate a worksheet that looked like the following:

     -----------------------------------------------------------
    |   |     A     |     B     |     C     |     D     | ...
     -----------------------------------------------------------
    | 1 |      1209 |           |           |           | ...
    | 2 |     01209 |           |           |           | ...
    | 3 | 01209     |           |           |           | ...
    | 4 | 01209     |           |           |           | ...


The examples are on different sides of the cells due to the fact that Excel displays strings with a left justification and numbers with a right justification by default. You can change this by using a format to justify the data, see L</CELL FORMATTING>.

It should be noted that if the user edits the data in examples C<A3> and C<A4> the strings will revert back to numbers. Again this is Excel's default behaviour. To avoid this you can use the text format C<@>:

    # Format as a string (01209)
    my $format2 = $workbook->add_format(num_format => '@');
    $worksheet->write_string('A5', '01209', $format2);

The C<keep_leading_zeros()> property is off by default. The C<keep_leading_zeros()> method takes 0 or 1 as an argument. It defaults to 1 if an argument isn't specified:

    $worksheet->keep_leading_zeros();  # Set on
    $worksheet->keep_leading_zeros(1); # Set on
    $worksheet->keep_leading_zeros(0); # Set off

See also the C<add_write_handler()> method.


=head2 write_blank($row, $column, $format)

Write a blank cell specified by C<$row> and C<$column>:

    $worksheet->write_blank(0, 0, $format);

This method is used to add formatting to a cell which doesn't contain a string or number value.

Excel differentiates between an "Empty" cell and a "Blank" cell. An "Empty" cell is a cell which doesn't contain data whilst a "Blank" cell is a cell which doesn't contain data but does contain formatting. Excel stores "Blank" cells but ignores "Empty" cells.

As such, if you write an empty cell without formatting it is ignored:

    $worksheet->write('A1',  undef, $format); # write_blank()
    $worksheet->write('A2',  undef         ); # Ignored

This seemingly uninteresting fact means that you can write arrays of data without special treatment for undef or empty string values.

See the note about L</Cell notation>.




=head2 write_row($row, $column, $array_ref, $format)

The C<write_row()> method can be used to write a 1D or 2D array of data in one go. This is useful for converting the results of a database query into an Excel worksheet. You must pass a reference to the array of data rather than the array itself. The C<write()> method is then called for each element of the data. For example:

    @array      = ('awk', 'gawk', 'mawk');
    $array_ref  = \@array;

    $worksheet->write_row(0, 0, $array_ref);

    # The above example is equivalent to:
    $worksheet->write(0, 0, $array[0]);
    $worksheet->write(0, 1, $array[1]);
    $worksheet->write(0, 2, $array[2]);


Note: For convenience the C<write()> method behaves in the same way as C<write_row()> if it is passed an array reference. Therefore the following two method calls are equivalent:

    $worksheet->write_row('A1', $array_ref); # Write a row of data
    $worksheet->write(    'A1', $array_ref); # Same thing

As with all of the write methods the C<$format> parameter is optional. If a format is specified it is applied to all the elements of the data array.

Array references within the data will be treated as columns. This allows you to write 2D arrays of data in one go. For example:

    @eec =  (
                ['maggie', 'milly', 'molly', 'may'  ],
                [13,       14,      15,      16     ],
                ['shell',  'star',  'crab',  'stone']
            );

    $worksheet->write_row('A1', \@eec);


Would produce a worksheet as follows:

     -----------------------------------------------------------
    |   |    A    |    B    |    C    |    D    |    E    | ...
     -----------------------------------------------------------
    | 1 | maggie  | 13      | shell   | ...     |  ...    | ...
    | 2 | milly   | 14      | star    | ...     |  ...    | ...
    | 3 | molly   | 15      | crab    | ...     |  ...    | ...
    | 4 | may     | 16      | stone   | ...     |  ...    | ...
    | 5 | ...     | ...     | ...     | ...     |  ...    | ...
    | 6 | ...     | ...     | ...     | ...     |  ...    | ...


To write the data in a row-column order refer to the C<write_col()> method below.

Any C<undef> values in the data will be ignored unless a format is applied to the data, in which case a formatted blank cell will be written. In either case the appropriate row or column value will still be incremented.

To find out more about array references refer to C<perlref> and C<perlreftut> in the main Perl documentation. To find out more about 2D arrays or "lists of lists" refer to C<perllol>.

The C<write_row()> method returns the first error encountered when writing the elements of the data or zero if no errors were encountered. See the return values described for the C<write()> method above.

See also the C<write_arrays.pl> program in the C<examples> directory of the distro.

The C<write_row()> method allows the following idiomatic conversion of a text file to an Excel file:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new('file.xls');
    my $worksheet = $workbook->add_worksheet();

    open INPUT, 'file.txt' or die "Couldn't open file: $!";

    $worksheet->write($.-1, 0, [split]) while <INPUT>;




=head2 write_col($row, $column, $array_ref, $format)

The C<write_col()> method can be used to write a 1D or 2D array of data in one go. This is useful for converting the results of a database query into an Excel worksheet. You must pass a reference to the array of data rather than the array itself. The C<write()> method is then called for each element of the data. For example:

    @array      = ('awk', 'gawk', 'mawk');
    $array_ref  = \@array;

    $worksheet->write_col(0, 0, $array_ref);

    # The above example is equivalent to:
    $worksheet->write(0, 0, $array[0]);
    $worksheet->write(1, 0, $array[1]);
    $worksheet->write(2, 0, $array[2]);

As with all of the write methods the C<$format> parameter is optional. If a format is specified it is applied to all the elements of the data array.

Array references within the data will be treated as rows. This allows you to write 2D arrays of data in one go. For example:

    @eec =  (
                ['maggie', 'milly', 'molly', 'may'  ],
                [13,       14,      15,      16     ],
                ['shell',  'star',  'crab',  'stone']
            );

    $worksheet->write_col('A1', \@eec);


Would produce a worksheet as follows:

     -----------------------------------------------------------
    |   |    A    |    B    |    C    |    D    |    E    | ...
     -----------------------------------------------------------
    | 1 | maggie  | milly   | molly   | may     |  ...    | ...
    | 2 | 13      | 14      | 15      | 16      |  ...    | ...
    | 3 | shell   | star    | crab    | stone   |  ...    | ...
    | 4 | ...     | ...     | ...     | ...     |  ...    | ...
    | 5 | ...     | ...     | ...     | ...     |  ...    | ...
    | 6 | ...     | ...     | ...     | ...     |  ...    | ...


To write the data in a column-row order refer to the C<write_row()> method above.

Any C<undef> values in the data will be ignored unless a format is applied to the data, in which case a formatted blank cell will be written. In either case the appropriate row or column value will still be incremented.

As noted above the C<write()> method can be used as a synonym for C<write_row()> and C<write_row()> handles nested array refs as columns. Therefore, the following two method calls are equivalent although the more explicit call to C<write_col()> would be preferable for maintainability:

    $worksheet->write_col('A1', $array_ref    ); # Write a column of data
    $worksheet->write(    'A1', [ $array_ref ]); # Same thing

To find out more about array references refer to C<perlref> and C<perlreftut> in the main Perl documentation. To find out more about 2D arrays or "lists of lists" refer to C<perllol>.

The C<write_col()> method returns the first error encountered when writing the elements of the data or zero if no errors were encountered. See the return values described for the C<write()> method above.

See also the C<write_arrays.pl> program in the C<examples> directory of the distro.




=head2 write_date_time($row, $col, $date_string, $format)

The C<write_date_time()> method can be used to write a date or time to the cell specified by C<$row> and C<$column>:

    $worksheet->write_date_time('A1', '2004-05-13T23:20', $date_format);

The C<$date_string> should be in the following format:

    yyyy-mm-ddThh:mm:ss.sss

This conforms to an ISO8601 date but it should be noted that the full range of ISO8601 formats are not supported.

The following variations on the C<$date_string> parameter are permitted:

    yyyy-mm-ddThh:mm:ss.sss         # Standard format
    yyyy-mm-ddT                     # No time
              Thh:mm:ss.sss         # No date
    yyyy-mm-ddThh:mm:ss.sssZ        # Additional Z (but not time zones)
    yyyy-mm-ddThh:mm:ss             # No fractional seconds
    yyyy-mm-ddThh:mm                # No seconds

Note that the C<T> is required in all cases.

A date should always have a C<$format>, otherwise it will appear as a number, see L</DATES AND TIME IN EXCEL> and L</CELL FORMATTING>. Here is a typical example:

    my $date_format = $workbook->add_format(num_format => 'mm/dd/yy');
    $worksheet->write_date_time('A1', '2004-05-13T23:20', $date_format);

Valid dates should be in the range 1900-01-01 to 9999-12-31, for the 1900 epoch and 1904-01-01 to 9999-12-31, for the 1904 epoch. As with Excel, dates outside these ranges will be written as a string.

See also the date_time.pl program in the C<examples> directory of the distro.




=head2 write_url($row, $col, $url, $label, $format)

Write a hyperlink to a URL in the cell specified by C<$row> and C<$column>. The hyperlink is comprised of two elements: the visible label and the invisible link. The visible label is the same as the link unless an alternative label is specified. The parameters C<$label> and the C<$format> are optional and their position is interchangeable.

The label is written using the C<write()> method. Therefore it is possible to write strings, numbers or formulas as labels.

There are four web style URI's supported: C<http://>, C<https://>, C<ftp://> and  C<mailto:>:

    $worksheet->write_url(0, 0,  'ftp://www.perl.org/'                  );
    $worksheet->write_url(1, 0,  'http://www.perl.com/', 'Perl home'    );
    $worksheet->write_url('A3',  'http://www.perl.com/', $format        );
    $worksheet->write_url('A4',  'http://www.perl.com/', 'Perl', $format);
    $worksheet->write_url('A5',  'mailto:jmcnamara@cpan.org'            );

There are two local URIs supported: C<internal:> and C<external:>. These are used for hyperlinks to internal worksheet references or external workbook and worksheet references:

    $worksheet->write_url('A6',  'internal:Sheet2!A1'                   );
    $worksheet->write_url('A7',  'internal:Sheet2!A1',   $format        );
    $worksheet->write_url('A8',  'internal:Sheet2!A1:B2'                );
    $worksheet->write_url('A9',  q{internal:'Sales Data'!A1}            );
    $worksheet->write_url('A10', 'external:c:\temp\foo.xls'             );
    $worksheet->write_url('A11', 'external:c:\temp\foo.xls#Sheet2!A1'   );
    $worksheet->write_url('A12', 'external:..\..\..\foo.xls'            );
    $worksheet->write_url('A13', 'external:..\..\..\foo.xls#Sheet2!A1'  );
    $worksheet->write_url('A13', 'external:\\\\NETWORK\share\foo.xls'   );

All of the these URI types are recognised by the C<write()> method, see above.

Worksheet references are typically of the form C<Sheet1!A1>. You can also refer to a worksheet range using the standard Excel notation: C<Sheet1!A1:B2>.

In external links the workbook and worksheet name must be separated by the C<#> character: C<external:Workbook.xls#Sheet1!A1'>.

You can also link to a named range in the target worksheet. For example say you have a named range called C<my_name> in the workbook C<c:\temp\foo.xls> you could link to it as follows:

    $worksheet->write_url('A14', 'external:c:\temp\foo.xls#my_name');

Note, you cannot currently create named ranges with C<Spreadsheet::WriteExcel>.

Excel requires that worksheet names containing spaces or non alphanumeric characters are single quoted as follows C<'Sales Data'!A1>. If you need to do this in a single quoted string then you can either escape the single quotes C<\'> or use the quote operator C<q{}> as described in C<perlop> in the main Perl documentation.

Links to network files are also supported. MS/Novell Network files normally begin with two back slashes as follows C<\\NETWORK\etc>. In order to generate this in a single or double quoted string you will have to escape the backslashes,  C<'\\\\NETWORK\etc'>.

If you are using double quote strings then you should be careful to escape anything that looks like a metacharacter. For more information  see C<perlfaq5: Why can't I use "C:\temp\foo" in DOS paths?>.

Finally, you can avoid most of these quoting problems by using forward slashes. These are translated internally to backslashes:

    $worksheet->write_url('A14', "external:c:/temp/foo.xls"             );
    $worksheet->write_url('A15', 'external://NETWORK/share/foo.xls'     );

See also, the note about L</Cell notation>.




=head2 write_url_range($row1, $col1, $row2, $col2, $url, $string, $format)

This method is essentially the same as the C<write_url()> method described above. The main difference is that you can specify a link for a range of cells:

    $worksheet->write_url(0, 0, 0, 3, 'ftp://www.perl.org/'              );
    $worksheet->write_url(1, 0, 0, 3, 'http://www.perl.com/', 'Perl home');
    $worksheet->write_url('A3:D3',    'internal:Sheet2!A1'               );
    $worksheet->write_url('A4:D4',    'external:c:\temp\foo.xls'         );


This method is generally only required when used in conjunction with merged cells. See the C<merge_range()> method and the C<merge> property of a Format object, L</CELL FORMATTING>.

There is no way to force this behaviour through the C<write()> method.

The parameters C<$string> and the C<$format> are optional and their position is interchangeable. However, they are applied only to the first cell in the range.

See also, the note about L</Cell notation>.




=head2 write_formula($row, $column, $formula, $format, $value)

Write a formula or function to the cell specified by C<$row> and C<$column>:

    $worksheet->write_formula(0, 0, '=$B$3 + B4'  );
    $worksheet->write_formula(1, 0, '=SIN(PI()/4)');
    $worksheet->write_formula(2, 0, '=SUM(B1:B5)' );
    $worksheet->write_formula('A4', '=IF(A3>1,"Yes", "No")'   );
    $worksheet->write_formula('A5', '=AVERAGE(1, 2, 3, 4)'    );
    $worksheet->write_formula('A6', '=DATEVALUE("1-Jan-2001")');

See the note about L</Cell notation>. For more information about writing Excel formulas see L</FORMULAS AND FUNCTIONS IN EXCEL>

See also the section "Improving performance when working with formulas" and the C<store_formula()> and C<repeat_formula()> methods.

If required, it is also possible to specify the calculated value of the formula. This is occasionally necessary when working with non-Excel applications that don't calculate the value of the formula. The calculated C<$value> is added at the end of the argument list:

    $worksheet->write('A1', '=2+2', $format, 4);

However, this probably isn't something that will ever need to do. If you do use this feature then do so with care.




=head2 store_formula($formula)

The C<store_formula()> method is used in conjunction with C<repeat_formula()> to speed up the generation of repeated formulas. See "Improving performance when working with formulas" in L</FORMULAS AND FUNCTIONS IN EXCEL>.

The C<store_formula()> method pre-parses a textual representation of a formula and stores it for use at a later stage by the C<repeat_formula()> method.

C<store_formula()> carries the same speed penalty as C<write_formula()>. However, in practice it will be used less frequently.

The return value of this method is a scalar that can be thought of as a reference to a formula.

    my $sin = $worksheet->store_formula('=SIN(A1)');
    my $cos = $worksheet->store_formula('=COS(A1)');

    $worksheet->repeat_formula('B1', $sin, $format, 'A1', 'A2');
    $worksheet->repeat_formula('C1', $cos, $format, 'A1', 'A2');

Although C<store_formula()> is a worksheet method the return value can be used in any worksheet:

    my $now = $worksheet->store_formula('=NOW()');

    $worksheet1->repeat_formula('B1', $now);
    $worksheet2->repeat_formula('B1', $now);
    $worksheet3->repeat_formula('B1', $now);



=head2 repeat_formula($row, $col, $formula, $format, ($pattern => $replace, ...))


The C<repeat_formula()> method is used in conjunction with C<store_formula()> to speed up the generation of repeated formulas.  See "Improving performance when working with formulas" in L</FORMULAS AND FUNCTIONS IN EXCEL>.

In many respects C<repeat_formula()> behaves like C<write_formula()> except that it is significantly faster.

The C<repeat_formula()> method creates a new formula based on the pre-parsed tokens returned by C<store_formula()>. The new formula is generated by substituting C<$pattern>, C<$replace> pairs in the stored formula:

    my $formula = $worksheet->store_formula('=A1 * 3 + 50');

    for my $row (0..99) {
        $worksheet->repeat_formula($row, 1, $formula, $format, 'A1', 'A'.($row +1));
    }

It should be noted that C<repeat_formula()> doesn't modify the tokens. In the above example the substitution is always made against the original token, C<A1>, which doesn't change.

As usual, you can use C<undef> if you don't wish to specify a C<$format>:

    $worksheet->repeat_formula('B2', $formula, $format, 'A1', 'A2');
    $worksheet->repeat_formula('B3', $formula, undef,   'A1', 'A3');

The substitutions are made from left to right and you can use as many C<$pattern>, C<$replace> pairs as you need. However, each substitution is made only once:

    my $formula = $worksheet->store_formula('=A1 + A1');

    # Gives '=B1 + A1'
    $worksheet->repeat_formula('B1', $formula, undef, 'A1', 'B1');

    # Gives '=B1 + B1'
    $worksheet->repeat_formula('B2', $formula, undef, ('A1', 'B1') x 2);

Since the C<$pattern> is interpolated each time that it is used it is worth using the C<qr> operator to quote the pattern. The C<qr> operator is explained in the C<perlop> man page.

    $worksheet->repeat_formula('B1', $formula, $format, qr/A1/, 'A2');

Care should be taken with the values that are substituted. The formula returned by C<repeat_formula()> contains several other tokens in addition to those in the formula and these might also match the  pattern that you are trying to replace. In particular you should avoid substituting a single 0, 1, 2 or 3.

You should also be careful to avoid false matches. For example the following snippet is meant to change the stored formula in steps from C<=A1 + SIN(A1)> to C<=A10 + SIN(A10)>.

    my $formula = $worksheet->store_formula('=A1 + SIN(A1)');

    for my $row (1 .. 10) {
        $worksheet->repeat_formula($row -1, 1, $formula, undef,
                                    qw/A1/, 'A' . $row,   #! Bad.
                                    qw/A1/, 'A' . $row    #! Bad.
                                  );
    }

However it contains a bug. In the last iteration of the loop when C<$row> is 10 the following substitutions will occur:

    s/A1/A10/;    changes    =A1 + SIN(A1)     to    =A10 + SIN(A1)
    s/A1/A10/;    changes    =A10 + SIN(A1)    to    =A100 + SIN(A1) # !!

The solution in this case is to use a more explicit match such as C<qw/^A1$/>:

        $worksheet->repeat_formula($row -1, 1, $formula, undef,
                                    qw/^A1$/, 'A' . $row,
                                    qw/^A1$/, 'A' . $row
                                  );

Another similar problem occurs due to the fact that substitutions are made in order. For example the following snippet is meant to change the stored formula from C<=A10 + A11>  to C<=A11 + A12>:

    my $formula = $worksheet->store_formula('=A10 + A11');

    $worksheet->repeat_formula('A1', $formula, undef,
                                qw/A10/, 'A11',   #! Bad.
                                qw/A11/, 'A12'    #! Bad.
                              );

However, the actual substitution yields C<=A12 + A11>:

    s/A10/A11/;    changes    =A10 + A11    to    =A11 + A11
    s/A11/A12/;    changes    =A11 + A11    to    =A12 + A11 # !!

The solution here would be to reverse the order of the substitutions or to start with a stored formula that won't yield a false match such as C<=X10 + Y11>:

    my $formula = $worksheet->store_formula('=X10 + Y11');

    $worksheet->repeat_formula('A1', $formula, undef,
                                qw/X10/, 'A11',
                                qw/Y11/, 'A12'
                              );


If you think that you have a problem related to a false match you can check the tokens that you are substituting against as follows.

    my $formula = $worksheet->store_formula('=A1*5+4');
    print "@$formula\n";

See also the C<repeat.pl> program in the C<examples> directory of the distro.




=head2 write_comment($row, $column, $string, ...)

The C<write_comment()> method is used to add a comment to a cell. A cell comment is indicated in Excel by a small red triangle in the upper right-hand corner of the cell. Moving the cursor over the red triangle will reveal the comment.

The following example shows how to add a comment to a cell:

    $worksheet->write        (2, 2, 'Hello');
    $worksheet->write_comment(2, 2, 'This is a comment.');

As usual you can replace the C<$row> and C<$column> parameters with an C<A1> cell reference. See the note about L</Cell notation>.

    $worksheet->write        ('C3', 'Hello');
    $worksheet->write_comment('C3', 'This is a comment.');

On systems with C<perl 5.8> and later the C<write_comment()> method will also handle strings in C<UTF-8> format.

    $worksheet->write_comment('C3', "\x{263a}");       # Smiley
    $worksheet->write_comment('C4', 'Comment ca va?');

In addition to the basic 3 argument form of C<write_comment()> you can pass in several optional key/value pairs to control the format of the comment. For example:

    $worksheet->write_comment('C3', 'Hello', visible => 1, author => 'Perl');

Most of these options are quite specific and in general the default comment behaviour will be all that you need. However, should you need greater control over the format of the cell comment the following options are available:

    encoding
    author
    author_encoding
    visible
    x_scale
    width
    y_scale
    height
    color
    start_cell
    start_row
    start_col
    x_offset
    y_offset


=over 4

=item Option: encoding

This option is used to indicate that the comment string is encoded as C<UTF-16BE>.

    my $comment = pack 'n', 0x263a; # UTF-16BE Smiley symbol

    $worksheet->write_comment('C3', $comment, encoding => 1);

If you wish to use Unicode characters in the comment string then the preferred method is to use perl 5.8 and C<UTF-8> strings, see L</UNICODE IN EXCEL>.


=item Option: author

This option is used to indicate who the author of the comment is. Excel displays the author of the comment in the status bar at the bottom of the worksheet. This is usually of interest in corporate environments where several people might review and provide comments to a workbook.

    $worksheet->write_comment('C3', 'Atonement', author => 'Ian McEwan');


=item Option: author_encoding

This option is used to indicate that the author string is encoded as C<UTF-16BE>.


=item Option: visible

This option is used to make a cell comment visible when the worksheet is opened. The default behaviour in Excel is that comments are initially hidden. However, it is also possible in Excel to make individual or all comments visible. In Spreadsheet::WriteExcel individual comments can be made visible as follows:

    $worksheet->write_comment('C3', 'Hello', visible => 1);

It is possible to make all comments in a worksheet visible using the C<show_comments()> worksheet method (see below). Alternatively, if all of the cell comments have been made visible you can hide individual comments:

    $worksheet->write_comment('C3', 'Hello', visible => 0);


=item Option: x_scale

This option is used to set the width of the cell comment box as a factor of the default width.

    $worksheet->write_comment('C3', 'Hello', x_scale => 2);
    $worksheet->write_comment('C4', 'Hello', x_scale => 4.2);


=item Option: width

This option is used to set the width of the cell comment box explicitly in pixels.

    $worksheet->write_comment('C3', 'Hello', width => 200);


=item Option: y_scale

This option is used to set the height of the cell comment box as a factor of the default height.

    $worksheet->write_comment('C3', 'Hello', y_scale => 2);
    $worksheet->write_comment('C4', 'Hello', y_scale => 4.2);


=item Option: height

This option is used to set the height of the cell comment box explicitly in pixels.

    $worksheet->write_comment('C3', 'Hello', height => 200);


=item Option: color

This option is used to set the background colour of cell comment box. You can use one of the named colours recognised by Spreadsheet::WriteExcel or a colour index. See L</COLOURS IN EXCEL>.

    $worksheet->write_comment('C3', 'Hello', color => 'green');
    $worksheet->write_comment('C4', 'Hello', color => 0x35);    # Orange


=item Option: start_cell

This option is used to set the cell in which the comment will appear. By default Excel displays comments one cell to the right and one cell above the cell to which the comment relates. However, you can change this behaviour if you wish. In the following example the comment which would appear by default in cell C<D2> is moved to C<E2>.

    $worksheet->write_comment('C3', 'Hello', start_cell => 'E2');


=item Option: start_row

This option is used to set the row in which the comment will appear. See the C<start_cell> option above. The row is zero indexed.

    $worksheet->write_comment('C3', 'Hello', start_row => 0);


=item Option: start_col

This option is used to set the column in which the comment will appear. See the C<start_cell> option above. The column is zero indexed.

    $worksheet->write_comment('C3', 'Hello', start_col => 4);


=item Option: x_offset

This option is used to change the x offset, in pixels, of a comment within a cell:

    $worksheet->write_comment('C3', $comment, x_offset => 30);


=item Option: y_offset

This option is used to change the y offset, in pixels, of a comment within a cell:

    $worksheet->write_comment('C3', $comment, x_offset => 30);


=back

You can apply as many of these options as you require.

See also L</ROW HEIGHTS AND WORKSHEET OBJECTS>.




=head2 show_comments()

This method is used to make all cell comments visible when a worksheet is opened.

Individual comments can be made visible using the C<visible> parameter of the C<write_comment> method (see above):

    $worksheet->write_comment('C3', 'Hello', visible => 1);

If all of the cell comments have been made visible you can hide individual comments as follows:

    $worksheet->write_comment('C3', 'Hello', visible => 0);




=head2 add_write_handler($re, $code_ref)

This method is used to extend the Spreadsheet::WriteExcel write() method to handle user defined data.

If you refer to the section on C<write()> above you will see that it acts as an alias for several more specific C<write_*> methods. However, it doesn't always act in exactly the way that you would like it to.

One solution is to filter the input data yourself and call the appropriate C<write_*> method. Another approach is to use the C<add_write_handler()> method to add your own automated behaviour to C<write()>.

The C<add_write_handler()> method take two arguments, C<$re>, a regular expression to match incoming data and C<$code_ref> a callback function to handle the matched data:

    $worksheet->add_write_handler(qr/^\d\d\d\d$/, \&my_write);

(In the these examples the C<qr> operator is used to quote the regular expression strings, see L<perlop> for more details).

The method is used as follows. say you wished to write 7 digit ID numbers as a string so that any leading zeros were preserved*, you could do something like the following:

    $worksheet->add_write_handler(qr/^\d{7}$/, \&write_my_id);


    sub write_my_id {
        my $worksheet = shift;
        return $worksheet->write_string(@_);
    }

* You could also use the C<keep_leading_zeros()> method for this.

Then if you call C<write()> with an appropriate string it will be handled automatically:

    # Writes 0000000. It would normally be written as a number; 0.
    $worksheet->write('A1', '0000000');

The callback function will receive a reference to the calling worksheet and all of the other arguments that were passed to C<write()>. The callback will see an C<@_> argument list that looks like the following:

    $_[0]   A ref to the calling worksheet. *
    $_[1]   Zero based row number.
    $_[2]   Zero based column number.
    $_[3]   A number or string or token.
    $_[4]   A format ref if any.
    $_[5]   Any other arguments.
    ...

    *  It is good style to shift this off the list so the @_ is the same
       as the argument list seen by write().

Your callback should C<return()> the return value of the C<write_*> method that was called or C<undef> to indicate that you rejected the match and want C<write()> to continue as normal.

So for example if you wished to apply the previous filter only to ID values that occur in the first column you could modify your callback function as follows:


    sub write_my_id {
        my $worksheet = shift;
        my $col       = $_[1];

        if ($col == 0) {
            return $worksheet->write_string(@_);
        }
        else {
            # Reject the match and return control to write()
            return undef;
        }
    }

Now, you will get different behaviour for the first column and other columns:

    $worksheet->write('A1', '0000000'); # Writes 0000000
    $worksheet->write('B1', '0000000'); # Writes 0


You may add more than one handler in which case they will be called in the order that they were added.

Note, the C<add_write_handler()> method is particularly suited for handling dates.

See the C<write_handler 1-4> programs in the C<examples> directory for further examples.




=head2 insert_image($row, $col, $filename, $x, $y, $scale_x, $scale_y)

This method can be used to insert a image into a worksheet. The image can be in PNG, JPEG or BMP format. The C<$x>, C<$y>, C<$scale_x> and C<$scale_y> parameters are optional.

    $worksheet1->insert_image('A1', 'perl.bmp');
    $worksheet2->insert_image('A1', '../images/perl.bmp');
    $worksheet3->insert_image('A1', '.c:\images\perl.bmp');

The parameters C<$x> and C<$y> can be used to specify an offset from the top left hand corner of the cell specified by C<$row> and C<$col>. The offset values are in pixels.

    $worksheet1->insert_image('A1', 'perl.bmp', 32, 10);

The default width of a cell is 63 pixels. The default height of a cell is 17 pixels. The pixels offsets can be calculated using the following relationships:

    Wp = int(12We)   if We <  1
    Wp = int(7We +5) if We >= 1
    Hp = int(4/3He)

    where:
    We is the cell width in Excels units
    Wp is width in pixels
    He is the cell height in Excels units
    Hp is height in pixels

The offsets can be greater than the width or height of the underlying cell. This can be occasionally useful if you wish to align two or more images relative to the same cell.

The parameters C<$scale_x> and C<$scale_y> can be used to scale the inserted image horizontally and vertically:

    # Scale the inserted image: width x 2.0, height x 0.8
    $worksheet->insert_image('A1', 'perl.bmp', 0, 0, 2, 0.8);

See also the C<images.pl> program in the C<examples> directory of the distro.

BMP images must be 24 bit, true colour, bitmaps. In general it is best to avoid BMP images since they aren't compressed. The older C<insert_bitmap()> method is still supported but deprecated.

See also L</ROW HEIGHTS AND WORKSHEET OBJECTS>.




=head2 insert_chart($row, $col, $chart, $x, $y, $scale_x, $scale_y)

This method can be used to insert a Chart object into a worksheet. The Chart must be created by the C<add_chart()> Workbook method  and it must have the C<embedded> option set.

    my $chart = $workbook->add_chart( type => 'line', embedded => 1 );

    # Configure the chart.
    ...

    # Insert the chart into the a worksheet.
    $worksheet->insert_chart('E2', $chart);

See C<add_chart()> for details on how to create the Chart object and L<Spreadsheet::WriteExcel::Chart> for details on how to configure it. See also the C<chart_*.pl> programs in the examples directory of the distro.

The C<$x>, C<$y>, C<$scale_x> and C<$scale_y> parameters are optional.

The parameters C<$x> and C<$y> can be used to specify an offset from the top left hand corner of the cell specified by C<$row> and C<$col>. The offset values are in pixels. See the C<insert_image> method above for more information on sizes.

    $worksheet1->insert_chart('E2', $chart, 3, 3);

The parameters C<$scale_x> and C<$scale_y> can be used to scale the inserted image horizontally and vertically:

    # Scale the width by 120% and the height by 150%
    $worksheet->insert_chart('E2', $chart, 0, 0, 1.2, 1.5);

The easiest way to calculate the required scaling is to create a test chart worksheet with Spreadsheet::WriteExcel. Then open the file, select the chart and drag the corner to get the required size. While holding down the mouse the scale of the resized chart is shown to the left of the formula bar.

See also L</ROW HEIGHTS AND WORKSHEET OBJECTS>.




=head2 embed_chart($row, $col, $filename, $x, $y, $scale_x, $scale_y)

This method can be used to insert a externally generated chart into a worksheet. The chart must first be extracted from an existing Excel file. This feature is semi-deprecated in favour of the "native" charts created using C<add_chart()>. Read C<external_charts.txt> (or C<.pod>) in the external_charts directory of the distro for a full explanation.

Here is an example:

    $worksheet->embed_chart('B2', 'sales_chart.bin');

The C<$x>, C<$y>, C<$scale_x> and C<$scale_y> parameters are optional. See C<insert_chart()> above for details.




=head2 data_validation()

The C<data_validation()> method is used to construct an Excel data validation or to limit the user input to a dropdown list of values.


    $worksheet->data_validation('B3',
        {
            validate => 'integer',
            criteria => '>',
            value    => 100,
        });

    $worksheet->data_validation('B5:B9',
        {
            validate => 'list',
            value    => ['open', 'high', 'close'],
        });

This method contains a lot of parameters and is described in detail in a separate section L</DATA VALIDATION IN EXCEL>.


See also the C<data_validate.pl> program in the examples directory of the distro




=head2 get_name()

The C<get_name()> method is used to retrieve the name of a worksheet. For example:

    foreach my $sheet ($workbook->sheets()) {
        print $sheet->get_name();
    }

For reasons related to the design of Spreadsheet::WriteExcel and to the internals of Excel there is no C<set_name()> method. The only way to set the worksheet name is via the C<add_worksheet()> method.




=head2 activate()

The C<activate()> method is used to specify which worksheet is initially visible in a multi-sheet workbook:

    $worksheet1 = $workbook->add_worksheet('To');
    $worksheet2 = $workbook->add_worksheet('the');
    $worksheet3 = $workbook->add_worksheet('wind');

    $worksheet3->activate();

This is similar to the Excel VBA activate method. More than one worksheet can be selected via the C<select()> method, see below, however only one worksheet can be active.

The default active worksheet is the first worksheet.




=head2 select()

The C<select()> method is used to indicate that a worksheet is selected in a multi-sheet workbook:

    $worksheet1->activate();
    $worksheet2->select();
    $worksheet3->select();

A selected worksheet has its tab highlighted. Selecting worksheets is a way of grouping them together so that, for example, several worksheets could be printed in one go. A worksheet that has been activated via the C<activate()> method will also appear as selected.




=head2 hide()

The C<hide()> method is used to hide a worksheet:

    $worksheet2->hide();

You may wish to hide a worksheet in order to avoid confusing a user with intermediate data or calculations.

A hidden worksheet can not be activated or selected so this method is mutually exclusive with the C<activate()> and C<select()> methods. In addition, since the first worksheet will default to being the active worksheet, you cannot hide the first worksheet without activating another sheet:

    $worksheet2->activate();
    $worksheet1->hide();




=head2 set_first_sheet()

The C<activate()> method determines which worksheet is initially selected. However, if there are a large number of worksheets the selected worksheet may not appear on the screen. To avoid this you can select which is the leftmost visible worksheet using C<set_first_sheet()>:

    for (1..20) {
        $workbook->add_worksheet;
    }

    $worksheet21 = $workbook->add_worksheet();
    $worksheet22 = $workbook->add_worksheet();

    $worksheet21->set_first_sheet();
    $worksheet22->activate();

This method is not required very often. The default value is the first worksheet.




=head2 protect($password)

The C<protect()> method is used to protect a worksheet from modification:

    $worksheet->protect();

It can be turned off in Excel via the C<Tools-E<gt>Protection-E<gt>Unprotect Sheet> menu command.

The C<protect()> method also has the effect of enabling a cell's C<locked> and C<hidden> properties if they have been set. A "locked" cell cannot be edited. A "hidden" cell will display the results of a formula but not the formula itself. In Excel a cell's locked property is on by default.

    # Set some format properties
    my $unlocked  = $workbook->add_format(locked => 0);
    my $hidden    = $workbook->add_format(hidden => 1);

    # Enable worksheet protection
    $worksheet->protect();

    # This cell cannot be edited, it is locked by default
    $worksheet->write('A1', '=1+2');

    # This cell can be edited
    $worksheet->write('A2', '=1+2', $unlocked);

    # The formula in this cell isn't visible
    $worksheet->write('A3', '=1+2', $hidden);

See also the C<set_locked> and C<set_hidden> format methods in L</CELL FORMATTING>.

You can optionally add a password to the worksheet protection:

    $worksheet->protect('drowssap');

Note, the worksheet level password in Excel provides very weak protection. It does not encrypt your data in any way and it is very easy to deactivate. Therefore, do not use the above method if you wish to protect sensitive data or calculations. However, before you get worried, Excel's own workbook level password protection does provide strong encryption in Excel 97+. For technical reasons this will never be supported by C<Spreadsheet::WriteExcel>.




=head2 set_selection($first_row, $first_col, $last_row, $last_col)

This method can be used to specify which cell or cells are selected in a worksheet. The most common requirement is to select a single cell, in which case C<$last_row> and C<$last_col> can be omitted. The active cell within a selected range is determined by the order in which C<$first> and C<$last> are specified. It is also possible to specify a cell or a range using A1 notation. See the note about L</Cell notation>.

Examples:

    $worksheet1->set_selection(3, 3);       # 1. Cell D4.
    $worksheet2->set_selection(3, 3, 6, 6); # 2. Cells D4 to G7.
    $worksheet3->set_selection(6, 6, 3, 3); # 3. Cells G7 to D4.
    $worksheet4->set_selection('D4');       # Same as 1.
    $worksheet5->set_selection('D4:G7');    # Same as 2.
    $worksheet6->set_selection('G7:D4');    # Same as 3.

The default cell selections is (0, 0), 'A1'.




=head2 set_row($row, $height, $format, $hidden, $level, $collapsed)

This method can be used to change the default properties of a row. All parameters apart from C<$row> are optional.

The most common use for this method is to change the height of a row:

    $worksheet->set_row(0, 20); # Row 1 height set to 20

If you wish to set the format without changing the height you can pass C<undef> as the height parameter:

    $worksheet->set_row(0, undef, $format);

The C<$format> parameter will be applied to any cells in the row that don't  have a format. For example

    $worksheet->set_row(0, undef, $format1);    # Set the format for row 1
    $worksheet->write('A1', 'Hello');           # Defaults to $format1
    $worksheet->write('B1', 'Hello', $format2); # Keeps $format2

If you wish to define a row format in this way you should call the method before any calls to C<write()>. Calling it afterwards will overwrite any format that was previously specified.

The C<$hidden> parameter should be set to 1 if you wish to hide a row. This can be used, for example, to hide intermediary steps in a complicated calculation:

    $worksheet->set_row(0, 20,    $format, 1);
    $worksheet->set_row(1, undef, undef,   1);

The C<$level> parameter is used to set the outline level of the row. Outlines are described in L</OUTLINES AND GROUPING IN EXCEL>. Adjacent rows with the same outline level are grouped together into a single outline.

The following example sets an outline level of 1 for rows 1 and 2 (zero-indexed):

    $worksheet->set_row(1, undef, undef, 0, 1);
    $worksheet->set_row(2, undef, undef, 0, 1);

The C<$hidden> parameter can also be used to hide collapsed outlined rows when used in conjunction with the C<$level> parameter.

    $worksheet->set_row(1, undef, undef, 1, 1);
    $worksheet->set_row(2, undef, undef, 1, 1);

For collapsed outlines you should also indicate which row has the collapsed C<+> symbol using the optional C<$collapsed> parameter.

    $worksheet->set_row(3, undef, undef, 0, 0, 1);

For a more complete example see the C<outline.pl> and C<outline_collapsed.pl> programs in the examples directory of the distro.

Excel allows up to 7 outline levels. Therefore the C<$level> parameter should be in the range C<0 E<lt>= $level E<lt>= 7>.




=head2 set_column($first_col, $last_col, $width, $format, $hidden, $level, $collapsed)

This method can be used to change the default properties of a single column or a range of columns. All parameters apart from C<$first_col> and C<$last_col> are optional.

If C<set_column()> is applied to a single column the value of C<$first_col> and C<$last_col> should be the same. In the case where C<$last_col> is zero it is set to the same value as C<$first_col>.

It is also possible, and generally clearer, to specify a column range using the form of A1 notation used for columns. See the note about L</Cell notation>.

Examples:

    $worksheet->set_column(0, 0,  20); # Column  A   width set to 20
    $worksheet->set_column(1, 3,  30); # Columns B-D width set to 30
    $worksheet->set_column('E:E', 20); # Column  E   width set to 20
    $worksheet->set_column('F:H', 30); # Columns F-H width set to 30

The width corresponds to the column width value that is specified in Excel. It is approximately equal to the length of a string in the default font of Arial 10. Unfortunately, there is no way to specify "AutoFit" for a column in the Excel file format. This feature is only available at runtime from within Excel.

As usual the C<$format> parameter is optional, for additional information, see L</CELL FORMATTING>. If you wish to set the format without changing the width you can pass C<undef> as the width parameter:

    $worksheet->set_column(0, 0, undef, $format);

The C<$format> parameter will be applied to any cells in the column that don't  have a format. For example

    $worksheet->set_column('A:A', undef, $format1); # Set format for col 1
    $worksheet->write('A1', 'Hello');               # Defaults to $format1
    $worksheet->write('A2', 'Hello', $format2);     # Keeps $format2

If you wish to define a column format in this way you should call the method before any calls to C<write()>. If you call it afterwards it won't have any effect.

A default row format takes precedence over a default column format

    $worksheet->set_row(0, undef,        $format1); # Set format for row 1
    $worksheet->set_column('A:A', undef, $format2); # Set format for col 1
    $worksheet->write('A1', 'Hello');               # Defaults to $format1
    $worksheet->write('A2', 'Hello');               # Defaults to $format2

The C<$hidden> parameter should be set to 1 if you wish to hide a column. This can be used, for example, to hide intermediary steps in a complicated calculation:

    $worksheet->set_column('D:D', 20,    $format, 1);
    $worksheet->set_column('E:E', undef, undef,   1);

The C<$level> parameter is used to set the outline level of the column. Outlines are described in L</OUTLINES AND GROUPING IN EXCEL>. Adjacent columns with the same outline level are grouped together into a single outline.

The following example sets an outline level of 1 for columns B to G:

    $worksheet->set_column('B:G', undef, undef, 0, 1);

The C<$hidden> parameter can also be used to hide collapsed outlined columns when used in conjunction with the C<$level> parameter.

    $worksheet->set_column('B:G', undef, undef, 1, 1);

For collapsed outlines you should also indicate which row has the collapsed C<+> symbol using the optional C<$collapsed> parameter.

    $worksheet->set_column('H:H', undef, undef, 0, 0, 1);

For a more complete example see the C<outline.pl> and C<outline_collapsed.pl> programs in the examples directory of the distro.

Excel allows up to 7 outline levels. Therefore the C<$level> parameter should be in the range C<0 E<lt>= $level E<lt>= 7>.




=head2 outline_settings($visible, $symbols_below, $symbols_right, $auto_style)

The C<outline_settings()> method is used to control the appearance of outlines in Excel. Outlines are described in L</OUTLINES AND GROUPING IN EXCEL>.

The C<$visible> parameter is used to control whether or not outlines are visible. Setting this parameter to 0 will cause all outlines on the worksheet to be hidden. They can be unhidden in Excel by means of the "Show Outline Symbols" command button. The default setting is 1 for visible outlines.

    $worksheet->outline_settings(0);

The C<$symbols_below> parameter is used to control whether the row outline symbol will appear above or below the outline level bar. The default setting is 1 for symbols to appear below the outline level bar.

The C<symbols_right> parameter is used to control whether the column outline symbol will appear to the left or the right of the outline level bar. The default setting is 1 for symbols to appear to the right of the outline level bar.

The C<$auto_style> parameter is used to control whether the automatic outline generator in Excel uses automatic styles when creating an outline. This has no effect on a file generated by C<Spreadsheet::WriteExcel> but it does have an effect on how the worksheet behaves after it is created. The default setting is 0 for "Automatic Styles" to be turned off.

The default settings for all of these parameters correspond to Excel's default parameters.


The worksheet parameters controlled by C<outline_settings()> are rarely used.




=head2 freeze_panes($row, $col, $top_row, $left_col)

This method can be used to divide a worksheet into horizontal or vertical regions known as panes and to also "freeze" these panes so that the splitter bars are not visible. This is the same as the C<Window-E<gt>Freeze Panes> menu command in Excel

The parameters C<$row> and C<$col> are used to specify the location of the split. It should be noted that the split is specified at the top or left of a cell and that the method uses zero based indexing. Therefore to freeze the first row of a worksheet it is necessary to specify the split at row 2 (which is 1 as the zero-based index). This might lead you to think that you are using a 1 based index but this is not the case.

You can set one of the C<$row> and C<$col> parameters as zero if you do not want either a vertical or horizontal split.

Examples:

    $worksheet->freeze_panes(1, 0); # Freeze the first row
    $worksheet->freeze_panes('A2'); # Same using A1 notation
    $worksheet->freeze_panes(0, 1); # Freeze the first column
    $worksheet->freeze_panes('B1'); # Same using A1 notation
    $worksheet->freeze_panes(1, 2); # Freeze first row and first 2 columns
    $worksheet->freeze_panes('C2'); # Same using A1 notation

The parameters C<$top_row> and C<$left_col> are optional. They are used to specify the top-most or left-most visible row or column in the scrolling region of the panes. For example to freeze the first row and to have the scrolling region begin at row twenty:

    $worksheet->freeze_panes(1, 0, 20, 0);

You cannot use A1 notation for the C<$top_row> and C<$left_col> parameters.


See also the C<panes.pl> program in the C<examples> directory of the distribution.




=head2 split_panes($y, $x, $top_row, $left_col)

This method can be used to divide a worksheet into horizontal or vertical regions known as panes. This method is different from the C<freeze_panes()> method in that the splits between the panes will be visible to the user and each pane will have its own scroll bars.

The parameters C<$y> and C<$x> are used to specify the vertical and horizontal position of the split. The units for C<$y> and C<$x> are the same as those used by Excel to specify row height and column width. However, the vertical and horizontal units are different from each other. Therefore you must specify the C<$y> and C<$x> parameters in terms of the row heights and column widths that you have set or the default values which are C<12.75> for a row and  C<8.43> for a column.

You can set one of the C<$y> and C<$x> parameters as zero if you do not want either a vertical or horizontal split. The parameters C<$top_row> and C<$left_col> are optional. They are used to specify the top-most or left-most visible row or column in the bottom-right pane.

Example:

    $worksheet->split_panes(12.75, 0,    1, 0); # First row
    $worksheet->split_panes(0,     8.43, 0, 1); # First column
    $worksheet->split_panes(12.75, 8.43, 1, 1); # First row and column

You cannot use A1 notation with this method.

See also the C<freeze_panes()> method and the C<panes.pl> program in the C<examples> directory of the distribution.


Note: This C<split_panes()> method was called C<thaw_panes()> in older versions. The older name is still available for backwards compatibility.




=head2 merge_range($first_row, $first_col, $last_row, $last_col, $token, $format, $utf_16_be)

Merging cells can be achieved by setting the C<merge> property of a Format object, see L</CELL FORMATTING>. However, this only allows simple Excel5 style horizontal merging which Excel refers to as "center across selection".

The C<merge_range()> method allows you to do Excel97+ style formatting where the cells can contain other types of alignment in addition to the merging:

    my $format = $workbook->add_format(
                                        border  => 6,
                                        valign  => 'vcenter',
                                        align   => 'center',
                                      );

    $worksheet->merge_range('B3:D4', 'Vertical and horizontal', $format);

B<WARNING>. The format object that is used with a C<merge_range()> method call is marked internally as being associated with a merged range. It is a fatal error to use a merged format in a non-merged cell. Instead you should use separate formats for merged and non-merged cells. This restriction will be removed in a future release.

The C<$utf_16_be> parameter is optional, see below.

C<merge_range()> writes its C<$token> argument using the worksheet C<write()> method. Therefore it will handle numbers, strings, formulas or urls as required.

Setting the C<merge> property of the format isn't required when you are using C<merge_range()>. In fact using it will exclude the use of any other horizontal alignment option.

On systems with C<perl 5.8> and later the C<merge_range()> method will also handle strings in C<UTF-8> format.

    $worksheet->merge_range('B3:D4', "\x{263a}", $format); # Smiley

On earlier Perl systems your can specify C<UTF-16BE> worksheet names using an additional optional parameter:

    my $str = pack 'n', 0x263a;
    $worksheet->merge_range('B3:D4', $str, $format, 1); # Smiley

The full possibilities of this method are shown in the C<merge3.pl> to C<merge6.pl> programs in the C<examples> directory of the distribution.




=head2 set_zoom($scale)

Set the worksheet zoom factor in the range C<10 E<lt>= $scale E<lt>= 400>:

    $worksheet1->set_zoom(50);
    $worksheet2->set_zoom(75);
    $worksheet3->set_zoom(300);
    $worksheet4->set_zoom(400);

The default zoom factor is 100. You cannot zoom to "Selection" because it is calculated by Excel at run-time.

Note, C<set_zoom()> does not affect the scale of the printed page. For that you should use C<set_print_scale()>.




=head2 right_to_left()

The C<right_to_left()> method is used to change the default direction of the worksheet from left-to-right, with the A1 cell in the top left, to right-to-left, with the he A1 cell in the top right.

    $worksheet->right_to_left();

This is useful when creating Arabic, Hebrew or other near or far eastern worksheets that use right-to-left as the default direction.




=head2 hide_zero()

The C<hide_zero()> method is used to hide any zero values that appear in cells.

    $worksheet->hide_zero();

In Excel this option is found under Tools->Options->View.




=head2 set_tab_color()

The C<set_tab_color()> method is used to change the colour of the worksheet tab. This feature is only available in Excel 2002 and later. You can use one of the standard colour names provided by the Format object or a colour index. See L</COLOURS IN EXCEL> and the C<set_custom_color()> method.

    $worksheet1->set_tab_color('red');
    $worksheet2->set_tab_color(0x0C);

See the C<tab_colors.pl> program in the examples directory of the distro.




=head2 autofilter($first_row, $first_col, $last_row, $last_col)

This method allows an autofilter to be added to a worksheet. An autofilter is a way of adding drop down lists to the headers of a 2D range of worksheet data. This in turn allow users to filter the data based on simple criteria so that some data is shown and some is hidden.

To add an autofilter to a worksheet:

    $worksheet->autofilter(0, 0, 10, 3);
    $worksheet->autofilter('A1:D11');    # Same as above in A1 notation.

Filter conditions can be applied using the C<filter_column()> method.

See the C<autofilter.pl> program in the examples directory of the distro for a more detailed example.




=head2 filter_column($column, $expression)


The C<filter_column> method can be used to filter columns in a autofilter range based on simple conditions.

B<NOTE:> It isn't sufficient to just specify the filter condition. You must also hide any rows that don't match the filter condition. Rows are hidden using the C<set_row()> C<visible> parameter. C<Spreadsheet::WriteExcel> cannot do this automatically since it isn't part of the file format. See the C<autofilter.pl> program in the examples directory of the distro for an example.

The conditions for the filter are specified using simple expressions:

    $worksheet->filter_column('A', 'x > 2000');
    $worksheet->filter_column('B', 'x > 2000 and x < 5000');

The C<$column> parameter can either be a zero indexed column number or a string column name.

The following operators are available:

    Operator        Synonyms
       ==           =   eq  =~
       !=           <>  ne  !=
       >
       <
       >=
       <=

       and          &&
       or           ||

The operator synonyms are just syntactic sugar to make you more comfortable using the expressions. It is important to remember that the expressions will be interpreted by Excel and not by perl.

An expression can comprise a single statement or two statements separated by the C<and> and C<or> operators. For example:

    'x <  2000'
    'x >  2000'
    'x == 2000'
    'x >  2000 and x <  5000'
    'x == 2000 or  x == 5000'

Filtering of blank or non-blank data can be achieved by using a value of C<Blanks> or C<NonBlanks> in the expression:

    'x == Blanks'
    'x == NonBlanks'

Top 10 style filters can be specified using a expression like the following:

    Top|Bottom 1-500 Items|%

For example:

    'Top    10 Items'
    'Bottom  5 Items'
    'Top    25 %'
    'Bottom 50 %'

Excel also allows some simple string matching operations:

    'x =~ b*'   # begins with b
    'x !~ b*'   # doesn't begin with b
    'x =~ *b'   # ends with b
    'x !~ *b'   # doesn't end with b
    'x =~ *b*'  # contains b
    'x !~ *b*'  # doesn't contains b

You can also use C<*> to match any character or number and C<?> to match any single character or number. No other regular expression quantifier is supported by Excel's filters. Excel's regular expression characters can be escaped using C<~>.

The placeholder variable C<x> in the above examples can be replaced by any simple string. The actual placeholder name is ignored internally so the following are all equivalent:

    'x     < 2000'
    'col   < 2000'
    'Price < 2000'

Also, note that a filter condition can only be applied to a column in a range specified by the C<autofilter()> Worksheet method.

See the C<autofilter.pl> program in the examples directory of the distro for a more detailed example.




=head1 PAGE SET-UP METHODS

Page set-up methods affect the way that a worksheet looks when it is printed. They control features such as page headers and footers and margins. These methods are really just standard worksheet methods. They are documented here in a separate section for the sake of clarity.

The following methods are available for page set-up:

    set_landscape()
    set_portrait()
    set_page_view()
    set_paper()
    center_horizontally()
    center_vertically()
    set_margins()
    set_header()
    set_footer()
    repeat_rows()
    repeat_columns()
    hide_gridlines()
    print_row_col_headers()
    print_area()
    print_across()
    fit_to_pages()
    set_start_page()
    set_print_scale()
    set_h_pagebreaks()
    set_v_pagebreaks()


A common requirement when working with Spreadsheet::WriteExcel is to apply the same page set-up features to all of the worksheets in a workbook. To do this you can use the C<sheets()> method of the C<workbook> class to access the array of worksheets in a workbook:

    foreach $worksheet ($workbook->sheets()) {
       $worksheet->set_landscape();
    }




=head2 set_landscape()

This method is used to set the orientation of a worksheet's printed page to landscape:

    $worksheet->set_landscape(); # Landscape mode




=head2 set_portrait()

This method is used to set the orientation of a worksheet's printed page to portrait. The default worksheet orientation is portrait, so you won't generally need to call this method.

    $worksheet->set_portrait(); # Portrait mode



=head2 set_page_view()

This method is used to display the worksheet in "Page View" mode. This is currently only supported by Mac Excel, where it is the default.

    $worksheet->set_page_view();



=head2 set_paper($index)

This method is used to set the paper format for the printed output of a worksheet. The following paper styles are available:

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


Note, it is likely that not all of these paper types will be available to the end user since it will depend on the paper formats that the user's printer supports. Therefore, it is best to stick to standard paper types.

    $worksheet->set_paper(1); # US Letter
    $worksheet->set_paper(9); # A4

If you do not specify a paper type the worksheet will print using the printer's default paper.




=head2 center_horizontally()

Center the worksheet data horizontally between the margins on the printed page:

    $worksheet->center_horizontally();




=head2 center_vertically()

Center the worksheet data vertically between the margins on the printed page:

    $worksheet->center_vertically();




=head2 set_margins($inches)

There are several methods available for setting the worksheet margins on the printed page:

    set_margins()        # Set all margins to the same value
    set_margins_LR()     # Set left and right margins to the same value
    set_margins_TB()     # Set top and bottom margins to the same value
    set_margin_left();   # Set left margin
    set_margin_right();  # Set right margin
    set_margin_top();    # Set top margin
    set_margin_bottom(); # Set bottom margin

All of these methods take a distance in inches as a parameter. Note: 1 inch = 25.4mm. ;-) The default left and right margin is 0.75 inch. The default top and bottom margin is 1.00 inch.



=head2 set_header($string, $margin)

Headers and footers are generated using a C<$string> which is a combination of plain text and control characters. The C<$margin> parameter is optional.

The available control character are:

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
    &Z                                      Workbook path

    &fontsize           Font                Font size
    &"font,style"                           Font name and style
    &U                                      Single underline
    &E                                      Double underline
    &S                                      Strikethrough
    &X                                      Superscript
    &Y                                      Subscript

    &&                  Miscellaneous       Literal ampersand &


Text in headers and footers can be justified (aligned) to the left, center and right by prefixing the text with the control characters C<&L>, C<&C> and C<&R>.

For example (with ASCII art representation of the results):

    $worksheet->set_header('&LHello');

     ---------------------------------------------------------------
    |                                                               |
    | Hello                                                         |
    |                                                               |


    $worksheet->set_header('&CHello');

     ---------------------------------------------------------------
    |                                                               |
    |                          Hello                                |
    |                                                               |


    $worksheet->set_header('&RHello');

     ---------------------------------------------------------------
    |                                                               |
    |                                                         Hello |
    |                                                               |


For simple text, if you do not specify any justification the text will be centred. However, you must prefix the text with C<&C> if you specify a font name or any other formatting:

    $worksheet->set_header('Hello');

     ---------------------------------------------------------------
    |                                                               |
    |                          Hello                                |
    |                                                               |


You can have text in each of the justification regions:

    $worksheet->set_header('&LCiao&CBello&RCielo');

     ---------------------------------------------------------------
    |                                                               |
    | Ciao                     Bello                          Cielo |
    |                                                               |


The information control characters act as variables that Excel will update as the workbook or worksheet changes. Times and dates are in the users default format:

    $worksheet->set_header('&CPage &P of &N');

     ---------------------------------------------------------------
    |                                                               |
    |                        Page 1 of 6                            |
    |                                                               |


    $worksheet->set_header('&CUpdated at &T');

     ---------------------------------------------------------------
    |                                                               |
    |                    Updated at 12:30 PM                        |
    |                                                               |



You can specify the font size of a section of the text by prefixing it with the control character C<&n> where C<n> is the font size:

    $worksheet1->set_header('&C&30Hello Big'  );
    $worksheet2->set_header('&C&10Hello Small');

You can specify the font of a section of the text by prefixing it with the control sequence C<&"font,style"> where C<fontname> is a font name such as "Courier New" or "Times New Roman" and C<style> is one of the standard Windows font descriptions: "Regular", "Italic", "Bold" or "Bold Italic":

    $worksheet1->set_header('&C&"Courier New,Italic"Hello');
    $worksheet2->set_header('&C&"Courier New,Bold Italic"Hello');
    $worksheet3->set_header('&C&"Times New Roman,Regular"Hello');

It is possible to combine all of these features together to create sophisticated headers and footers. As an aid to setting up complicated headers and footers you can record a page set-up as a macro in Excel and look at the format strings that VBA produces. Remember however that VBA uses two double quotes C<""> to indicate a single double quote. For the last example above the equivalent VBA code looks like this:

    .LeftHeader   = ""
    .CenterHeader = "&""Times New Roman,Regular""Hello"
    .RightHeader  = ""


To include a single literal ampersand C<&> in a header or footer you should use a double ampersand C<&&>:

    $worksheet1->set_header('&CCuriouser && Curiouser - Attorneys at Law');

As stated above the margin parameter is optional. As with the other margins the value should be in inches. The default header and footer margin is 0.50 inch. The header and footer margin size can be set as follows:

    $worksheet->set_header('&CHello', 0.75);

The header and footer margins are independent of the top and bottom margins.

Note, the header or footer string must be less than 255 characters. Strings longer than this will not be written and a warning will be generated.

On systems with C<perl 5.8> and later the C<set_header()> method can also handle Unicode strings in C<UTF-8> format.

    $worksheet->set_header("&C\x{263a}")


See, also the C<headers.pl> program in the C<examples> directory of the distribution.




=head2 set_footer()

The syntax of the C<set_footer()> method is the same as C<set_header()>,  see above.




=head2 repeat_rows($first_row, $last_row)

Set the number of rows to repeat at the top of each printed page.

For large Excel documents it is often desirable to have the first row or rows of the worksheet print out at the top of each page. This can be achieved by using the C<repeat_rows()> method. The parameters C<$first_row> and C<$last_row> are zero based. The C<$last_row> parameter is optional if you only wish to specify one row:

    $worksheet1->repeat_rows(0);    # Repeat the first row
    $worksheet2->repeat_rows(0, 1); # Repeat the first two rows




=head2 repeat_columns($first_col, $last_col)

Set the columns to repeat at the left hand side of each printed page.

For large Excel documents it is often desirable to have the first column or columns of the worksheet print out at the left hand side of each page. This can be achieved by using the C<repeat_columns()> method. The parameters C<$first_column> and C<$last_column> are zero based. The C<$last_column> parameter is optional if you only wish to specify one column. You can also specify the columns using A1 column notation, see the note about L</Cell notation>.

    $worksheet1->repeat_columns(0);     # Repeat the first column
    $worksheet2->repeat_columns(0, 1);  # Repeat the first two columns
    $worksheet3->repeat_columns('A:A'); # Repeat the first column
    $worksheet4->repeat_columns('A:B'); # Repeat the first two columns




=head2 hide_gridlines($option)

This method is used to hide the gridlines on the screen and printed page. Gridlines are the lines that divide the cells on a worksheet. Screen and printed gridlines are turned on by default in an Excel worksheet. If you have defined your own cell borders you may wish to hide the default gridlines.

    $worksheet->hide_gridlines();

The following values of C<$option> are valid:

    0 : Don't hide gridlines
    1 : Hide printed gridlines only
    2 : Hide screen and printed gridlines

If you don't supply an argument or use C<undef> the default option is 1, i.e. only the printed gridlines are hidden.




=head2 print_row_col_headers()

Set the option to print the row and column headers on the printed page.

An Excel worksheet looks something like the following;

     ------------------------------------------
    |   |   A   |   B   |   C   |   D   |  ...
     ------------------------------------------
    | 1 |       |       |       |       |  ...
    | 2 |       |       |       |       |  ...
    | 3 |       |       |       |       |  ...
    | 4 |       |       |       |       |  ...
    |...|  ...  |  ...  |  ...  |  ...  |  ...

The headers are the letters and numbers at the top and the left of the worksheet. Since these headers serve mainly as a indication of position on the worksheet they generally do not appear on the printed page. If you wish to have them printed you can use the C<print_row_col_headers()> method :

    $worksheet->print_row_col_headers();

Do not confuse these headers with page headers as described in the C<set_header()> section above.




=head2 print_area($first_row, $first_col, $last_row, $last_col)

This method is used to specify the area of the worksheet that will be printed. All four parameters must be specified. You can also use A1 notation, see the note about L</Cell notation>.


    $worksheet1->print_area('A1:H20');    # Cells A1 to H20
    $worksheet2->print_area(0, 0, 19, 7); # The same
    $worksheet2->print_area('A:H');       # Columns A to H if rows have data




=head2 print_across()

The C<print_across> method is used to change the default print direction. This is referred to by Excel as the sheet "page order".

    $worksheet->print_across();

The default page order is shown below for a worksheet that extends over 4 pages. The order is called "down then across":

    [1] [3]
    [2] [4]

However, by using the C<print_across> method the print order will be changed to "across then down":

    [1] [2]
    [3] [4]




=head2 fit_to_pages($width, $height)

The C<fit_to_pages()> method is used to fit the printed area to a specific number of pages both vertically and horizontally. If the printed area exceeds the specified number of pages it will be scaled down to fit. This guarantees that the printed area will always appear on the specified number of pages even if the page size or margins change.

    $worksheet1->fit_to_pages(1, 1); # Fit to 1x1 pages
    $worksheet2->fit_to_pages(2, 1); # Fit to 2x1 pages
    $worksheet3->fit_to_pages(1, 2); # Fit to 1x2 pages

The print area can be defined using the C<print_area()> method as described above.

A common requirement is to fit the printed output to I<n> pages wide but have the height be as long as necessary. To achieve this set the C<$height> to zero or leave it blank:

    $worksheet1->fit_to_pages(1, 0); # 1 page wide and as long as necessary
    $worksheet2->fit_to_pages(1);    # The same


Note that although it is valid to use both C<fit_to_pages()> and C<set_print_scale()> on the same worksheet only one of these options can be active at a time. The last method call made will set the active option.

Note that C<fit_to_pages()> will override any manual page breaks that are defined in the worksheet.




=head2 set_start_page($start_page)

The C<set_start_page()> method is used to set the number of the starting page when the worksheet is printed out. The default value is 1.

    $worksheet->set_start_page(2);




=head2 set_print_scale($scale)

Set the scale factor of the printed page. Scale factors in the range C<10 E<lt>= $scale E<lt>= 400> are valid:

    $worksheet1->set_print_scale(50);
    $worksheet2->set_print_scale(75);
    $worksheet3->set_print_scale(300);
    $worksheet4->set_print_scale(400);

The default scale factor is 100. Note, C<set_print_scale()> does not affect the scale of the visible page in Excel. For that you should use C<set_zoom()>.

Note also that although it is valid to use both C<fit_to_pages()> and C<set_print_scale()> on the same worksheet only one of these options can be active at a time. The last method call made will set the active option.




=head2 set_h_pagebreaks(@breaks)

Add horizontal page breaks to a worksheet. A page break causes all the data that follows it to be printed on the next page. Horizontal page breaks act between rows. To create a page break between rows 20 and 21 you must specify the break at row 21. However in zero index notation this is actually row 20. So you can pretend for a small while that you are using 1 index notation:

    $worksheet1->set_h_pagebreaks(20); # Break between row 20 and 21

The C<set_h_pagebreaks()> method will accept a list of page breaks and you can call it more than once:

    $worksheet2->set_h_pagebreaks( 20,  40,  60,  80, 100); # Add breaks
    $worksheet2->set_h_pagebreaks(120, 140, 160, 180, 200); # Add some more

Note: If you specify the "fit to page" option via the C<fit_to_pages()> method it will override all manual page breaks.

There is a silent limitation of about 1000 horizontal page breaks per worksheet in line with an Excel internal limitation.




=head2 set_v_pagebreaks(@breaks)

Add vertical page breaks to a worksheet. A page break causes all the data that follows it to be printed on the next page. Vertical page breaks act between columns. To create a page break between columns 20 and 21 you must specify the break at column 21. However in zero index notation this is actually column 20. So you can pretend for a small while that you are using 1 index notation:

    $worksheet1->set_v_pagebreaks(20); # Break between column 20 and 21

The C<set_v_pagebreaks()> method will accept a list of page breaks and you can call it more than once:

    $worksheet2->set_v_pagebreaks( 20,  40,  60,  80, 100); # Add breaks
    $worksheet2->set_v_pagebreaks(120, 140, 160, 180, 200); # Add some more

Note: If you specify the "fit to page" option via the C<fit_to_pages()> method it will override all manual page breaks.




=head1 CELL FORMATTING

This section describes the methods and properties that are available for formatting cells in Excel. The properties of a cell that can be formatted include: fonts, colours, patterns, borders, alignment and number formatting.


=head2 Creating and using a Format object

Cell formatting is defined through a Format object. Format objects are created by calling the workbook C<add_format()> method as follows:

    my $format1 = $workbook->add_format();       # Set properties later
    my $format2 = $workbook->add_format(%props); # Set at creation

The format object holds all the formatting properties that can be applied to a cell, a row or a column. The process of setting these properties is discussed in the next section.

Once a Format object has been constructed and its properties have been set it can be passed as an argument to the worksheet C<write> methods as follows:

    $worksheet->write(0, 0, 'One', $format);
    $worksheet->write_string(1, 0, 'Two', $format);
    $worksheet->write_number(2, 0, 3, $format);
    $worksheet->write_blank(3, 0, $format);

Formats can also be passed to the worksheet C<set_row()> and C<set_column()> methods to define the default property for a row or column.

    $worksheet->set_row(0, 15, $format);
    $worksheet->set_column(0, 0, 15, $format);




=head2 Format methods and Format properties

The following table shows the Excel format categories, the formatting properties that can be applied and the equivalent object method:


    Category   Description       Property        Method Name
    --------   -----------       --------        -----------
    Font       Font type         font            set_font()
               Font size         size            set_size()
               Font color        color           set_color()
               Bold              bold            set_bold()
               Italic            italic          set_italic()
               Underline         underline       set_underline()
               Strikeout         font_strikeout  set_font_strikeout()
               Super/Subscript   font_script     set_font_script()
               Outline           font_outline    set_font_outline()
               Shadow            font_shadow     set_font_shadow()

    Number     Numeric format    num_format      set_num_format()

    Protection Lock cells        locked          set_locked()
               Hide formulas     hidden          set_hidden()

    Alignment  Horizontal align  align           set_align()
               Vertical align    valign          set_align()
               Rotation          rotation        set_rotation()
               Text wrap         text_wrap       set_text_wrap()
               Justify last      text_justlast   set_text_justlast()
               Center across     center_across   set_center_across()
               Indentation       indent          set_indent()
               Shrink to fit     shrink          set_shrink()

    Pattern    Cell pattern      pattern         set_pattern()
               Background color  bg_color        set_bg_color()
               Foreground color  fg_color        set_fg_color()

    Border     Cell border       border          set_border()
               Bottom border     bottom          set_bottom()
               Top border        top             set_top()
               Left border       left            set_left()
               Right border      right           set_right()
               Border color      border_color    set_border_color()
               Bottom color      bottom_color    set_bottom_color()
               Top color         top_color       set_top_color()
               Left color        left_color      set_left_color()
               Right color       right_color     set_right_color()

There are two ways of setting Format properties: by using the object method interface or by setting the property directly. For example, a typical use of the method interface would be as follows:

    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_color('red');

By comparison the properties can be set directly by passing a hash of properties to the Format constructor:

    my $format = $workbook->add_format(bold => 1, color => 'red');

or after the Format has been constructed by means of the C<set_format_properties()> method as follows:

    my $format = $workbook->add_format();
    $format->set_format_properties(bold => 1, color => 'red');

You can also store the properties in one or more named hashes and pass them to the required method:

    my %font    = (
                    font  => 'Arial',
                    size  => 12,
                    color => 'blue',
                    bold  => 1,
                  );

    my %shading = (
                    bg_color => 'green',
                    pattern  => 1,
                  );


    my $format1 = $workbook->add_format(%font);           # Font only
    my $format2 = $workbook->add_format(%font, %shading); # Font and shading


The provision of two ways of setting properties might lead you to wonder which is the best way. The method mechanism may be better is you prefer setting properties via method calls (which the author did when the code was first written) otherwise passing properties to the constructor has proved to be a little more flexible and self documenting in practice. An additional advantage of working with property hashes is that it allows you to share formatting between workbook objects as shown in the example above.

The Perl/Tk style of adding properties is also supported:

    my %font    = (
                    -font      => 'Arial',
                    -size      => 12,
                    -color     => 'blue',
                    -bold      => 1,
                  );




=head2 Working with formats

The default format is Arial 10 with all other properties off.

Each unique format in Spreadsheet::WriteExcel must have a corresponding Format object. It isn't possible to use a Format with a write() method and then redefine the Format for use at a later stage. This is because a Format is applied to a cell not in its current state but in its final state. Consider the following example:

    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_color('red');
    $worksheet->write('A1', 'Cell A1', $format);
    $format->set_color('green');
    $worksheet->write('B1', 'Cell B1', $format);

Cell A1 is assigned the Format C<$format> which is initially set to the colour red. However, the colour is subsequently set to green. When Excel displays Cell A1 it will display the final state of the Format which in this case will be the colour green.

In general a method call without an argument will turn a property on, for example:

    my $format1 = $workbook->add_format();
    $format1->set_bold();  # Turns bold on
    $format1->set_bold(1); # Also turns bold on
    $format1->set_bold(0); # Turns bold off




=head1 FORMAT METHODS

The Format object methods are described in more detail in the following sections. In addition, there is a Perl program called C<formats.pl> in the C<examples> directory of the WriteExcel distribution. This program creates an Excel workbook called C<formats.xls> which contains examples of almost all the format types.

The following Format methods are available:

    set_font()
    set_size()
    set_color()
    set_bold()
    set_italic()
    set_underline()
    set_font_strikeout()
    set_font_script()
    set_font_outline()
    set_font_shadow()
    set_num_format()
    set_locked()
    set_hidden()
    set_align()
    set_rotation()
    set_text_wrap()
    set_text_justlast()
    set_center_across()
    set_indent()
    set_shrink()
    set_pattern()
    set_bg_color()
    set_fg_color()
    set_border()
    set_bottom()
    set_top()
    set_left()
    set_right()
    set_border_color()
    set_bottom_color()
    set_top_color()
    set_left_color()
    set_right_color()


The above methods can also be applied directly as properties. For example C<< $format->set_bold() >> is equivalent to C<< $workbook->add_format(bold => 1) >>.


=head2 set_format_properties(%properties)

The properties of an existing Format object can be also be set by means of C<set_format_properties()>:

    my $format = $workbook->add_format();
    $format->set_format_properties(bold => 1, color => 'red');

However, this method is here mainly for legacy reasons. It is preferable to set the properties in the format constructor:

    my $format = $workbook->add_format(bold => 1, color => 'red');


=head2 set_font($fontname)

    Default state:      Font is Arial
    Default action:     None
    Valid args:         Any valid font name

Specify the font used:

    $format->set_font('Times New Roman');

Excel can only display fonts that are installed on the system that it is running on. Therefore it is best to use the fonts that come as standard such as 'Arial', 'Times New Roman' and 'Courier New'. See also the Fonts worksheet created by formats.pl




=head2 set_size()

    Default state:      Font size is 10
    Default action:     Set font size to 1
    Valid args:         Integer values from 1 to as big as your screen.


Set the font size. Excel adjusts the height of a row to accommodate the largest font size in the row. You can also explicitly specify the height of a row using the set_row() worksheet method.

    my $format = $workbook->add_format();
    $format->set_size(30);





=head2 set_color()

    Default state:      Excels default color, usually black
    Default action:     Set the default color
    Valid args:         Integers from 8..63 or the following strings:
                        'black'
                        'blue'
                        'brown'
                        'cyan'
                        'gray'
                        'green'
                        'lime'
                        'magenta'
                        'navy'
                        'orange'
                        'pink'
                        'purple'
                        'red'
                        'silver'
                        'white'
                        'yellow'

Set the font colour. The C<set_color()> method is used as follows:

    my $format = $workbook->add_format();
    $format->set_color('red');
    $worksheet->write(0, 0, 'wheelbarrow', $format);

Note: The C<set_color()> method is used to set the colour of the font in a cell. To set the colour of a cell use the C<set_bg_color()> and C<set_pattern()> methods.

For additional examples see the 'Named colors' and 'Standard colors' worksheets created by formats.pl in the examples directory.

See also L</COLOURS IN EXCEL>.




=head2 set_bold()

    Default state:      bold is off
    Default action:     Turn bold on
    Valid args:         0, 1 [1]

Set the bold property of the font:

    $format->set_bold();  # Turn bold on

[1] Actually, values in the range 100..1000 are also valid. 400 is normal, 700 is bold and 1000 is very bold indeed. It is probably best to set the value to 1 and use normal bold.




=head2 set_italic()

    Default state:      Italic is off
    Default action:     Turn italic on
    Valid args:         0, 1

Set the italic property of the font:

    $format->set_italic();  # Turn italic on




=head2 set_underline()

    Default state:      Underline is off
    Default action:     Turn on single underline
    Valid args:         0  = No underline
                        1  = Single underline
                        2  = Double underline
                        33 = Single accounting underline
                        34 = Double accounting underline

Set the underline property of the font.

    $format->set_underline();   # Single underline




=head2 set_font_strikeout()

    Default state:      Strikeout is off
    Default action:     Turn strikeout on
    Valid args:         0, 1

Set the strikeout property of the font.




=head2 set_font_script()

    Default state:      Super/Subscript is off
    Default action:     Turn Superscript on
    Valid args:         0  = Normal
                        1  = Superscript
                        2  = Subscript

Set the superscript/subscript property of the font. This format is currently not very useful.




=head2 set_font_outline()

    Default state:      Outline is off
    Default action:     Turn outline on
    Valid args:         0, 1

Macintosh only.




=head2 set_font_shadow()

    Default state:      Shadow is off
    Default action:     Turn shadow on
    Valid args:         0, 1

Macintosh only.




=head2 set_num_format()

    Default state:      General format
    Default action:     Format index 1
    Valid args:         See the following table

This method is used to define the numerical format of a number in Excel. It controls whether a number is displayed as an integer, a floating point number, a date, a currency value or some other user defined format.

The numerical format of a cell can be specified by using a format string or an index to one of Excel's built-in formats:

    my $format1 = $workbook->add_format();
    my $format2 = $workbook->add_format();
    $format1->set_num_format('d mmm yyyy'); # Format string
    $format2->set_num_format(0x0f);         # Format index

    $worksheet->write(0, 0, 36892.521, $format1);      # 1 Jan 2001
    $worksheet->write(0, 0, 36892.521, $format2);      # 1-Jan-01


Using format strings you can define very sophisticated formatting of numbers.

    $format01->set_num_format('0.000');
    $worksheet->write(0,  0, 3.1415926, $format01);    # 3.142

    $format02->set_num_format('#,##0');
    $worksheet->write(1,  0, 1234.56,   $format02);    # 1,235

    $format03->set_num_format('#,##0.00');
    $worksheet->write(2,  0, 1234.56,   $format03);    # 1,234.56

    $format04->set_num_format('$0.00');
    $worksheet->write(3,  0, 49.99,     $format04);    # $49.99

    # Note you can use other currency symbols such as the pound or yen as well.
    # Other currencies may require the use of Unicode.

    $format07->set_num_format('mm/dd/yy');
    $worksheet->write(6,  0, 36892.521, $format07);    # 01/01/01

    $format08->set_num_format('mmm d yyyy');
    $worksheet->write(7,  0, 36892.521, $format08);    # Jan 1 2001

    $format09->set_num_format('d mmmm yyyy');
    $worksheet->write(8,  0, 36892.521, $format09);    # 1 January 2001

    $format10->set_num_format('dd/mm/yyyy hh:mm AM/PM');
    $worksheet->write(9,  0, 36892.521, $format10);    # 01/01/2001 12:30 AM

    $format11->set_num_format('0 "dollar and" .00 "cents"');
    $worksheet->write(10, 0, 1.87,      $format11);    # 1 dollar and .87 cents

    # Conditional formatting
    $format12->set_num_format('[Green]General;[Red]-General;General');
    $worksheet->write(11, 0, 123,       $format12);    # > 0 Green
    $worksheet->write(12, 0, -45,       $format12);    # < 0 Red
    $worksheet->write(13, 0, 0,         $format12);    # = 0 Default colour

    # Zip code
    $format13->set_num_format('00000');
    $worksheet->write(14, 0, '01209',   $format13);


The number system used for dates is described in L</DATES AND TIME IN EXCEL>.

The colour format should have one of the following values:

    [Black] [Blue] [Cyan] [Green] [Magenta] [Red] [White] [Yellow]

Alternatively you can specify the colour based on a colour index as follows: C<[Color n]>, where n is a standard Excel colour index - 7. See the 'Standard colors' worksheet created by formats.pl.

For more information refer to the documentation on formatting in the C<docs> directory of the Spreadsheet::WriteExcel distro, the Excel on-line help or L<http://office.microsoft.com/en-gb/assistance/HP051995001033.aspx>.

You should ensure that the format string is valid in Excel prior to using it in WriteExcel.

Excel's built-in formats are shown in the following table:

    Index   Index   Format String
    0       0x00    General
    1       0x01    0
    2       0x02    0.00
    3       0x03    #,##0
    4       0x04    #,##0.00
    5       0x05    ($#,##0_);($#,##0)
    6       0x06    ($#,##0_);[Red]($#,##0)
    7       0x07    ($#,##0.00_);($#,##0.00)
    8       0x08    ($#,##0.00_);[Red]($#,##0.00)
    9       0x09    0%
    10      0x0a    0.00%
    11      0x0b    0.00E+00
    12      0x0c    # ?/?
    13      0x0d    # ??/??
    14      0x0e    m/d/yy
    15      0x0f    d-mmm-yy
    16      0x10    d-mmm
    17      0x11    mmm-yy
    18      0x12    h:mm AM/PM
    19      0x13    h:mm:ss AM/PM
    20      0x14    h:mm
    21      0x15    h:mm:ss
    22      0x16    m/d/yy h:mm
    ..      ....    ...........
    37      0x25    (#,##0_);(#,##0)
    38      0x26    (#,##0_);[Red](#,##0)
    39      0x27    (#,##0.00_);(#,##0.00)
    40      0x28    (#,##0.00_);[Red](#,##0.00)
    41      0x29    _(* #,##0_);_(* (#,##0);_(* "-"_);_(@_)
    42      0x2a    _($* #,##0_);_($* (#,##0);_($* "-"_);_(@_)
    43      0x2b    _(* #,##0.00_);_(* (#,##0.00);_(* "-"??_);_(@_)
    44      0x2c    _($* #,##0.00_);_($* (#,##0.00);_($* "-"??_);_(@_)
    45      0x2d    mm:ss
    46      0x2e    [h]:mm:ss
    47      0x2f    mm:ss.0
    48      0x30    ##0.0E+0
    49      0x31    @


For examples of these formatting codes see the 'Numerical formats' worksheet created by formats.pl. See also the number_formats1.html and the number_formats2.html documents in the C<docs> directory of the distro.

Note 1. Numeric formats 23 to 36 are not documented by Microsoft and may differ in international versions.

Note 2. In Excel 5 the dollar sign appears as a dollar sign. In Excel 97-2000 it appears as the defined local currency symbol.

Note 3. The red negative numeric formats display slightly differently in Excel 5 and Excel 97-2000.




=head2 set_locked()

    Default state:      Cell locking is on
    Default action:     Turn locking on
    Valid args:         0, 1

This property can be used to prevent modification of a cells contents. Following Excel's convention, cell locking is turned on by default. However, it only has an effect if the worksheet has been protected, see the worksheet C<protect()> method.

    my $locked  = $workbook->add_format();
    $locked->set_locked(1); # A non-op

    my $unlocked = $workbook->add_format();
    $locked->set_locked(0);

    # Enable worksheet protection
    $worksheet->protect();

    # This cell cannot be edited.
    $worksheet->write('A1', '=1+2', $locked);

    # This cell can be edited.
    $worksheet->write('A2', '=1+2', $unlocked);

Note: This offers weak protection even with a password, see the note in relation to the C<protect()> method.




=head2 set_hidden()

    Default state:      Formula hiding is off
    Default action:     Turn hiding on
    Valid args:         0, 1

This property is used to hide a formula while still displaying its result. This is generally used to hide complex calculations from end users who are only interested in the result. It only has an effect if the worksheet has been protected, see the worksheet C<protect()> method.

    my $hidden = $workbook->add_format();
    $hidden->set_hidden();

    # Enable worksheet protection
    $worksheet->protect();

    # The formula in this cell isn't visible
    $worksheet->write('A1', '=1+2', $hidden);


Note: This offers weak protection even with a password, see the note in relation to the C<protect()> method.




=head2 set_align()

    Default state:      Alignment is off
    Default action:     Left alignment
    Valid args:         'left'              Horizontal
                        'center'
                        'right'
                        'fill'
                        'justify'
                        'center_across'

                        'top'               Vertical
                        'vcenter'
                        'bottom'
                        'vjustify'

This method is used to set the horizontal and vertical text alignment within a cell. Vertical and horizontal alignments can be combined. The method is used as follows:

    my $format = $workbook->add_format();
    $format->set_align('center');
    $format->set_align('vcenter');
    $worksheet->set_row(0, 30);
    $worksheet->write(0, 0, 'X', $format);

Text can be aligned across two or more adjacent cells using the C<center_across> property. However, for genuine merged cells it is better to use the C<merge_range()> worksheet method.

The C<vjustify> (vertical justify) option can be used to provide automatic text wrapping in a cell. The height of the cell will be adjusted to accommodate the wrapped text. To specify where the text wraps use the C<set_text_wrap()> method.


For further examples see the 'Alignment' worksheet created by formats.pl.




=head2 set_center_across()

    Default state:      Center across selection is off
    Default action:     Turn center across on
    Valid args:         1

Text can be aligned across two or more adjacent cells using the C<set_center_across()> method. This is an alias for the C<set_align('center_across')> method call.

Only one cell should contain the text, the other cells should be blank:

    my $format = $workbook->add_format();
    $format->set_center_across();

    $worksheet->write(1, 1, 'Center across selection', $format);
    $worksheet->write_blank(1, 2, $format);

See also the C<merge1.pl> to C<merge6.pl> programs in the C<examples> directory and the C<merge_range()> method.



=head2 set_text_wrap()

    Default state:      Text wrap is off
    Default action:     Turn text wrap on
    Valid args:         0, 1


Here is an example using the text wrap property, the escape character C<\n> is used to indicate the end of line:

    my $format = $workbook->add_format();
    $format->set_text_wrap();
    $worksheet->write(0, 0, "It's\na bum\nwrap", $format);

Excel will adjust the height of the row to accommodate the wrapped text. A similar effect can be obtained without newlines using the C<set_align('vjustify')> method. See the C<textwrap.pl> program in the C<examples> directory.




=head2 set_rotation()

    Default state:      Text rotation is off
    Default action:     None
    Valid args:         Integers in the range -90 to 90 and 270

Set the rotation of the text in a cell. The rotation can be any angle in the range -90 to 90 degrees.

    my $format = $workbook->add_format();
    $format->set_rotation(30);
    $worksheet->write(0, 0, 'This text is rotated', $format);


The angle 270 is also supported. This indicates text where the letters run from top to bottom.



=head2 set_indent()


    Default state:      Text indentation is off
    Default action:     Indent text 1 level
    Valid args:         Positive integers


This method can be used to indent text. The argument, which should be an integer, is taken as the level of indentation:


    my $format = $workbook->add_format();
    $format->set_indent(2);
    $worksheet->write(0, 0, 'This text is indented', $format);


Indentation is a horizontal alignment property. It will override any other horizontal properties but it can be used in conjunction with vertical properties.




=head2 set_shrink()


    Default state:      Text shrinking is off
    Default action:     Turn "shrink to fit" on
    Valid args:         1


This method can be used to shrink text so that it fits in a cell.


    my $format = $workbook->add_format();
    $format->set_shrink();
    $worksheet->write(0, 0, 'Honey, I shrunk the text!', $format);




=head2 set_text_justlast()

    Default state:      Justify last is off
    Default action:     Turn justify last on
    Valid args:         0, 1


Only applies to Far Eastern versions of Excel.




=head2 set_pattern()

    Default state:      Pattern is off
    Default action:     Solid fill is on
    Valid args:         0 .. 18

Set the background pattern of a cell.

Examples of the available patterns are shown in the 'Patterns' worksheet created by formats.pl. However, it is unlikely that you will ever need anything other than Pattern 1 which is a solid fill of the background color.




=head2 set_bg_color()

    Default state:      Color is off
    Default action:     Solid fill.
    Valid args:         See set_color()

The C<set_bg_color()> method can be used to set the background colour of a pattern. Patterns are defined via the C<set_pattern()> method. If a pattern hasn't been defined then a solid fill pattern is used as the default.

Here is an example of how to set up a solid fill in a cell:

    my $format = $workbook->add_format();

    $format->set_pattern(); # This is optional when using a solid fill

    $format->set_bg_color('green');
    $worksheet->write('A1', 'Ray', $format);

For further examples see the 'Patterns' worksheet created by formats.pl.




=head2 set_fg_color()

    Default state:      Color is off
    Default action:     Solid fill.
    Valid args:         See set_color()


The C<set_fg_color()> method can be used to set the foreground colour of a pattern.

For further examples see the 'Patterns' worksheet created by formats.pl.




=head2 set_border()

    Also applies to:    set_bottom()
                        set_top()
                        set_left()
                        set_right()

    Default state:      Border is off
    Default action:     Set border type 1
    Valid args:         0-13, See below.

A cell border is comprised of a border on the bottom, top, left and right. These can be set to the same value using C<set_border()> or individually using the relevant method calls shown above.

The following shows the border styles sorted by Spreadsheet::WriteExcel index number:

    Index   Name            Weight   Style
    =====   =============   ======   ===========
    0       None            0
    1       Continuous      1        -----------
    2       Continuous      2        -----------
    3       Dash            1        - - - - - -
    4       Dot             1        . . . . . .
    5       Continuous      3        -----------
    6       Double          3        ===========
    7       Continuous      0        -----------
    8       Dash            2        - - - - - -
    9       Dash Dot        1        - . - . - .
    10      Dash Dot        2        - . - . - .
    11      Dash Dot Dot    1        - . . - . .
    12      Dash Dot Dot    2        - . . - . .
    13      SlantDash Dot   2        / - . / - .


The following shows the borders sorted by style:

    Name            Weight   Style         Index
    =============   ======   ===========   =====
    Continuous      0        -----------   7
    Continuous      1        -----------   1
    Continuous      2        -----------   2
    Continuous      3        -----------   5
    Dash            1        - - - - - -   3
    Dash            2        - - - - - -   8
    Dash Dot        1        - . - . - .   9
    Dash Dot        2        - . - . - .   10
    Dash Dot Dot    1        - . . - . .   11
    Dash Dot Dot    2        - . . - . .   12
    Dot             1        . . . . . .   4
    Double          3        ===========   6
    None            0                      0
    SlantDash Dot   2        / - . / - .   13


The following shows the borders in the order shown in the Excel Dialog.

    Index   Style             Index   Style
    =====   =====             =====   =====
    0       None              12      - . . - . .
    7       -----------       13      / - . / - .
    4       . . . . . .       10      - . - . - .
    11      - . . - . .       8       - - - - - -
    9       - . - . - .       2       -----------
    3       - - - - - -       5       -----------
    1       -----------       6       ===========


Examples of the available border styles are shown in the 'Borders' worksheet created by formats.pl.




=head2 set_border_color()

    Also applies to:    set_bottom_color()
                        set_top_color()
                        set_left_color()
                        set_right_color()

    Default state:      Color is off
    Default action:     Undefined
    Valid args:         See set_color()


Set the colour of the cell borders. A cell border is comprised of a border on the bottom, top, left and right. These can be set to the same colour using C<set_border_color()> or individually using the relevant method calls shown above. Examples of the border styles and colours are shown in the 'Borders' worksheet created by formats.pl.





=head2 copy($format)


This method is used to copy all of the properties from one Format object to another:

    my $lorry1 = $workbook->add_format();
    $lorry1->set_bold();
    $lorry1->set_italic();
    $lorry1->set_color('red');    # lorry1 is bold, italic and red

    my $lorry2 = $workbook->add_format();
    $lorry2->copy($lorry1);
    $lorry2->set_color('yellow'); # lorry2 is bold, italic and yellow

The C<copy()> method is only useful if you are using the method interface to Format properties. It generally isn't required if you are setting Format properties directly using hashes.


Note: this is not a copy constructor, both objects must exist prior to copying.




=head1 UNICODE IN EXCEL

The following is a brief introduction to handling Unicode in C<Spreadsheet::WriteExcel>.

I<For a more general introduction to Unicode handling in Perl see> L<perlunitut> and L<perluniintro>.

When using Spreadsheet::WriteExcel the best and easiest way to write unicode strings to an Excel file is to use C<UTF-8> encoded strings and perl 5.8 (or later). Spreadsheet::WriteExcel also allows you to write unicode strings using older perls but it generally requires more work, as explained below.

Internally, Excel encodes unicode data as C<UTF-16LE> (where LE means little-endian). If you are using perl 5.8+ then Spreadsheet::WriteExcel will convert C<UTF-8> strings to C<UTF-16LE> when required. No further intervention is required from the programmer, for example:

    # perl 5.8+ example:
    my $smiley = "\x{263A}";

    $worksheet->write('A1', 'Hello world'); # ASCII
    $worksheet->write('A2', $smiley);       # UTF-8

Spreadsheet::WriteExcel also lets you write unicode data as C<UTF-16>. Since the majority of CPAN modules default to C<UTF-16BE> (big-endian) Spreadsheet::WriteExcel also uses C<UTF-16BE> and converts it internally to C<UTF-16LE>:

    # perl 5.005 example:
    my $smiley = pack 'n', 0x263A;

    $worksheet->write               ('A3', 'Hello world'); # ASCII
    $worksheet->write_utf16be_string('A4', $smiley);       # UTF-16

Although the above examples look similar there is an important difference. With C<uft8> and perl 5.8+ Spreadsheet::WriteExcel treats C<UTF-8> strings in exactly the same way as any other string. However, with C<UTF16> data we need to distinguish it from other strings either by calling a separate function or by passing an additional flag to indicate the data type.

If you are dealing with non-ASCII characters that aren't in C<UTF-8> then perl 5.8+ provides useful tools in the guise of the C<Encode> module to help you to convert to the required format. For example:

    use Encode 'decode';

    my $string = 'some string with koi8-r characters';
       $string = decode('koi8-r', $string); # koi8-r to utf8

Alternatively you can read data from an encoded file and convert it to C<UTF-8> as you read it in:


    my $file = 'unicode_koi8r.txt';
    open FH, '<:encoding(koi8-r)', $file  or die "Couldn't open $file: $!\n";

    my $row = 0;
    while (<FH>) {
        # Data read in is now in utf8 format.
        chomp;
        $worksheet->write($row++, 0,  $_);
    }

These methodologies are explained in more detail in L<perlunitut>, L<perluniintro> and L<perlunicode>.

See also the C<unicode_*.pl> programs in the examples directory of the distro.




=head1 COLOURS IN EXCEL

Excel provides a colour palette of 56 colours. In Spreadsheet::WriteExcel these colours are accessed via their palette index in the range 8..63. This index is used to set the colour of fonts, cell patterns and cell borders. For example:

    my $format = $workbook->add_format(
                                        color => 12, # index for blue
                                        font  => 'Arial',
                                        size  => 12,
                                        bold  => 1,
                                     );

The most commonly used colours can also be accessed by name. The name acts as a simple alias for the colour index:

    black     =>    8
    blue      =>   12
    brown     =>   16
    cyan      =>   15
    gray      =>   23
    green     =>   17
    lime      =>   11
    magenta   =>   14
    navy      =>   18
    orange    =>   53
    pink      =>   33
    purple    =>   20
    red       =>   10
    silver    =>   22
    white     =>    9
    yellow    =>   13

For example:

    my $font = $workbook->add_format(color => 'red');

Users of VBA in Excel should note that the equivalent colour indices are in the range 1..56 instead of 8..63.

If the default palette does not provide a required colour you can override one of the built-in values. This is achieved by using the C<set_custom_color()> workbook method to adjust the RGB (red green blue) components of the colour:

    my $ferrari = $workbook->set_custom_color(40, 216, 12, 12);

    my $format  = $workbook->add_format(
                                        bg_color => $ferrari,
                                        pattern  => 1,
                                        border   => 1
                                      );

    $worksheet->write_blank('A1', $format);

The default Excel colour palette is shown in C<palette.html> in the C<docs> directory  of the distro. You can generate an Excel version of the palette using C<colors.pl> in the C<examples> directory.




=head1 DATES AND TIME IN EXCEL

There are two important things to understand about dates and times in Excel:

=over 4

=item 1 A date/time in Excel is a real number plus an Excel number format.

=item 2 Spreadsheet::WriteExcel doesn't automatically convert date/time strings in C<write()> to an Excel date/time.

=back

These two points are explained in more detail below along with some suggestions on how to convert times and dates to the required format.


=head2 An Excel date/time is a number plus a format

If you write a date string with C<write()> then all you will get is a string:

    $worksheet->write('A1', '02/03/04'); # !! Writes a string not a date. !!

Dates and times in Excel are represented by real numbers, for example "Jan 1 2001 12:30 AM" is represented by the number 36892.521.

The integer part of the number stores the number of days since the epoch and the fractional part stores the percentage of the day.

A date or time in Excel is just like any other number. To have the number display as a date you must apply an Excel number format to it. Here are some examples.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new('date_examples.xls');
    my $worksheet = $workbook->add_worksheet();

    $worksheet->set_column('A:A', 30); # For extra visibility.

    my $number    = 39506.5;

    $worksheet->write('A1', $number);            #     39506.5

    my $format2 = $workbook->add_format(num_format => 'dd/mm/yy');
    $worksheet->write('A2', $number , $format2); #     28/02/08

    my $format3 = $workbook->add_format(num_format => 'mm/dd/yy');
    $worksheet->write('A3', $number , $format3); #     02/28/08

    my $format4 = $workbook->add_format(num_format => 'd-m-yyyy');
    $worksheet->write('A4', $number , $format4); #     28-2-2008

    my $format5 = $workbook->add_format(num_format => 'dd/mm/yy hh:mm');
    $worksheet->write('A5', $number , $format5); #     28/02/08 12:00

    my $format6 = $workbook->add_format(num_format => 'd mmm yyyy');
    $worksheet->write('A6', $number , $format6); #     28 Feb 2008

    my $format7 = $workbook->add_format(num_format => 'mmm d yyyy hh:mm AM/PM');
    $worksheet->write('A7', $number , $format7); #     Feb 28 2008 12:00 PM


=head2 Spreadsheet::WriteExcel doesn't automatically convert date/time strings

Spreadsheet::WriteExcel doesn't automatically convert input date strings into Excel's formatted date numbers due to the large number of possible date formats and also due to the possibility of misinterpretation.

For example, does C<02/03/04> mean March 2 2004, February 3 2004 or even March 4 2002.

Therefore, in order to handle dates you will have to convert them to numbers and apply an Excel format. Some methods for converting dates are listed in the next section.

The most direct way is to convert your dates to the ISO8601 C<yyyy-mm-ddThh:mm:ss.sss> date format and use the C<write_date_time()> worksheet method:

    $worksheet->write_date_time('A2', '2001-01-01T12:20', $format);

See the C<write_date_time()> section of the documentation for more details.

A general methodology for handling date strings with C<write_date_time()> is:

    1. Identify incoming date/time strings with a regex.
    2. Extract the component parts of the date/time using the same regex.
    3. Convert the date/time to the ISO8601 format.
    4. Write the date/time using write_date_time() and a number format.

Here is an example:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook    = Spreadsheet::WriteExcel->new('example.xls');
    my $worksheet   = $workbook->add_worksheet();

    # Set the default format for dates.
    my $date_format = $workbook->add_format(num_format => 'mmm d yyyy');

    # Increase column width to improve visibility of data.
    $worksheet->set_column('A:C', 20);

    # Simulate reading from a data source.
    my $row = 0;

    while (<DATA>) {
        chomp;

        my $col  = 0;
        my @data = split ' ';

        for my $item (@data) {

            # Match dates in the following formats: d/m/yy, d/m/yyyy
            if ($item =~ qr[^(\d{1,2})/(\d{1,2})/(\d{4})$]) {

                # Change to the date format required by write_date_time().
                my $date = sprintf "%4d-%02d-%02dT", $3, $2, $1;

                $worksheet->write_date_time($row, $col++, $date, $date_format);
            }
            else {
                # Just plain data
                $worksheet->write($row, $col++, $item);
            }
        }
        $row++;
    }

    __DATA__
    Item    Cost    Date
    Book    10      1/9/2007
    Beer    4       12/9/2007
    Bed     500     5/10/2007

For a slightly more advanced solution you can modify the C<write()> method to handle date formats of your choice via the C<add_write_handler()> method. See the C<add_write_handler()> section of the docs and the write_handler3.pl and write_handler4.pl programs in the examples directory of the distro.


=head2 Converting dates and times to an Excel date or time

The C<write_date_time()> method above is just one way of handling dates and times.

The L<Spreadsheet::WriteExcel::Utility> module which is included in the distro has date/time handling functions:

    use Spreadsheet::WriteExcel::Utility;

    $date           = xl_date_list(2002, 1, 1);         # 37257
    $date           = xl_parse_date("11 July 1997");    # 35622
    $time           = xl_parse_time('3:21:36 PM');      # 0.64
    $date           = xl_decode_date_EU("13 May 2002"); # 37389

Note: some of these functions require additional CPAN modules.

For date conversions using the CPAN C<DateTime> framework see L<DateTime::Format::Excel> L<http://search.cpan.org/search?dist=DateTime-Format-Excel>.




=head1 OUTLINES AND GROUPING IN EXCEL


Excel allows you to group rows or columns so that they can be hidden or displayed with a single mouse click. This feature is referred to as outlines.

Outlines can reduce complex data down to a few salient sub-totals or summaries.

This feature is best viewed in Excel but the following is an ASCII representation of what a worksheet with three outlines might look like. Rows 3-4 and rows 7-8 are grouped at level 2. Rows 2-9 are grouped at level 1. The lines at the left hand side are called outline level bars.


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


Clicking the minus sign on each of the level 2 outlines will collapse and hide the data as shown in the next figure. The minus sign changes to a plus sign to indicate that the data in the outline is hidden.

            ------------------------------------------
     1 2 3 |   |   A   |   B   |   C   |   D   |  ...
            ------------------------------------------
      _    | 1 |   A   |       |       |       |  ...
     |     | 2 |   B   |       |       |       |  ...
     | +   | 5 |   E   |       |       |       |  ...
     |     | 6 |   F   |       |       |       |  ...
     | +   | 9 |   I   |       |       |       |  ...
     -     | . |  ...  |  ...  |  ...  |  ...  |  ...


Clicking on the minus sign on the level 1 outline will collapse the remaining rows as follows:

            ------------------------------------------
     1 2 3 |   |   A   |   B   |   C   |   D   |  ...
            ------------------------------------------
           | 1 |   A   |       |       |       |  ...
     +     | . |  ...  |  ...  |  ...  |  ...  |  ...


Grouping in C<Spreadsheet::WriteExcel> is achieved by setting the outline level via the C<set_row()> and C<set_column()> worksheet methods:

    set_row($row, $height, $format, $hidden, $level, $collapsed)
    set_column($first_col, $last_col, $width, $format, $hidden, $level, $collapsed)

The following example sets an outline level of 1 for rows 1 and 2 (zero-indexed) and columns B to G. The parameters C<$height> and C<$XF> are assigned default values since they are undefined:

    $worksheet->set_row(1, undef, undef, 0, 1);
    $worksheet->set_row(2, undef, undef, 0, 1);
    $worksheet->set_column('B:G', undef, undef, 0, 1);

Excel allows up to 7 outline levels. Therefore the C<$level> parameter should be in the range C<0 E<lt>= $level E<lt>= 7>.

Rows and columns can be collapsed by setting the C<$hidden> flag for the hidden rows/columns and setting the C<$collapsed> flag for the row/column that has the collapsed C<+> symbol:

    $worksheet->set_row(1, undef, undef, 1, 1);
    $worksheet->set_row(2, undef, undef, 1, 1);
    $worksheet->set_row(3, undef, undef, 0, 0, 1);        # Collapsed flag.

    $worksheet->set_column('B:G', undef, undef, 1, 1);
    $worksheet->set_column('H:H', undef, undef, 0, 0, 1); # Collapsed flag.

Note: Setting the C<$collapsed> flag is particularly important for compatibility with OpenOffice.org and Gnumeric.

For a more complete example see the C<outline.pl> and C<outline_collapsed.pl> programs in the examples directory of the distro.

Some additional outline properties can be set via the C<outline_settings()> worksheet method, see above.




=head1 DATA VALIDATION IN EXCEL

Data validation is a feature of Excel which allows you to restrict the data that a users enters in a cell and to display help and warning messages. It also allows you to restrict input to values in a drop down list.

A typical use case might be to restrict data in a cell to integer values in a certain range, to provide a help message to indicate the required value and to issue a warning if the input data doesn't meet the stated criteria. In Spreadsheet::WriteExcel we could do that as follows:

    $worksheet->data_validation('B3',
        {
            validate        => 'integer',
            criteria        => 'between',
            minimum         => 1,
            maximum         => 100,
            input_title     => 'Input an integer:',
            input_message   => 'Between 1 and 100',
            error_message   => 'Sorry, try again.',
        });

The above example would look like this in Excel: L<http://homepage.eircom.net/~jmcnamara/perl/data_validation.jpg>.

=begin html

<center>
<img src="http://homepage.eircom.net/~jmcnamara/perl/data_validation.jpg" alt="The output from the above example"/>
</center>

=end html

For more information on data validation see the following Microsoft support article "Description and examples of data validation in Excel": L<http://support.microsoft.com/kb/211485>.

The following sections describe how to use the C<data_validation()> method and its various options.


=head2 data_validation($row, $col, { parameter => 'value', ... })

The C<data_validation()> method is used to construct an Excel data validation.

It can be applied to a single cell or a range of cells. You can pass 3 parameters such as  C<($row, $col, {...})> or 5 parameters such as C<($first_row, $first_col, $last_row, $last_col, {...})>. You can also use C<A1> style notation. For example:

    $worksheet->data_validation(0, 0,       {...});
    $worksheet->data_validation(0, 0, 4, 1, {...});

    # Which are the same as:

    $worksheet->data_validation('A1',       {...});
    $worksheet->data_validation('A1:B5',    {...});

See also the note about L</Cell notation> for more information.


The last parameter in C<data_validation()> must be a hash ref containing the parameters that describe the type and style of the data validation. The allowable parameters are:

    validate
    criteria
    value | minimum | source
    maximum
    ignore_blank
    dropdown

    input_title
    input_message
    show_input

    error_title
    error_message
    error_type
    show_error

These parameters are explained in the following sections. Most of the parameters are optional, however, you will generally require the three main options C<validate>, C<criteria> and C<value>.

    $worksheet->data_validation('B3',
        {
            validate => 'integer',
            criteria => '>',
            value    => 100,
        });

The C<data_validation> method returns:

     0 for success.
    -1 for insufficient number of arguments.
    -2 for row or column out of bounds.
    -3 for incorrect parameter or value.


=head2 validate

This parameter is passed in a hash ref to C<data_validation()>.

The C<validate> parameter is used to set the type of data that you wish to validate. It is always required and it has no default value. Allowable values are:

    any
    integer
    decimal
    list
    date
    time
    length
    custom

=over

=item * B<any> is used to specify that the type of data is unrestricted. This is the same as not applying a data validation. It is only provided for completeness and isn't used very often in the context of Spreadsheet::WriteExcel.

=item * B<integer> restricts the cell to integer values. Excel refers to this as 'whole number'.

    validate => 'integer',
    criteria => '>',
    value    => 100,

=item * B<decimal> restricts the cell to decimal values.

    validate => 'decimal',
    criteria => '>',
    value    => 38.6,

=item * B<list> restricts the cell to a set of user specified values. These can be passed in an array ref or as a cell range (named ranges aren't currently supported):

    validate => 'list',
    value    => ['open', 'high', 'close'],
    # Or like this:
    value    => 'B1:B3',

Excel requires that range references are only to cells on the same worksheet.

=item * B<date> restricts the cell to date values. Dates in Excel are expressed as integer values but you can also pass an ISO860 style string as used in C<write_date_time()>. See also L</DATES AND TIME IN EXCEL> for more information about working with Excel's dates.

    validate => 'date',
    criteria => '>',
    value    => 39653, # 24 July 2008
    # Or like this:
    value    => '2008-07-24T',

=item * B<time> restricts the cell to time values. Times in Excel are expressed as decimal values but you can also pass an ISO860 style string as used in C<write_date_time()>. See also L</DATES AND TIME IN EXCEL> for more information about working with Excel's times.

    validate => 'time',
    criteria => '>',
    value    => 0.5, # Noon
    # Or like this:
    value    => 'T12:00:00',

=item * B<length> restricts the cell data based on an integer string length. Excel refers to this as 'Text length'.

    validate => 'length',
    criteria => '>',
    value    => 10,

=item * B<custom> restricts the cell based on an external Excel formula that returns a C<TRUE/FALSE> value.

    validate => 'custom',
    value    => '=IF(A10>B10,TRUE,FALSE)',

=back


=head2 criteria

This parameter is passed in a hash ref to C<data_validation()>.

The C<criteria> parameter is used to set the criteria by which the data in the cell is validated. It is almost always required except for the C<list> and C<custom> validate options. It has no default value. Allowable values are:

    'between'
    'not between'
    'equal to'                  |  '=='  |  '='
    'not equal to'              |  '!='  |  '<>'
    'greater than'              |  '>'
    'less than'                 |  '<'
    'greater than or equal to'  |  '>='
    'less than or equal to'     |  '<='

You can either use Excel's textual description strings, in the first column above, or the more common operator alternatives. The following are equivalent:

    validate => 'integer',
    criteria => 'greater than',
    value    => 100,

    validate => 'integer',
    criteria => '>',
    value    => 100,

The C<list> and C<custom> validate options don't require a C<criteria>. If you specify one it will be ignored.

    validate => 'list',
    value    => ['open', 'high', 'close'],

    validate => 'custom',
    value    => '=IF(A10>B10,TRUE,FALSE)',


=head2 value | minimum | source

This parameter is passed in a hash ref to C<data_validation()>.

The C<value> parameter is used to set the limiting value to which the C<criteria> is applied. It is always required and it has no default value. You can also use the synonyms C<minimum> or C<source> to make the validation a little clearer and closer to Excel's description of the parameter:

    # Use 'value'
    validate => 'integer',
    criteria => '>',
    value    => 100,

    # Use 'minimum'
    validate => 'integer',
    criteria => 'between',
    minimum  => 1,
    maximum  => 100,

    # Use 'source'
    validate => 'list',
    source   => '$B$1:$B$3',


=head2 maximum

This parameter is passed in a hash ref to C<data_validation()>.

The C<maximum> parameter is used to set the upper limiting value when the C<criteria> is either C<'between'> or C<'not between'>:

    validate => 'integer',
    criteria => 'between',
    minimum  => 1,
    maximum  => 100,


=head2 ignore_blank

This parameter is passed in a hash ref to C<data_validation()>.

The C<ignore_blank> parameter is used to toggle on and off the 'Ignore blank' option in the Excel data validation dialog. When the option is on the data validation is not applied to blank data in the cell. It is on by default.

    ignore_blank => 0,  # Turn the option off


=head2 dropdown

This parameter is passed in a hash ref to C<data_validation()>.

The C<dropdown> parameter is used to toggle on and off the 'In-cell dropdown' option in the Excel data validation dialog. When the option is on a dropdown list will be shown for C<list> validations. It is on by default.

    dropdown => 0,      # Turn the option off


=head2 input_title

This parameter is passed in a hash ref to C<data_validation()>.

The C<input_title> parameter is used to set the title of the input message that is displayed when a cell is entered. It has no default value and is only displayed if the input message is displayed. See the C<input_message> parameter below.

    input_title   => 'This is the input title',

The maximum title length is 32 characters. UTF8 strings are handled automatically in perl 5.8 and later.


=head2 input_message

This parameter is passed in a hash ref to C<data_validation()>.

The C<input_message> parameter is used to set the input message that is displayed when a cell is entered. It has no default value.

    validate      => 'integer',
    criteria      => 'between',
    minimum       => 1,
    maximum       => 100,
    input_title   => 'Enter the applied discount:',
    input_message => 'between 1 and 100',

The message can be split over several lines using newlines, C<"\n"> in double quoted strings.

    input_message => "This is\na test.",

The maximum message length is 255 characters. UTF8 strings are handled automatically in perl 5.8 and later.


=head2 show_input

This parameter is passed in a hash ref to C<data_validation()>.

The C<show_input> parameter is used to toggle on and off the 'Show input message when cell is selected' option in the Excel data validation dialog. When the option is off an input message is not displayed even if it has been set using C<input_message>. It is on by default.

    show_input => 0,      # Turn the option off


=head2 error_title

This parameter is passed in a hash ref to C<data_validation()>.

The C<error_title> parameter is used to set the title of the error message that is displayed when the data validation criteria is not met. The default error title is 'Microsoft Excel'.

    error_title   => 'Input value is not valid',

The maximum title length is 32 characters. UTF8 strings are handled automatically in perl 5.8 and later.


=head2 error_message

This parameter is passed in a hash ref to C<data_validation()>.

The C<error_message> parameter is used to set the error message that is displayed when a cell is entered. The default error message is "The value you entered is not valid.\nA user has restricted values that can be entered into the cell.".

    validate      => 'integer',
    criteria      => 'between',
    minimum       => 1,
    maximum       => 100,
    error_title   => 'Input value is not valid',
    error_message => 'It should be an integer between 1 and 100',

The message can be split over several lines using newlines, C<"\n"> in double quoted strings.

    input_message => "This is\na test.",

The maximum message length is 255 characters. UTF8 strings are handled automatically in perl 5.8 and later.


=head2 error_type

This parameter is passed in a hash ref to C<data_validation()>.

The C<error_type> parameter is used to specify the type of error dialog that is displayed. There are 3 options:

    'stop'
    'warning'
    'information'

The default is C<'stop'>.


=head2 show_error

This parameter is passed in a hash ref to C<data_validation()>.

The C<show_error> parameter is used to toggle on and off the 'Show error alert after invalid data is entered' option in the Excel data validation dialog. When the option is off an error message is not displayed even if it has been set using C<error_message>. It is on by default.

    show_error => 0,      # Turn the option off

=head2 Data Validation Examples

Example 1. Limiting input to an integer greater than a fixed value.

    $worksheet->data_validation('A1',
        {
            validate        => 'integer',
            criteria        => '>',
            value           => 0,
        });

Example 2. Limiting input to an integer greater than a fixed value where the value is referenced from a cell.

    $worksheet->data_validation('A2',
        {
            validate        => 'integer',
            criteria        => '>',
            value           => '=E3',
        });

Example 3. Limiting input to a decimal in a fixed range.

    $worksheet->data_validation('A3',
        {
            validate        => 'decimal',
            criteria        => 'between',
            minimum         => 0.1,
            maximum         => 0.5,
        });

Example 4. Limiting input to a value in a dropdown list.

    $worksheet->data_validation('A4',
        {
            validate        => 'list',
            source          => ['open', 'high', 'close'],
        });

Example 5. Limiting input to a value in a dropdown list where the list is specified as a cell range.

    $worksheet->data_validation('A5',
        {
            validate        => 'list',
            source          => '=E4:G4',
        });

Example 6. Limiting input to a date in a fixed range.

    $worksheet->data_validation('A6',
        {
            validate        => 'date',
            criteria        => 'between',
            minimum         => '2008-01-01T',
            maximum         => '2008-12-12T',
        });

Example 7. Displaying a message when the cell is selected.

    $worksheet->data_validation('A7',
        {
            validate      => 'integer',
            criteria      => 'between',
            minimum       => 1,
            maximum       => 100,
            input_title   => 'Enter an integer:',
            input_message => 'between 1 and 100',
        });

See also the C<data_validate.pl> program in the examples directory of the distro.



=head1 ROW HEIGHTS AND WORKSHEET OBJECTS

The following relates to worksheet objects such as images, comments and charts.

If you specify the height of a row that contains a worksheet object then Spreadsheet::WriteExcel will adjust the height of the object to maintain its default or user specified dimensions. In this way the object won't appear stretched or compressed in Excel.

However, Excel can also adjust the height of a row automatically if it contains cells that have the text wrap property set or contain large fonts. In these cases the height of the row is unknown to Spreadsheet::WriteExcel at execution time and the scaling calculations it performs are incorrect. The effect of this is that the  object is stretched with the row when it is displayed in Excel.

In order to avoid this issue you should use the C<set_row()> method to explicitly specify the height of any row that may otherwise be changed automatically by Excel.




=head1 FORMULAS AND FUNCTIONS IN EXCEL


=head2 Caveats

The first thing to note is that there are still some outstanding issues with the implementation of formulas and functions:

    1. Writing a formula is much slower than writing the equivalent string.
    2. You cannot use array constants, i.e. {1;2;3}, in functions.
    3. Unary minus isn't supported.
    4. Whitespace is not preserved around operators.
    5. Named ranges are not supported.
    6. Array formulas are not supported.

However, these constraints will be removed in future versions. They are here because of a trade-off between features and time. Also, it is possible to work around issue 1 using the C<store_formula()> and C<repeat_formula()> methods as described later in this section.



=head2 Introduction

The following is a brief introduction to formulas and functions in Excel and Spreadsheet::WriteExcel.

A formula is a string that begins with an equals sign:

    '=A1+B1'
    '=AVERAGE(1, 2, 3)'

The formula can contain numbers, strings, boolean values, cell references, cell ranges and functions. Named ranges are not supported. Formulas should be written as they appear in Excel, that is cells and functions must be in uppercase.

Cells in Excel are referenced using the A1 notation system where the column is designated by a letter and the row by a number. Columns range from A to IV i.e. 0 to 255, rows range from 1 to 65536. The C<Spreadsheet::WriteExcel::Utility> module that is included in the distro contains helper functions for dealing with A1 notation, for example:

    use Spreadsheet::WriteExcel::Utility;

    ($row, $col) = xl_cell_to_rowcol('C2');  # (1, 2)
    $str         = xl_rowcol_to_cell(1, 2);  # C2

The Excel C<$> notation in cell references is also supported. This allows you to specify whether a row or column is relative or absolute. This only has an effect if the cell is copied. The following examples show relative and absolute values.

    '=A1'   # Column and row are relative
    '=$A1'  # Column is absolute and row is relative
    '=A$1'  # Column is relative and row is absolute
    '=$A$1' # Column and row are absolute

Formulas can also refer to cells in other worksheets of the current workbook. For example:

    '=Sheet2!A1'
    '=Sheet2!A1:A5'
    '=Sheet2:Sheet3!A1'
    '=Sheet2:Sheet3!A1:A5'
    q{='Test Data'!A1}
    q{='Test Data1:Test Data2'!A1}

The sheet reference and the cell reference are separated by  C<!> the exclamation mark symbol. If worksheet names contain spaces, commas or parentheses then Excel requires that the name is enclosed in single quotes as shown in the last two examples above. In order to avoid using a lot of escape characters you can use the quote operator C<q{}> to protect the quotes. See C<perlop> in the main Perl documentation. Only valid sheet names that have been added using the C<add_worksheet()> method can be used in formulas. You cannot reference external workbooks.


The following table lists the operators that are available in Excel's formulas. The majority of the operators are the same as Perl's, differences are indicated:

    Arithmetic operators:
    =====================
    Operator  Meaning                   Example
       +      Addition                  1+2
       -      Subtraction               2-1
       *      Multiplication            2*3
       /      Division                  1/4
       ^      Exponentiation            2^3      # Equivalent to **
       -      Unary minus               -(1+2)   # Not yet supported
       %      Percent (Not modulus)     13%      # Not supported, [1]


    Comparison operators:
    =====================
    Operator  Meaning                   Example
        =     Equal to                  A1 =  B1 # Equivalent to ==
        <>    Not equal to              A1 <> B1 # Equivalent to !=
        >     Greater than              A1 >  B1
        <     Less than                 A1 <  B1
        >=    Greater than or equal to  A1 >= B1
        <=    Less than or equal to     A1 <= B1


    String operator:
    ================
    Operator  Meaning                   Example
        &     Concatenation             "Hello " & "World!" # [2]


    Reference operators:
    ====================
    Operator  Meaning                   Example
        :     Range operator            A1:A4               # [3]
        ,     Union operator            SUM(1, 2+2, B3)     # [4]


    Notes:
    [1]: You can get a percentage with formatting and modulus with MOD().
    [2]: Equivalent to ("Hello " . "World!") in Perl.
    [3]: This range is equivalent to cells A1, A2, A3 and A4.
    [4]: The comma behaves like the list separator in Perl.

The range and comma operators can have different symbols in non-English versions of Excel. These will be supported in a later version of Spreadsheet::WriteExcel. European users of Excel take note:

    $worksheet->write('A1', '=SUM(1; 2; 3)'); # Wrong!!
    $worksheet->write('A1', '=SUM(1, 2, 3)'); # Okay

The following table lists all of the core functions supported by Excel 5 and Spreadsheet::WriteExcel. Any additional functions that are available through the "Analysis ToolPak" or other add-ins are not supported. These functions have all been tested to verify that they work.

    ABS           DB            INDIRECT      NORMINV       SLN
    ACOS          DCOUNT        INFO          NORMSDIST     SLOPE
    ACOSH         DCOUNTA       INT           NORMSINV      SMALL
    ADDRESS       DDB           INTERCEPT     NOT           SQRT
    AND           DEGREES       IPMT          NOW           STANDARDIZE
    AREAS         DEVSQ         IRR           NPER          STDEV
    ASIN          DGET          ISBLANK       NPV           STDEVP
    ASINH         DMAX          ISERR         ODD           STEYX
    ATAN          DMIN          ISERROR       OFFSET        SUBSTITUTE
    ATAN2         DOLLAR        ISLOGICAL     OR            SUBTOTAL
    ATANH         DPRODUCT      ISNA          PEARSON       SUM
    AVEDEV        DSTDEV        ISNONTEXT     PERCENTILE    SUMIF
    AVERAGE       DSTDEVP       ISNUMBER      PERCENTRANK   SUMPRODUCT
    BETADIST      DSUM          ISREF         PERMUT        SUMSQ
    BETAINV       DVAR          ISTEXT        PI            SUMX2MY2
    BINOMDIST     DVARP         KURT          PMT           SUMX2PY2
    CALL          ERROR.TYPE    LARGE         POISSON       SUMXMY2
    CEILING       EVEN          LEFT          POWER         SYD
    CELL          EXACT         LEN           PPMT          T
    CHAR          EXP           LINEST        PROB          TAN
    CHIDIST       EXPONDIST     LN            PRODUCT       TANH
    CHIINV        FACT          LOG           PROPER        TDIST
    CHITEST       FALSE         LOG10         PV            TEXT
    CHOOSE        FDIST         LOGEST        QUARTILE      TIME
    CLEAN         FIND          LOGINV        RADIANS       TIMEVALUE
    CODE          FINV          LOGNORMDIST   RAND          TINV
    COLUMN        FISHER        LOOKUP        RANK          TODAY
    COLUMNS       FISHERINV     LOWER         RATE          TRANSPOSE
    COMBIN        FIXED         MATCH         REGISTER.ID   TREND
    CONCATENATE   FLOOR         MAX           REPLACE       TRIM
    CONFIDENCE    FORECAST      MDETERM       REPT          TRIMMEAN
    CORREL        FREQUENCY     MEDIAN        RIGHT         TRUE
    COS           FTEST         MID           ROMAN         TRUNC
    COSH          FV            MIN           ROUND         TTEST
    COUNT         GAMMADIST     MINUTE        ROUNDDOWN     TYPE
    COUNTA        GAMMAINV      MINVERSE      ROUNDUP       UPPER
    COUNTBLANK    GAMMALN       MIRR          ROW           VALUE
    COUNTIF       GEOMEAN       MMULT         ROWS          VAR
    COVAR         GROWTH        MOD           RSQ           VARP
    CRITBINOM     HARMEAN       MODE          SEARCH        VDB
    DATE          HLOOKUP       MONTH         SECOND        VLOOKUP
    DATEVALUE     HOUR          N             SIGN          WEEKDAY
    DAVERAGE      HYPGEOMDIST   NA            SIN           WEIBULL
    DAY           IF            NEGBINOMDIST  SINH          YEAR
    DAYS360       INDEX         NORMDIST      SKEW          ZTEST

You can also modify the module to support function names in the following languages: German, French, Spanish, Portuguese, Dutch, Finnish, Italian and Swedish. See the C<function_locale.pl> program in the C<examples> directory of the distro.

For a general introduction to Excel's formulas and an explanation of the syntax of the function refer to the Excel help files or the following: L<http://office.microsoft.com/en-us/assistance/CH062528031033.aspx>.

If your formula doesn't work in Spreadsheet::WriteExcel try the following:

    1. Verify that the formula works in Excel (or Gnumeric or OpenOffice.org).
    2. Ensure that it isn't on the Caveats list shown above.
    3. Ensure that cell references and formula names are in uppercase.
    4. Ensure that you are using ':' as the range operator, A1:A4.
    5. Ensure that you are using ',' as the union operator, SUM(1,2,3).
    6. Ensure that the function is in the above table.

If you go through steps 1-6 and you still have a problem, mail me.




=head2 Improving performance when working with formulas

Writing a large number of formulas with Spreadsheet::WriteExcel can be slow. This is due to the fact that each formula has to be parsed and with the current implementation this is computationally expensive.

However, in a lot of cases the formulas that you write will be quite similar, for example:

    $worksheet->write_formula('B1',    '=A1 * 3 + 50',    $format);
    $worksheet->write_formula('B2',    '=A2 * 3 + 50',    $format);
    ...
    ...
    $worksheet->write_formula('B99',   '=A999 * 3 + 50',  $format);
    $worksheet->write_formula('B1000', '=A1000 * 3 + 50', $format);

In this example the cell reference changes in iterations from C<A1> to C<A1000>. The parser treats this variable as a I<token> and arranges it according to predefined rules. However, since the parser is oblivious to the value of the token, it is essentially performing the same calculation 1000 times. This is inefficient.

The way to avoid this inefficiency and thereby speed up the writing of formulas is to parse the formula once and then repeatedly substitute similar tokens.

A formula can be parsed and stored via the C<store_formula()> worksheet method. You can then use the C<repeat_formula()> method to substitute C<$pattern>, C<$replace> pairs in the stored formula:

    my $formula = $worksheet->store_formula('=A1 * 3 + 50');

    for my $row (0..999) {
        $worksheet->repeat_formula($row, 1, $formula, $format, 'A1', 'A'.($row +1));
    }

On an arbitrary test machine this method was 10 times faster than the brute force method shown above.

For more information about how Spreadsheet::WriteExcel parses and stores formulas see the C<Spreadsheet::WriteExcel::Formula> man page.

It should be noted however that the overall speed of direct formula parsing will be improved in a future version.




=head1 EXAMPLES

See L<Spreadsheet::WriteExcel::Examples> for a full list of examples.


=head2 Example 1

The following example shows some of the basic features of Spreadsheet::WriteExcel.


    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    # Create a new workbook called simple.xls and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('simple.xls');
    my $worksheet = $workbook->add_worksheet();

    # The general syntax is write($row, $column, $token). Note that row and
    # column are zero indexed

    # Write some text
    $worksheet->write(0, 0,  'Hi Excel!');


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


=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/a_simple.jpg" width="640" height="420" alt="Output from a_simple.pl" /></center></p>

=end html




=head2 Example 2

The following is a general example which demonstrates some features of working with multiple worksheets.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    # Create a new Excel workbook
    my $workbook = Spreadsheet::WriteExcel->new('regions.xls');

    # Add some worksheets
    my $north = $workbook->add_worksheet('North');
    my $south = $workbook->add_worksheet('South');
    my $east  = $workbook->add_worksheet('East');
    my $west  = $workbook->add_worksheet('West');

    # Add a Format
    my $format = $workbook->add_format();
    $format->set_bold();
    $format->set_color('blue');

    # Add a caption to each worksheet
    foreach my $worksheet ($workbook->sheets()) {
        $worksheet->write(0, 0, 'Sales', $format);
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


=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/regions.jpg" width="640" height="420" alt="Output from regions.pl" /></center></p>

=end html




=head2 Example 3

This example shows how to use a conditional numerical format with colours to indicate if a share price has gone up or down.

    use strict;
    use Spreadsheet::WriteExcel;

    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('stocks.xls');
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


    # Create a format for the price change. This is an example of a
    # conditional format. The number is formatted as a percentage. If it is
    # positive it is formatted in green, if it is negative it is formatted
    # in red and if it is zero it is formatted as the default font colour
    # (in this case black). Note: the [Green] format produces an unappealing
    # lime green. Try [Color 10] instead for a dark green.
    #
    my $f_change = $workbook->add_format();
    $f_change->set_align('left');
    $f_change->set_num_format('[Green]0.0%;[Red]-0.0%;0.0%');


    # Write out the data
    $worksheet->write(0, 0, 'Company',$header);
    $worksheet->write(0, 1, 'Price',  $header);
    $worksheet->write(0, 2, 'Volume', $header);
    $worksheet->write(0, 3, 'Change', $header);

    $worksheet->write(1, 0, 'Damage Inc.'       );
    $worksheet->write(1, 1, 30.25,    $f_price ); # $30.25
    $worksheet->write(1, 2, 1234567,  $f_volume); # 1,234,567
    $worksheet->write(1, 3, 0.085,    $f_change); # 8.5% in green

    $worksheet->write(2, 0, 'Dump Corp.'        );
    $worksheet->write(2, 1, 1.56,     $f_price ); # $1.56
    $worksheet->write(2, 2, 7564,     $f_volume); # 7,564
    $worksheet->write(2, 3, -0.015,   $f_change); # -1.5% in red

    $worksheet->write(3, 0, 'Rev Ltd.'          );
    $worksheet->write(3, 1, 0.13,     $f_price ); # $0.13
    $worksheet->write(3, 2, 321,      $f_volume); # 321
    $worksheet->write(3, 3, 0,        $f_change); # 0 in the font color (black)


=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stocks.jpg" width="640" height="420" alt="Output from stocks.pl" /></center></p>

=end html




=head2 Example 4

The following is a simple example of using functions.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    # Create a new workbook and add a worksheet
    my $workbook  = Spreadsheet::WriteExcel->new('stats.xls');
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


=begin html

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/stats.jpg" width="640" height="420" alt="Output from stats.pl" /></center></p>

=end html




=head2 Example 5

The following example converts a tab separated file called C<tab.txt> into an Excel file called C<tab.xls>.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    open (TABFILE, 'tab.txt') or die "tab.txt: $!";

    my $workbook  = Spreadsheet::WriteExcel->new('tab.xls');
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


NOTE: This is a simple conversion program for illustrative purposes only. For converting a CSV or Tab separated or any other type of delimited text file to Excel I recommend the more rigorous csv2xls program that is part of H.Merijn Brand's Text::CSV_XS module distro.

See the examples/csv2xls link here: L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>.




=head2 Additional Examples

The following is a description of the example files that are provided
in the standard Spreadsheet::WriteExcel distribution. They demonstrate the
different features and options of the module.
See L<Spreadsheet::WriteExcel::Examples> for more details.

    Getting started
    ===============
    a_simple.pl             A get started example with some basic features.
    demo.pl                 A demo of some of the available features.
    regions.pl              A simple example of multiple worksheets.
    stats.pl                Basic formulas and functions.
    formats.pl              All the available formatting on several worksheets.
    bug_report.pl           A template for submitting bug reports.


    Advanced
    ========
    autofilter.pl           Examples of worksheet autofilters.
    autofit.pl              Simulate Excel's autofit for column widths.
    bigfile.pl              Write past the 7MB limit with OLE::Storage_Lite.
    cgi.pl                  A simple CGI program.
    chart_area.pl           A demo of area style charts.
    chart_bar.pl            A demo of bar (vertical histogram) style charts.
    chart_column.pl         A demo of column (histogram) style charts.
    chart_line.pl           A demo of line style charts.
    chart_pie.pl            A demo of pie style charts.
    chart_scatter.pl        A demo of scatter style charts.
    chart_stock.pl          A demo of stock style charts.
    chess.pl                An example of reusing formatting via properties.
    colors.pl               A demo of the colour palette and named colours.
    comments1.pl            Add comments to worksheet cells.
    comments2.pl            Add comments with advanced options.
    copyformat.pl           Example of copying a cell format.
    data_validate.pl        An example of data validation and dropdown lists.
    date_time.pl            Write dates and times with write_date_time().
    defined_name.pl         Example of how to create defined names.
    diag_border.pl          A simple example of diagonal cell borders.
    easter_egg.pl           Expose the Excel97 flight simulator.
    filehandle.pl           Examples of working with filehandles.
    formula_result.pl       Formulas with user specified results.
    headers.pl              Examples of worksheet headers and footers.
    hide_sheet.pl           Simple example of hiding a worksheet.
    hyperlink1.pl           Shows how to create web hyperlinks.
    hyperlink2.pl           Examples of internal and external hyperlinks.
    images.pl               Adding images to worksheets.
    indent.pl               An example of cell indentation.
    merge1.pl               A simple example of cell merging.
    merge2.pl               A simple example of cell merging with formatting.
    merge3.pl               Add hyperlinks to merged cells.
    merge4.pl               An advanced example of merging with formatting.
    merge5.pl               An advanced example of merging with formatting.
    merge6.pl               An example of merging with Unicode strings.
    mod_perl1.pl            A simple mod_perl 1 program.
    mod_perl2.pl            A simple mod_perl 2 program.
    outline.pl              An example of outlines and grouping.
    outline_collapsed.pl    An example of collapsed outlines.
    panes.pl                An examples of how to create panes.
    properties.pl           Add document properties to a workbook.
    protection.pl           Example of cell locking and formula hiding.
    repeat.pl               Example of writing repeated formulas.
    right_to_left.pl        Change default sheet direction to right to left.
    row_wrap.pl             How to wrap data from one worksheet onto another.
    sales.pl                An example of a simple sales spreadsheet.
    sendmail.pl             Send an Excel email attachment using Mail::Sender.
    stats_ext.pl            Same as stats.pl with external references.
    stocks.pl               Demonstrates conditional formatting.
    tab_colors.pl           Example of how to set worksheet tab colours.
    textwrap.pl             Demonstrates text wrapping options.
    win32ole.pl             A sample Win32::OLE example for comparison.
    write_arrays.pl         Example of writing 1D or 2D arrays of data.
    write_handler1.pl       Example of extending the write() method. Step 1.
    write_handler2.pl       Example of extending the write() method. Step 2.
    write_handler3.pl       Example of extending the write() method. Step 3.
    write_handler4.pl       Example of extending the write() method. Step 4.
    write_to_scalar.pl      Example of writing an Excel file to a Perl scalar.


    Unicode
    =======
    unicode_utf16.pl        Simple example of using Unicode UTF16 strings.
    unicode_utf16_japan.pl  Write Japanese Unicode strings using UTF-16.
    unicode_cyrillic.pl     Write Russian Cyrillic strings using UTF-8.
    unicode_list.pl         List the chars in a Unicode font.
    unicode_2022_jp.pl      Japanese: ISO-2022-JP to utf8 in perl 5.8.
    unicode_8859_11.pl      Thai:     ISO-8859_11 to utf8 in perl 5.8.
    unicode_8859_7.pl       Greek:    ISO-8859_7  to utf8 in perl 5.8.
    unicode_big5.pl         Chinese:  BIG5        to utf8 in perl 5.8.
    unicode_cp1251.pl       Russian:  CP1251      to utf8 in perl 5.8.
    unicode_cp1256.pl       Arabic:   CP1256      to utf8 in perl 5.8.
    unicode_koi8r.pl        Russian:  KOI8-R      to utf8 in perl 5.8.
    unicode_polish_utf8.pl  Polish :  UTF8        to utf8 in perl 5.8.
    unicode_shift_jis.pl    Japanese: Shift JIS   to utf8 in perl 5.8.


    Utility
    =======
    csv2xls.pl              Program to convert a CSV file to an Excel file.
    tab2xls.pl              Program to convert a tab separated file to xls.
    datecalc1.pl            Convert Unix/Perl time to Excel time.
    datecalc2.pl            Calculate an Excel date using Date::Calc.
    lecxe.pl                Convert Excel to WriteExcel using Win32::OLE.


    Developer
    =========
    convertA1.pl            Helper functions for dealing with A1 notation.
    function_locale.pl      Add non-English function names to Formula.pm.
    writeA1.pl              Example of how to extend the module.



=head1 LIMITATIONS

The following limits are imposed by Excel:

    Description                          Limit
    -----------------------------------  ------
    Maximum number of chars in a string  32767
    Maximum number of columns            256
    Maximum number of rows               65536
    Maximum chars in a sheet name        31
    Maximum chars in a header/footer     254

For Excel 2007+ file limits see the L<Excel::Writer::XLSX> module.

The minimum file size is 6K due to the OLE overhead. The maximum file size is approximately 7MB (7087104 bytes) of BIFF data. This can be extended by installing Takanori Kawai's OLE::Storage_Lite module L<http://search.cpan.org/search?dist=OLE-Storage_Lite> see the C<bigfile.pl> example in the C<examples> directory of the distro.




=head1 DOWNLOADING

The latest version of this module is always available at: L<http://search.cpan.org/search?dist=Spreadsheet-WriteExcel/>.




=head1 REQUIREMENTS

This module requires Perl >= 5.005, Parse::RecDescent, File::Temp and OLE::Storage_Lite:

    http://search.cpan.org/search?dist=Parse-RecDescent/ # For formulas.
    http://search.cpan.org/search?dist=File-Temp/        # For set_tempdir().
    http://search.cpan.org/search?dist=OLE-Storage_Lite/ # For files > 7MB.

Note, these aren't strict requirements. Spreadsheet::WriteExcel will work without these modules if you don't use write_formula(), set_tempdir() or create files greater than 7MB. However, it is best to install them if possible and they will be installed automatically if you use a tool such as CPAN.pm or ppm.


=head1 INSTALLATION

See the INSTALL or install.html docs that come with the distribution or: L<http://search.cpan.org/src/JMCNAMARA/Spreadsheet-WriteExcel-2.31/INSTALL>.




=head1 PORTABILITY

Spreadsheet::WriteExcel will work on the majority of Windows, UNIX and Macintosh platforms. Specifically, the module will work on any system where perl packs floats in the 64 bit IEEE format. The float must also be in little-endian format but it will be reversed if necessary. Thus:

    print join(' ', map { sprintf '%#02x', $_ } unpack('C*', pack 'd', 1.2345)), "\n";

should give (or in reverse order):

    0x8d 0x97 0x6e 0x12 0x83 0xc0 0xf3 0x3f

In general, if you don't know whether your system supports a 64 bit IEEE float or not, it probably does. If your system doesn't, WriteExcel will C<croak()> with the message given in the L</DIAGNOSTICS> section. You can check which platforms the module has been tested on at the CPAN testers site: L<http://testers.cpan.org/search?request=dist&dist=Spreadsheet-WriteExcel>.




=head1 DIAGNOSTICS


=over 4

=item Filename required by Spreadsheet::WriteExcel->new()

A filename must be given in the constructor.

=item Can't open filename. It may be in use or protected.

The file cannot be opened for writing. The directory that you are writing to  may be protected or the file may be in use by another program.

=item Unable to create tmp files via File::Temp::tempfile()...

This is a C<-w> warning. You will see it if you are using Spreadsheet::WriteExcel in an environment where temporary files cannot be created, in which case all data will be stored in memory. The warning is for information only: it does not affect creation but it will affect the speed of execution for large files. See the C<set_tempdir> workbook method.


=item Maximum file size, 7087104, exceeded.

The current OLE implementation only supports a maximum BIFF file of this size. This limit can be extended, see the L</LIMITATIONS> section.

=item Can't locate Parse/RecDescent.pm in @INC ...

Spreadsheet::WriteExcel requires the Parse::RecDescent module. Download it from CPAN: L<http://search.cpan.org/search?dist=Parse-RecDescent>

=item Couldn't parse formula ...

There are a large number of warnings which relate to badly formed formulas and functions. See the L</FORMULAS AND FUNCTIONS IN EXCEL> section for suggestions on how to avoid these errors. You should also check the formula in Excel to ensure that it is valid.

=item Required floating point format not supported on this platform.

Operating system doesn't support 64 bit IEEE float or it is byte-ordered in a way unknown to WriteExcel.


=item 'file.xls' cannot be accessed. The file may be read-only ...

You may sometimes encounter the following error when trying to open a file in Excel: "file.xls cannot be accessed. The file may be read-only, or you may be trying to access a read-only location. Or, the server the document is stored on may not be responding."

This error generally means that the Excel file has been corrupted. There are two likely causes of this: the file was FTPed in ASCII mode instead of binary mode or else the file was created with C<UTF-8> data returned by an XML parser. See L</Warning about XML::Parser and perl 5.6> for further details.

=back




=head1 THE EXCEL BINARY FORMAT

The following is some general information about the Excel binary format for anyone who may be interested.

Excel data is stored in the "Binary Interchange File Format" (BIFF) file format. Details of this format are given in "Excel 97-2007 Binary File Format Specification" L<http://www.microsoft.com/interop/docs/OfficeBinaryFormats.mspx>.

Daniel Rentz of OpenOffice.org has also written a detailed description of the Excel workbook records, see L<http://sc.openoffice.org/excelfileformat.pdf>.

Charles Wybble has collected together additional information about the Excel file format. See "The Chicago Project" at L<http://chicago.sourceforge.net/devel/>.

The BIFF data is stored along with other data in an OLE Compound File. This is a structured storage which acts like a file system within a file. A Compound File is comprised of storages and streams which, to follow the file system analogy, are like directories and files.

The OLE format is explained in the "Windows Compound Binary File Format Specification" L<http://www.microsoft.com/interop/docs/supportingtechnologies.mspx>

The Digital Imaging Group have also detailed the OLE format in the JPEG2000 specification: see Appendix A of L<http://www.i3a.org/pdf/wg1n1017.pdf>.

Please note that the provision of this information does not constitute an invitation to start hacking at the BIFF or OLE file formats. There are more interesting ways to waste your time. ;-)




=head1 WRITING EXCEL FILES

Depending on your requirements, background and general sensibilities you may prefer one of the following methods of getting data into Excel:

=over 4

=item * Win32::OLE module and office automation

This requires a Windows platform and an installed copy of Excel. This is the most powerful and complete method for interfacing with Excel. See L<http://www.activestate.com/ASPN/Reference/Products/ActivePerl-5.6/faq/Windows/ActivePerl-Winfaq12.html> and L<http://www.activestate.com/ASPN/Reference/Products/ActivePerl-5.6/site/lib/Win32/OLE.html>. If your main platform is UNIX but you have the resources to set up a separate Win32/MSOffice server, you can convert office documents to text, postscript or PDF using Win32::OLE. For a demonstration of how to do this using Perl see Docserver: L<http://search.cpan.org/search?mode=module&query=docserver>.

=item * CSV, comma separated variables or text

If the file extension is C<csv>, Excel will open and convert this format automatically. Generating a valid CSV file isn't as easy as it seems. Have a look at the DBD::RAM, DBD::CSV, Text::xSV and Text::CSV_XS modules.

=item * DBI with DBD::ADO or DBD::ODBC

Excel files contain an internal index table that allows them to act like a database file. Using one of the standard Perl database modules you can connect to an Excel file as a database.

=item * DBD::Excel

You can also access Spreadsheet::WriteExcel using the standard DBI interface via Takanori Kawai's DBD::Excel module L<http://search.cpan.org/dist/DBD-Excel>

=item * Spreadsheet::WriteExcelXML

This module allows you to create an Excel XML file using the same interface as Spreadsheet::WriteExcel. See: L<http://search.cpan.org/dist/Spreadsheet-WriteExcelXML>

=item * Excel::Template

This module allows you to create an Excel file from an XML template in a manner similar to HTML::Template. See L<http://search.cpan.org/dist/Excel-Template/>.

=item * Spreadsheet::WriteExcel::FromXML

This module allows you to turn a simple XML file into an Excel file using Spreadsheet::WriteExcel as a back-end. The format of the XML file is defined by a supplied DTD: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel-FromXML>.

=item * Spreadsheet::WriteExcel::Simple

This provides an easier interface to Spreadsheet::WriteExcel: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel-Simple>.

=item * Spreadsheet::WriteExcel::FromDB

This is a useful module for creating Excel files directly from a DB table: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel-FromDB>.

=item * HTML tables

This is an easy way of adding formatting via a text based format.

=item * XML or HTML

The Excel XML and HTML file specification are available from L<http://msdn.microsoft.com/library/officedev/ofxml2k/ofxml2k.htm>.

=back

For other Perl-Excel modules try the following search: L<http://search.cpan.org/search?mode=module&query=excel>.




=head1 READING EXCEL FILES

To read data from Excel files try:

=over 4

=item * Spreadsheet::ParseExcel

This uses the OLE::Storage-Lite module to extract data from an Excel file. L<http://search.cpan.org/dist/Spreadsheet-ParseExcel>.

=item * Spreadsheet::ParseExcel_XLHTML

This module uses Spreadsheet::ParseExcel's interface but uses xlHtml (see below) to do the conversion: L<http://search.cpan.org/dist/Spreadsheet-ParseExcel_XLHTML>
Spreadsheet::ParseExcel_XLHTML

=item * xlHtml

This is an open source "Excel to HTML Converter" C/C++ project at L<http://chicago.sourceforge.net/xlhtml/>.

=item * DBD::Excel (reading)

You can also access Spreadsheet::ParseExcel using the standard DBI interface via  Takanori Kawai's DBD::Excel module L<http://search.cpan.org/dist/DBD-Excel>.

=item * Win32::OLE module and office automation (reading)

See, the section L<WRITING EXCEL FILES>.

=item * HTML tables (reading)

If the files are saved from Excel in a HTML format the data can be accessed using HTML::TableExtract L<http://search.cpan.org/dist/HTML-TableExtract>.

=item * DBI with DBD::ADO or DBD::ODBC.

See, the section L<WRITING EXCEL FILES>.

=item * XML::Excel

Converts Excel files to XML using Spreadsheet::ParseExcel L<http://search.cpan.org/dist/XML-Excel>.

=item * OLE::Storage, aka LAOLA

This is a Perl interface to OLE file formats. In particular, the distro contains an Excel to HTML converter called Herbert, L<http://user.cs.tu-berlin.de/~schwartz/pmh/>. This has been superseded by the Spreadsheet::ParseExcel module.

=back


For other Perl-Excel modules try the following search: L<http://search.cpan.org/search?mode=module&query=excel>.

If you wish to view Excel files on a UNIX/Linux platform check out the excellent Gnumeric spreadsheet application at L<http://www.gnome.org/projects/gnumeric/> or OpenOffice.org at L<http://www.openoffice.org/>.

If you wish to view Excel files on a Windows platform which doesn't have Excel installed you can use the free Microsoft Excel Viewer L<http://office.microsoft.com/downloads/2000/xlviewer.aspx>.




=head1 MODIFYING AND REWRITING EXCEL FILES

An Excel file is a binary file within a binary file. It contains several interlinked checksums and changing even one byte can cause it to become corrupted.

As such you cannot simply append or update an Excel file. The only way to achieve this is to read the entire file into memory, make the required changes or additions and then write the file out again.

You can read and rewrite an Excel file using the Spreadsheet::ParseExcel::SaveParser module which is a wrapper around Spreadsheet::ParseExcel and Spreadsheet::WriteExcel. It is part of the Spreadsheet::ParseExcel package: L<http://search.cpan.org/search?dist=Spreadsheet-ParseExcel>.

However, you can only rewrite the features that Spreadsheet::WriteExcel supports so macros, graphs and some other features in the original Excel file will be lost. Also, formulas aren't rewritten, only the result of a formula is written.

Here is an example:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::ParseExcel;
    use Spreadsheet::ParseExcel::SaveParser;

    # Open the template with SaveParser
    my $parser   = new Spreadsheet::ParseExcel::SaveParser;
    my $template = $parser->Parse('template.xls');

    my $sheet    = 0;
    my $row      = 0;
    my $col      = 0;

    # Get the format from the cell
    my $format   = $template->{Worksheet}[$sheet]
                            ->{Cells}[$row][$col]
                            ->{FormatNo};

    # Write data to some cells
    $template->AddCell(0, $row,   $col,   1,     $format);
    $template->AddCell(0, $row+1, $col, "Hello", $format);

    # Add a new worksheet
    $template->AddWorksheet('New Data');

    # The SaveParser SaveAs() method returns a reference to a
    # Spreadsheet::WriteExcel object. If you wish you can then
    # use this to access any of the methods that aren't
    # available from the SaveParser object. If you don't need
    # to do this just use SaveAs().
    #
    my $workbook;

    {
        # SaveAs generates a lot of harmless warnings about unset
        # Worksheet properties. You can ignore them if you wish.
        local $^W = 0;

        # Rewrite the file or save as a new file
        $workbook = $template->SaveAs('new.xls');
    }

    # Use Spreadsheet::WriteExcel methods
    my $worksheet  = $workbook->sheets(0);

    $worksheet->write($row+2, $col, "World2");

    $workbook->close();




=head1 Warning about XML::Parser and perl 5.6

You must be careful when using Spreadsheet::WriteExcel in conjunction with perl 5.6 and XML::Parser (and other XML parsers) due to the fact that the data returned by the parser is generally in C<UTF-8> format.

When C<UTF-8> strings are added to Spreadsheet::WriteExcel's internal data it causes the generated Excel file to become corrupt.

Note, this doesn't affect perl 5.005 (which doesn't try to handle C<UTF-8>) or 5.8 (which handles it correctly).

To avoid this problem you should upgrade to perl 5.8, if possible, or else you should convert the output data from XML::Parser to ASCII or ISO-8859-1 using one of the following methods:

    $new_str = pack 'C*', unpack 'U*', $utf8_str;


    use Unicode::MapUTF8 'from_utf8';
    $new_str = from_utf8({-str => $utf8_str, -charset => 'ISO-8859-1'});




=head1 Warning about Office Service Pack 3

If you have Office Service Pack 3 (SP3) installed you may see the following warning when you open a file created by Spreadsheet::WriteExcel:

    "File Error: data may have been lost".

This is usually caused by multiple instances of data in a cell.

SP3 changed Excel's default behaviour when it encounters multiple data in a cell so that it issues a warning when the file is opened and it displays the first data that was written. Prior to SP3 it didn't issue a warning and displayed the last data written.

For a longer discussion and some workarounds see the following: L<http://groups.google.com/group/spreadsheet-writeexcel/browse_thread/thread/3dcea40e6620af3a>.




=head1 BUGS

Formulas are formulae.

XML and C<UTF-8> data on perl 5.6 can cause Excel files created by Spreadsheet::WriteExcel to become corrupt. See L</Warning about XML::Parser and perl 5.6> for further details.

The format object that is used with a C<merge_range()> method call is marked internally as being associated with a merged range. It is a fatal error to use a merged format in a non-merged cell. The current workaround is to use separate formats for merged and non-merged cell. This restriction will be removed in a future release.

Nested formulas sometimes aren't parsed correctly and give a result of "#VALUE". If you come across a formula that parses like this, let me know.

Spreadsheet::ParseExcel: All formulas created by Spreadsheet::WriteExcel are read as having a value of zero. This is because Spreadsheet::WriteExcel only stores the formula and not the calculated result.

OpenOffice.org: No known issues in this release.

Gnumeric: No known issues in this release.

If you wish to submit a bug report run the C<bug_report.pl> program in the C<examples> directory of the distro.




=head1 Migrating to Excel::Writer::XLSX

Spreadsheet::WriteExcel is in maintenance only mode and has effectively been superseded by L<Excel::Writer::XLSX>.

Excel::Writer::XLSX is an API compatible, drop-in replacement for Spreadsheet::WriteExcel. It also has many more features such as conditional formats, better charts, better formula handling, Excel tables and even sparklines.


To convert your Spreadsheet::WriteExcel program to Excel::Writer::XLSX you only need do the following:

=over

=item * Substitute Excel::Writer::XLSX for Spreadsheet::WriteExcel in your program.

=item * Change the file extension of the output file from C<.xls> to C<.xlsx>.

=item * Optionally replace C<store_formula()> and C<repeat_formula()> with C<write_formula()> which is no longer an expensive operation in Excel::Writer::XLSX. However, you can leave them unchanged if required.

=back

There are some differences between the formats and the modules that are worth noting:

=over

=item * The default font in the XLSX format is Calibri 11 not Arial 10.

=item * Default column widths and row heights are different between XLS and XLSX.

=item * The Excel::Writer::XLSX module uses more memory by default but has a optimisation mode to reduce usage for large files.

=item * The XLSX format doesn't have reading support that is as complete as Spreadsheet::ParseExcel.

=back



=head1 REPOSITORY

The Spreadsheet::WriteExcel source code in host on github: L<http://github.com/jmcnamara/spreadsheet-writeexcel>.




=head1 MAILING LIST

There is a Google group for discussing and asking questions about Spreadsheet::WriteExcel. This is a good place to search to see if your question has been asked before:  L<http://groups.google.com/group/spreadsheet-writeexcel>.

=begin html

<center>
<table style="background-color: #fff; padding: 5px;" cellspacing="0">
  <tr><td>
  <img src="http://groups.google.com/intl/en/images/logos/groups_logo_sm.gif"
         height="30" width="140" alt="Google Groups"/>
  </td></tr>
  <tr><td>
  <a href="http://groups.google.com/group/spreadsheet-writeexcel">Spreadsheet::WriteExcel</a>
  </td></tr>
</table>
</center>

=end html



Alternatively you can keep up to date with future releases by subscribing at:
L<http://freshmeat.net/projects/writeexcel/>.




=head1 DONATIONS

If you'd care to donate to the Spreadsheet::WriteExcel project, you can do so via PayPal: L<http://tinyurl.com/7ayes>.




=head1 SEE ALSO

Spreadsheet::ParseExcel: L<http://search.cpan.org/dist/Spreadsheet-ParseExcel>.

Spreadsheet-WriteExcel-FromXML: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel-FromXML>.

Spreadsheet::WriteExcel::FromDB: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel-FromDB>.

Excel::Template: L<http://search.cpan.org/~rkinyon/Excel-Template/>.

DateTime::Format::Excel: L<http://search.cpan.org/dist/DateTime-Format-Excel>.

"Reading and writing Excel files with Perl" by Teodor Zlatanov, at IBM developerWorks: L<http://www-106.ibm.com/developerworks/library/l-pexcel/>.

"Excel-Dateien mit Perl erstellen - Controller im Gluck" by Peter Dintelmann and Christian Kirsch in the German Unix/web journal iX: L<http://www.heise.de/ix/artikel/2001/06/175/>.

Spreadsheet::WriteExcel documentation in Japanese by Takanori Kawai. L<http://member.nifty.ne.jp/hippo2000/perltips/Spreadsheet/WriteExcel.htm>.

Oesterly user brushes with fame:
L<http://oesterly.com/releases/12102000.html>.

The csv2xls program that is part of Text::CSV_XS:
L<http://search.cpan.org/~hmbrand/Text-CSV_XS/MANIFEST>.



=head1 ACKNOWLEDGMENTS


The following people contributed to the debugging and testing of Spreadsheet::WriteExcel:

Alexander Farber, Andre de Bruin, Arthur@ais, Artur Silveira da Cunha, Bob Rose, Borgar Olsen, Brian Foley, Brian White, Bob Mackay, Cedric Bouvier, Chad Johnson, CPAN testers, Damyan Ivanov, Daniel Berger, Daniel Gardner, Dmitry Kochurov, Eric Frazier, Ernesto Baschny, Felipe Perez Galiana, Gordon Simpson, Hanc Pavel, Harold Bamford, James Holmes, James Wilkinson, Johan Ekenberg, Johann Hanne, Jonathan Scott Duff, J.C. Wren, Kenneth Stacey, Keith Miller, Kyle Krom, Marc Rosenthal, Markus Schmitz, Michael Braig, Michael Buschauer, Mike Blazer, Michael Erickson, Michael W J West, Ning Xie, Paul J. Falbe, Paul Medynski, Peter Dintelmann, Pierre Laplante, Praveen Kotha, Reto Badertscher, Rich Sorden, Shane Ashby, Sharron McKenzie, Shenyu Zheng, Stephan Loescher, Steve Sapovits, Sven Passig, Svetoslav Marinov, Tamas Gulacsi, Troy Daniels, Vahe Sarkissian.

The following people contributed patches, examples or Excel information:

Andrew Benham, Bill Young, Cedric Bouvier, Charles Wybble, Daniel Rentz, David Robins, Franco Venturi, Guy Albertelli, Ian Penman, John Heitmann, Jon Guy, Kyle R. Burton, Pierre-Jean Vouette, Rubio, Marco Geri, Mark Fowler, Matisse Enzer, Sam Kington, Takanori Kawai, Tom O'Sullivan.

Many thanks to Ron McKelvey, Ronzo Consulting for Siemens, who sponsored the development of the formula caching routines.

Many thanks to Cassens Transport who sponsored the development of the embedded charts and autofilters.

Additional thanks to Takanori Kawai for translating the documentation into Japanese.

Gunnar Wolf maintains the Debian distro.

Thanks to Damian Conway for the excellent Parse::RecDescent.

Thanks to Tim Jenness for File::Temp.

Thanks to Michael Meeks and Jody Goldberg for their work on Gnumeric.




=head1 DISCLAIMER OF WARRANTY

Because this software is licensed free of charge, there is no warranty for the software, to the extent permitted by applicable law. Except when otherwise stated in writing the copyright holders and/or other parties provide the software "as is" without warranty of any kind, either expressed or implied, including, but not limited to, the implied warranties of merchantability and fitness for a particular purpose. The entire risk as to the quality and performance of the software is with you. Should the software prove defective, you assume the cost of all necessary servicing, repair, or correction.

In no event unless required by applicable law or agreed to in writing will any copyright holder, or any other party who may modify and/or redistribute the software as permitted by the above licence, be liable to you for damages, including any general, special, incidental, or consequential damages arising out of the use or inability to use the software (including but not limited to loss of data or data being rendered inaccurate or losses sustained by you or third parties or a failure of the software to operate with any other software), even if such holder or other party has been advised of the possibility of such damages.




=head1 LICENSE

Either the Perl Artistic Licence L<http://dev.perl.org/licenses/artistic.html> or the GPL L<http://www.opensource.org/licenses/gpl-license.php>.




=head1 AUTHOR

John McNamara jmcnamara@cpan.org

    The ashtray says
    You were up all night.
    When you went to bed
    With your darkest mind.
    Your pillow wept
    And covered your eyes.
    And you finally slept
    While the sun caught fire.

    You've changed.
      -- Jeff Tweedy




=head1 COPYRIGHT

Copyright MM-MMXII, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
