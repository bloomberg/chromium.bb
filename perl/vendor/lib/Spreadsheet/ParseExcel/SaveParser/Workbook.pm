package Spreadsheet::ParseExcel::SaveParser::Workbook;

###############################################################################
#
# Spreadsheet::ParseExcel::SaveParser::Workbook - A class for SaveParser Workbooks.
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

use base 'Spreadsheet::ParseExcel::Workbook';
our $VERSION = '0.65';

#==============================================================================
# Spreadsheet::ParseExcel::SaveParser::Workbook
#==============================================================================

sub new {
    my ( $sPkg, $oBook ) = @_;
    return undef unless ( defined $oBook );
    my %oThis = %$oBook;
    bless \%oThis, $sPkg;

    # re-bless worksheets (and set their _Book properties !!!)
    my $sWkP = ref($sPkg) || "$sPkg";
    $sWkP =~ s/Workbook$/Worksheet/;
    map { bless( $_, $sWkP ); } @{ $oThis{Worksheet} };
    map { $_->{_Book} = \%oThis; } @{ $oThis{Worksheet} };
    return \%oThis;
}

#------------------------------------------------------------------------------
# Parse (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub Parse {
    my ( $sClass, $sFile, $oWkFmt ) = @_;
    my $oBook = Spreadsheet::ParseExcel::Workbook->Parse( $sFile, $oWkFmt );
    bless $oBook, $sClass;

    # re-bless worksheets (and set their _Book properties !!!)
    my $sWkP = ref($sClass) || "$sClass";
    $sWkP =~ s/Workbook$/Worksheet/;
    map { bless( $_, $sWkP ); } @{ $oBook->{Worksheet} };
    map { $_->{_Book} = $oBook; } @{ $oBook->{Worksheet} };
    return $oBook;
}

#------------------------------------------------------------------------------
# SaveAs (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub SaveAs {
    my ( $oBook, $sName ) = @_;

    # Create a new Excel workbook
    my $oWrEx = Spreadsheet::WriteExcel->new($sName);
    $oWrEx->compatibility_mode();
    my %hFmt;

    my $iNo  = 0;
    my @aAlH = (
        'left', 'left',    'center', 'right',
        'fill', 'justify', 'merge',  'equal_space'
    );
    my @aAlV = ( 'top', 'vcenter', 'bottom', 'vjustify', 'vequal_space' );

    foreach my $pFmt ( @{ $oBook->{Format} } ) {
        my $oFmt = $oWrEx->addformat();    # Add Formats
        unless ( $pFmt->{Style} ) {
            $hFmt{$iNo} = $oFmt;
            my $rFont = $pFmt->{Font};

            $oFmt->set_font( $rFont->{Name} );
            $oFmt->set_size( $rFont->{Height} );
            $oFmt->set_color( $rFont->{Color} );
            $oFmt->set_bold( $rFont->{Bold} );
            $oFmt->set_italic( $rFont->{Italic} );
            $oFmt->set_underline( $rFont->{Underline} );
            $oFmt->set_font_strikeout( $rFont->{Strikeout} );
            $oFmt->set_font_script( $rFont->{Super} );

            $oFmt->set_hidden( $rFont->{Hidden} );    #Add

            $oFmt->set_locked( $pFmt->{Lock} );

            $oFmt->set_align( $aAlH[ $pFmt->{AlignH} ] );
            $oFmt->set_align( $aAlV[ $pFmt->{AlignV} ] );

            $oFmt->set_rotation( $pFmt->{Rotate} );

            $oFmt->set_num_format(
                $oBook->{FmtClass}->FmtStringDef( $pFmt->{FmtIdx}, $oBook ) );

            $oFmt->set_text_wrap( $pFmt->{Wrap} );

            $oFmt->set_pattern( $pFmt->{Fill}->[0] );
            $oFmt->set_fg_color( $pFmt->{Fill}->[1] )
              if ( ( $pFmt->{Fill}->[1] >= 8 )
                && ( $pFmt->{Fill}->[1] <= 63 ) );
            $oFmt->set_bg_color( $pFmt->{Fill}->[2] )
              if ( ( $pFmt->{Fill}->[2] >= 8 )
                && ( $pFmt->{Fill}->[2] <= 63 ) );

            $oFmt->set_left(
                ( $pFmt->{BdrStyle}->[0] > 7 ) ? 3 : $pFmt->{BdrStyle}->[0] );
            $oFmt->set_right(
                ( $pFmt->{BdrStyle}->[1] > 7 ) ? 3 : $pFmt->{BdrStyle}->[1] );
            $oFmt->set_top(
                ( $pFmt->{BdrStyle}->[2] > 7 ) ? 3 : $pFmt->{BdrStyle}->[2] );
            $oFmt->set_bottom(
                ( $pFmt->{BdrStyle}->[3] > 7 ) ? 3 : $pFmt->{BdrStyle}->[3] );

            $oFmt->set_left_color( $pFmt->{BdrColor}->[0] )
              if ( ( $pFmt->{BdrColor}->[0] >= 8 )
                && ( $pFmt->{BdrColor}->[0] <= 63 ) );
            $oFmt->set_right_color( $pFmt->{BdrColor}->[1] )
              if ( ( $pFmt->{BdrColor}->[1] >= 8 )
                && ( $pFmt->{BdrColor}->[1] <= 63 ) );
            $oFmt->set_top_color( $pFmt->{BdrColor}->[2] )
              if ( ( $pFmt->{BdrColor}->[2] >= 8 )
                && ( $pFmt->{BdrColor}->[2] <= 63 ) );
            $oFmt->set_bottom_color( $pFmt->{BdrColor}->[3] )
              if ( ( $pFmt->{BdrColor}->[3] >= 8 )
                && ( $pFmt->{BdrColor}->[3] <= 63 ) );
        }
        $iNo++;
    }
    for ( my $iSheet = 0 ; $iSheet < $oBook->{SheetCount} ; $iSheet++ ) {
        my $oWkS = $oBook->{Worksheet}[$iSheet];
        my $oWrS = $oWrEx->addworksheet( $oWkS->{Name} );

        #Landscape
        if ( !$oWkS->{Landscape} ) {    # Landscape (0:Horizontal, 1:Vertical)
            $oWrS->set_landscape();
        }
        else {
            $oWrS->set_portrait();
        }

        #Protect
        if ( defined $oWkS->{Protect} )
        {    # Protect ('':NoPassword, Password:Password)
            if ( $oWkS->{Protect} ne '' ) {
                $oWrS->protect( $oWkS->{Protect} );
            }
            else {
                $oWrS->protect();
            }
        }
        if ( $oWkS->{Scale} != 100 ) {

            # Pages on fit with width and Heigt
            $oWrS->fit_to_pages( $oWkS->{FitWidth}, $oWkS->{FitHeight} );

            #Print Scale and reset FitWidth/FitHeight
            $oWrS->set_print_scale( $oWkS->{Scale} );
        }
        else {

            #Print Scale
            $oWrS->set_print_scale( $oWkS->{Scale} );

            # Pages on fit with width and Heigt
            $oWrS->fit_to_pages( $oWkS->{FitWidth}, $oWkS->{FitHeight} );
        }

        # Paper Size
        $oWrS->set_paper( $oWkS->{PaperSize} );

        # Margin
        $oWrS->set_margin_left( $oWkS->{LeftMargin} );
        $oWrS->set_margin_right( $oWkS->{RightMargin} );
        $oWrS->set_margin_top( $oWkS->{TopMargin} );
        $oWrS->set_margin_bottom( $oWkS->{BottomMargin} );

        # HCenter
        $oWrS->center_horizontally() if ( $oWkS->{HCenter} );

        # VCenter
        $oWrS->center_vertically() if ( $oWkS->{VCenter} );

        # Header, Footer
        $oWrS->set_header( $oWkS->{Header}, $oWkS->{HeaderMargin} );
        $oWrS->set_footer( $oWkS->{Footer}, $oWkS->{FooterMargin} );

        # Print Area
        if ( ref( $oBook->{PrintArea}[$iSheet] ) eq 'ARRAY' ) {
            my $raP;
            for $raP ( @{ $oBook->{PrintArea}[$iSheet] } ) {
                $oWrS->print_area(@$raP);
            }
        }

        # Print Title
        my $raW;
        foreach $raW ( @{ $oBook->{PrintTitle}[$iSheet]->{Row} } ) {
            $oWrS->repeat_rows(@$raW);
        }
        foreach $raW ( @{ $oBook->{PrintTitle}[$iSheet]->{Column} } ) {
            $oWrS->repeat_columns(@$raW);
        }

        # Print Gridlines
        if ( $oWkS->{PrintGrid} == 1 ) {
            $oWrS->hide_gridlines(0);
        }
        else {
            $oWrS->hide_gridlines(1);
        }

        # Print Headings
        if ( $oWkS->{PrintHeaders} ) {
            $oWrS->print_row_col_headers();
        }

        # Horizontal Page Breaks
        $oWrS->set_h_pagebreaks( @{ $oWkS->{HPageBreak} } );

        # Veritical Page Breaks
        $oWrS->set_v_pagebreaks( @{ $oWkS->{VPageBreak} } );



#        PageStart    => $oWkS->{PageStart},            # Page number for start
#        UsePage      => $oWkS->{UsePage},              # Use own start page number
#        NoColor      => $oWkS->{NoColor},               # Print in black-white
#        Draft        => $oWkS->{Draft},                 # Print in draft mode
#        Notes        => $oWkS->{Notes},                 # Print notes
#        LeftToRight  => $oWkS->{LeftToRight},           # Left to Right


        for (
            my $iC = $oWkS->{MinCol} ;
            defined $oWkS->{MaxCol} && $iC <= $oWkS->{MaxCol} ;
            $iC++
          )
        {
            if ( defined $oWkS->{ColWidth}[$iC] ) {
                if ( $oWkS->{ColWidth}[$iC] > 0 ) {
                    $oWrS->set_column( $iC, $iC, $oWkS->{ColWidth}[$iC] )
                      ;    #, undef, 1) ;
                }
                else {
                    $oWrS->set_column( $iC, $iC, 0, undef, 1 );
                }
            }
        }
        
		my $merged_areas = $oWkS->get_merged_areas();
		my $merged_areas_h = {};
		if ($merged_areas) {
			foreach my $range (@$merged_areas) {
				$merged_areas_h->{$range->[0]}{$range->[1]} = $range;
			}
		}
		
		for (
            my $iR = $oWkS->{MinRow} ;
            defined $oWkS->{MaxRow} && $iR <= $oWkS->{MaxRow} ;
            $iR++
          )
        {
            $oWrS->set_row( $iR, $oWkS->{RowHeight}[$iR] );
            for (
                my $iC = $oWkS->{MinCol} ;
                defined $oWkS->{MaxCol} && $iC <= $oWkS->{MaxCol} ;
                $iC++
              )
            {

                my $oWkC = $oWkS->{Cells}[$iR][$iC];
                if ($oWkC) {
                    if ( $oWkC->{Merged} and exists $merged_areas_h->{$iR}{$iC} ) {
                        my $oFmtN = $oWrEx->addformat();
                        $oFmtN->copy( $hFmt{ $oWkC->{FormatNo} } );
						$oWrS->merge_range (
							@{$merged_areas_h->{$iR}{$iC}},
                            $oBook->{FmtClass}
                              ->TextFmt( $oWkC->{Val}, $oWkC->{Code} ),
                            $oFmtN
                        );
                    }
                    else {
                        $oWrS->write(
                            $iR,
                            $iC,
                            $oBook->{FmtClass}
                              ->TextFmt( $oWkC->{Val}, $oWkC->{Code} ),
                            $hFmt{ $oWkC->{FormatNo} }
                        );
                    }
                }
            }
        }
    }
    return $oWrEx;
}

#------------------------------------------------------------------------------
# AddWorksheet (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub AddWorksheet {
    my ( $oBook, $sName, %hAttr ) = @_;
    $oBook->AddFormat if ( $#{ $oBook->{Format} } < 0 );
    $hAttr{Name}         ||= $sName;
    $hAttr{LeftMargin}   ||= 0;
    $hAttr{RightMargin}  ||= 0;
    $hAttr{TopMargin}    ||= 0;
    $hAttr{BottomMargin} ||= 0;
    $hAttr{HeaderMargin} ||= 0;
    $hAttr{FooterMargin} ||= 0;
    $hAttr{FitWidth}     ||= 0;
    $hAttr{FitHeight}    ||= 0;
    $hAttr{PrintGrid}    ||= 0;
    my $oWkS = Spreadsheet::ParseExcel::SaveParser::Worksheet->new(%hAttr);
    $oWkS->{_Book}                              = $oBook;
    $oWkS->{_SheetNo}                           = $oBook->{SheetCount};
    $oBook->{Worksheet}[ $oBook->{SheetCount} ] = $oWkS;
    $oBook->{SheetCount}++;
    return $oWkS;    #$oBook->{SheetCount} - 1;
}

#------------------------------------------------------------------------------
# AddFont (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub AddFont {
    my ( $oBook, %hAttr ) = @_;
    $hAttr{Name}      ||= 'Arial';
    $hAttr{Height}    ||= 10;
    $hAttr{Bold}      ||= 0;
    $hAttr{Italic}    ||= 0;
    $hAttr{Underline} ||= 0;
    $hAttr{Strikeout} ||= 0;
    $hAttr{Super}     ||= 0;
    push @{ $oBook->{Font} }, Spreadsheet::ParseExcel::Font->new(%hAttr);
    return $#{ $oBook->{Font} };
}

#------------------------------------------------------------------------------
# AddFormat (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub AddFormat {
    my ( $oBook, %hAttr ) = @_;
    $hAttr{Fill}     ||= [ 0, 0, 0 ];
    $hAttr{BdrStyle} ||= [ 0, 0, 0, 0 ];
    $hAttr{BdrColor} ||= [ 0, 0, 0, 0 ];
    $hAttr{AlignH}    ||= 0;
    $hAttr{AlignV}    ||= 0;
    $hAttr{Rotate}    ||= 0;
    $hAttr{Landscape} ||= 0;
    $hAttr{FmtIdx}    ||= 0;

    if ( !defined( $hAttr{Font} ) ) {
        my $oFont;
        if ( defined $hAttr{FontNo} ) {
            $oFont = $oBook->{Font}[ $hAttr{FontNo} ];
        }
        elsif ( !defined $oFont ) {
            if ( $#{ $oBook->{Font} } >= 0 ) {
                $oFont = $oBook->{Font}[0];
            }
            else {
                my $iNo = $oBook->AddFont;
                $oFont = $oBook->{Font}[$iNo];
            }
        }
        $hAttr{Font} = $oFont;
    }
    push @{ $oBook->{Format} }, Spreadsheet::ParseExcel::Format->new(%hAttr);
    return $#{ $oBook->{Format} };
}

#------------------------------------------------------------------------------
# AddCell (for Spreadsheet::ParseExcel::SaveParser::Workbook)
#------------------------------------------------------------------------------
sub AddCell {
    my ( $oBook, $iSheet, $iR, $iC, $sVal, $oCell, $sCode ) = @_;
    my %rhKey;
    $oCell ||= $oBook->{Worksheet}[$iSheet]
		->{Cells}[$iR][$iC]->{FormatNo} || 0;
    my $iFmt =
      ( UNIVERSAL::isa( $oCell, 'Spreadsheet::ParseExcel::Cell' ) )
      ? $oCell->{FormatNo}
      : ( ref($oCell) ) ? 0
      :                   $oCell + 0;
    $rhKey{FormatNo}    = $iFmt;
    $rhKey{Format}      = $oBook->{Format}[$iFmt];
    $rhKey{Val}         = $sVal;
    $rhKey{Code}        = $sCode || '_native_';
    $oBook->{_CurSheet} = $iSheet;

	my $merged_areas = $oBook->{Worksheet}[$iSheet]->get_merged_areas();
	my $merged_areas_h = {};
	if ($merged_areas) {
		foreach my $range (@$merged_areas) {
			$merged_areas_h->{$range->[0]}{$range->[1]} = $range;
		}
	}

    my $oNewCell =
      Spreadsheet::ParseExcel::_NewCell( $oBook, $iR, $iC, %rhKey );
    Spreadsheet::ParseExcel::_SetDimension( $oBook, $iR, $iC, $iC );

	$oNewCell->{Merged} = 1
		if exists $merged_areas_h->{$iR}{$iC};

    return $oNewCell;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::SaveParser::Workbook - A class for SaveParser Workbooks.

=head1 SYNOPSIS

See the documentation for Spreadsheet::ParseExcel.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::ParseExcel. See the documentation for Spreadsheet::ParseExcel.

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
