package Spreadsheet::ParseXLSX;
our $AUTHORITY = 'cpan:DOY';
$Spreadsheet::ParseXLSX::VERSION = '0.27';
use strict;
use warnings;
use 5.010;
# ABSTRACT: parse XLSX files

use Archive::Zip;
use Graphics::ColorUtils 'rgb2hls', 'hls2rgb';
use Scalar::Util 'openhandle';
use Spreadsheet::ParseExcel 0.61;
use XML::Twig;

use Spreadsheet::ParseXLSX::Decryptor;



sub new {
    my $class = shift;
    my (%args) = @_;

    my $self = bless {}, $class;
    $self->{Password} = $args{Password} if defined $args{Password};

    return $self;
}


sub parse {
    my $self = shift;
    my ($file, $formatter) = @_;

    my $zip = Archive::Zip->new;
    my $workbook = Spreadsheet::ParseExcel::Workbook->new;

    if ($self->_check_signature($file)) {
        my $decrypted_file = Spreadsheet::ParseXLSX::Decryptor->open(
            $file,
            $self->{Password}
        );
        $file = $decrypted_file if $decrypted_file;
    }

    if (openhandle($file)) {
        bless $file, 'IO::File' if ref($file) eq 'GLOB'; # sigh
        my $fh = ref($file) eq 'File::Temp'
            ? IO::File->new("<&=" . fileno($file))
            : $file;
        $zip->readFromFileHandle($fh) == Archive::Zip::AZ_OK
            or die "Can't open filehandle as a zip file";
        $workbook->{File} = undef;
        $workbook->{__tempfile} = $file;
    }
    elsif (ref($file) eq 'SCALAR') {
        open my $fh, '+<', $file
            or die "Can't create filehandle from memory data";
        $zip->readFromFileHandle($fh) == Archive::Zip::AZ_OK
            or die "Can't open scalar ref as a zip file";
        $workbook->{File} = undef;
    }
    elsif (!ref($file)) {
        $zip->read($file) == Archive::Zip::AZ_OK
            or die "Can't open file '$file' as a zip file";
        $workbook->{File} = $file;
    }
    else {
        die "Argument to 'new' must be a filename, open filehandle, or scalar ref";
    }

    return $self->_parse_workbook($zip, $workbook, $formatter);
}

sub _check_signature {
    my $self = shift;
    my ($file) = @_;

    my $signature = '';
    if (openhandle($file)) {
        bless $file, 'IO::File' if ref($file) eq 'GLOB'; # sigh
        $file->read($signature, 2);
        $file->seek(-2, IO::File::SEEK_CUR);
    }
    elsif (ref($file) eq 'SCALAR') {
        $signature = substr($$file, 0, 2);
    }
    elsif (!ref($file)) {
        my $fh = IO::File->new($file, 'r');
        $fh->read($signature, 2);
        $fh->close;
    }

    return $signature eq "\xd0\xcf";
}

sub _parse_workbook {
    my $self = shift;
    my ($zip, $workbook, $formatter) = @_;

    my $files = $self->_extract_files($zip);

    my ($version)    = $files->{workbook}->find_nodes('//s:fileVersion');
    my ($properties) = $files->{workbook}->find_nodes('//s:workbookPr');

    if ($version) {
        $workbook->{Version} = $version->att('appName')
                             . ($version->att('lowestEdited')
                                 ? ('-' . $version->att('lowestEdited'))
                                 : (""));
    }

    $workbook->{Flg1904} = $self->_xml_boolean($properties->att('date1904'))
        if $properties;

    $workbook->{FmtClass} = $formatter || Spreadsheet::ParseExcel::FmtDefault->new;

    my $themes = $self->_parse_themes((values %{ $files->{themes} })[0]); # XXX

    $workbook->{Color} = $themes->{Color};

    my $styles = $self->_parse_styles($workbook, $files->{styles});

    $workbook->{Format}    = $styles->{Format};
    $workbook->{FormatStr} = $styles->{FormatStr};
    $workbook->{Font}      = $styles->{Font};

    if ($files->{strings}) {
        my %string_parse_data = $self->_parse_shared_strings(
            $files->{strings},
            $themes->{Color}
        );
        $workbook->{PkgStr} = $string_parse_data{PkgStr};
        $workbook->{Rich}   = $string_parse_data{Rich};
    }

    # $workbook->{StandardWidth} = ...;

    # $workbook->{Author} = ...;

    # $workbook->{PrintArea} = ...;
    # $workbook->{PrintTitle} = ...;

    my @sheets = map {
        my $idx = $_->att('rels:id');
        if ($files->{sheets}{$idx}) {
          my $sheet = Spreadsheet::ParseExcel::Worksheet->new(
              Name     => $_->att('name'),
              _Book    => $workbook,
              _SheetNo => $idx,
          );
          $sheet->{SheetHidden} = 1 if defined $_->att('state') and $_->att('state') eq 'hidden';
          $self->_parse_sheet($sheet, $files->{sheets}{$idx});
          ($sheet)
        } else {
          ()
        }
    } $files->{workbook}->find_nodes('//s:sheets/s:sheet');

    $workbook->{Worksheet}  = \@sheets;
    $workbook->{SheetCount} = scalar(@sheets);

    my ($node) = $files->{workbook}->find_nodes('//s:workbookView');
    my $selected = $node ? $node->att('activeTab') : undef;
    $workbook->{SelectedSheet} = defined($selected) ? 0+$selected : 0;

    return $workbook;
}

sub _parse_sheet {
    my $self = shift;
    my ($sheet, $sheet_file) = @_;

    $sheet->{MinRow} = 0;
    $sheet->{MinCol} = 0;
    $sheet->{MaxRow} = -1;
    $sheet->{MaxCol} = -1;
    $sheet->{Selection} = [ 0, 0 ];

    my %merged_cells;

    my @column_formats;
    my @column_widths;
    my @columns_hidden;
    my @row_heights;
    my @rows_hidden;

    my $default_row_height   = 15;
    my $default_column_width = 10;

    my %cells;
    my $row_idx = 0;

    my $sheet_xml = $self->_new_twig(
        twig_roots => {
            #XXX need a fallback here, the dimension tag is optional
            's:dimension' => sub {
                my ($twig, $dimension) = @_;

                my ($rmin, $cmin, $rmax, $cmax) = $self->_dimensions(
                    $dimension->att('ref')
                );

                $sheet->{MinRow} = $rmin;
                $sheet->{MinCol} = $cmin;
                $sheet->{MaxRow} = $rmax ? $rmax : -1;
                $sheet->{MaxCol} = $cmax ? $cmax : -1;

                $twig->purge;
            },

            's:headerFooter' => sub {
                my ($twig, $hf) = @_;

                my ($helem, $felem) = map {
                    $hf->first_child("s:$_")
                } qw(oddHeader oddFooter);
                $sheet->{Header} = $helem->text
                    if $helem;
                $sheet->{Footer} = $felem->text
                    if $felem;

                $twig->purge;
            },

            's:pageMargins' => sub {
                my ($twig, $margin) = @_;
                map {
                    my $key = "\u${_}Margin";
                    $sheet->{$key} = defined $margin->att($_)
                                    ? $margin->att($_) : 0
                } qw(left right top bottom header footer);

                $twig->purge;
            },

            's:pageSetup' => sub {
                my ($twig, $setup) = @_;
                $sheet->{Scale} = defined $setup->att('scale')
                                ? $setup->att('scale')
                                : 100;
                $sheet->{Landscape} = ($setup->att('orientation') || '') ne 'landscape';
                $sheet->{PaperSize} = defined $setup->att('paperSize')
                                    ? $setup->att('paperSize')
                                    : 1;
                $sheet->{PageStart} = $setup->att('firstPageNumber');
                $sheet->{UsePage} = $self->_xml_boolean($setup->att('useFirstPageNumber'));
                $sheet->{HorizontalDPI} = $setup->att('horizontalDpi');
                $sheet->{VerticalDPI} = $setup->att('verticalDpi');

                $twig->purge;
            },

            's:mergeCells/s:mergeCell' => sub {
                my ( $twig, $merge_area ) = @_;

                if (my $ref = $merge_area->att('ref')) {
                    my ($topleft, $bottomright) = $ref =~ /([^:]+):([^:]+)/;

                    my ($toprow, $leftcol)     = $self->_cell_to_row_col($topleft);
                    my ($bottomrow, $rightcol) = $self->_cell_to_row_col($bottomright);

                    push @{ $sheet->{MergedArea} }, [
                        $toprow, $leftcol,
                        $bottomrow, $rightcol,
                    ];
                    for my $row ($toprow .. $bottomrow) {
                        for my $col ($leftcol .. $rightcol) {
                            $merged_cells{"$row;$col"} = 1;
                        }
                    }
                }

                $twig->purge;
            },

            's:sheetFormatPr' => sub {
                my ( $twig, $format ) = @_;

                $default_row_height   = $format->att('defaultRowHeight')
                  unless defined $default_row_height;
                $default_column_width = $format->att('baseColWidth')
                  unless defined $default_column_width;

                $twig->purge;
            },

            's:col' => sub {
                my ( $twig, $col ) = @_;

                for my $colnum ($col->att('min')..$col->att('max')) {
                    $column_widths[$colnum - 1] = $col->att('width');
                    $column_formats[$colnum - 1] = $col->att('style');
                    $columns_hidden[$colnum - 1] = $self->_xml_boolean($col->att('hidden'));
                }

                $twig->purge;
            },

            's:selection' => sub {
                my ( $twig, $selection ) = @_;

                if (my $cell = $selection->att('activeCell')) {
                    $sheet->{Selection} = [ $self->_cell_to_row_col($cell) ];
                }
                elsif (my $range = $selection->att('sqref')) {
                    my ($topleft, $bottomright) = $range =~ /([^:]+):([^:]+)/;
                    $sheet->{Selection} = [
                        $self->_cell_to_row_col($topleft),
                        $self->_cell_to_row_col($bottomright),
                    ];
                }

                $twig->purge;
            },

            's:sheetPr/s:tabColor' => sub {
                my ( $twig, $tab_color ) = @_;

                $sheet->{TabColor} = $self->_color($sheet->{_Book}{Color}, $tab_color);

                $twig->purge;
            },

            's:sheetData/s:row' => sub {
                my ( $twig, $row_elt ) = @_;
                my $explicit_row_idx = $row_elt->att('r');
                $row_idx = $explicit_row_idx - 1 if defined $explicit_row_idx;

                $row_heights[$row_idx] = $row_elt->att('ht');
                $rows_hidden[$row_idx] = $self->_xml_boolean($row_elt->att('hidden'));

                my $col_idx = 0;
                for my $cell ( $row_elt->children('s:c') ){
                    my $loc = $cell->att('r');
                    my ($row, $col);
                    if ($loc) {
                        ($row, $col) = $self->_cell_to_row_col($loc);
                        if ($row != $row_idx) {
                            warn "mismatched coords: got $loc for cell in row $row_idx";
                        }
                        $col_idx = $col - 1;
                    }
                    else {
                        ($row, $col) = ($row_idx, $col_idx);
                    }
                    $sheet->{MaxRow} = $row
                        if $sheet->{MaxRow} < $row;
                    $sheet->{MaxCol} = $col
                        if $sheet->{MaxCol} < $col;
                    my $type = $cell->att('t') || 'n';
                    my $val_xml;
                    if ($type ne 'inlineStr') {
                        $val_xml = $cell->first_child('s:v');
                    }
                    elsif (defined $cell->first_child('s:is')) {
                        $val_xml = ($cell->find_nodes('.//s:t'))[0];
                    }
                    my $val = $val_xml ? $val_xml->text : undef;

                    my $long_type;
                    my $Rich;
                    if (!defined($val)) {
                        $long_type = 'Text';
                        $val = '';
                    }
                    elsif ($type eq 's') {
                        $long_type = 'Text';
                        $Rich = $sheet->{_Book}{Rich}->{$val};
                        $val  = $sheet->{_Book}{PkgStr}[$val];
                    }
                    elsif ($type eq 'n') {
                        $long_type = 'Numeric';
                        $val = defined($val) ? 0+$val : undef;
                    }
                    elsif ($type eq 'd') {
                        $long_type = 'Date';
                    }
                    elsif ($type eq 'b') {
                        $long_type = 'Text';
                        $val = $val ? "TRUE" : "FALSE";
                    }
                    elsif ($type eq 'e') {
                        $long_type = 'Text';
                    }
                    elsif ($type eq 'str' || $type eq 'inlineStr') {
                        $long_type = 'Text';
                    }
                    else {
                        die "unimplemented type $type"; # XXX
                    }

                    my $format_idx = $cell->att('s') || 0;
                    my $format = $sheet->{_Book}{Format}[$format_idx];
                    die "unknown format $format_idx" unless $format;

                    # see the list of built-in formats below in _parse_styles
                    # XXX probably should figure this out from the actual format string,
                    # but that's not entirely trivial
                    if (grep { $format->{FmtIdx} == $_ } 14..22, 45..47) {
                        $long_type = 'Date';
                    }

                    my $formula = $cell->first_child('s:f');
                    my $cell = Spreadsheet::ParseExcel::Cell->new(
                        Val      => $val,
                        Type     => $long_type,
                        Merged   => undef, # fix up later
                        Format   => $format,
                        FormatNo => $format_idx,
                        ($formula
                            ? (Formula => $formula->text)
                            : ()),
                        Rich     => $Rich,
                    );
                    $cell->{_Value} = $sheet->{_Book}{FmtClass}->ValFmt(
                        $cell, $sheet->{_Book}
                    );
                    $cells{"$row;$col"} = $cell;
                    $sheet->{Cells}[$row][$col] = $cell;
                    $col_idx++;
                }

                $twig->purge;
                $row_idx++;
            },
        }
    );

    $sheet_xml->parse( $sheet_file );

    for my $key (keys %merged_cells) {
        $cells{$key}{Merged} = 1 if $cells{$key};
    }

    if ( ! $sheet->{Cells} ){
        $sheet->{MaxRow} = $sheet->{MaxCol} = -1;
    }

    $sheet->{DefRowHeight} = 0+$default_row_height;
    $sheet->{DefColWidth} = 0+$default_column_width;
    $sheet->{RowHeight} = [
        map { defined $_ ? 0+$_ : 0+$default_row_height } @row_heights
    ];
    $sheet->{RowHidden} = \@rows_hidden;
    $sheet->{ColWidth} = [
        map { defined $_ ? 0+$_ : 0+$default_column_width } @column_widths
    ];
    $sheet->{ColFmtNo} = \@column_formats;
    $sheet->{ColHidden} = \@columns_hidden;

}

sub _get_text_and_rich_font_by_cell {
    my $self = shift;
    my ($si, $theme_colors) = @_;

    # XXX
    my %default_font_opts = (
        Height         => 12,
        Color          => '#000000',
        Name           => '',
        Bold           => 0,
        Italic         => 0,
        Underline      => 0,
        UnderlineStyle => 0,
        Strikeout      => 0,
        Super          => 0,
    );

    my $string_text = '';
    my @rich_font_by_cell;
    my @nodes_r = $si->find_nodes('.//s:r');
    if (@nodes_r > 0) {
        for my $chunk (map { $_->children } @nodes_r) {
            my $string_length = length($string_text);
            if ($chunk->name eq 's:t') {
                if (!@rich_font_by_cell) {
                    push @rich_font_by_cell, [
                        $string_length,
                        Spreadsheet::ParseExcel::Font->new(%default_font_opts)
                    ];
                }
                $string_text .= $chunk->text;
            }
            elsif ($chunk->name eq 's:rPr') {
                my %format_text = %default_font_opts;
                for my $node_format ($chunk->children) {
                    if ($node_format->name eq 's:sz') {
                        $format_text{Height} = $node_format->att('val');
                    }
                    elsif ($node_format->name eq 's:color') {
                        $format_text{Color} = $self->_color(
                            $theme_colors,
                            $node_format
                        );
                    }
                    elsif ($node_format->name eq 's:rFont') {
                        $format_text{Name} = $node_format->att('val');
                    }
                    elsif ($node_format->name eq 's:b') {
                        $format_text{Bold} = 1;
                    }
                    elsif ($node_format->name eq 's:i') {
                        $format_text{Italic} = 1;
                    }
                    elsif ($node_format->name eq 's:u') {
                        $format_text{Underline} = 1;
                        if (defined $node_format->att('val')) {
                            $format_text{UnderlineStyle} = 2;
                        } else {
                            $format_text{UnderlineStyle} = 1;
                        }
                    }
                    elsif ($node_format->name eq 's:strike') {
                        $format_text{Strikeout} = 1;
                    }
                    elsif ($node_format->name eq 's:vertAlign') {
                        if ($node_format->att('val') eq 'superscript') {
                            $format_text{Super} = 1;
                        }
                        elsif ($node_format->att('val') eq 'subscript') {
                            $format_text{Super} = 2;
                        }
                    }
                }
                push @rich_font_by_cell, [
                    $string_length,
                    Spreadsheet::ParseExcel::Font->new(%format_text)
                ];
            }
        }
    }
    else {
        $string_text = join '', map { $_->text } $si->find_nodes('.//s:t');
    }

    return (
        String => $string_text,
        Rich => \@rich_font_by_cell,
    );
}

sub _parse_shared_strings {
    my $self = shift;
    my ($strings, $theme_colors) = @_;

    my $PkgStr = [];

    my %richfonts;
    if ($strings) {
        my $xml = $self->_new_twig(
            twig_handlers => {
                's:si' => sub {
                    my ( $twig, $si ) = @_;

                    my %text_rich = $self->_get_text_and_rich_font_by_cell(
                        $si,
                        $theme_colors
                    );
                    $richfonts{scalar @$PkgStr} = $text_rich{Rich};
                    push @$PkgStr, $text_rich{String};
                    $twig->purge;
                },
            }
        );
        $xml->parse( $strings );
    }
    return (
        Rich   => \%richfonts,
        PkgStr => $PkgStr,
    );
}

sub _parse_themes {
    my $self = shift;
    my ($themes) = @_;

    return {} unless $themes;

    my @color = map {
        $_->name eq 'drawmain:sysClr' ? $_->att('lastClr') : $_->att('val')
    } $themes->find_nodes('//drawmain:clrScheme/*/*');

    # this shouldn't be necessary, but the documentation is wrong here
    # see http://stackoverflow.com/questions/2760976/theme-confusion-in-spreadsheetml
    ($color[0], $color[1]) = ($color[1], $color[0]);
    ($color[2], $color[3]) = ($color[3], $color[2]);

    return {
        Color => \@color,
    }
}

sub _parse_styles {
    my $self = shift;
    my ($workbook, $styles) = @_;

    # these defaults are from
    # http://social.msdn.microsoft.com/Forums/en-US/oxmlsdk/thread/e27aaf16-b900-4654-8210-83c5774a179c
    my %default_format_str = (
        0  => 'GENERAL',
        1  => '0',
        2  => '0.00',
        3  => '#,##0',
        4  => '#,##0.00',
        5  => '$#,##0_);($#,##0)',
        6  => '$#,##0_);[Red]($#,##0)',
        7  => '$#,##0.00_);($#,##0.00)',
        8  => '$#,##0.00_);[Red]($#,##0.00)',
        9  => '0%',
        10 => '0.00%',
        11 => '0.00E+00',
        12 => '# ?/?',
        13 => '# ??/??',
        14 => 'm/d/yyyy',
        15 => 'd-mmm-yy',
        16 => 'd-mmm',
        17 => 'mmm-yy',
        18 => 'h:mm AM/PM',
        19 => 'h:mm:ss AM/PM',
        20 => 'h:mm',
        21 => 'h:mm:ss',
        22 => 'm/d/yyyy h:mm',
        37 => '#,##0_);(#,##0)',
        38 => '#,##0_);[Red](#,##0)',
        39 => '#,##0.00_);(#,##0.00)',
        40 => '#,##0.00_);[Red](#,##0.00)',
        45 => 'mm:ss',
        46 => '[h]:mm:ss',
        47 => 'mm:ss.0',
        48 => '##0.0E+0',
        49 => '@',
    );

    my %default_format_opts = (
        IgnoreFont         => 1,
        IgnoreFill         => 1,
        IgnoreBorder       => 1,
        IgnoreAlignment    => 1,
        IgnoreNumberFormat => 1,
        IgnoreProtection   => 1,
        FontNo             => 0,
        FmtIdx             => 0,
        Lock               => 1,
        Hidden             => 0,
        AlignH             => 0,
        Wrap               => 0,
        AlignV             => 2,
        Rotate             => 0,
        Indent             => 0,
        Shrink             => 0,
        BdrStyle           => [0, 0, 0, 0],
        BdrColor           => [undef, undef, undef, undef],
        BdrDiag            => [0, 0, undef],
        Fill               => [0, undef, undef],
    );

    if (!$styles) {
        # XXX i guess?
        my $font = Spreadsheet::ParseExcel::Font->new(
            Height         => 12,
            Color          => '#000000',
            Name           => '',
        );
        my $format = Spreadsheet::ParseExcel::Format->new(
            %default_format_opts,
            Font => $font,
        );

        return {
            FormatStr => \%default_format_str,
            Font      => [ $font ],
            Format    => [ $format ],
        };
    }

    my %halign = (
        center           => 2,
        centerContinuous => 6,
        distributed      => 7,
        fill             => 4,
        general          => 0,
        justify          => 5,
        left             => 1,
        right            => 3,
    );

    my %valign = (
        bottom      => 2,
        center      => 1,
        distributed => 4,
        justify     => 3,
        top         => 0,
    );

    my %border = (
        dashDot          => 9,
        dashDotDot       => 11,
        dashed           => 3,
        dotted           => 4,
        double           => 6,
        hair             => 7,
        medium           => 2,
        mediumDashDot    => 10,
        mediumDashDotDot => 12,
        mediumDashed     => 8,
        none             => 0,
        slantDashDot     => 13,
        thick            => 5,
        thin             => 1,
    );

    my %fill = (
        darkDown        => 7,
        darkGray        => 3,
        darkGrid        => 9,
        darkHorizontal  => 5,
        darkTrellis     => 10,
        darkUp          => 8,
        darkVertical    => 6,
        gray0625        => 18,
        gray125         => 17,
        lightDown       => 13,
        lightGray       => 4,
        lightGrid       => 15,
        lightHorizontal => 11,
        lightTrellis    => 16,
        lightUp         => 14,
        lightVertical   => 12,
        mediumGray      => 2,
        none            => 0,
        solid           => 1,
    );

    my @fills = map {
        my $pattern_type = $_->att('patternType');
        [
            ($pattern_type ? $fill{$pattern_type} : 0),
            $self->_color($workbook->{Color}, $_->first_child('s:fgColor'), 1),
            $self->_color($workbook->{Color}, $_->first_child('s:bgColor'), 1),
        ]
    } $styles->find_nodes('//s:fills/s:fill/s:patternFill');

    my @borders = map {
        my $border = $_;
        my ($ddiag, $udiag) = map {
            $self->_xml_boolean($border->att($_))
        } qw(diagonalDown diagonalUp);
        my %borderstyles = map {
            my $e = $border->first_child("s:$_");
            $_ => ($e ? $e->att('style') || 'none' : 'none')
        } qw(left right top bottom diagonal);
        my %bordercolors = map {
            my $e = $border->first_child("s:$_");
            $_ => ($e ? $e->first_child('s:color') : undef)
        } qw(left right top bottom diagonal);
        # XXX specs say "begin" and "end" rather than "left" and "right",
        # but... that's not what seems to be in the file itself (sigh)
        {
            colors => [
                map {
                    $self->_color($workbook->{Color}, $bordercolors{$_})
                } qw(left right top bottom)
            ],
            styles => [
                map {
                    $border{$borderstyles{$_}}
                } qw(left right top bottom)
            ],
            diagonal => [
                ( $ddiag &&  $udiag ? 3
               :  $ddiag && !$udiag ? 2
               : !$ddiag &&  $udiag ? 1
               :                      0),
                $border{$borderstyles{diagonal}},
                $self->_color($workbook->{Color}, $bordercolors{diagonal}),
            ],
        }
    } $styles->find_nodes('//s:borders/s:border');

    my %format_str = (
        %default_format_str,
        (map {
            $_->att('numFmtId') => $_->att('formatCode')
        } $styles->find_nodes('//s:numFmts/s:numFmt')),
    );

    my @font = map {
        my $vert = $_->first_child('s:vertAlign');
        my $under = $_->first_child('s:u');
        my $heightelem = $_->first_child('s:sz');
        # XXX i guess 12 is okay?
        my $height = 0+($heightelem ? $heightelem->att('val') : 12);
        my $nameelem = $_->first_child('s:name');
        my $name = $nameelem ? $nameelem->att('val') : '';
        Spreadsheet::ParseExcel::Font->new(
            Height         => $height,
            # Attr           => $iAttr,
            # XXX not sure if there's a better way to keep the indexing stuff
            # intact rather than just going straight to #xxxxxx
            # XXX also not sure what it means for the color tag to be missing,
            # just assuming black for now
            Color          => ($_->first_child('s:color')
                ? $self->_color(
                    $workbook->{Color},
                    $_->first_child('s:color')
                )
                : '#000000'
            ),
            Super          => ($vert
                ? ($vert->att('val') eq 'superscript' ? 1
                 : $vert->att('val') eq 'subscript'   ? 2
                 :                                      0)
                : 0
            ),
            # XXX not sure what the single accounting and double accounting
            # underline styles map to in xlsx. also need to map the new
            # underline styles
            UnderlineStyle => ($under
                # XXX sometimes style xml files can contain just <u/> with no
                # val attribute. i think this means single underline, but not
                # sure
                ? (!$under->att('val')            ? 1
                 : $under->att('val') eq 'single' ? 1
                 : $under->att('val') eq 'double' ? 2
                 :                                  0)
                : 0
            ),
            Name           => $name,

            Bold      => $_->has_child('s:b') ? 1 : 0,
            Italic    => $_->has_child('s:i') ? 1 : 0,
            Underline => $_->has_child('s:u') ? 1 : 0,
            Strikeout => $_->has_child('s:strike') ? 1 : 0,
        )
    } $styles->find_nodes('//s:fonts/s:font');

    my @format = map {
        my $xml_fmt = $_;
        my $alignment  = $xml_fmt->first_child('s:alignment');
        my $protection = $xml_fmt->first_child('s:protection');
        my %ignore = map {
            ("Ignore$_" => !$self->_xml_boolean($xml_fmt->att("apply$_")))
        } qw(Font Fill Border Alignment NumberFormat Protection);
        my %opts = (
            %default_format_opts,
            %ignore,
        );

        $opts{FmtIdx}   = 0+($xml_fmt->att('numFmtId')||0);
        $opts{FontNo}   = 0+($xml_fmt->att('fontId')||0);
        $opts{Font}     = $font[$opts{FontNo}];
        $opts{Fill}     = $fills[$xml_fmt->att('fillId')||0];
        $opts{BdrStyle} = $borders[$xml_fmt->att('borderId')||0]{styles};
        $opts{BdrColor} = $borders[$xml_fmt->att('borderId')||0]{colors};
        $opts{BdrDiag}  = $borders[$xml_fmt->att('borderId')||0]{diagonal};

        if ($alignment) {
            $opts{AlignH} = $halign{$alignment->att('horizontal') || 'general'};
            $opts{Wrap}   = $self->_xml_boolean($alignment->att('wrapText'));
            $opts{AlignV} = $valign{$alignment->att('vertical') || 'bottom'};
            $opts{Rotate} = $alignment->att('textRotation');
            $opts{Indent} = $alignment->att('indent');
            $opts{Shrink} = $self->_xml_boolean($alignment->att('shrinkToFit'));
            # JustLast => $iJustL,
        }

        if ($protection) {
            $opts{Lock} = defined $protection->att('locked')
                ? $self->_xml_boolean($protection->att('locked'))
                : 1;
            $opts{Hidden} = $self->_xml_boolean($protection->att('hidden'));
        }

        # Style    => $iStyle,
        # Key123   => $i123,
        # Merge   => $iMerge,
        # ReadDir => $iReadDir,
        Spreadsheet::ParseExcel::Format->new(%opts)
    } $styles->find_nodes('//s:cellXfs/s:xf');

    return {
        FormatStr => \%format_str,
        Font      => \@font,
        Format    => \@format,
    }
}

sub _extract_files {
    my $self = shift;
    my ($zip) = @_;

    my $type_base =
        'http://schemas.openxmlformats.org/officeDocument/2006/relationships';

    my $rels = $self->_parse_xml(
        $zip,
        $self->_rels_for(''),
    );
    my $wb_name = ($rels->find_nodes(
        qq<//packagerels:Relationship[\@Type="$type_base/officeDocument"]>
    ))[0]->att('Target');
    $wb_name =~ s{^/}{};
    my $wb_xml = $self->_parse_xml($zip, $wb_name);

    my $path_base = $self->_base_path_for($wb_name);
    my $wb_rels = $self->_parse_xml(
        $zip,
        $self->_rels_for($wb_name),
    );

    my $get_path = sub {
        my ($p) = @_;

        return $p =~ s{^/}{}
            ? $p
            : $path_base . $p;
    };

    my ($strings_xml) = map {
        $self->_zip_file_member($zip, $get_path->($_->att('Target')))
    } $wb_rels->find_nodes(qq<//packagerels:Relationship[\@Type="$type_base/sharedStrings"]>);

    my ($styles_xml) = map {
        $self->_parse_xml(
            $zip,
            $get_path->($_->att('Target'))
        )
    } $wb_rels->find_nodes(qq<//packagerels:Relationship[\@Type="$type_base/styles"]>);

    my %worksheet_xml = map {
        ($_->att('Id') => $self->_zip_file_member($zip, $get_path->($_->att('Target'))))
    } $wb_rels->find_nodes(qq<//packagerels:Relationship[\@Type="$type_base/worksheet"]>);

    my %themes_xml = map {
        $_->att('Id') => $self->_parse_xml($zip, $get_path->($_->att('Target')))
    } $wb_rels->find_nodes(qq<//packagerels:Relationship[\@Type="$type_base/theme"]>);

    return {
        workbook => $wb_xml,
        sheets   => \%worksheet_xml,
        themes   => \%themes_xml,
        ($styles_xml
            ? (styles  => $styles_xml)
            : ()),
        ($strings_xml
            ? (strings => $strings_xml)
            : ()),
    };
}

sub _parse_xml {
    my $self = shift;
    my ($zip, $subfile, $map_xmlns) = @_;

    my $xml = $self->_new_twig;
    $xml->parse($self->_zip_file_member($zip, $subfile));

    return $xml;
}

sub _zip_file_member {
    my $self = shift;
    my ($zip, $name) = @_;

    my @members = $zip->membersMatching(qr/^$name$/i);
    die "no subfile named $name" unless @members;

    return scalar $members[0]->contents;
}

sub _rels_for {
    my $self = shift;
    my ($file) = @_;

    my @path = split '/', $file;
    my $name = pop @path;
    $name = '' unless defined $name;
    push @path, '_rels';
    push @path, "$name.rels";

    return join '/', @path;
}

sub _base_path_for {
    my $self = shift;
    my ($file) = @_;

    my @path = split '/', $file;
    pop @path;

    return join('/', @path) . '/';
}

sub _dimensions {
    my $self = shift;
    my ($dim) = @_;

    my ($topleft, $bottomright) = split ':', $dim;
    $bottomright = $topleft unless defined $bottomright;

    my ($rmin, $cmin) = $self->_cell_to_row_col($topleft);
    my ($rmax, $cmax) = $self->_cell_to_row_col($bottomright);

    return ($rmin, $cmin, $rmax, $cmax);
}

sub _cell_to_row_col {
    my $self = shift;
    my ($cell) = @_;

    my ($col, $row) = $cell =~ /([A-Z]+)([0-9]+)/;

    my $ncol = 0;
    for my $char (split //, $col) {
        $ncol *= 26;
        $ncol += ord($char) - ord('A') + 1;
    }
    $ncol = $ncol - 1;

    my $nrow = $row - 1;

    return ($nrow, $ncol);
}

sub _xml_boolean {
    my $self = shift;
    my ($bool) = @_;
    return defined($bool) && ($bool eq 'true' || $bool eq '1');
}

sub _color {
    my $self = shift;
    my ($colors, $color_node, $fill) = @_;

    my $color;
    if ($color_node && !$self->_xml_boolean($color_node->att('auto'))) {
        if (defined $color_node->att('indexed')) {
            # see https://rt.cpan.org/Public/Bug/Display.html?id=93065
            if ($fill && $color_node->att('indexed') == 64) {
                return '#FFFFFF';
            }
            else {
                $color = '#' . Spreadsheet::ParseExcel->ColorIdxToRGB(
                    $color_node->att('indexed')
                );
            }
        }
        elsif (defined $color_node->att('rgb')) {
            $color = '#' . substr($color_node->att('rgb'), 2, 6);
        }
        elsif (defined $color_node->att('theme')) {
            my $theme = $colors->[$color_node->att('theme')];
            if (defined $theme) {
                $color = "#$theme";
            }
            else {
                return undef;
            }
        }

        $color = $self->_apply_tint($color, $color_node->att('tint'))
            if $color_node->att('tint');
    }

    return $color;
}

sub _apply_tint {
    my $self = shift;
    my ($color, $tint) = @_;

    my ($r, $g, $b) = map { oct("0x$_") } $color =~ /#(..)(..)(..)/;
    my ($h, $l, $s) = rgb2hls($r, $g, $b);

    if ($tint < 0) {
        $l = $l * (1.0 + $tint);
    }
    else {
        $l = $l * (1.0 - $tint) + (1.0 - 1.0 * (1.0 - $tint));
    }

    return scalar hls2rgb($h, $l, $s);
}

sub _new_twig {
    my $self = shift;
    my %opts = @_;

    return XML::Twig->new(
        map_xmlns => {
            'http://schemas.openxmlformats.org/spreadsheetml/2006/main' => 's',
            'http://schemas.openxmlformats.org/package/2006/relationships' => 'packagerels',
            'http://schemas.openxmlformats.org/officeDocument/2006/relationships' => 'rels',
            'http://schemas.openxmlformats.org/drawingml/2006/main' => 'drawmain',
        },
        keep_original_prefix => 1,
        %opts,
    );
}


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Spreadsheet::ParseXLSX - parse XLSX files

=head1 VERSION

version 0.27

=head1 SYNOPSIS

  use Spreadsheet::ParseXLSX;

  my $parser = Spreadsheet::ParseXLSX->new;
  my $workbook = $parser->parse("file.xlsx");
  # see Spreadsheet::ParseExcel for further documentation

=head1 DESCRIPTION

This module is an adaptor for L<Spreadsheet::ParseExcel> that reads XLSX files.
For documentation about the various data that you can retrieve from these
classes, please see L<Spreadsheet::ParseExcel>,
L<Spreadsheet::ParseExcel::Workbook>, L<Spreadsheet::ParseExcel::Worksheet>,
and L<Spreadsheet::ParseExcel::Cell>.

=head1 METHODS

=head2 new(%opts)

Returns a new parser instance. Takes a hash of parameters:

=over 4

=item Password

Password to use for decrypting encrypted files.

=back

=head2 parse($file, $formatter)

Parses an XLSX file. Parsing errors throw an exception. C<$file> can be either
a filename or an open filehandle. Returns a
L<Spreadsheet::ParseExcel::Workbook> instance containing the parsed data.
The C<$formatter> argument is an optional formatter class as described in L<Spreadsheet::ParseExcel>.

=head1 INCOMPATIBILITIES

This module returns data using classes from L<Spreadsheet::ParseExcel>, so for
the most part, it should just be a drop-in replacement. That said, there are a
couple areas where the data returned is intentionally different:

=over 4

=item Colors

In Spreadsheet::ParseExcel, colors are represented by integers which index into
the color table, and you have to use
C<< Spreadsheet::ParseExcel->ColorIdxToRGB >> in order to get the actual value
out. In Spreadsheet::ParseXLSX, while the color table still exists, cells are
also allowed to specify their color directly rather than going through the
color table. In order to avoid confusion, I normalize all color values in
Spreadsheet::ParseXLSX to their string RGB format (C<"#0088ff">). This affects
the C<Fill>, C<BdrColor>, and C<BdrDiag> properties of formats, and the
C<Color> property of fonts. Note that the default color is represented by
C<undef> (the same thing that C<ColorIdxToRGB> would return).

=item Formulas

Spreadsheet::ParseExcel doesn't support formulas. Spreadsheet::ParseXLSX
provides basic formula support by returning the text of the formula as part of
the cell data. You can access it via C<< $cell->{Formula} >>. Note that the
restriction still holds that formula cell values aren't available unless they
were explicitly provided when the spreadsheet was written.

=back

=head1 BUGS

=over 4

=item Large spreadsheets may cause segfaults on perl 5.14 and earlier

This module internally uses XML::Twig, which makes it potentially subject to
L<Bug #71636 for XML-Twig: Segfault with medium-sized document|https://rt.cpan.org/Public/Bug/Display.html?id=71636>
on perl versions 5.14 and below (the underlying bug with perl weak references
was fixed in perl 5.15.5). The larger and more complex the spreadsheet, the
more likely to be affected, but the actual size at which it segfaults is
platform dependent. On a 64-bit perl with 7.6gb memory, it was seen on
spreadsheets about 300mb and above. You can work around this adding
C<XML::Twig::_set_weakrefs(0)> to your code before parsing the spreadsheet,
although this may have other consequences such as memory leaks.

=item Worksheets without the C<dimension> tag are not supported

=item Intra-cell formatting is discarded

=item Shared formulas are not supported

Shared formula support will require an actual formula parser and quite a bit of
custom logic, since the only thing stored in the document is the formula for
the base cell - updating the cell references in the formulas in the rest of the
cells is handled by the application. Values for these cells are still handled
properly.

=back

In addition, there are still a few areas which are not yet implemented (the
XLSX spec is quite large). If you run into any of those, bug reports are quite
welcome.

Please report any bugs to GitHub Issues at
L<https://github.com/doy/spreadsheet-parsexlsx/issues>.

=head1 SEE ALSO

L<Spreadsheet::ParseExcel>: The equivalent, for XLS files.

L<Spreadsheet::XLSX>: An older, less robust and featureful implementation.

=head1 SUPPORT

You can find this documentation for this module with the perldoc command.

    perldoc Spreadsheet::ParseXLSX

You can also look for information at:

=over 4

=item * MetaCPAN

L<https://metacpan.org/release/Spreadsheet-ParseXLSX>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Spreadsheet-ParseXLSX>

=item * Github

L<https://github.com/doy/spreadsheet-parsexlsx>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Spreadsheet-ParseXLSX>

=back

=head1 SPONSORS

Parts of this code were paid for by

=over 4

=item Socialflow L<http://socialflow.com>

=back

=head1 AUTHOR

Jesse Luehrs <doy@tozt.net>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2016 by Jesse Luehrs.

This is free software, licensed under:

  The MIT (X11) License

=cut
