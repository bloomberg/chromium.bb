package Spreadsheet::WriteExcel::Chart;

###############################################################################
#
# Chart - A writer class for Excel Charts.
#
# Used in conjunction with Spreadsheet::WriteExcel.
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

use Exporter;
use strict;
use Carp;
use FileHandle;
use Spreadsheet::WriteExcel::Worksheet;


use vars qw($VERSION @ISA);
@ISA = qw(Spreadsheet::WriteExcel::Worksheet);

$VERSION = '2.40';

###############################################################################
#
# Formatting information.
#
# perltidy with options: -mbl=2 -pt=0 -nola
#
# Any camel case Hungarian notation style variable names in the BIFF record
# writing sub-routines below are for similarity with names used in the Excel
# documentation. Otherwise lowercase underscore style names are used.
#


###############################################################################
#
# The chart class hierarchy is as follows. Chart.pm acts as a factory for the
# sub-classes.
#
#
#     Spreadsheet::WriteExcel::BIFFwriter
#                     ^
#                     |
#     Spreadsheet::WriteExcel::Worksheet
#                     ^
#                     |
#     Spreadsheet::WriteExcel::Chart
#                     ^
#                     |
#     Spreadsheet::WriteExcel::Chart::* (sub-types)
#


###############################################################################
#
# factory()
#
# Factory method for returning chart objects based on their class type.
#
sub factory {

    my $current_class  = shift;
    my $chart_subclass = shift;

    $chart_subclass = ucfirst lc $chart_subclass;

    my $module = "Spreadsheet::WriteExcel::Chart::" . $chart_subclass;

    eval "require $module";

    # TODO. Need to re-raise this error from Workbook::add_chart().
    die "Chart type '$chart_subclass' not supported in add_chart()\n" if $@;

    return $module->new( @_ );
}


###############################################################################
#
# new()
#
# Default constructor for sub-classes.
#
sub new {

    my $class = shift;
    my $self  = Spreadsheet::WriteExcel::Worksheet->new( @_ );

    $self->{_sheet_type}  = 0x0200;
    $self->{_orientation} = 0x0;
    $self->{_series}      = [];
    $self->{_embedded}    = 0;

    bless $self, $class;
    $self->_set_default_properties();
    $self->_set_default_config_data();
    return $self;
}


###############################################################################
#
# Public methods.
#
###############################################################################


###############################################################################
#
# add_series()
#
# Add a series and it's properties to a chart.
#
sub add_series {

    my $self = shift;
    my %arg  = @_;

    croak "Must specify 'values' in add_series()" if !exists $arg{values};

    # Parse the ranges to validate them and extract salient information.
    my @value_data    = $self->_parse_series_formula( $arg{values} );
    my @category_data = $self->_parse_series_formula( $arg{categories} );
    my $name_formula  = $self->_parse_series_formula( $arg{name_formula} );

    # Default category count to the same as the value count if not defined.
    if ( !defined $category_data[1] ) {
        $category_data[1] = $value_data[1];
    }

    # Add the parsed data to the user supplied data.
    %arg = (
        @_,
        _values       => \@value_data,
        _categories   => \@category_data,
        _name_formula => $name_formula
    );

    # Encode the Series name.
    my ( $name, $encoding ) =
      $self->_encode_utf16( $arg{name}, $arg{name_encoding} );

    $arg{name}          = $name;
    $arg{name_encoding} = $encoding;

    push @{ $self->{_series} }, \%arg;
}


###############################################################################
#
# set_x_axis()
#
# Set the properties of the X-axis.
#
sub set_x_axis {

    my $self = shift;
    my %arg  = @_;

    my ( $name, $encoding ) =
      $self->_encode_utf16( $arg{name}, $arg{name_encoding} );

    my $formula = $self->_parse_series_formula( $arg{name_formula} );

    $self->{_x_axis_name}     = $name;
    $self->{_x_axis_encoding} = $encoding;
    $self->{_x_axis_formula}  = $formula;
}


###############################################################################
#
# set_y_axis()
#
# Set the properties of the Y-axis.
#
sub set_y_axis {

    my $self = shift;
    my %arg  = @_;

    my ( $name, $encoding ) =
      $self->_encode_utf16( $arg{name}, $arg{name_encoding} );

    my $formula = $self->_parse_series_formula( $arg{name_formula} );

    $self->{_y_axis_name}     = $name;
    $self->{_y_axis_encoding} = $encoding;
    $self->{_y_axis_formula}  = $formula;
}


###############################################################################
#
# set_title()
#
# Set the properties of the chart title.
#
sub set_title {

    my $self = shift;
    my %arg  = @_;

    my ( $name, $encoding ) =
      $self->_encode_utf16( $arg{name}, $arg{name_encoding} );

    my $formula = $self->_parse_series_formula( $arg{name_formula} );

    $self->{_title_name}     = $name;
    $self->{_title_encoding} = $encoding;
    $self->{_title_formula}  = $formula;
}


###############################################################################
#
# set_legend()
#
# Set the properties of the chart legend.
#
sub set_legend {

    my $self = shift;
    my %arg  = @_;

    if ( defined $arg{position} ) {
        if ( lc $arg{position} eq 'none' ) {
            $self->{_legend}->{_visible} = 0;
        }
    }
}


###############################################################################
#
# set_plotarea()
#
# Set the properties of the chart plotarea.
#
sub set_plotarea {

    my $self = shift;
    my %arg  = @_;
    return unless keys %arg;

    my $area = $self->{_plotarea};

    # Set the plotarea visibility.
    if ( defined $arg{visible} ) {
        $area->{_visible} = $arg{visible};
        return if !$area->{_visible};
    }

    # TODO. could move this out of if statement.
    $area->{_bg_color_index} = 0x08;

    # Set the chart background colour.
    if ( defined $arg{color} ) {
        my ( $index, $rgb ) = $self->_get_color_indices( $arg{color} );
        if ( defined $index ) {
            $area->{_fg_color_index} = $index;
            $area->{_fg_color_rgb}   = $rgb;
            $area->{_bg_color_index} = 0x08;
            $area->{_bg_color_rgb}   = 0x000000;
        }
    }

    # Set the border line colour.
    if ( defined $arg{line_color} ) {
        my ( $index, $rgb ) = $self->_get_color_indices( $arg{line_color} );
        if ( defined $index ) {
            $area->{_line_color_index} = $index;
            $area->{_line_color_rgb}   = $rgb;
        }
    }

    # Set the border line pattern.
    if ( defined $arg{line_pattern} ) {
        my $pattern = $self->_get_line_pattern( $arg{line_pattern} );
        $area->{_line_pattern} = $pattern;
    }

    # Set the border line weight.
    if ( defined $arg{line_weight} ) {
        my $weight = $self->_get_line_weight( $arg{line_weight} );
        $area->{_line_weight} = $weight;
    }
}


###############################################################################
#
# set_chartarea()
#
# Set the properties of the chart chartarea.
#
sub set_chartarea {

    my $self = shift;
    my %arg  = @_;
    return unless keys %arg;

    my $area = $self->{_chartarea};

    # Embedded automatic line weight has a different default value.
    $area->{_line_weight} = 0xFFFF if $self->{_embedded};


    # Set the chart background colour.
    if ( defined $arg{color} ) {
        my ( $index, $rgb ) = $self->_get_color_indices( $arg{color} );
        if ( defined $index ) {
            $area->{_fg_color_index} = $index;
            $area->{_fg_color_rgb}   = $rgb;
            $area->{_bg_color_index} = 0x08;
            $area->{_bg_color_rgb}   = 0x000000;
            $area->{_area_pattern}   = 1;
            $area->{_area_options}   = 0x0000 if $self->{_embedded};
            $area->{_visible}        = 1;
        }
    }

    # Set the border line colour.
    if ( defined $arg{line_color} ) {
        my ( $index, $rgb ) = $self->_get_color_indices( $arg{line_color} );
        if ( defined $index ) {
            $area->{_line_color_index} = $index;
            $area->{_line_color_rgb}   = $rgb;
            $area->{_line_pattern}     = 0x00;
            $area->{_line_options}     = 0x0000;
            $area->{_visible}          = 1;
        }
    }

    # Set the border line pattern.
    if ( defined $arg{line_pattern} ) {
        my $pattern = $self->_get_line_pattern( $arg{line_pattern} );
        $area->{_line_pattern}     = $pattern;
        $area->{_line_options}     = 0x0000;
        $area->{_line_color_index} = 0x4F if !defined $arg{line_color};
        $area->{_visible}          = 1;
    }

    # Set the border line weight.
    if ( defined $arg{line_weight} ) {
        my $weight = $self->_get_line_weight( $arg{line_weight} );
        $area->{_line_weight}      = $weight;
        $area->{_line_options}     = 0x0000;
        $area->{_line_pattern}     = 0x00 if !defined $arg{line_pattern};
        $area->{_line_color_index} = 0x4F if !defined $arg{line_color};
        $area->{_visible}          = 1;
    }
}


###############################################################################
#
# Internal methods. The following section of methods are used for the internal
# structuring of the Chart object and file format.
#
###############################################################################


###############################################################################
#
# _prepend(), overridden.
#
# The parent Worksheet class needs to store some data in memory and some in
# temporary files for efficiency. The Chart* classes don't need to do this
# since they are dealing with smaller amounts of data so we override
# _prepend() to turn it into an _append() method. This allows for a more
# natural method calling order.
#
sub _prepend {

    my $self = shift;

    $self->{_using_tmpfile} = 0;

    return $self->_append( @_ );
}


###############################################################################
#
# _close(), overridden.
#
# Create and store the Chart data structures.
#
sub _close {

    my $self = shift;

    # Ignore any data that has been written so far since it is probably
    # from unwanted Worksheet method calls.
    $self->{_data} = '';

    # TODO. Check for charts without a series?

    # Store the chart BOF.
    $self->_store_bof( 0x0020 );

    # Store the tab color.
    $self->_store_tab_color();

    # Store the page header
    $self->_store_header();

    # Store the page footer
    $self->_store_footer();

    # Store the page horizontal centering
    $self->_store_hcenter();

    # Store the page vertical centering
    $self->_store_vcenter();

    # Store the left margin
    $self->_store_margin_left();

    # Store the right margin
    $self->_store_margin_right();

    # Store the top margin
    $self->_store_margin_top();

    # Store the bottom margin
    $self->_store_margin_bottom();

    # Store the page setup
    $self->_store_setup();

    # Store the sheet password
    $self->_store_password();

    # Start of Chart specific records.

    # Store the FBI font records.
    $self->_store_fbi( @{ $self->{_config}->{_font_numbers} } );
    $self->_store_fbi( @{ $self->{_config}->{_font_series} } );
    $self->_store_fbi( @{ $self->{_config}->{_font_title} } );
    $self->_store_fbi( @{ $self->{_config}->{_font_axes} } );

    # Ignore UNITS record.

    # Store the Chart sub-stream.
    $self->_store_chart_stream();

    # Append the sheet dimensions
    $self->_store_dimensions();

    # TODO add SINDEX and NUMBER records.

    if ( !$self->{_embedded} ) {
        $self->_store_window2();
    }

    # Store the sheet SCL record.
    if ( !$self->{_embedded} ) {
        $self->_store_zoom();
    }

    $self->_store_eof();
}


###############################################################################
#
# _store_window2(), overridden.
#
# Write BIFF record Window2. Note, this overrides the parent Worksheet
# record because the Chart version of the record is smaller and is used
# mainly to indicate if the chart tab is selected or not.
#
sub _store_window2 {

    use integer;    # Avoid << shift bug in Perl 5.6.0 on HP-UX

    my $self = shift;

    my $record  = 0x023E;    # Record identifier
    my $length  = 0x000A;    # Number of bytes to follow
    my $grbit   = 0x0000;    # Option flags
    my $rwTop   = 0x0000;    # Top visible row
    my $colLeft = 0x0000;    # Leftmost visible column
    my $rgbHdr  = 0x0000;    # Row/col heading, grid color

    # The options flags that comprise $grbit
    my $fDspFmla       = 0;                     # 0 - bit
    my $fDspGrid       = 0;                     # 1
    my $fDspRwCol      = 0;                     # 2
    my $fFrozen        = 0;                     # 3
    my $fDspZeros      = 0;                     # 4
    my $fDefaultHdr    = 0;                     # 5
    my $fArabic        = 0;                     # 6
    my $fDspGuts       = 0;                     # 7
    my $fFrozenNoSplit = 0;                     # 0 - bit
    my $fSelected      = $self->{_selected};    # 1
    my $fPaged         = 0;                     # 2
    my $fBreakPreview  = 0;                     # 3

    #<<< Perltidy ignore this.
    $grbit             = $fDspFmla;
    $grbit            |= $fDspGrid       << 1;
    $grbit            |= $fDspRwCol      << 2;
    $grbit            |= $fFrozen        << 3;
    $grbit            |= $fDspZeros      << 4;
    $grbit            |= $fDefaultHdr    << 5;
    $grbit            |= $fArabic        << 6;
    $grbit            |= $fDspGuts       << 7;
    $grbit            |= $fFrozenNoSplit << 8;
    $grbit            |= $fSelected      << 9;
    $grbit            |= $fPaged         << 10;
    $grbit            |= $fBreakPreview  << 11;
    #>>>

    my $header = pack( "vv", $record, $length );
    my $data = pack( "vvvV", $grbit, $rwTop, $colLeft, $rgbHdr );

    $self->_append( $header, $data );
}


###############################################################################
#
# _parse_series_formula()
#
# Parse the formula used to define a series. We also extract some range
# information required for _store_series() and the SERIES record.
#
sub _parse_series_formula {

    my $self = shift;

    my $formula  = $_[0];
    my $encoding = 0;
    my $length   = 0;
    my $count    = 0;
    my @tokens;

    return '' if !defined $formula;

    # Strip the = sign at the beginning of the formula string
    $formula =~ s(^=)();

    # Parse the formula using the parser in Formula.pm
    my $parser = $self->{_parser};

    # In order to raise formula errors from the point of view of the calling
    # program we use an eval block and re-raise the error from here.
    #
    eval { @tokens = $parser->parse_formula( $formula ) };

    if ( $@ ) {
        $@ =~ s/\n$//;    # Strip the \n used in the Formula.pm die().
        croak $@;         # Re-raise the error.
    }

    # Force ranges to be a reference class.
    s/_ref3d/_ref3dR/     for @tokens;
    s/_range3d/_range3dR/ for @tokens;
    s/_name/_nameR/       for @tokens;

    # Parse the tokens into a formula string.
    $formula = $parser->parse_tokens( @tokens );

    # Return formula for a single cell as used by title and series name.
    if ( ord $formula == 0x3A ) {
        return $formula;
    }

    # Extract the range from the parse formula.
    if ( ord $formula == 0x3B ) {
        my ( $ptg, $ext_ref, $row_1, $row_2, $col_1, $col_2 ) = unpack 'Cv5',
          $formula;

        # TODO. Remove high bit on relative references.
        $count = $row_2 - $row_1 + 1;
    }

    return ( $formula, $count );
}

###############################################################################
#
# _encode_utf16()
#
# Convert UTF8 strings used in the chart to UTF16.
#
sub _encode_utf16 {

    my $self = shift;

    my $string = shift;
    my $encoding = shift || 0;

    # Exit if the $string isn't defined, i.e., hasn't been set by user.
    return ( undef, undef ) if !defined $string;

    # Return if encoding is set, i.e., string has been manually encoded.
    #return ( undef, undef ) if $string == 1;

    # Handle utf8 strings in perl 5.8.
    if ( $] >= 5.008 ) {
        require Encode;

        if ( Encode::is_utf8( $string ) ) {
            $string = Encode::encode( "UTF-16BE", $string );
            $encoding = 1;
        }
    }

    # Chart strings are limited to 255 characters.
    my $limit = $encoding ? 255 * 2 : 255;

    if ( length $string >= $limit ) {

        # truncate the string and raise a warning.
        $string = substr $string, 0, $limit;
        carp 'Chart strings must be less than 256 characters. '
          . 'String truncated';
    }

    return ( $string, $encoding );
}


###############################################################################
#
# _get_color_indices()
#
# Convert the user specified colour index or string to an colour index and
# RGB colour number.
#
sub _get_color_indices {

    my $self  = shift;
    my $color = shift;
    my $index;
    my $rgb;

    return ( undef, undef ) if !defined $color;

    my %colors = (
        aqua    => 0x0F,
        cyan    => 0x0F,
        black   => 0x08,
        blue    => 0x0C,
        brown   => 0x10,
        magenta => 0x0E,
        fuchsia => 0x0E,
        gray    => 0x17,
        grey    => 0x17,
        green   => 0x11,
        lime    => 0x0B,
        navy    => 0x12,
        orange  => 0x35,
        pink    => 0x21,
        purple  => 0x14,
        red     => 0x0A,
        silver  => 0x16,
        white   => 0x09,
        yellow  => 0x0D,
    );


    # Check for the various supported colour index/name possibilities.
    if ( exists $colors{$color} ) {

        # Colour matches one of the supported colour names.
        $index = $colors{$color};
    }
    elsif ( $color =~ m/\D/ ) {

        # Return undef if $color is a string but not one of the supported ones.
        return ( undef, undef );
    }
    elsif ( $color < 8 || $color > 63 ) {

        # Return undef if index is out of range.
        return ( undef, undef );
    }
    else {

        # We should have a valid color index in a valid range.
        $index = $color;
    }

    $rgb = $self->_get_color_rbg( $index );
    return ( $index, $rgb );
}


###############################################################################
#
# _get_color_rbg()
#
# Get the RedGreenBlue number for the colour index from the Workbook palette.
#
sub _get_color_rbg {

    my $self  = shift;
    my $index = shift;

    # Adjust colour index from 8-63 (user range) to 0-55 (Excel range).
    $index -= 8;

    my @red_green_blue = @{ $self->{_palette}->[$index] };
    return unpack 'V', pack 'C*', @red_green_blue;
}


###############################################################################
#
# _get_line_pattern()
#
# Get the Excel chart index for line pattern that corresponds to the user
# defined value.
#
sub _get_line_pattern {

    my $self    = shift;
    my $value   = lc shift;
    my $default = 0;
    my $pattern;

    my %patterns = (
        0              => 5,
        1              => 0,
        2              => 1,
        3              => 2,
        4              => 3,
        5              => 4,
        6              => 7,
        7              => 6,
        8              => 8,
        'solid'        => 0,
        'dash'         => 1,
        'dot'          => 2,
        'dash-dot'     => 3,
        'dash-dot-dot' => 4,
        'none'         => 5,
        'dark-gray'    => 6,
        'medium-gray'  => 7,
        'light-gray'   => 8,
    );

    if ( exists $patterns{$value} ) {
        $pattern = $patterns{$value};
    }
    else {
        $pattern = $default;
    }

    return $pattern;
}


###############################################################################
#
# _get_line_weight()
#
# Get the Excel chart index for line weight that corresponds to the user
# defined value.
#
sub _get_line_weight {

    my $self    = shift;
    my $value   = lc shift;
    my $default = 0;
    my $weight;

    my %weights = (
        1          => -1,
        2          => 0,
        3          => 1,
        4          => 2,
        'hairline' => -1,
        'narrow'   => 0,
        'medium'   => 1,
        'wide'     => 2,
    );

    if ( exists $weights{$value} ) {
        $weight = $weights{$value};
    }
    else {
        $weight = $default;
    }

    return $weight;
}


###############################################################################
#
# _store_chart_stream()
#
# Store the CHART record and it's substreams.
#
sub _store_chart_stream {

    my $self = shift;

    $self->_store_chart( @{ $self->{_config}->{_chart} } );

    $self->_store_begin();

    # Store the chart SCL record.
    if ( !$self->{_embedded} ) {
        $self->_store_zoom();
    }

    $self->_store_plotgrowth();

    if ( $self->{_chartarea}->{_visible} ) {
        $self->_store_chartarea_frame_stream();
    }

    # Store SERIES stream for each series.
    my $index = 0;
    for my $series ( @{ $self->{_series} } ) {

        $self->_store_series_stream(
            _index            => $index,
            _value_formula    => $series->{_values}->[0],
            _value_count      => $series->{_values}->[1],
            _category_count   => $series->{_categories}->[1],
            _category_formula => $series->{_categories}->[0],
            _name             => $series->{name},
            _name_encoding    => $series->{name_encoding},
            _name_formula     => $series->{_name_formula},
        );

        $index++;
    }

    $self->_store_shtprops();

    # Write the TEXT streams.
    for my $font_index ( 5 .. 6 ) {
        $self->_store_defaulttext();
        $self->_store_series_text_stream( $font_index );
    }

    $self->_store_axesused( 1 );
    $self->_store_axisparent_stream();

    if ( defined $self->{_title_name} || defined $self->{_title_formula} ) {
        $self->_store_title_text_stream();
    }

    $self->_store_end();

}


###############################################################################
#
# _store_series_stream()
#
# Write the SERIES chart substream.
#
sub _store_series_stream {

    my $self = shift;
    my %arg  = @_;

    my $name_type     = $arg{_name_formula}     ? 2 : 1;
    my $value_type    = $arg{_value_formula}    ? 2 : 0;
    my $category_type = $arg{_category_formula} ? 2 : 0;

    $self->_store_series( $arg{_value_count}, $arg{_category_count} );

    $self->_store_begin();

    # Store the Series name AI record.
    $self->_store_ai( 0, $name_type, $arg{_name_formula} );
    if ( defined $arg{_name} ) {
        $self->_store_seriestext( $arg{_name}, $arg{_name_encoding} );
    }

    $self->_store_ai( 1, $value_type,    $arg{_value_formula} );
    $self->_store_ai( 2, $category_type, $arg{_category_formula} );
    $self->_store_ai( 3, 1,              '' );

    $self->_store_dataformat_stream( $arg{_index}, $arg{_index}, 0xFFFF );
    $self->_store_sertocrt( 0 );
    $self->_store_end();
}


###############################################################################
#
# _store_dataformat_stream()
#
# Write the DATAFORMAT chart substream.
#
sub _store_dataformat_stream {

    my $self = shift;

    my $series_index = shift;

    $self->_store_dataformat( $series_index, $series_index, 0xFFFF );

    $self->_store_begin();
    $self->_store_3dbarshape();
    $self->_store_end();
}


###############################################################################
#
# _store_series_text_stream()
#
# Write the series TEXT substream.
#
sub _store_series_text_stream {

    my $self = shift;

    my $font_index = shift;

    $self->_store_text( @{ $self->{_config}->{_series_text} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_series_text_pos} } );
    $self->_store_fontx( $font_index );
    $self->_store_ai( 0, 1, '' );
    $self->_store_end();
}


###############################################################################
#
# _store_x_axis_text_stream()
#
# Write the X-axis TEXT substream.
#
sub _store_x_axis_text_stream {

    my $self = shift;

    my $formula = $self->{_x_axis_formula};
    my $ai_type = $formula ? 2 : 1;

    $self->_store_text( @{ $self->{_config}->{_x_axis_text} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_x_axis_text_pos} } );
    $self->_store_fontx( 8 );
    $self->_store_ai( 0, $ai_type, $formula );

    if ( defined $self->{_x_axis_name} ) {
        $self->_store_seriestext( $self->{_x_axis_name},
            $self->{_x_axis_encoding},
        );
    }

    $self->_store_objectlink( 3 );
    $self->_store_end();
}


###############################################################################
#
# _store_y_axis_text_stream()
#
# Write the Y-axis TEXT substream.
#
sub _store_y_axis_text_stream {

    my $self = shift;

    my $formula = $self->{_y_axis_formula};
    my $ai_type = $formula ? 2 : 1;

    $self->_store_text( @{ $self->{_config}->{_y_axis_text} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_y_axis_text_pos} } );
    $self->_store_fontx( 8 );
    $self->_store_ai( 0, $ai_type, $formula );

    if ( defined $self->{_y_axis_name} ) {
        $self->_store_seriestext( $self->{_y_axis_name},
            $self->{_y_axis_encoding},
        );
    }

    $self->_store_objectlink( 2 );
    $self->_store_end();
}


###############################################################################
#
# _store_legend_text_stream()
#
# Write the legend TEXT substream.
#
sub _store_legend_text_stream {

    my $self = shift;

    $self->_store_text( @{ $self->{_config}->{_legend_text} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_legend_text_pos} } );
    $self->_store_ai( 0, 1, '' );

    $self->_store_end();
}


###############################################################################
#
# _store_title_text_stream()
#
# Write the title TEXT substream.
#
sub _store_title_text_stream {

    my $self = shift;

    my $formula = $self->{_title_formula};
    my $ai_type = $formula ? 2 : 1;

    $self->_store_text( @{ $self->{_config}->{_title_text} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_title_text_pos} } );
    $self->_store_fontx( 7 );
    $self->_store_ai( 0, $ai_type, $formula );

    if ( defined $self->{_title_name} ) {
        $self->_store_seriestext( $self->{_title_name},
            $self->{_title_encoding},
        );
    }

    $self->_store_objectlink( 1 );
    $self->_store_end();
}


###############################################################################
#
# _store_axisparent_stream()
#
# Write the AXISPARENT chart substream.
#
sub _store_axisparent_stream {

    my $self = shift;

    $self->_store_axisparent( @{ $self->{_config}->{_axisparent} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_axisparent_pos} } );
    $self->_store_axis_category_stream();
    $self->_store_axis_values_stream();

    if ( defined $self->{_x_axis_name} || defined $self->{_x_axis_formula} ) {
        $self->_store_x_axis_text_stream();
    }

    if ( defined $self->{_y_axis_name} || defined $self->{_y_axis_formula} ) {
        $self->_store_y_axis_text_stream();
    }

    if ( $self->{_plotarea}->{_visible} ) {
        $self->_store_plotarea();
        $self->_store_plotarea_frame_stream();
    }

    $self->_store_chartformat_stream();
    $self->_store_end();
}


###############################################################################
#
# _store_axis_category_stream()
#
# Write the AXIS chart substream for the chart category.
#
sub _store_axis_category_stream {

    my $self = shift;

    $self->_store_axis( 0 );

    $self->_store_begin();
    $self->_store_catserrange();
    $self->_store_axcext();
    $self->_store_tick();
    $self->_store_end();
}


###############################################################################
#
# _store_axis_values_stream()
#
# Write the AXIS chart substream for the chart values.
#
sub _store_axis_values_stream {

    my $self = shift;

    $self->_store_axis( 1 );

    $self->_store_begin();
    $self->_store_valuerange();
    $self->_store_tick();
    $self->_store_axislineformat();
    $self->_store_lineformat( 0x00000000, 0x0000, 0xFFFF, 0x0009, 0x004D );
    $self->_store_end();
}


###############################################################################
#
# _store_plotarea_frame_stream()
#
# Write the FRAME chart substream for the plotarea.
#
sub _store_plotarea_frame_stream {

    my $self = shift;

    my $area = $self->{_plotarea};

    $self->_store_frame( 0x00, 0x03 );
    $self->_store_begin();

    $self->_store_lineformat(
        $area->{_line_color_rgb}, $area->{_line_pattern},
        $area->{_line_weight},    $area->{_line_options},
        $area->{_line_color_index}
    );

    $self->_store_areaformat(
        $area->{_fg_color_rgb},   $area->{_bg_color_rgb},
        $area->{_area_pattern},   $area->{_area_options},
        $area->{_fg_color_index}, $area->{_bg_color_index}
    );

    $self->_store_end();
}


###############################################################################
#
# _store_chartarea_frame_stream()
#
# Write the FRAME chart substream for the chartarea.
#
sub _store_chartarea_frame_stream {

    my $self = shift;

    my $area = $self->{_chartarea};

    $self->_store_frame( 0x00, 0x02 );
    $self->_store_begin();

    $self->_store_lineformat(
        $area->{_line_color_rgb}, $area->{_line_pattern},
        $area->{_line_weight},    $area->{_line_options},
        $area->{_line_color_index}
    );

    $self->_store_areaformat(
        $area->{_fg_color_rgb},   $area->{_bg_color_rgb},
        $area->{_area_pattern},   $area->{_area_options},
        $area->{_fg_color_index}, $area->{_bg_color_index}
    );

    $self->_store_end();
}

###############################################################################
#
# _store_chartformat_stream()
#
# Write the CHARTFORMAT chart substream.
#
sub _store_chartformat_stream {

    my $self = shift;

    # The _vary_data_color is set by classes that need it, like Pie.
    $self->_store_chartformat( $self->{_vary_data_color} );

    $self->_store_begin();

    # Store the BIFF record that will define the chart type.
    $self->_store_chart_type();

    # Note, the CHARTFORMATLINK record is only written by Excel.

    if ( $self->{_legend}->{_visible} ) {
        $self->_store_legend_stream();
    }

    $self->_store_marker_dataformat_stream();
    $self->_store_end();
}


###############################################################################
#
# _store_chart_type()
#
# This is an abstract method that is overridden by the sub-classes to define
# the chart types such as Column, Line, Pie, etc.
#
sub _store_chart_type {

}


###############################################################################
#
# _store_marker_dataformat_stream()
#
# This is an abstract method that is overridden by the sub-classes to define
# properties of markers, linetypes, pie formats and other.
#
sub _store_marker_dataformat_stream {

}


###############################################################################
#
# _store_legend_stream()
#
# Write the LEGEND chart substream.
#
sub _store_legend_stream {

    my $self = shift;

    $self->_store_legend( @{ $self->{_config}->{_legend} } );

    $self->_store_begin();
    $self->_store_pos( @{ $self->{_config}->{_legend_pos} } );
    $self->_store_legend_text_stream();
    $self->_store_end();
}


###############################################################################
#
# BIFF Records.
#
###############################################################################


###############################################################################
#
# _store_3dbarshape()
#
# Write the 3DBARSHAPE chart BIFF record.
#
sub _store_3dbarshape {

    my $self = shift;

    my $record = 0x105F;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $riser  = 0x00;      # Shape of base.
    my $taper  = 0x00;      # Column taper type.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $riser;
    $data .= pack 'C', $taper;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_ai()
#
# Write the AI chart BIFF record.
#
sub _store_ai {

    my $self = shift;

    my $record       = 0x1051;        # Record identifier.
    my $length       = 0x0008;        # Number of bytes to follow.
    my $id           = $_[0];         # Link index.
    my $type         = $_[1];         # Reference type.
    my $formula      = $_[2];         # Pre-parsed formula.
    my $format_index = $_[3] || 0;    # Num format index.
    my $grbit        = 0x0000;        # Option flags.

    my $formula_length = length $formula;

    $length += $formula_length;

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $id;
    $data .= pack 'C', $type;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $format_index;
    $data .= pack 'v', $formula_length;
    $data .= $formula;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_areaformat()
#
# Write the AREAFORMAT chart BIFF record. Contains the patterns and colours
# of a chart area.
#
sub _store_areaformat {

    my $self = shift;

    my $record    = 0x100A;    # Record identifier.
    my $length    = 0x0010;    # Number of bytes to follow.
    my $rgbFore   = $_[0];     # Foreground RGB colour.
    my $rgbBack   = $_[1];     # Background RGB colour.
    my $pattern   = $_[2];     # Pattern.
    my $grbit     = $_[3];     # Option flags.
    my $indexFore = $_[4];     # Index to Foreground colour.
    my $indexBack = $_[5];     # Index to Background colour.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $rgbFore;
    $data .= pack 'V', $rgbBack;
    $data .= pack 'v', $pattern;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $indexFore;
    $data .= pack 'v', $indexBack;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axcext()
#
# Write the AXCEXT chart BIFF record.
#
sub _store_axcext {

    my $self = shift;

    my $record       = 0x1062;    # Record identifier.
    my $length       = 0x0012;    # Number of bytes to follow.
    my $catMin       = 0x0000;    # Minimum category on axis.
    my $catMax       = 0x0000;    # Maximum category on axis.
    my $catMajor     = 0x0001;    # Value of major unit.
    my $unitMajor    = 0x0000;    # Units of major unit.
    my $catMinor     = 0x0001;    # Value of minor unit.
    my $unitMinor    = 0x0000;    # Units of minor unit.
    my $unitBase     = 0x0000;    # Base unit of axis.
    my $catCrossDate = 0x0000;    # Crossing point.
    my $grbit        = 0x00EF;    # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $catMin;
    $data .= pack 'v', $catMax;
    $data .= pack 'v', $catMajor;
    $data .= pack 'v', $unitMajor;
    $data .= pack 'v', $catMinor;
    $data .= pack 'v', $unitMinor;
    $data .= pack 'v', $unitBase;
    $data .= pack 'v', $catCrossDate;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axesused()
#
# Write the AXESUSED chart BIFF record.
#
sub _store_axesused {

    my $self = shift;

    my $record   = 0x1046;    # Record identifier.
    my $length   = 0x0002;    # Number of bytes to follow.
    my $num_axes = $_[0];     # Number of axes used.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $num_axes;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axis()
#
# Write the AXIS chart BIFF record to define the axis type.
#
sub _store_axis {

    my $self = shift;

    my $record    = 0x101D;        # Record identifier.
    my $length    = 0x0012;        # Number of bytes to follow.
    my $type      = $_[0];         # Axis type.
    my $reserved1 = 0x00000000;    # Reserved.
    my $reserved2 = 0x00000000;    # Reserved.
    my $reserved3 = 0x00000000;    # Reserved.
    my $reserved4 = 0x00000000;    # Reserved.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $type;
    $data .= pack 'V', $reserved1;
    $data .= pack 'V', $reserved2;
    $data .= pack 'V', $reserved3;
    $data .= pack 'V', $reserved4;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axislineformat()
#
# Write the AXISLINEFORMAT chart BIFF record.
#
sub _store_axislineformat {

    my $self = shift;

    my $record      = 0x1021;    # Record identifier.
    my $length      = 0x0002;    # Number of bytes to follow.
    my $line_format = 0x0001;    # Axis line format.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $line_format;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_axisparent()
#
# Write the AXISPARENT chart BIFF record.
#
sub _store_axisparent {

    my $self = shift;

    my $record = 0x1041;    # Record identifier.
    my $length = 0x0012;    # Number of bytes to follow.
    my $iax    = $_[0];     # Axis index.
    my $x      = $_[1];     # X-coord.
    my $y      = $_[2];     # Y-coord.
    my $dx     = $_[3];     # Length of x axis.
    my $dy     = $_[4];     # Length of y axis.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $iax;
    $data .= pack 'V', $x;
    $data .= pack 'V', $y;
    $data .= pack 'V', $dx;
    $data .= pack 'V', $dy;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_begin()
#
# Write the BEGIN chart BIFF record to indicate the start of a sub stream.
#
sub _store_begin {

    my $self = shift;

    my $record = 0x1033;    # Record identifier.
    my $length = 0x0000;    # Number of bytes to follow.

    my $header = pack 'vv', $record, $length;

    $self->_append( $header );
}


###############################################################################
#
# _store_catserrange()
#
# Write the CATSERRANGE chart BIFF record.
#
sub _store_catserrange {

    my $self = shift;

    my $record   = 0x1020;    # Record identifier.
    my $length   = 0x0008;    # Number of bytes to follow.
    my $catCross = 0x0001;    # Value/category crossing.
    my $catLabel = 0x0001;    # Frequency of labels.
    my $catMark  = 0x0001;    # Frequency of ticks.
    my $grbit    = 0x0001;    # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $catCross;
    $data .= pack 'v', $catLabel;
    $data .= pack 'v', $catMark;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_chart()
#
# Write the CHART BIFF record. This indicates the start of the chart sub-stream
# and contains dimensions of the chart on the display. Units are in 1/72 inch
# and are 2 byte integer with 2 byte fraction.
#
sub _store_chart {

    my $self = shift;

    my $record = 0x1002;    # Record identifier.
    my $length = 0x0010;    # Number of bytes to follow.
    my $x_pos  = $_[0];     # X pos of top left corner.
    my $y_pos  = $_[1];     # Y pos of top left corner.
    my $dx     = $_[2];     # X size.
    my $dy     = $_[3];     # Y size.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $x_pos;
    $data .= pack 'V', $y_pos;
    $data .= pack 'V', $dx;
    $data .= pack 'V', $dy;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_chartformat()
#
# Write the CHARTFORMAT chart BIFF record. The parent record for formatting
# of a chart group.
#
sub _store_chartformat {

    my $self = shift;

    my $record    = 0x1014;        # Record identifier.
    my $length    = 0x0014;        # Number of bytes to follow.
    my $reserved1 = 0x00000000;    # Reserved.
    my $reserved2 = 0x00000000;    # Reserved.
    my $reserved3 = 0x00000000;    # Reserved.
    my $reserved4 = 0x00000000;    # Reserved.
    my $grbit     = $_[0] || 0;    # Option flags.
    my $icrt      = 0x0000;        # Drawing order.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $reserved1;
    $data .= pack 'V', $reserved2;
    $data .= pack 'V', $reserved3;
    $data .= pack 'V', $reserved4;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $icrt;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_chartline()
#
# Write the CHARTLINE chart BIFF record.
#
sub _store_chartline {

    my $self = shift;

    my $record = 0x101C;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $type   = 0x0001;    # Drop/hi-lo line type.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $type;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_charttext()
#
# Write the TEXT chart BIFF record.
#
sub _store_charttext {

    my $self = shift;

    my $record           = 0x1025;        # Record identifier.
    my $length           = 0x0020;        # Number of bytes to follow.
    my $horz_align       = 0x02;          # Horizontal alignment.
    my $vert_align       = 0x02;          # Vertical alignment.
    my $bg_mode          = 0x0001;        # Background display.
    my $text_color_rgb   = 0x00000000;    # Text RGB colour.
    my $text_x           = 0xFFFFFF46;    # Text x-pos.
    my $text_y           = 0xFFFFFF06;    # Text y-pos.
    my $text_dx          = 0x00000000;    # Width.
    my $text_dy          = 0x00000000;    # Height.
    my $grbit1           = 0x00B1;        # Options
    my $text_color_index = 0x004D;        # Auto Colour.
    my $grbit2           = 0x0000;        # Data label placement.
    my $rotation         = 0x0000;        # Text rotation.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $horz_align;
    $data .= pack 'C', $vert_align;
    $data .= pack 'v', $bg_mode;
    $data .= pack 'V', $text_color_rgb;
    $data .= pack 'V', $text_x;
    $data .= pack 'V', $text_y;
    $data .= pack 'V', $text_dx;
    $data .= pack 'V', $text_dy;
    $data .= pack 'v', $grbit1;
    $data .= pack 'v', $text_color_index;
    $data .= pack 'v', $grbit2;
    $data .= pack 'v', $rotation;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_dataformat()
#
# Write the DATAFORMAT chart BIFF record. This record specifies the series
# that the subsequent sub stream refers to.
#
sub _store_dataformat {

    my $self = shift;

    my $record        = 0x1006;    # Record identifier.
    my $length        = 0x0008;    # Number of bytes to follow.
    my $series_index  = $_[0];     # Series index.
    my $series_number = $_[1];     # Series number. (Same as index).
    my $point_number  = $_[2];     # Point number.
    my $grbit         = 0x0000;    # Format flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $point_number;
    $data .= pack 'v', $series_index;
    $data .= pack 'v', $series_number;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_defaulttext()
#
# Write the DEFAULTTEXT chart BIFF record. Identifier for subsequent TEXT
# record.
#
sub _store_defaulttext {

    my $self = shift;

    my $record = 0x1024;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $type   = 0x0002;    # Type.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $type;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_dropbar()
#
# Write the DROPBAR chart BIFF record.
#
sub _store_dropbar {

    my $self = shift;

    my $record      = 0x103D;    # Record identifier.
    my $length      = 0x0002;    # Number of bytes to follow.
    my $percent_gap = 0x0096;    # Drop bar width gap (%).

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $percent_gap;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_end()
#
# Write the END chart BIFF record to indicate the end of a sub stream.
#
sub _store_end {

    my $self = shift;

    my $record = 0x1034;    # Record identifier.
    my $length = 0x0000;    # Number of bytes to follow.

    my $header = pack 'vv', $record, $length;

    $self->_append( $header );
}


###############################################################################
#
# _store_fbi()
#
# Write the FBI chart BIFF record. Specifies the font information at the time
# it was applied to the chart.
#
sub _store_fbi {

    my $self = shift;

    my $record       = 0x1060;        # Record identifier.
    my $length       = 0x000A;        # Number of bytes to follow.
    my $index        = $_[0];         # Font index.
    my $height       = $_[1] * 20;    # Default font height in twips.
    my $width_basis  = $_[2];         # Width basis, in twips.
    my $height_basis = $_[3];         # Height basis, in twips.
    my $scale_basis  = $_[4];         # Scale by chart area or plot area.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $width_basis;
    $data .= pack 'v', $height_basis;
    $data .= pack 'v', $height;
    $data .= pack 'v', $scale_basis;
    $data .= pack 'v', $index;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_fontx()
#
# Write the FONTX chart BIFF record which contains the index of the FONT
# record in the Workbook.
#
sub _store_fontx {

    my $self = shift;

    my $record = 0x1026;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $index  = $_[0];     # Font index.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $index;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_frame()
#
# Write the FRAME chart BIFF record.
#
sub _store_frame {

    my $self = shift;

    my $record     = 0x1032;    # Record identifier.
    my $length     = 0x0004;    # Number of bytes to follow.
    my $frame_type = $_[0];     # Frame type.
    my $grbit      = $_[1];     # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $frame_type;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_legend()
#
# Write the LEGEND chart BIFF record. The Marcus Horan method.
#
sub _store_legend {

    my $self = shift;

    my $record   = 0x1015;    # Record identifier.
    my $length   = 0x0014;    # Number of bytes to follow.
    my $x        = $_[0];     # X-position.
    my $y        = $_[1];     # Y-position.
    my $width    = $_[2];     # Width.
    my $height   = $_[3];     # Height.
    my $wType    = $_[4];     # Type.
    my $wSpacing = $_[5];     # Spacing.
    my $grbit    = $_[6];     # Option flags.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $x;
    $data .= pack 'V', $y;
    $data .= pack 'V', $width;
    $data .= pack 'V', $height;
    $data .= pack 'C', $wType;
    $data .= pack 'C', $wSpacing;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_lineformat()
#
# Write the LINEFORMAT chart BIFF record.
#
sub _store_lineformat {

    my $self = shift;

    my $record = 0x1007;    # Record identifier.
    my $length = 0x000C;    # Number of bytes to follow.
    my $rgb    = $_[0];     # Line RGB colour.
    my $lns    = $_[1];     # Line pattern.
    my $we     = $_[2];     # Line weight.
    my $grbit  = $_[3];     # Option flags.
    my $index  = $_[4];     # Index to colour of line.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $rgb;
    $data .= pack 'v', $lns;
    $data .= pack 'v', $we;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $index;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_markerformat()
#
# Write the MARKERFORMAT chart BIFF record.
#
sub _store_markerformat {

    my $self = shift;

    my $record  = 0x1009;    # Record identifier.
    my $length  = 0x0014;    # Number of bytes to follow.
    my $rgbFore = $_[0];     # Foreground RGB color.
    my $rgbBack = $_[1];     # Background RGB color.
    my $marker  = $_[2];     # Type of marker.
    my $grbit   = $_[3];     # Format flags.
    my $icvFore = $_[4];     # Color index marker border.
    my $icvBack = $_[5];     # Color index marker fill.
    my $miSize  = $_[6];     # Size of line markers.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $rgbFore;
    $data .= pack 'V', $rgbBack;
    $data .= pack 'v', $marker;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $icvFore;
    $data .= pack 'v', $icvBack;
    $data .= pack 'V', $miSize;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_objectlink()
#
# Write the OBJECTLINK chart BIFF record.
#
sub _store_objectlink {

    my $self = shift;

    my $record      = 0x1027;    # Record identifier.
    my $length      = 0x0006;    # Number of bytes to follow.
    my $link_type   = $_[0];     # Object text link type.
    my $link_index1 = 0x0000;    # Link index 1.
    my $link_index2 = 0x0000;    # Link index 2.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $link_type;
    $data .= pack 'v', $link_index1;
    $data .= pack 'v', $link_index2;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_pieformat()
#
# Write the PIEFORMAT chart BIFF record.
#
sub _store_pieformat {

    my $self = shift;

    my $record  = 0x100B;    # Record identifier.
    my $length  = 0x0002;    # Number of bytes to follow.
    my $percent = 0x0000;    # Distance % from center.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $percent;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_plotarea()
#
# Write the PLOTAREA chart BIFF record. This indicates that the subsequent
# FRAME record belongs to a plot area.
#
sub _store_plotarea {

    my $self = shift;

    my $record = 0x1035;    # Record identifier.
    my $length = 0x0000;    # Number of bytes to follow.

    my $header = pack 'vv', $record, $length;

    $self->_append( $header );
}


###############################################################################
#
# _store_plotgrowth()
#
# Write the PLOTGROWTH chart BIFF record.
#
sub _store_plotgrowth {

    my $self = shift;

    my $record  = 0x1064;        # Record identifier.
    my $length  = 0x0008;        # Number of bytes to follow.
    my $dx_plot = 0x00010000;    # Horz growth for font scale.
    my $dy_plot = 0x00010000;    # Vert growth for font scale.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'V', $dx_plot;
    $data .= pack 'V', $dy_plot;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_pos()
#
# Write the POS chart BIFF record. Generally not required when using
# automatic positioning.
#
sub _store_pos {

    my $self = shift;

    my $record  = 0x104F;    # Record identifier.
    my $length  = 0x0014;    # Number of bytes to follow.
    my $mdTopLt = $_[0];     # Top left.
    my $mdBotRt = $_[1];     # Bottom right.
    my $x1      = $_[2];     # X coordinate.
    my $y1      = $_[3];     # Y coordinate.
    my $x2      = $_[4];     # Width.
    my $y2      = $_[5];     # Height.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $mdTopLt;
    $data .= pack 'v', $mdBotRt;
    $data .= pack 'V', $x1;
    $data .= pack 'V', $y1;
    $data .= pack 'V', $x2;
    $data .= pack 'V', $y2;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_serauxtrend()
#
# Write the SERAUXTREND chart BIFF record.
#
sub _store_serauxtrend {

    my $self = shift;

    my $record     = 0x104B;    # Record identifier.
    my $length     = 0x001C;    # Number of bytes to follow.
    my $reg_type   = $_[0];     # Regression type.
    my $poly_order = $_[1];     # Polynomial order.
    my $equation   = $_[2];     # Display equation.
    my $r_squared  = $_[3];     # Display R-squared.
    my $intercept;              # Forced intercept.
    my $forecast;               # Forecast forward.
    my $backcast;               # Forecast backward.

    # TODO. When supported, intercept needs to be NAN if not used.
    # Also need to reverse doubles.
    $intercept = pack 'H*', 'FFFFFFFF0001FFFF';
    $forecast  = pack 'H*', '0000000000000000';
    $backcast  = pack 'H*', '0000000000000000';


    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $reg_type;
    $data .= pack 'C', $poly_order;
    $data .= $intercept;
    $data .= pack 'C', $equation;
    $data .= pack 'C', $r_squared;
    $data .= $forecast;
    $data .= $backcast;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_series()
#
# Write the SERIES chart BIFF record.
#
sub _store_series {

    my $self = shift;

    my $record         = 0x1003;    # Record identifier.
    my $length         = 0x000C;    # Number of bytes to follow.
    my $category_type  = 0x0001;    # Type: category.
    my $value_type     = 0x0001;    # Type: value.
    my $category_count = $_[0];     # Num of categories.
    my $value_count    = $_[1];     # Num of values.
    my $bubble_type    = 0x0001;    # Type: bubble.
    my $bubble_count   = 0x0000;    # Num of bubble values.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $category_type;
    $data .= pack 'v', $value_type;
    $data .= pack 'v', $category_count;
    $data .= pack 'v', $value_count;
    $data .= pack 'v', $bubble_type;
    $data .= pack 'v', $bubble_count;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_seriestext()
#
# Write the SERIESTEXT chart BIFF record.
#
sub _store_seriestext {

    my $self = shift;

    my $record   = 0x100D;         # Record identifier.
    my $length   = 0x0000;         # Number of bytes to follow.
    my $id       = 0x0000;         # Text id.
    my $str      = $_[0];          # Text.
    my $encoding = $_[1];          # String encoding.
    my $cch      = length $str;    # String length.

    # Character length is num of chars not num of bytes
    $cch /= 2 if $encoding;

    # Change the UTF-16 name from BE to LE
    $str = pack 'n*', unpack 'v*', $str if $encoding;

    $length = 4 + length( $str );

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $id;
    $data .= pack 'C', $cch;
    $data .= pack 'C', $encoding;

    $self->_append( $header, $data, $str );
}


###############################################################################
#
# _store_serparent()
#
# Write the SERPARENT chart BIFF record.
#
sub _store_serparent {

    my $self = shift;

    my $record = 0x104A;    # Record identifier.
    my $length = 0x0002;    # Number of bytes to follow.
    my $series = $_[0];     # Series parent.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $series;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_sertocrt()
#
# Write the SERTOCRT chart BIFF record to indicate the chart group index.
#
sub _store_sertocrt {

    my $self = shift;

    my $record     = 0x1045;    # Record identifier.
    my $length     = 0x0002;    # Number of bytes to follow.
    my $chartgroup = 0x0000;    # Chart group index.

    my $header = pack 'vv', $record, $length;
    my $data = pack 'v', $chartgroup;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_shtprops()
#
# Write the SHTPROPS chart BIFF record.
#
sub _store_shtprops {

    my $self = shift;

    my $record      = 0x1044;    # Record identifier.
    my $length      = 0x0004;    # Number of bytes to follow.
    my $grbit       = 0x000E;    # Option flags.
    my $empty_cells = 0x0000;    # Empty cell handling.

    $grbit = 0x000A if $self->{_embedded};

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $empty_cells;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_text()
#
# Write the TEXT chart BIFF record.
#
sub _store_text {

    my $self = shift;

    my $record   = 0x1025;           # Record identifier.
    my $length   = 0x0020;           # Number of bytes to follow.
    my $at       = 0x02;             # Horizontal alignment.
    my $vat      = 0x02;             # Vertical alignment.
    my $wBkgMode = 0x0001;           # Background display.
    my $rgbText  = 0x0000;           # Text RGB colour.
    my $x        = $_[0];            # Text x-pos.
    my $y        = $_[1];            # Text y-pos.
    my $dx       = $_[2];            # Width.
    my $dy       = $_[3];            # Height.
    my $grbit1   = $_[4];            # Option flags.
    my $icvText  = 0x004D;           # Auto Colour.
    my $grbit2   = $_[5];            # Show legend.
    my $rotation = $_[6] || 0x00;    # Show value.


    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $at;
    $data .= pack 'C', $vat;
    $data .= pack 'v', $wBkgMode;
    $data .= pack 'V', $rgbText;
    $data .= pack 'V', $x;
    $data .= pack 'V', $y;
    $data .= pack 'V', $dx;
    $data .= pack 'V', $dy;
    $data .= pack 'v', $grbit1;
    $data .= pack 'v', $icvText;
    $data .= pack 'v', $grbit2;
    $data .= pack 'v', $rotation;

    $self->_append( $header, $data );
}

###############################################################################
#
# _store_tick()
#
# Write the TICK chart BIFF record.
#
sub _store_tick {

    my $self = shift;

    my $record    = 0x101E;        # Record identifier.
    my $length    = 0x001E;        # Number of bytes to follow.
    my $tktMajor  = 0x02;          # Type of major tick mark.
    my $tktMinor  = 0x00;          # Type of minor tick mark.
    my $tlt       = 0x03;          # Tick label position.
    my $wBkgMode  = 0x01;          # Background mode.
    my $rgb       = 0x00000000;    # Tick-label RGB colour.
    my $reserved1 = 0x00000000;    # Reserved.
    my $reserved2 = 0x00000000;    # Reserved.
    my $reserved3 = 0x00000000;    # Reserved.
    my $reserved4 = 0x00000000;    # Reserved.
    my $grbit     = 0x0023;        # Option flags.
    my $index     = 0x004D;        # Colour index.
    my $reserved5 = 0x0000;        # Reserved.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'C', $tktMajor;
    $data .= pack 'C', $tktMinor;
    $data .= pack 'C', $tlt;
    $data .= pack 'C', $wBkgMode;
    $data .= pack 'V', $rgb;
    $data .= pack 'V', $reserved1;
    $data .= pack 'V', $reserved2;
    $data .= pack 'V', $reserved3;
    $data .= pack 'V', $reserved4;
    $data .= pack 'v', $grbit;
    $data .= pack 'v', $index;
    $data .= pack 'v', $reserved5;

    $self->_append( $header, $data );
}


###############################################################################
#
# _store_valuerange()
#
# Write the VALUERANGE chart BIFF record.
#
sub _store_valuerange {

    my $self = shift;

    my $record   = 0x101F;        # Record identifier.
    my $length   = 0x002A;        # Number of bytes to follow.
    my $numMin   = 0x00000000;    # Minimum value on axis.
    my $numMax   = 0x00000000;    # Maximum value on axis.
    my $numMajor = 0x00000000;    # Value of major increment.
    my $numMinor = 0x00000000;    # Value of minor increment.
    my $numCross = 0x00000000;    # Value where category axis crosses.
    my $grbit    = 0x011F;        # Format flags.

    # TODO. Reverse doubles when they are handled.

    my $header = pack 'vv', $record, $length;
    my $data = '';
    $data .= pack 'd', $numMin;
    $data .= pack 'd', $numMax;
    $data .= pack 'd', $numMajor;
    $data .= pack 'd', $numMinor;
    $data .= pack 'd', $numCross;
    $data .= pack 'v', $grbit;

    $self->_append( $header, $data );
}


###############################################################################
#
# Config data.
#
###############################################################################


###############################################################################
#
# _set_default_properties()
#
# Setup the default properties for a chart.
#
sub _set_default_properties {

    my $self = shift;

    $self->{_legend} = {
        _visible  => 1,
        _position => 0,
        _vertical => 0,
    };

    $self->{_chartarea} = {
        _visible          => 0,
        _fg_color_index   => 0x4E,
        _fg_color_rgb     => 0xFFFFFF,
        _bg_color_index   => 0x4D,
        _bg_color_rgb     => 0x000000,
        _area_pattern     => 0x0000,
        _area_options     => 0x0000,
        _line_pattern     => 0x0005,
        _line_weight      => 0xFFFF,
        _line_color_index => 0x4D,
        _line_color_rgb   => 0x000000,
        _line_options     => 0x0008,
    };

    $self->{_plotarea} = {
        _visible          => 1,
        _fg_color_index   => 0x16,
        _fg_color_rgb     => 0xC0C0C0,
        _bg_color_index   => 0x4F,
        _bg_color_rgb     => 0x000000,
        _area_pattern     => 0x0001,
        _area_options     => 0x0000,
        _line_pattern     => 0x0000,
        _line_weight      => 0x0000,
        _line_color_index => 0x17,
        _line_color_rgb   => 0x808080,
        _line_options     => 0x0000,
    };
}


###############################################################################
#
# _set_default_config_data()
#
# Setup the default configuration data for a chart.
#
sub _set_default_config_data {

    my $self = shift;

    #<<< Perltidy ignore this.
    $self->{_config} = {
        _axisparent      => [ 0, 0x00F8, 0x01F5, 0x0E7F, 0x0B36              ],
        _axisparent_pos  => [ 2, 2, 0x008C, 0x01AA, 0x0EEA, 0x0C52           ],
        _chart           => [ 0x0000, 0x0000, 0x02DD51E0, 0x01C2B838         ],
        _font_numbers    => [ 5, 10, 0x38B8, 0x22A1, 0x0000                  ],
        _font_series     => [ 6, 10, 0x38B8, 0x22A1, 0x0001                  ],
        _font_title      => [ 7, 12, 0x38B8, 0x22A1, 0x0000                  ],
        _font_axes       => [ 8, 10, 0x38B8, 0x22A1, 0x0001                  ],
        _legend          => [ 0x05F9, 0x0EE9, 0x047D, 0x9C, 0x00, 0x01, 0x0F ],
        _legend_pos      => [ 5, 2, 0x05F9, 0x0EE9, 0, 0                     ],
        _legend_text     => [ 0xFFFFFF46, 0xFFFFFF06, 0, 0, 0x00B1, 0x0000   ],
        _legend_text_pos => [ 2, 2, 0, 0, 0, 0                               ],
        _series_text     => [ 0xFFFFFF46, 0xFFFFFF06, 0, 0, 0x00B1, 0x1020   ],
        _series_text_pos => [ 2, 2, 0, 0, 0, 0                               ],
        _title_text      => [ 0x06E4, 0x0051, 0x01DB, 0x00C4, 0x0081, 0x1030 ],
        _title_text_pos  => [ 2, 2, 0, 0, 0x73, 0x1D                         ],
        _x_axis_text     => [ 0x07E1, 0x0DFC, 0xB2, 0x9C, 0x0081, 0x0000     ],
        _x_axis_text_pos => [ 2, 2, 0, 0,  0x2B,  0x17                       ],
        _y_axis_text     => [ 0x002D, 0x06AA, 0x5F, 0x1CC, 0x0281, 0x00, 90  ],
        _y_axis_text_pos => [ 2, 2, 0, 0, 0x17,  0x44                        ],
    }; #>>>


}


###############################################################################
#
# _set_embedded_config_data()
#
# Setup the default configuration data for an embedded chart.
#
sub _set_embedded_config_data {

    my $self = shift;

    $self->{_embedded} = 1;

    $self->{_chartarea} = {
        _visible          => 1,
        _fg_color_index   => 0x4E,
        _fg_color_rgb     => 0xFFFFFF,
        _bg_color_index   => 0x4D,
        _bg_color_rgb     => 0x000000,
        _area_pattern     => 0x0001,
        _area_options     => 0x0001,
        _line_pattern     => 0x0000,
        _line_weight      => 0x0000,
        _line_color_index => 0x4D,
        _line_color_rgb   => 0x000000,
        _line_options     => 0x0009,
    };


    #<<< Perltidy ignore this.
    $self->{_config} = {
        _axisparent      => [ 0, 0x01D8, 0x031D, 0x0D79, 0x07E9              ],
        _axisparent_pos  => [ 2, 2, 0x010C, 0x0292, 0x0E46, 0x09FD           ],
        _chart           => [ 0x0000, 0x0000, 0x01847FE8, 0x00F47FE8         ],
        _font_numbers    => [ 5, 10, 0x1DC4, 0x1284, 0x0000                  ],
        _font_series     => [ 6, 10, 0x1DC4, 0x1284, 0x0001                  ],
        _font_title      => [ 7, 12, 0x1DC4, 0x1284, 0x0000                  ],
        _font_axes       => [ 8, 10, 0x1DC4, 0x1284, 0x0001                  ],
        _legend          => [ 0x044E, 0x0E4A, 0x088D, 0x0123, 0x0, 0x1, 0xF  ],
        _legend_pos      => [ 5, 2, 0x044E, 0x0E4A, 0, 0                     ],
        _legend_text     => [ 0xFFFFFFD9, 0xFFFFFFC1, 0, 0, 0x00B1, 0x0000   ],
        _legend_text_pos => [ 2, 2, 0, 0, 0, 0                               ],
        _series_text     => [ 0xFFFFFFD9, 0xFFFFFFC1, 0, 0, 0x00B1, 0x1020   ],
        _series_text_pos => [ 2, 2, 0, 0, 0, 0                               ],
        _title_text      => [ 0x060F, 0x004C, 0x038A, 0x016F, 0x0081, 0x1030 ],
        _title_text_pos  => [ 2, 2, 0, 0, 0x73, 0x1D                         ],
        _x_axis_text     => [ 0x07EF, 0x0C8F, 0x153, 0x123, 0x81, 0x00       ],
        _x_axis_text_pos => [ 2, 2, 0, 0, 0x2B, 0x17                         ],
        _y_axis_text     => [ 0x0057, 0x0564, 0xB5, 0x035D, 0x0281, 0x00, 90 ],
        _y_axis_text_pos => [ 2, 2, 0, 0, 0x17, 0x44                         ],
    }; #>>>
}



1;


__END__


=head1 NAME

Chart - A writer class for Excel Charts.

=head1 SYNOPSIS

To create a simple Excel file with a chart using Spreadsheet::WriteExcel:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new( 'chart.xls' );
    my $worksheet = $workbook->add_worksheet();

    my $chart     = $workbook->add_chart( type => 'column' );

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

The C<Chart> module is an abstract base class for modules that implement charts in L<Spreadsheet::WriteExcel>. The information below is applicable to all of the available subclasses.

The C<Chart> module isn't used directly, a chart object is created via the Workbook C<add_chart()> method where the chart type is specified:

    my $chart = $workbook->add_chart( type => 'column' );

Currently the supported chart types are:

=over

=item * C<area>: Creates an Area (filled line) style chart. See L<Spreadsheet::WriteExcel::Chart::Area>.

=item * C<bar>: Creates a Bar style (transposed histogram) chart. See L<Spreadsheet::WriteExcel::Chart::Bar>.

=item * C<column>: Creates a column style (histogram) chart. See L<Spreadsheet::WriteExcel::Chart::Column>.

=item * C<line>: Creates a Line style chart. See L<Spreadsheet::WriteExcel::Chart::Line>.

=item * C<pie>: Creates an Pie style chart. See L<Spreadsheet::WriteExcel::Chart::Pie>.

=item * C<scatter>: Creates an Scatter style chart. See L<Spreadsheet::WriteExcel::Chart::Scatter>.

=item * C<stock>: Creates an Stock style chart. See L<Spreadsheet::WriteExcel::Chart::Stock>.

=back

More charts and sub-types will be supported in time. See the L</TODO> section.

Methods that are common to all chart types are documented below.

=head1 CHART METHODS

=head2 add_series()

In an Excel chart a "series" is a collection of information such as values, x-axis labels and the name that define which data is plotted. These settings are displayed when you select the C<< Chart -> Source Data... >> menu option.

With a Spreadsheet::WriteExcel chart object the C<add_series()> method is used to set the properties for a series:

    $chart->add_series(
        categories    => '=Sheet1!$A$2:$A$10',
        values        => '=Sheet1!$B$2:$B$10',
        name          => 'Series name',
        name_formula  => '=Sheet1!$B$1',
    );

The properties that can be set are:

=over

=item * C<values>

This is the most important property of a series and must be set for every chart object. It links the chart with the worksheet data that it displays. Note the format that should be used for the formula. See L</Working with Cell Ranges>.

=item * C<categories>

This sets the chart category labels. The category is more or less the same as the X-axis. In most chart types the C<categories> property is optional and the chart will just assume a sequential series from C<1 .. n>.

=item * C<name>

Set the name for the series. The name is displayed in the chart legend and in the formula bar. The name property is optional and if it isn't supplied will default to C<Series 1 .. n>.

=item * C<name_formula>

Optional, can be used to link the name to a worksheet cell. See L</Chart names and links>.

=back

You can add more than one series to a chart, in fact some chart types such as C<stock> require it. The series numbering and order in the final chart is the same as the order in which that are added.

    # Add the first series.
    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
        name       => 'Test data series 1',
    );

    # Add another series. Category is the same but values are different.
    $chart->add_series(
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
        name       => 'Test data series 2',
    );



=head2 set_x_axis()

The C<set_x_axis()> method is used to set properties of the X axis.

    $chart->set_x_axis( name => 'Sample length (m)' );

The properties that can be set are:

=over

=item * C<name>

Set the name (title or caption) for the axis. The name is displayed below the X axis. This property is optional. The default is to have no axis name.

=item * C<name_formula>

Optional, can be used to link the name to a worksheet cell. See L</Chart names and links>.

=back

Additional axis properties such as range, divisions and ticks will be made available in later releases. See the L</TODO> section.


=head2 set_y_axis()

The C<set_y_axis()> method is used to set properties of the Y axis.

    $chart->set_y_axis( name => 'Sample weight (kg)' );

The properties that can be set are:

=over

=item * C<name>

Set the name (title or caption) for the axis. The name is displayed to the left of the Y axis. This property is optional. The default is to have no axis name.

=item * C<name_formula>

Optional, can be used to link the name to a worksheet cell. See L</Chart names and links>.

=back

Additional axis properties such as range, divisions and ticks will be made available in later releases. See the L</TODO> section.

=head2 set_title()

The C<set_title()> method is used to set properties of the chart title.

    $chart->set_title( name => 'Year End Results' );

The properties that can be set are:

=over

=item * C<name>

Set the name (title) for the chart. The name is displayed above the chart. This property is optional. The default is to have no chart title.

=item * C<name_formula>

Optional, can be used to link the name to a worksheet cell. See L</Chart names and links>.

=back


=head2 set_legend()

The C<set_legend()> method is used to set properties of the chart legend.

    $chart->set_legend( position => 'none' );

The properties that can be set are:

=over

=item * C<position>

Set the position of the chart legend.

    $chart->set_legend( position => 'none' );

The default legend position is C<bottom>. The currently supported chart positions are:

    none
    bottom

The other legend positions will be added soon.

=back


=head2 set_chartarea()

The C<set_chartarea()> method is used to set the properties of the chart area. In Excel the chart area is the background area behind the chart.

The properties that can be set are:

=over

=item * C<color>

Set the colour of the chart area. The Excel default chart area color is 'white', index 9. See L</Chart object colours>.

=item * C<line_color>

Set the colour of the chart area border line. The Excel default border line colour is 'black', index 9.  See L</Chart object colours>.

=item * C<line_pattern>

Set the pattern of the of the chart area border line. The Excel default pattern is 'none', index 0 for a chart sheet and 'solid', index 1, for an embedded chart. See L</Chart line patterns>.

=item * C<line_weight>

Set the weight of the of the chart area border line. The Excel default weight is 'narrow', index 2. See L</Chart line weights>.

=back

Here is an example of setting several properties:

    $chart->set_chartarea(
        color        => 'red',
        line_color   => 'black',
        line_pattern => 2,
        line_weight  => 3,
    );

Note, for chart sheets the chart area border is off by default. For embedded charts this is on by default.

=head2 set_plotarea()

The C<set_plotarea()> method is used to set properties of the plot area of a chart. In Excel the plot area is the area between the axes on which the chart series are plotted.

The properties that can be set are:

=over

=item * C<visible>

Set the visibility of the plot area. The default is 1 for visible. Set to 0 to hide the plot area and have the same colour as the background chart area.

=item * C<color>

Set the colour of the plot area. The Excel default plot area color is 'silver', index 23. See L</Chart object colours>.

=item * C<line_color>

Set the colour of the plot area border line. The Excel default border line colour is 'gray', index 22. See L</Chart object colours>.

=item * C<line_pattern>

Set the pattern of the of the plot area border line. The Excel default pattern is 'solid', index 1. See L</Chart line patterns>.

=item * C<line_weight>

Set the weight of the of the plot area border line. The Excel default weight is 'narrow', index 2. See L</Chart line weights>.

=back

Here is an example of setting several properties:

    $chart->set_plotarea(
        color        => 'red',
        line_color   => 'black',
        line_pattern => 2,
        line_weight  => 3,
    );



=head1 WORKSHEET METHODS

In Excel a chart sheet (i.e, a chart that isn't embedded) shares properties with data worksheets such as tab selection, headers, footers, margins and print properties.

In Spreadsheet::WriteExcel you can set chart sheet properties using the same methods that are used for Worksheet objects.

The following Worksheet methods are also available through a non-embedded Chart object:

    get_name()
    activate()
    select()
    hide()
    set_first_sheet()
    protect()
    set_zoom()
    set_tab_color()

    set_landscape()
    set_portrait()
    set_paper()
    set_margins()
    set_header()
    set_footer()

See L<Spreadsheet::WriteExcel> for a detailed explanation of these methods.

=head1 EXAMPLE

Here is a complete example that demonstrates some of the available features when creating a chart.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::WriteExcel;

    my $workbook  = Spreadsheet::WriteExcel->new( 'chart_area.xls' );
    my $worksheet = $workbook->add_worksheet();
    my $bold      = $workbook->add_format( bold => 1 );

    # Add the worksheet data that the charts will refer to.
    my $headings = [ 'Number', 'Sample 1', 'Sample 2' ];
    my $data = [
        [ 2, 3, 4, 5, 6, 7 ],
        [ 1, 4, 5, 2, 1, 5 ],
        [ 3, 6, 7, 5, 4, 3 ],
    ];

    $worksheet->write( 'A1', $headings, $bold );
    $worksheet->write( 'A2', $data );

    # Create a new chart object. In this case an embedded chart.
    my $chart = $workbook->add_chart( type => 'area', embedded => 1 );

    # Configure the first series. (Sample 1)
    $chart->add_series(
        name       => 'Sample 1',
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$B$2:$B$7',
    );

    # Configure the second series. (Sample 2)
    $chart->add_series(
        name       => 'Sample 2',
        categories => '=Sheet1!$A$2:$A$7',
        values     => '=Sheet1!$C$2:$C$7',
    );

    # Add a chart title and some axis labels.
    $chart->set_title ( name => 'Results of sample analysis' );
    $chart->set_x_axis( name => 'Test number' );
    $chart->set_y_axis( name => 'Sample length (cm)' );

    # Insert the chart into the worksheet (with an offset).
    $worksheet->insert_chart( 'D2', $chart, 25, 10 );

    __END__


=begin html

<p>This will produce a chart that looks like this:</p>

<p><center><img src="http://homepage.eircom.net/~jmcnamara/perl/images/area1.jpg" width="527" height="320" alt="Chart example." /></center></p>

=end html


=head1 Chart object colours

Many of the chart objects supported by Spreadsheet::WriteExcl allow the default colours to be changed. Excel provides a palette of 56 colours and in Spreadsheet::WriteExcel these colours are accessed via their palette index in the range 8..63.

The most commonly used colours can be accessed by name or index.

    black   =>   8,    green    =>  17,    navy     =>  18,
    white   =>   9,    orange   =>  53,    pink     =>  33,
    red     =>  10,    gray     =>  23,    purple   =>  20,
    blue    =>  12,    lime     =>  11,    silver   =>  22,
    yellow  =>  13,    cyan     =>  15,
    brown   =>  16,    magenta  =>  14,

For example the following are equivalent.

    $chart->set_plotarea( color => 10    );
    $chart->set_plotarea( color => 'red' );

The colour palette is shown in C<palette.html> in the C<docs> directory  of the distro. An Excel version of the palette can be generated using C<colors.pl> in the C<examples> directory.

User defined colours can be set using the C<set_custom_color()> workbook method. This and other aspects of using colours are discussed in the "Colours in Excel" section of the main Spreadsheet::WriteExcel documentation: L<http://search.cpan.org/dist/Spreadsheet-WriteExcel/lib/Spreadsheet/WriteExcel.pm#COLOURS_IN_EXCEL>.

=head1 Chart line patterns

Chart lines patterns can be set using either an index or a name:

    $chart->set_plotarea( weight => 2      );
    $chart->set_plotarea( weight => 'dash' );

Chart lines have 9 possible patterns are follows:

    'none'         => 0,
    'solid'        => 1,
    'dash'         => 2,
    'dot'          => 3,
    'dash-dot'     => 4,
    'dash-dot-dot' => 5,
    'medium-gray'  => 6,
    'dark-gray'    => 7,
    'light-gray'   => 8,

The patterns 1-8 are shown in order in the drop down dialog boxes in Excel. The default pattern is 'solid', index 1.


=head1 Chart line weights

Chart lines weights can be set using either an index or a name:

    $chart->set_plotarea( weight => 1          );
    $chart->set_plotarea( weight => 'hairline' );

Chart lines have 4 possible weights are follows:

    'hairline' => 1,
    'narrow'   => 2,
    'medium'   => 3,
    'wide'     => 4,

The weights 1-4 are shown in order in the drop down dialog boxes in Excel. The default weight is 'narrow', index 2.


=head1 Chart names and links

The C<add_series())>, C<set_x_axis()>, C<set_y_axis()> and C<set_title()> methods all support a C<name> property. In general these names can be either a static string or a link to a worksheet cell. If you choose to use the C<name_formula> property to specify a link then you should also the C<name> property. This isn't strictly required by Excel but some third party applications expect it to be present.

    $chart->set_title(
        name          => 'Year End Results',
        name_formula  => '=Sheet1!$C$1',
    );

These links should be used sparingly since they aren't commonly used in Excel charts.


=head1 Chart names and Unicode

The C<add_series())>, C<set_x_axis()>, C<set_y_axis()> and C<set_title()> methods all support a C<name> property. These names can be UTF8 strings if you are using perl 5.8+.


    # perl 5.8+ example:
    my $smiley = "\x{263A}";

    $chart->set_title( name => "Best. Results. Ever! $smiley" );

For older perls you write Unicode strings as UTF-16BE by adding a C<name_encoding> property:

    # perl 5.005 example:
    my $utf16be_name = pack 'n', 0x263A;

    $chart->set_title(
        name          => $utf16be_name,
        name_encoding => 1,
    );

This methodology is explained in the "UNICODE IN EXCEL" section of L<Spreadsheet::WriteExcel> but is semi-deprecated. If you are using Unicode the easiest option is to just use UTF8 in perl 5.8+.


=head1 Working with Cell Ranges


In the section on C<add_series()> it was noted that the series must be defined using a range formula:

    $chart->add_series( values => '=Sheet1!$B$2:$B$10' );

The worksheet name must be specified (even for embedded charts) and the cell references must be "absolute" references, i.e., they must contain C<$> signs. This is the format that is required by Excel for chart references.

Since it isn't very convenient to work with this type of string programmatically the L<Spreadsheet::WriteExcel::Utility> module, which is included with Spreadsheet::WriteExcel, provides a function called C<xl_range_formula()> to convert from zero based row and column cell references to an A1 style formula string.

The syntax is:

    xl_range_formula($sheetname, $row_1, $row_2, $col_1, $col_2)

If you include it in your program, using the standard import syntax, you can use the function as follows:


    # Include the Utility module or just the function you need.
    use Spreadsheet::WriteExcel::Utility qw( xl_range_formula );
    ...

    # Then use it as required.
    $chart->add_series(
        categories    => xl_range_formula( 'Sheet1', 1, 9, 0, 0 ),
        values        => xl_range_formula( 'Sheet1', 1, 9, 1, 1 ),
    );

    # Which is the same as:
    $chart->add_series(
        categories    => '=Sheet1!$A$2:$A$10',
        values        => '=Sheet1!$B$2:$B$10',
    );

See L<Spreadsheet::WriteExcel::Utility> for more details.


=head1 TODO

Charts in Spreadsheet::WriteExcel are a work in progress. More chart types and features will be added in time. Please be patient. Even a small feature can take a week or more to implement, test and document.

Features that are on the TODO list and will be added are:

=over

=item *  Chart sub-types.

=item * Colours and formatting options. For now you will have to make do with the default Excel colours and formats.

=item * Axis controls, gridlines.

=item * 3D charts.

=item * Embedded data in charts for third party application support. See Known Issues.

=item * Additional chart types such as Bubble and Radar. Send an email if you are interested in other types and they will be added to the queue.

=back

If you are interested in sponsoring a feature let me know.

=head1 KNOWN ISSUES

=over

=item * Currently charts don't contain embedded data from which the charts can be rendered. Excel and most other third party applications ignore this and read the data via the links that have been specified. However, some applications may complain or not render charts correctly. The preview option in Mac OS X is an known example. This will be fixed in a later release.

=item * When there are several charts with titles set in a workbook some of the titles may display at a font size of 10 instead of the default 12 until another chart with the title set is viewed.

=item * Stock (and other) charts should have the X-axis dates aligned at an angle for clarity. This will be fixed at a later stage.

=back


=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.

