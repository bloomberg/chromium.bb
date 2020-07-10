package Spreadsheet::ParseExcel;

##############################################################################
#
# Spreadsheet::ParseExcel - Extract information from an Excel file.
#
# Copyright (c) 2014      Douglas Wilson
# Copyright (c) 2009-2013 John McNamara
# Copyright (c) 2006-2008 Gabor Szabo
# Copyright (c) 2000-2008 Takanori Kawai
#
# perltidy with standard settings.
#
# Documentation after __END__
#

use strict;
use warnings;
use 5.008;

use OLE::Storage_Lite;
use File::Basename qw(fileparse);
use IO::File;
use Config;

use Crypt::RC4;
use Digest::Perl::MD5;

our $VERSION = '0.65';

use Spreadsheet::ParseExcel::Workbook;
use Spreadsheet::ParseExcel::Worksheet;
use Spreadsheet::ParseExcel::Font;
use Spreadsheet::ParseExcel::Format;
use Spreadsheet::ParseExcel::Cell;
use Spreadsheet::ParseExcel::FmtDefault;

my $currentbook;
my @aColor = (
    '000000',    # 0x00
    'FFFFFF', 'FFFFFF', 'FFFFFF', 'FFFFFF',
    'FFFFFF', 'FFFFFF', 'FFFFFF', '000000',    # 0x08
    'FFFFFF', 'FF0000', '00FF00', '0000FF',
    'FFFF00', 'FF00FF', '00FFFF', '800000',    # 0x10
    '008000', '000080', '808000', '800080',
    '008080', 'C0C0C0', '808080', '9999FF',    # 0x18
    '993366', 'FFFFCC', 'CCFFFF', '660066',
    'FF8080', '0066CC', 'CCCCFF', '000080',    # 0x20
    'FF00FF', 'FFFF00', '00FFFF', '800080',
    '800000', '008080', '0000FF', '00CCFF',    # 0x28
    'CCFFFF', 'CCFFCC', 'FFFF99', '99CCFF',
    'FF99CC', 'CC99FF', 'FFCC99', '3366FF',    # 0x30
    '33CCCC', '99CC00', 'FFCC00', 'FF9900',
    'FF6600', '666699', '969696', '003366',    # 0x38
    '339966', '003300', '333300', '993300',
    '993366', '333399', '333333', '000000'     # 0x40
);
use constant verExcel95 => 0x500;
use constant verExcel97 => 0x600;
use constant verBIFF2   => 0x00;
use constant verBIFF3   => 0x02;
use constant verBIFF4   => 0x04;
use constant verBIFF5   => 0x08;
use constant verBIFF8   => 0x18;

use constant MS_BIFF_CRYPTO_NONE => 0;
use constant MS_BIFF_CRYPTO_XOR  => 1;
use constant MS_BIFF_CRYPTO_RC4  => 2;

use constant sizeof_BIFF_8_FILEPASS => ( 6 + 3 * 16 );

use constant REKEY_BLOCK => 0x400;

# Error code for some of the common parsing errors.
use constant ErrorNone          => 0;
use constant ErrorNoFile        => 1;
use constant ErrorNoExcelData   => 2;
use constant ErrorFileEncrypted => 3;

# Color index for the 'auto' color
use constant AutoColor => 64;

our %error_strings = (
    ErrorNone,          '',                               # 0
    ErrorNoFile,        'File not found',                 # 1
    ErrorNoExcelData,   'No Excel data found in file',    # 2
    ErrorFileEncrypted, 'File is encrypted',              # 3

);


our %ProcTbl = (

    #Develpers' Kit P291
    0x14   => \&_subHeader,            # Header
    0x15   => \&_subFooter,            # Footer
    0x18   => \&_subName,              # NAME(?)
    0x1A   => \&_subVPageBreak,        # Vertical Page Break
    0x1B   => \&_subHPageBreak,        # Horizontal Page Break
    0x22   => \&_subFlg1904,           # 1904 Flag
    0x26   => \&_subMargin,            # Left Margin
    0x27   => \&_subMargin,            # Right Margin
    0x28   => \&_subMargin,            # Top Margin
    0x29   => \&_subMargin,            # Bottom Margin
    0x2A   => \&_subPrintHeaders,      # Print Headers
    0x2B   => \&_subPrintGridlines,    # Print Gridlines
    0x3C   => \&_subContinue,          # Continue
    0x3D   => \&_subWindow1,           # Window1
    0x43   => \&_subXF,                # XF for Excel < 4.
    0x0443 => \&_subXF,                # XF for Excel = 4.
    0x862  => \&_subSheetLayout,       # Sheet Layout
    0x1B8  => \&_subHyperlink,         # HYPERLINK

    #Develpers' Kit P292
    0x55 => \&_subDefColWidth,         # Consider
    0x5C => \&_subWriteAccess,         # WRITEACCESS
    0x7D => \&_subColInfo,             # Colinfo
    0x7E => \&_subRK,                  # RK
    0x81 => \&_subWSBOOL,              # WSBOOL
    0x83 => \&_subHcenter,             # HCENTER
    0x84 => \&_subVcenter,             # VCENTER
    0x85 => \&_subBoundSheet,          # BoundSheet

    0x92 => \&_subPalette,             # Palette, fgp

    0x99 => \&_subStandardWidth,       # Standard Col

    #Develpers' Kit P293
    0xA1 => \&_subSETUP,               # SETUP
    0xBD => \&_subMulRK,               # MULRK
    0xBE => \&_subMulBlank,            # MULBLANK
    0xD6 => \&_subRString,             # RString

    #Develpers' Kit P294
    0xE0 => \&_subXF,                  # ExTended Format
    0xE5 => \&_subMergeArea,           # MergeArea (Not Documented)
    0xFC => \&_subSST,                 # Shared String Table
    0xFD => \&_subLabelSST,            # Label SST

    #Develpers' Kit P295
    0x201 => \&_subBlank,              # Blank

    0x202 => \&_subInteger,            # Integer(Not Documented)
    0x203 => \&_subNumber,             # Number
    0x204 => \&_subLabel,              # Label
    0x205 => \&_subBoolErr,            # BoolErr
    0x207 => \&_subString,             # STRING
    0x208 => \&_subRow,                # RowData
    0x221 => \&_subArray,              # Array (Consider)
    0x225 => \&_subDefaultRowHeight,   # Consider

    0x31  => \&_subFont,               # Font
    0x231 => \&_subFont,               # Font

    0x27E => \&_subRK,                 # RK
    0x41E => \&_subFormat,             # Format

    0x06  => \&_subFormula,            # Formula
    0x406 => \&_subFormula,            # Formula

    0x009 => \&_subBOF,                # BOF(BIFF2)
    0x209 => \&_subBOF,                # BOF(BIFF3)
    0x409 => \&_subBOF,                # BOF(BIFF4)
    0x809 => \&_subBOF,                # BOF(BIFF5-8)
);

our $BIGENDIAN;
our $PREFUNC;
our $_use_perlio;

#------------------------------------------------------------------------------
# Spreadsheet::ParseExcel->new
#------------------------------------------------------------------------------
sub new {
    my ( $class, %hParam ) = @_;

    if ( not defined $_use_perlio ) {
        if (   exists $Config{useperlio}
            && defined $Config{useperlio}
            && $Config{useperlio} eq "define" )
        {
            $_use_perlio = 1;
        }
        else {
            $_use_perlio = 0;
            require IO::Scalar;
            import IO::Scalar;
        }
    }

    # Check ENDIAN(Little: Intel etc. BIG: Sparc etc)
    $BIGENDIAN =
        ( defined $hParam{Endian} ) ? $hParam{Endian}
      : ( unpack( "H08", pack( "L", 2 ) ) eq '02000000' ) ? 0
      :                                                     1;
    my $self = {};
    bless $self, $class;

    $self->{GetContent} = \&_subGetContent;

    if ( $hParam{EventHandlers} ) {
        $self->SetEventHandlers( $hParam{EventHandlers} );
    }
    else {
        $self->SetEventHandlers( \%ProcTbl );
    }
    if ( $hParam{AddHandlers} ) {
        foreach my $sKey ( keys( %{ $hParam{AddHandlers} } ) ) {
            $self->SetEventHandler( $sKey, $hParam{AddHandlers}->{$sKey} );
        }
    }
    $self->{CellHandler} = $hParam{CellHandler};
    $self->{NotSetCell}  = $hParam{NotSetCell};
    $self->{Object}      = $hParam{Object};


    if ( defined $hParam{Password} ) {
        $self->{Password} = $hParam{Password};
    }
    else {
        $self->{Password} = 'VelvetSweatshop';
    }

    $self->{_error_status} = ErrorNone;
    return $self;
}

#------------------------------------------------------------------------------
# Spreadsheet::ParseExcel->SetEventHandler
#------------------------------------------------------------------------------
sub SetEventHandler {
    my ( $self, $key, $sub_ref ) = @_;
    $self->{FuncTbl}->{$key} = $sub_ref;
}

#------------------------------------------------------------------------------
# Spreadsheet::ParseExcel->SetEventHandlers
#------------------------------------------------------------------------------
sub SetEventHandlers {
    my ( $self, $rhTbl ) = @_;
    $self->{FuncTbl} = undef;
    foreach my $sKey ( keys %$rhTbl ) {
        $self->{FuncTbl}->{$sKey} = $rhTbl->{$sKey};
    }
}

#------------------------------------------------------------------------------
# Decryption routines
# based on sources of gnumeric (ms-biff.c ms-excel-read.c)
#------------------------------------------------------------------------------
sub md5state {
    my ( $md5 ) = @_;
    my $s = '';
    for ( my $i = 0 ; $i < 4 ; $i++ ) {
        my $v = $md5->{_state}[$i];
        $s .= chr( $v & 0xff );
        $s .= chr( ( $v >> 8 ) & 0xff );
        $s .= chr( ( $v >> 16 ) & 0xff );
        $s .= chr( ( $v >> 24 ) & 0xff );
    }

    return $s;
}

sub MakeKey {
    my ( $block, $key, $valContext ) = @_;

    my $pwarray = "\0" x 64;

    substr( $pwarray, 0, 5 ) = substr( $valContext, 0, 5 );

    substr( $pwarray, 5, 1 ) = chr( $block & 0xff );
    substr( $pwarray, 6, 1 ) = chr( ( $block >> 8 ) & 0xff );
    substr( $pwarray, 7, 1 ) = chr( ( $block >> 16 ) & 0xff );
    substr( $pwarray, 8, 1 ) = chr( ( $block >> 24 ) & 0xff );

    substr( $pwarray, 9,  1 ) = "\x80";
    substr( $pwarray, 56, 1 ) = "\x48";

    my $md5 = Digest::Perl::MD5->new();
    $md5->add( $pwarray );

    my $s = md5state( $md5 );

    ${$key} = Crypt::RC4->new( $s );
}

sub VerifyPassword {
    my ( $password, $docid, $salt_data, $hashedsalt_data, $valContext ) = @_;

    my $pwarray = "\0" x 64;
    my $i;
    my $md5 = Digest::Perl::MD5->new();

    for ( $i = 0 ; $i < length( $password ) ; $i++ ) {
        my $o = ord( substr( $password, $i, 1 ) );
        substr( $pwarray, 2 * $i, 1 ) = chr( $o & 0xff );
        substr( $pwarray, 2 * $i + 1, 1 ) = chr( ( $o >> 8 ) & 0xff );
    }
    substr( $pwarray, 2 * $i, 1 ) = chr( 0x80 );
    substr( $pwarray, 56, 1 ) = chr( ( $i << 4 ) & 0xff );

    $md5->add( $pwarray );

    my $mdContext1 = md5state( $md5 );

    my $offset    = 0;
    my $keyoffset = 0;
    my $tocopy    = 5;

    $md5->reset;

    while ( $offset != 16 ) {
        if ( ( 64 - $offset ) < 5 ) {
            $tocopy = 64 - $offset;
        }

        substr( $pwarray, $offset, $tocopy ) =
          substr( $mdContext1, $keyoffset, $tocopy );

        $offset += $tocopy;

        if ( $offset == 64 ) {
            $md5->add( $pwarray );
            $keyoffset = $tocopy;
            $tocopy    = 5 - $tocopy;
            $offset    = 0;
            next;
        }

        $keyoffset = 0;
        $tocopy    = 5;
        substr( $pwarray, $offset, 16 ) = $docid;
        $offset += 16;
    }

    substr( $pwarray, 16, 1 )  = "\x80";
    substr( $pwarray, 17, 47 ) = "\0" x 47;
    substr( $pwarray, 56, 1 )  = "\x80";
    substr( $pwarray, 57, 1 )  = "\x0a";

    $md5->add( $pwarray );
    ${$valContext} = md5state( $md5 );

    my $key;

    MakeKey( 0, \$key, ${$valContext} );

    my $salt       = $key->RC4( $salt_data );
    my $hashedsalt = $key->RC4( $hashedsalt_data );

    $salt .= "\x80" . "\0" x 47;

    substr( $salt, 56, 1 ) = "\x80";

    $md5->reset;
    $md5->add( $salt );
    my $mdContext2 = md5state( $md5 );

    return ( $mdContext2 eq $hashedsalt );
}

sub SkipBytes {
    my ( $q, $start, $count ) = @_;

    my $scratch = "\0" x REKEY_BLOCK;
    my $block;

    $block = int( ( $start + $count ) / REKEY_BLOCK );

    if ( $block != $q->{block} ) {
        MakeKey( $q->{block} = $block, \$q->{rc4_key}, $q->{md5_ctxt} );
        $count = ( $start + $count ) % REKEY_BLOCK;
    }

    $q->{rc4_key}->RC4( substr( $scratch, 0, $count ) );

    return 1;
}

sub SetDecrypt {
    my ( $q, $version, $password ) = @_;

    if ( $q->{opcode} != 0x2f ) {
        return 0;
    }

    if ( $password eq '' ) {
        return 0;
    }

    # TODO old versions decryption
    #if (version < MS_BIFF_V8 || q->data[0] == 0)
    #    return ms_biff_pre_biff8_query_set_decrypt (q, password);

    if ( $q->{length} != sizeof_BIFF_8_FILEPASS ) {
        return 0;
    }

    unless (
        VerifyPassword(
            $password,
            substr( $q->{data}, 6,  16 ),
            substr( $q->{data}, 22, 16 ),
            substr( $q->{data}, 38, 16 ),
            \$q->{md5_ctxt}
        )
      )
    {
        return 0;
    }

    $q->{encryption} = MS_BIFF_CRYPTO_RC4;
    $q->{block}      = -1;

    # The first record after FILEPASS seems to be unencrypted
    $q->{dont_decrypt_next_record} = 1;

    # Pretend to decrypt the entire stream up till this point, it was
    # encrypted, but do it anyway to keep the rc4 state in sync

    SkipBytes( $q, 0, $q->{streamPos} );

    return 1;
}

sub InitStream {
    my ( $stream_data ) = @_;
    my %q;

    $q{opcode} = 0;
    $q{length} = 0;
    $q{data}   = '';

    $q{stream}    = $stream_data;              # data stream
    $q{streamLen} = length( $stream_data );    # stream length
    $q{streamPos} = 0;                         # stream position

    $q{encryption}               = 0;
    $q{xor_key}                  = '';
    $q{rc4_key}                  = '';
    $q{md5_ctxt}                 = '';
    $q{block}                    = 0;
    $q{dont_decrypt_next_record} = 0;

    return \%q;
}

sub QueryNext {
    my ( $q ) = @_;

    if ( $q->{streamPos} + 4 >= $q->{streamLen} ) {
        return 0;
    }

    my $data = substr( $q->{stream}, $q->{streamPos}, 4 );

    ( $q->{opcode}, $q->{length} ) = unpack( 'v2', $data );

    # No biff record should be larger than around 20,000.
    if ( $q->{length} >= 20000 ) {
        return 0;
    }

    if ( $q->{length} > 0 ) {
        $q->{data} = substr( $q->{stream}, $q->{streamPos} + 4, $q->{length} );
    }
    else {
        $q->{data}                     = undef;
        $q->{dont_decrypt_next_record} = 1;
    }

    if ( $q->{encryption} == MS_BIFF_CRYPTO_RC4 ) {
        if ( $q->{dont_decrypt_next_record} ) {
            SkipBytes( $q, $q->{streamPos}, 4 + $q->{length} );
            $q->{dont_decrypt_next_record} = 0;
        }
        else {
            my $pos  = $q->{streamPos};
            my $data = $q->{data};
            my $len  = $q->{length};
            my $res  = '';

            # Pretend to decrypt header.
            SkipBytes( $q, $pos, 4 );
            $pos += 4;

            while ( $q->{block} != int( ( $pos + $len ) / REKEY_BLOCK ) ) {
                my $step = REKEY_BLOCK - ( $pos % REKEY_BLOCK );
                $res .= $q->{rc4_key}->RC4( substr( $data, 0, $step ) );
                $data = substr( $data, $step );
                $pos += $step;
                $len -= $step;
                MakeKey( ++$q->{block}, \$q->{rc4_key}, $q->{md5_ctxt} );
            }

            $res .= $q->{rc4_key}->RC4( substr( $data, 0, $len ) );
            $q->{data} = $res;
        }
    }
    elsif ( $q->{encryption} == MS_BIFF_CRYPTO_XOR ) {

        # not implemented
        return 0;
    }
    elsif ( $q->{encryption} == MS_BIFF_CRYPTO_NONE ) {

    }

    $q->{streamPos} += 4 + $q->{length};

    return 1;
}

###############################################################################
#
# Parse()
#
# Parse the Excel file and convert it into a tree of objects..
#
sub parse {

    my ( $self, $source, $formatter ) = @_;

    my $workbook = Spreadsheet::ParseExcel::Workbook->new();
    $currentbook = $workbook;
    $workbook->{SheetCount} = 0;
    $workbook->{CellHandler} = $self->{CellHandler};
    $workbook->{NotSetCell}  = $self->{NotSetCell};
    $workbook->{Object}      = $self->{Object};
    $workbook->{aColor}      = [ @aColor ];

    my ( $biff_data, $data_length ) = $self->_get_content( $source, $workbook );
    return undef if not $biff_data;

    if ( $formatter ) {
        $workbook->{FmtClass} = $formatter;
    }
    else {
        $workbook->{FmtClass} = Spreadsheet::ParseExcel::FmtDefault->new();
    }

    # Parse the BIFF data.
    my $stream = InitStream( $biff_data );

    while ( QueryNext( $stream ) ) {

        my $record        = $stream->{opcode};
        my $record_length = $stream->{length};

        my $record_header = $stream->{data};

        # If the file contains a FILEPASS record we assume that it is encrypted
        # and cannot be parsed.
        if ( $record == 0x002F ) {
            unless ( SetDecrypt( $stream, '', $self->{Password} ) ) {
                $self->{_error_status} = ErrorFileEncrypted;
                return undef;
            }
        }

        # Special case of a formula String with no string.
        if (   $workbook->{_PrevPos}
            && ( defined $self->{FuncTbl}->{$record} )
            && ( $record != 0x207 ) )
        {
            my $iPos = $workbook->{_PrevPos};
            $workbook->{_PrevPos} = undef;

            my ( $row, $col, $format_index ) = @$iPos;
            _NewCell(
                $workbook, $row, $col,
                Kind     => 'Formula String',
                Val      => '',
                FormatNo => $format_index,
                Format   => $workbook->{Format}[$format_index],
                Numeric  => 0,
                Code     => undef,
                Book     => $workbook,
            );
        }

        # If the BIFF record matches 0x0*09 then it is a BOF record.
        # We reset the _skip_chart flag to ensure we check the sheet type.
        if ( ( $record & 0xF0FF ) == 0x09 ) {
            $workbook->{_skip_chart} = 0;
        }

        if ( defined $self->{FuncTbl}->{$record} && !$workbook->{_skip_chart} )
        {
            $self->{FuncTbl}->{$record}
              ->( $workbook, $record, $record_length, $record_header );
        }

        $PREFUNC = $record if ( $record != 0x3C );    #Not Continue

        last if defined $workbook->{_ParseAbort};
    }

    foreach my $worksheet (@{$workbook->{Worksheet}} ) {
        # Install hyperlinks into each cell
        # Range is undocumented for user; allows reuse of data

        if ($worksheet->{HyperLinks}) {
            foreach my $link (@{$worksheet->{HyperLinks}}) {
                for( my $row = $link->[3]; $row <= $link->[4]; $row++ ) {
                    for( my $col = $link->[5]; $col <= $link->[6]; $col++ ) {
                        $worksheet->{Cells}[$row][$col]{Hyperlink} = $link;
                    }
                }
            }
        }
    }
    return $workbook;
}

###############################################################################
#
# _get_content()
#
# Get the Excel BIFF content from the file or filehandle.
#
sub _get_content {

    my ( $self, $source, $workbook ) = @_;
    my ( $biff_data, $data_length );

    # Reset the error status in case method is called more than once.
    $self->{_error_status} = ErrorNone;
 
    my $ref = ref($source);

    if ( $ref ) {
         if ( $ref eq 'SCALAR' ) {

             # Specified by a scalar buffer.
             ( $biff_data, $data_length ) = $self->{GetContent}->( $source );

         }
         elsif ( $ref eq 'ARRAY' ) {

             # Specified by file content
             $workbook->{File} = undef;
             my $sData = join( '', @$source );
             ( $biff_data, $data_length ) = $self->{GetContent}->( \$sData );
        }
        else {

             # Assume filehandle

             # For CGI.pm (Light FileHandle)
             my $sBuff = '';
             if ( eval { binmode( $source ) } ) {
                 my $sWk;

                 while ( read( $source, $sWk, 4096 ) ) {
                     $sBuff .= $sWk;
                 }
             }
             else {

                 # Assume IO::Wrap or some other filehandle-like OO-only object
                 my $sWk;

                 # IO::Wrap does not implement binmode
                 eval { $source->binmode() };

                 while ( $source->read( $sWk, 4096 ) ) {
                     $sBuff .= $sWk;
                 }
             }

             ( $biff_data, $data_length ) = $self->{GetContent}->( \$sBuff );

         }
    }
    else {

        # Specified by filename .
        $workbook->{File} = $source;

        if ( !-e $source ) {
            $self->{_error_status} = ErrorNoFile;
            return undef;
        }

        ( $biff_data, $data_length ) = $self->{GetContent}->( $source );
    }

    # If the read was successful return the data.
    if ( $data_length ) {
        return ( $biff_data, $data_length );
    }
    else {
        $self->{_error_status} = ErrorNoExcelData;
        return undef;
    }

}

#------------------------------------------------------------------------------
# _subGetContent (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _subGetContent {
    my ( $sFile ) = @_;

    my $oOl = OLE::Storage_Lite->new( $sFile );
    return ( undef, undef ) unless ( $oOl );
    my @aRes = $oOl->getPpsSearch(
        [
            OLE::Storage_Lite::Asc2Ucs( 'Book' ),
            OLE::Storage_Lite::Asc2Ucs( 'Workbook' )
        ],
        1, 1
    );
    return ( undef, undef ) if ( $#aRes < 0 );

    #Hack from Herbert
    if ( $aRes[0]->{Data} ) {
        return ( $aRes[0]->{Data}, length( $aRes[0]->{Data} ) );
    }

    #Same as OLE::Storage_Lite
    my $oIo;

    #1. $sFile is Ref of scalar
    if ( ref( $sFile ) eq 'SCALAR' ) {
        if ( $_use_perlio ) {
            open $oIo, "<", \$sFile;
        }
        else {
            $oIo = IO::Scalar->new;
            $oIo->open( $sFile );
        }
    }

    #2. $sFile is a IO::Handle object
    elsif ( UNIVERSAL::isa( $sFile, 'IO::Handle' ) ) {
        $oIo = $sFile;
        binmode( $oIo );
    }

    #3. $sFile is a simple filename string
    elsif ( !ref( $sFile ) ) {
        $oIo = IO::File->new;
        $oIo->open( "<$sFile" ) || return undef;
        binmode( $oIo );
    }
    my $sWk;
    my $sBuff = '';

    while ( $oIo->read( $sWk, 4096 ) ) {    #4_096 has no special meanings
        $sBuff .= $sWk;
    }
    $oIo->close();

    #Not Excel file (simple method)
    return ( undef, undef ) if ( substr( $sBuff, 0, 1 ) ne "\x09" );
    return ( $sBuff, length( $sBuff ) );
}

#------------------------------------------------------------------------------
# _subBOF (for Spreadsheet::ParseExcel) Developers' Kit : P303
#------------------------------------------------------------------------------
sub _subBOF {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iVer, $iDt ) = unpack( "v2", $sWk );

    #Workbook Global
    if ( $iDt == 0x0005 ) {
        $oBook->{Version} = unpack( "v", $sWk );
        $oBook->{BIFFVersion} =
          ( $oBook->{Version} == verExcel95 ) ? verBIFF5 : verBIFF8;
        $oBook->{_CurSheet}  = undef;
        $oBook->{_CurSheet_} = -1;
    }

    #Worksheet or Dialogsheet
    elsif ( $iDt != 0x0020 ) {    #if($iDt == 0x0010)
        if ( defined $oBook->{_CurSheet_} ) {
            $oBook->{_CurSheet} = $oBook->{_CurSheet_} + 1;
            $oBook->{_CurSheet_}++;

            (
                $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{SheetVersion},
                $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{SheetType},
              )
              = unpack( "v2", $sWk )
              if ( length( $sWk ) > 4 );
        }
        else {
            $oBook->{BIFFVersion} = int( $bOp / 0x100 );
            if (   ( $oBook->{BIFFVersion} == verBIFF2 )
                || ( $oBook->{BIFFVersion} == verBIFF3 )
                || ( $oBook->{BIFFVersion} == verBIFF4 ) )
            {
                $oBook->{Version}   = $oBook->{BIFFVersion};
                $oBook->{_CurSheet} = 0;
                $oBook->{Worksheet}[ $oBook->{SheetCount} ] =
                  Spreadsheet::ParseExcel::Worksheet->new(
                    _Name    => '',
                    Name     => '',
                    _Book    => $oBook,
                    _SheetNo => $oBook->{SheetCount},
                  );
                $oBook->{SheetCount}++;
            }
        }
    }
    else {

        # Set flag to ignore all chart records until we reach another BOF.
        $oBook->{_skip_chart} = 1;
    }
}

#------------------------------------------------------------------------------
# _subBlank (for Spreadsheet::ParseExcel) DK:P303
#------------------------------------------------------------------------------
sub _subBlank {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF ) = unpack( "v3", $sWk );
    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'BLANK',
        Val      => '',
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => undef,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subInteger (for Spreadsheet::ParseExcel) Not in DK
#------------------------------------------------------------------------------
sub _subInteger {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF, $sTxt, $sDum );

    ( $iR, $iC, $iF, $sDum, $sTxt ) = unpack( "v3cv", $sWk );
    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'INTEGER',
        Val      => $sTxt,
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => undef,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subNumber (for Spreadsheet::ParseExcel)  : DK: P354
#------------------------------------------------------------------------------
sub _subNumber {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;

    my ( $iR, $iC, $iF ) = unpack( "v3", $sWk );
    my $dVal = _convDval( substr( $sWk, 6, 8 ) );
    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'Number',
        Val      => $dVal,
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 1,
        Code     => undef,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _convDval (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _convDval {
    my ( $sWk ) = @_;
    return
      unpack( "d",
        ( $BIGENDIAN ) ? pack( "c8", reverse( unpack( "c8", $sWk ) ) ) : $sWk );
}

#------------------------------------------------------------------------------
# _subRString (for Spreadsheet::ParseExcel) DK:P405
#------------------------------------------------------------------------------
sub _subRString {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF, $iL, $sTxt );
    ( $iR, $iC, $iF, $iL ) = unpack( "v4", $sWk );
    $sTxt = substr( $sWk, 8, $iL );

    #Has STRUN
    if ( length( $sWk ) > ( 8 + $iL ) ) {
        _NewCell(
            $oBook, $iR, $iC,
            Kind     => 'RString',
            Val      => $sTxt,
            FormatNo => $iF,
            Format   => $oBook->{Format}[$iF],
            Numeric  => 0,
            Code     => '_native_',                        #undef,
            Book     => $oBook,
            Rich     => substr( $sWk, ( 8 + $iL ) + 1 ),
        );
    }
    else {
        _NewCell(
            $oBook, $iR, $iC,
            Kind     => 'RString',
            Val      => $sTxt,
            FormatNo => $iF,
            Format   => $oBook->{Format}[$iF],
            Numeric  => 0,
            Code     => '_native_',
            Book     => $oBook,
        );
    }

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subBoolErr (for Spreadsheet::ParseExcel) DK:P306
#------------------------------------------------------------------------------
sub _subBoolErr {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF ) = unpack( "v3", $sWk );
    my ( $iVal, $iFlg ) = unpack( "cc", substr( $sWk, 6, 2 ) );
    my $sTxt = DecodeBoolErr( $iVal, $iFlg );

    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'BoolError',
        Val      => $sTxt,
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => undef,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

###############################################################################
#
# _subRK()
#
# Decode the RK BIFF record.
#
sub _subRK {

    my ( $workbook, $biff_number, $length, $data ) = @_;

    my ( $row, $col, $format_index, $rk_number ) = unpack( 'vvvV', $data );

    my $number = _decode_rk_number( $rk_number );

    _NewCell(
        $workbook, $row, $col,
        Kind     => 'RK',
        Val      => $number,
        FormatNo => $format_index,
        Format   => $workbook->{Format}->[$format_index],
        Numeric  => 1,
        Code     => undef,
        Book     => $workbook,
    );

    # Store the max and min row/col values.
    _SetDimension( $workbook, $row, $col, $col );
}

#------------------------------------------------------------------------------
# _subArray (for Spreadsheet::ParseExcel)   DK:P297
#------------------------------------------------------------------------------
sub _subArray {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iBR, $iER, $iBC, $iEC ) = unpack( "v2c2", $sWk );

}

#------------------------------------------------------------------------------
# _subFormula (for Spreadsheet::ParseExcel) DK:P336
#------------------------------------------------------------------------------
sub _subFormula {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF ) = unpack( "v3", $sWk );

    my ( $iFlg ) = unpack( "v", substr( $sWk, 12, 2 ) );
    if ( $iFlg == 0xFFFF ) {
        my ( $iKind ) = unpack( "c", substr( $sWk, 6, 1 ) );
        my ( $iVal )  = unpack( "c", substr( $sWk, 8, 1 ) );

        if ( ( $iKind == 1 ) or ( $iKind == 2 ) ) {
            my $sTxt =
              ( $iKind == 1 )
              ? DecodeBoolErr( $iVal, 0 )
              : DecodeBoolErr( $iVal, 1 );
            _NewCell(
                $oBook, $iR, $iC,
                Kind     => 'Formula Bool',
                Val      => $sTxt,
                FormatNo => $iF,
                Format   => $oBook->{Format}[$iF],
                Numeric  => 0,
                Code     => undef,
                Book     => $oBook,
            );
        }
        else {    # Result (Reserve Only)
            $oBook->{_PrevPos} = [ $iR, $iC, $iF ];
        }
    }
    else {
        my $dVal = _convDval( substr( $sWk, 6, 8 ) );
        _NewCell(
            $oBook, $iR, $iC,
            Kind     => 'Formula Number',
            Val      => $dVal,
            FormatNo => $iF,
            Format   => $oBook->{Format}[$iF],
            Numeric  => 1,
            Code     => undef,
            Book     => $oBook,
        );
    }

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subString (for Spreadsheet::ParseExcel)  DK:P414
#------------------------------------------------------------------------------
sub _subString {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;

    #Position (not enough for ARRAY)

    my $iPos = $oBook->{_PrevPos};
    return undef unless ( $iPos );
    $oBook->{_PrevPos} = undef;
    my ( $iR, $iC, $iF ) = @$iPos;

    my ( $iLen, $sTxt, $sCode );
    if ( $oBook->{BIFFVersion} == verBIFF8 ) {
        my ( $raBuff, $iLen ) = _convBIFF8String( $oBook, $sWk, 1 );
        $sTxt = $raBuff->[0];
        $sCode = ( $raBuff->[1] ) ? 'ucs2' : undef;
    }
    elsif ( $oBook->{BIFFVersion} == verBIFF5 ) {
        $sCode = '_native_';
        $iLen  = unpack( "v", $sWk );
        $sTxt  = substr( $sWk, 2, $iLen );
    }
    else {
        $sCode = '_native_';
        $iLen  = unpack( "c", $sWk );
        $sTxt  = substr( $sWk, 1, $iLen );
    }
    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'String',
        Val      => $sTxt,
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => $sCode,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subLabel (for Spreadsheet::ParseExcel)   DK:P344
#------------------------------------------------------------------------------
sub _subLabel {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF ) = unpack( "v3", $sWk );
    my ( $sLbl, $sCode );

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        my ( $raBuff, $iLen, $iStPos, $iLenS ) =
          _convBIFF8String( $oBook, substr( $sWk, 6 ), 1 );
        $sLbl = $raBuff->[0];
        $sCode = ( $raBuff->[1] ) ? 'ucs2' : undef;
    }

    #Before BIFF8
    else {
        $sLbl = substr( $sWk, 8 );
        $sCode = '_native_';
    }
    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'Label',
        Val      => $sLbl,
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => $sCode,
        Book     => $oBook,
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

###############################################################################
#
# _subMulRK()
#
# Decode the Multiple RK BIFF record.
#
sub _subMulRK {

    my ( $workbook, $biff_number, $length, $data ) = @_;

    # JMN: I don't know why this is here.
    return if $workbook->{SheetCount} <= 0;

    my ( $row, $first_col ) = unpack( "v2", $data );
    my $last_col = unpack( "v", substr( $data, length( $data ) - 2, 2 ) );

    # Iterate over the RK array and decode the data.
    my $pos = 4;
    for my $col ( $first_col .. $last_col ) {

        my $data = substr( $data, $pos, 6 );
        my ( $format_index, $rk_number ) = unpack 'vV', $data;
        my $number = _decode_rk_number( $rk_number );

        _NewCell(
            $workbook, $row, $col,
            Kind     => 'MulRK',
            Val      => $number,
            FormatNo => $format_index,
            Format   => $workbook->{Format}->[$format_index],
            Numeric  => 1,
            Code     => undef,
            Book     => $workbook,
        );
        $pos += 6;
    }

    # Store the max and min row/col values.
    _SetDimension( $workbook, $row, $first_col, $last_col );
}

#------------------------------------------------------------------------------
# _subMulBlank (for Spreadsheet::ParseExcel) DK:P349
#------------------------------------------------------------------------------
sub _subMulBlank {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iSc ) = unpack( "v2", $sWk );
    my $iEc = unpack( "v", substr( $sWk, length( $sWk ) - 2, 2 ) );
    my $iPos = 4;
    for ( my $iC = $iSc ; $iC <= $iEc ; $iC++ ) {
        my $iF = unpack( 'v', substr( $sWk, $iPos, 2 ) );
        _NewCell(
            $oBook, $iR, $iC,
            Kind     => 'MulBlank',
            Val      => '',
            FormatNo => $iF,
            Format   => $oBook->{Format}[$iF],
            Numeric  => 0,
            Code     => undef,
            Book     => $oBook,
        );
        $iPos += 2;
    }

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iSc, $iEc );
}

#------------------------------------------------------------------------------
# _subLabelSST (for Spreadsheet::ParseExcel) DK: P345
#------------------------------------------------------------------------------
sub _subLabelSST {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iR, $iC, $iF, $iIdx ) = unpack( 'v3V', $sWk );

    _NewCell(
        $oBook, $iR, $iC,
        Kind     => 'PackedIdx',
        Val      => $oBook->{PkgStr}[$iIdx]->{Text},
        FormatNo => $iF,
        Format   => $oBook->{Format}[$iF],
        Numeric  => 0,
        Code     => ( $oBook->{PkgStr}[$iIdx]->{Unicode} ) ? 'ucs2' : undef,
        Book     => $oBook,
        Rich     => $oBook->{PkgStr}[$iIdx]->{Rich},
    );

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iC, $iC );
}

#------------------------------------------------------------------------------
# _subFlg1904 (for Spreadsheet::ParseExcel) DK:P296
#------------------------------------------------------------------------------
sub _subFlg1904 {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    $oBook->{Flg1904} = unpack( "v", $sWk );
}

#------------------------------------------------------------------------------
# _subRow (for Spreadsheet::ParseExcel) DK:P403
#------------------------------------------------------------------------------
sub _subRow {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    #0. Get Worksheet info (MaxRow, MaxCol, MinRow, MinCol)
    my ( $iR, $iSc, $iEc, $iHght, $undef1, $undef2, $iGr, $iXf ) =
      unpack( "v8", $sWk );
    $iEc--;

    if ( $iGr & 0x20 ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{RowHidden}[$iR] = 1;
    }

    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{RowHeight}[$iR] = $iHght / 20;

    #2.MaxRow, MaxCol, MinRow, MinCol
    _SetDimension( $oBook, $iR, $iSc, $iEc );
}

#------------------------------------------------------------------------------
# _SetDimension (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _SetDimension {
    my ( $oBook, $iR, $iSc, $iEc ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    #2.MaxRow, MaxCol, MinRow, MinCol
    #2.1 MinRow
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinRow} = $iR
      unless ( defined $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinRow} )
      and ( $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinRow} <= $iR );

    #2.2 MaxRow
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxRow} = $iR
      unless ( defined $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxRow} )
      and ( $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxRow} > $iR );

    #2.3 MinCol
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinCol} = $iSc
      unless ( defined $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinCol} )
      and ( $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MinCol} <= $iSc );

    #2.4 MaxCol
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxCol} = $iEc
      unless ( defined $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxCol} )
      and ( $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{MaxCol} > $iEc );

}

#------------------------------------------------------------------------------
# _subDefaultRowHeight (for Spreadsheet::ParseExcel)    DK: P318
#------------------------------------------------------------------------------
sub _subDefaultRowHeight {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    #1. RowHeight
    my ( $iDum, $iHght ) = unpack( "v2", $sWk );
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{DefRowHeight} = $iHght / 20;

}

#------------------------------------------------------------------------------
# _subStandardWidth(for Spreadsheet::ParseExcel)    DK:P413
#------------------------------------------------------------------------------
sub _subStandardWidth {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my $iW = unpack( "v", $sWk );
    $oBook->{StandardWidth} = _convert_col_width( $oBook, $iW );
}

###############################################################################
#
# _subDefColWidth()
#
# Read the DEFCOLWIDTH Biff record. This gives the width in terms of chars
# and is different from the width in the COLINFO record.
#
sub _subDefColWidth {

    my ( $self, $record, $length, $data ) = @_;

    my $width = unpack 'v', $data;

    # Adjustment for default Arial 10 width.
    $width = 8.43 if $width == 8;

    $self->{Worksheet}->[ $self->{_CurSheet} ]->{DefColWidth} = $width;
}

###############################################################################
#
# _convert_col_width()
#
# Converts from the internal Excel column width units to user units seen in the
# interface. It is first necessary to convert the internal width to pixels and
# then to user units. The conversion is specific to a default font of Arial 10.
# TODO, the conversion should be extended to other fonts and sizes.
#
sub _convert_col_width {

    my $self        = shift;
    my $excel_width = shift;

    # Convert from Excel units to pixels (rounded up).
    my $pixels = int( 0.5 + $excel_width * 7 / 256 );

    # Convert from pixels to user units.
    # The conversion is different for columns <= 1 user unit (12 pixels).
    my $user_width;
    if ( $pixels <= 12 ) {
        $user_width = $pixels / 12;
    }
    else {
        $user_width = ( $pixels - 5 ) / 7;
    }

    # Round up to 2 decimal places.
    $user_width = int( $user_width * 100 + 0.5 ) / 100;

    return $user_width;
}

#------------------------------------------------------------------------------
# _subColInfo (for Spreadsheet::ParseExcel) DK:P309
#------------------------------------------------------------------------------
sub _subColInfo {

    my ( $oBook, $bOp, $bLen, $sWk ) = @_;

    return undef unless defined $oBook->{_CurSheet};

    my ( $iSc, $iEc, $iW, $iXF, $iGr ) = unpack( "v5", $sWk );

    for ( my $i = $iSc ; $i <= $iEc ; $i++ ) {

        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{ColWidth}[$i] =
          _convert_col_width( $oBook, $iW );

        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{ColFmtNo}[$i] = $iXF;

        if ( $iGr & 0x01 ) {
            $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{ColHidden}[$i] = 1;
        }
    }
}

#------------------------------------------------------------------------------
# _subWindow1 Window information P 273
#------------------------------------------------------------------------------
sub _subWindow1 {
    my ( $workbook, $op, $len, $wk ) = @_;

    return if ( $workbook->{BIFFVersion} <= verBIFF4() );

    my (
        $hpos,     $vpos,        $width,
        $height,   $options,     $active,
        $firsttab, $numselected, $tabbarwidth
    ) = unpack( "v9", $wk );

    $workbook->{ActiveSheet} = $active;
}

#------------------------------------------------------------------------------
# _subSheetLayout OpenOffice 5.96 (P207)
#------------------------------------------------------------------------------
sub _subSheetLayout {
    my ( $workbook, $op, $len, $wk ) = @_;

    my @unused;
    (
        my $rc,
        @unused[ 1 .. 10 ],
        @unused[ 11 .. 14 ],
        my $color, @unused[ 15, 16 ]
    ) = unpack( "vC10C4vC2", $wk );

    return unless ( $rc == 0x0862 );

    $workbook->{Worksheet}[ $workbook->{_CurSheet} ]->{TabColor} = $color;
}

#------------------------------------------------------------------------------
# _subHyperlink OpenOffice 5.96 (P182)
#
# Also see: http://msdn.microsoft.com/en-us/library/gg615407(v=office.14).aspx
#------------------------------------------------------------------------------

# Helper: Extract a GID, returns as text string

sub _getguid {
    my( $wk ) = @_;
    my( $text, $guidl, $guids1, $guids2, @guidb );

    ( $guidl, $guids1, $guids2, @guidb[0..7] ) = unpack( 'Vv2C8', $wk );

    $text =  sprintf( '%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X', $guidl, $guids1, $guids2, @guidb);
    return $text;
}

# Helper: Extract a counted (16-bit) unicode string, returns string,
# updates $offset
# $zterm == 1 if string is null-terminated. 
# $bc if length is in bytes (not chars)

sub _getustr {
    my( $wk, $offset, $zterm, $bc ) = @_;

    my $len = unpack( 'V', substr( $wk, $offset ) );
    $offset += 4;

    if( $bc ) {
        $len /= 2;
    }
    $len -= $zterm;
    my $text = join( '', map { chr $_ } unpack( "v$len", substr( $wk, $offset ) ) );
    $text =~ s/\0.*\z// if( $zterm );
    $_[1] = ( $offset += ($len + $zterm) *2 );
    return $text;
}

# HYPERLINK record

sub _subHyperlink {
    my ( $workbook, $op, $len, $wk ) = @_;

    # REF
    my( $srow, $erow, $scol, $ecol ) = unpack( 'v4', $wk );

    my $guid = _getguid( substr( $wk, 8 ) );
    return unless( $guid eq '79EAC9D0-BAF9-11CE-8C82-00AA004BA90B' );

    my( $stmvers, $flags ) = unpack( 'VV', substr( $wk, 24 ) );
    return if( $flags & 0x60 || $stmvers != 2 );

    my $offset = 32;
    my( $desc,$frame, $link, $mark );

    if( ($flags & 0x14) == 0x14 ) {
        $desc = _getustr( $wk, $offset, 1, 0 );
    }

    if( $flags & 0x80 ) {
        $frame = _getustr( $wk, $offset, 1, 0 );
    }

    $link = '';
    if( $flags & 0x100 ) {
        # UNC path
        $link = 'file:///' . _getustr( $wk, $offset, 1, 0 );
    } elsif( $flags & 0x1 )  {
        # Has link (URI)
        $guid = _getguid( substr( $wk, $offset ) );
        $offset += 16;
        if( $guid eq '79EAC9E0-BAF9-11CE-8C82-00AA004BA90B' ) {
            # URI
            $link = _getustr( $wk, $offset, 1, 1 );
        } elsif( $guid eq '00000303-0000-0000-C000-000000000046' ) {
            # Local file
            $link = 'file:///';
            # !($flags & 2) = 'relative path'
            if( !($flags & 0x2) ) {
                my $file = $workbook->{File};
                if( defined $file && length $file ) {
                    $link .= (fileparse($file))[1];
                }
                else {
                    $link .= '%REL%'
                }
            }
            my $dirn = unpack( 'v', substr( $wk, $offset ) );
            $offset += 2;
            $link .= '..\\' x $dirn;
            my $namelen = unpack( 'V', substr( $wk, $offset ) );
            $offset += 4;
            my $name = unpack( 'Z*', substr( $wk, $offset ) );
            $offset += $namelen;
            $offset += 24;
            my $size = unpack( 'V', substr( $wk, $offset ) );
            $offset += 4;
            if( $size ) {
                my $xlen = unpack( 'V', substr( $wk, $offset ) ) / 2;
                $name = join( '', map { chr $_} unpack( "v$xlen", substr( $wk, $offset+4+2) ) );
                $offset += $size;
            }
            $link .= $name;
        } else {
            return;
        }
    }

    # Text mark (Fragment identifier)
    if( $flags & 0x8 ) {
        # Cellrefs contain reserved characters, so url-encode
        my $fragment = _getustr( $wk, $offset, 1 );
        $fragment =~ s/([^\w.~-])/sprintf( '%%%02X', ord( $1 ) )/gems;
        $link .= '#' . $fragment;
    }

    # Update loop at end of parse() if this changes

    push @{ $workbook->{Worksheet}[ $workbook->{_CurSheet} ]->{HyperLinks} }, [
                         $desc, $link, $frame, $srow, $erow, $scol, $ecol ];
}

#------------------------------------------------------------------------------
# _subSST (for Spreadsheet::ParseExcel) DK:P413
#------------------------------------------------------------------------------
sub _subSST {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    _subStrWk( $oBook, substr( $sWk, 8 ) );
}

#------------------------------------------------------------------------------
# _subContinue (for Spreadsheet::ParseExcel)    DK:P311
#------------------------------------------------------------------------------
sub _subContinue {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;

    #if(defined $self->{FuncTbl}->{$bOp}) {
    #    $self->{FuncTbl}->{$PREFUNC}->($oBook, $bOp, $bLen, $sWk);
    #}

    _subStrWk( $oBook, $sWk, 1 ) if ( $PREFUNC == 0xFC );
}

#------------------------------------------------------------------------------
# _subWriteAccess (for Spreadsheet::ParseExcel) DK:P451
#------------------------------------------------------------------------------
sub _subWriteAccess {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return if ( defined $oBook->{_Author} );

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        $oBook->{Author} = _convBIFF8String( $oBook, $sWk );
    }

    #Before BIFF8
    else {
        my ( $iLen ) = unpack( "c", $sWk );
        $oBook->{Author} =
          $oBook->{FmtClass}->TextFmt( substr( $sWk, 1, $iLen ), '_native_' );
    }
}

#------------------------------------------------------------------------------
# _convBIFF8String (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _convBIFF8String {
    my ( $oBook, $sWk, $iCnvFlg ) = @_;
    my ( $iLen, $iFlg ) = unpack( "vc", $sWk );
    my ( $iHigh, $iExt, $iRich ) = ( $iFlg & 0x01, $iFlg & 0x04, $iFlg & 0x08 );
    my ( $iStPos, $iExtCnt, $iRichCnt, $sStr );

    #2. Rich and Ext
    if ( $iRich && $iExt ) {
        $iStPos = 9;
        ( $iRichCnt, $iExtCnt ) = unpack( 'vV', substr( $sWk, 3, 6 ) );
    }
    elsif ( $iRich ) {    #Only Rich
        $iStPos   = 5;
        $iRichCnt = unpack( 'v', substr( $sWk, 3, 2 ) );
        $iExtCnt  = 0;
    }
    elsif ( $iExt ) {     #Only Ext
        $iStPos   = 7;
        $iRichCnt = 0;
        $iExtCnt  = unpack( 'V', substr( $sWk, 3, 4 ) );
    }
    else {                #Nothing Special
        $iStPos   = 3;
        $iExtCnt  = 0;
        $iRichCnt = 0;
    }

    #3.Get String
    if ( $iHigh ) {       #Compressed
        $iLen *= 2;
        $sStr = substr( $sWk, $iStPos, $iLen );
        _SwapForUnicode( \$sStr );
        $sStr = $oBook->{FmtClass}->TextFmt( $sStr, 'ucs2' )
          unless ( $iCnvFlg );
    }
    else {                #Not Compressed
        $sStr = substr( $sWk, $iStPos, $iLen );
        $sStr = $oBook->{FmtClass}->TextFmt( $sStr, undef ) unless ( $iCnvFlg );
    }

    #4. return
    if ( wantarray ) {

        #4.1 Get Rich and Ext
        if ( length( $sWk ) < $iStPos + $iLen + $iRichCnt * 4 + $iExtCnt ) {
            return (
                [ undef, $iHigh, undef, undef ],
                $iStPos + $iLen + $iRichCnt * 4 + $iExtCnt,
                $iStPos, $iLen
            );
        }
        else {
            return (
                [
                    $sStr,
                    $iHigh,
                    substr( $sWk, $iStPos + $iLen, $iRichCnt * 4 ),
                    substr( $sWk, $iStPos + $iLen + $iRichCnt * 4, $iExtCnt )
                ],
                $iStPos + $iLen + $iRichCnt * 4 + $iExtCnt,
                $iStPos, $iLen
            );
        }
    }
    else {
        return $sStr;
    }
}

#------------------------------------------------------------------------------
# _subXF (for Spreadsheet::ParseExcel)     DK:P453
#------------------------------------------------------------------------------
sub _subXF {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;

    my ( $iFnt, $iIdx );
    my (
        $iLock,    $iHidden, $iStyle,  $i123,   $iAlH,    $iWrap,
        $iAlV,     $iJustL,  $iRotate, $iInd,   $iShrink, $iMerge,
        $iReadDir, $iBdrD,   $iBdrSL,  $iBdrSR, $iBdrST,  $iBdrSB,
        $iBdrSD,   $iBdrCL,  $iBdrCR,  $iBdrCT, $iBdrCB,  $iBdrCD,
        $iFillP,   $iFillCF, $iFillCB
    );


    if ( $oBook->{BIFFVersion} == verBIFF4 ) {

        # Minimal support for Excel 4. We just get the font and format indices
        # so that the cell data value can be formatted.
        ( $iFnt, $iIdx, ) = unpack( "CC", $sWk );
    }
    elsif ( $oBook->{BIFFVersion} == verBIFF8 ) {
        my ( $iGen, $iAlign, $iGen2, $iBdr1, $iBdr2, $iBdr3, $iPtn );

        ( $iFnt, $iIdx, $iGen, $iAlign, $iGen2, $iBdr1, $iBdr2, $iBdr3, $iPtn )
          = unpack( "v7Vv", $sWk );
        $iLock   = ( $iGen & 0x01 )   ? 1 : 0;
        $iHidden = ( $iGen & 0x02 )   ? 1 : 0;
        $iStyle  = ( $iGen & 0x04 )   ? 1 : 0;
        $i123    = ( $iGen & 0x08 )   ? 1 : 0;
        $iAlH    = ( $iAlign & 0x07 );
        $iWrap   = ( $iAlign & 0x08 ) ? 1 : 0;
        $iAlV    = ( $iAlign & 0x70 ) / 0x10;
        $iJustL  = ( $iAlign & 0x80 ) ? 1 : 0;

        $iRotate = ( ( $iAlign & 0xFF00 ) / 0x100 ) & 0x00FF;
        $iRotate = 90            if ( $iRotate == 255 );
        $iRotate = 90 - $iRotate if ( $iRotate > 90 );

        $iInd     = ( $iGen2 & 0x0F );
        $iShrink  = ( $iGen2 & 0x10 ) ? 1 : 0;
        $iMerge   = ( $iGen2 & 0x20 ) ? 1 : 0;
        $iReadDir = ( ( $iGen2 & 0xC0 ) / 0x40 ) & 0x03;
        $iBdrSL   = $iBdr1 & 0x0F;
        $iBdrSR   = ( ( $iBdr1 & 0xF0 ) / 0x10 ) & 0x0F;
        $iBdrST   = ( ( $iBdr1 & 0xF00 ) / 0x100 ) & 0x0F;
        $iBdrSB   = ( ( $iBdr1 & 0xF000 ) / 0x1000 ) & 0x0F;

        $iBdrCL = ( ( $iBdr2 & 0x7F ) ) & 0x7F;
        $iBdrCR = ( ( $iBdr2 & 0x3F80 ) / 0x80 ) & 0x7F;
        $iBdrD  = ( ( $iBdr2 & 0xC000 ) / 0x4000 ) & 0x3;

        $iBdrCT = ( ( $iBdr3 & 0x7F ) ) & 0x7F;
        $iBdrCB = ( ( $iBdr3 & 0x3F80 ) / 0x80 ) & 0x7F;
        $iBdrCD = ( ( $iBdr3 & 0x1FC000 ) / 0x4000 ) & 0x7F;
        $iBdrSD = ( ( $iBdr3 & 0x1E00000 ) / 0x200000 ) & 0xF;
        $iFillP = ( ( $iBdr3 & 0xFC000000 ) / 0x4000000 ) & 0x3F;

        $iFillCF = ( $iPtn & 0x7F );
        $iFillCB = ( ( $iPtn & 0x3F80 ) / 0x80 ) & 0x7F;
    }
    else {
        my ( $iGen, $iAlign, $iPtn, $iPtn2, $iBdr1, $iBdr2 );

        ( $iFnt, $iIdx, $iGen, $iAlign, $iPtn, $iPtn2, $iBdr1, $iBdr2 ) =
          unpack( "v8", $sWk );
        $iLock   = ( $iGen & 0x01 ) ? 1 : 0;
        $iHidden = ( $iGen & 0x02 ) ? 1 : 0;
        $iStyle  = ( $iGen & 0x04 ) ? 1 : 0;
        $i123    = ( $iGen & 0x08 ) ? 1 : 0;

        $iAlH   = ( $iAlign & 0x07 );
        $iWrap  = ( $iAlign & 0x08 ) ? 1 : 0;
        $iAlV   = ( $iAlign & 0x70 ) / 0x10;
        $iJustL = ( $iAlign & 0x80 ) ? 1 : 0;

        $iRotate = ( ( $iAlign & 0x300 ) / 0x100 ) & 0x3;

        $iFillCF = ( $iPtn & 0x7F );
        $iFillCB = ( ( $iPtn & 0x1F80 ) / 0x80 ) & 0x7F;

        $iFillP = ( $iPtn2 & 0x3F );
        $iBdrSB = ( ( $iPtn2 & 0x1C0 ) / 0x40 ) & 0x7;
        $iBdrCB = ( ( $iPtn2 & 0xFE00 ) / 0x200 ) & 0x7F;

        $iBdrST = ( $iBdr1 & 0x07 );
        $iBdrSL = ( ( $iBdr1 & 0x38 ) / 0x8 ) & 0x07;
        $iBdrSR = ( ( $iBdr1 & 0x1C0 ) / 0x40 ) & 0x07;
        $iBdrCT = ( ( $iBdr1 & 0xFE00 ) / 0x200 ) & 0x7F;

        $iBdrCL = ( $iBdr2 & 0x7F ) & 0x7F;
        $iBdrCR = ( ( $iBdr2 & 0x3F80 ) / 0x80 ) & 0x7F;
    }

    push @{ $oBook->{Format} }, Spreadsheet::ParseExcel::Format->new(
        FontNo => $iFnt,
        Font   => $oBook->{Font}[$iFnt],
        FmtIdx => $iIdx,

        Lock     => $iLock,
        Hidden   => $iHidden,
        Style    => $iStyle,
        Key123   => $i123,
        AlignH   => $iAlH,
        Wrap     => $iWrap,
        AlignV   => $iAlV,
        JustLast => $iJustL,
        Rotate   => $iRotate,

        Indent  => $iInd,
        Shrink  => $iShrink,
        Merge   => $iMerge,
        ReadDir => $iReadDir,

        BdrStyle => [ $iBdrSL, $iBdrSR,  $iBdrST, $iBdrSB ],
        BdrColor => [ $iBdrCL, $iBdrCR,  $iBdrCT, $iBdrCB ],
        BdrDiag  => [ $iBdrD,  $iBdrSD,  $iBdrCD ],
        Fill     => [ $iFillP, $iFillCF, $iFillCB ],
    );
}

#------------------------------------------------------------------------------
# _subFormat (for Spreadsheet::ParseExcel)  DK: P336
#------------------------------------------------------------------------------
sub _subFormat {

    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my $sFmt;

    if ( $oBook->{BIFFVersion} <= verBIFF5 ) {
        $sFmt = substr( $sWk, 3, unpack( 'c', substr( $sWk, 2, 1 ) ) );
        $sFmt = $oBook->{FmtClass}->TextFmt( $sFmt, '_native_' );
    }
    else {
        $sFmt = _convBIFF8String( $oBook, substr( $sWk, 2 ) );
    }

    my $format_index = unpack( 'v', substr( $sWk, 0, 2 ) );

    # Excel 4 and earlier used an index of 0 to indicate that a built-in format
    # that was stored implicitly.
    if ( $oBook->{BIFFVersion} <= verBIFF4 && $format_index == 0 ) {
        $format_index = keys %{ $oBook->{FormatStr} };
    }

    $oBook->{FormatStr}->{$format_index} = $sFmt;
}

#------------------------------------------------------------------------------
# _subPalette (for Spreadsheet::ParseExcel) DK: P393
#------------------------------------------------------------------------------
sub _subPalette {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    for ( my $i = 0 ; $i < unpack( 'v', $sWk ) ; $i++ ) {

        #        push @aColor, unpack('H6', substr($sWk, $i*4+2));
        $oBook->{aColor}[ $i + 8 ] = unpack( 'H6', substr( $sWk, $i * 4 + 2 ) );
    }
}

#------------------------------------------------------------------------------
# _subFont (for Spreadsheet::ParseExcel) DK:P333
#------------------------------------------------------------------------------
sub _subFont {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iHeight, $iAttr, $iCIdx, $iBold, $iSuper, $iUnderline, $sFntName );
    my ( $bBold, $bItalic, $bUnderline, $bStrikeout );

    if ( $oBook->{BIFFVersion} == verBIFF8 ) {
        ( $iHeight, $iAttr, $iCIdx, $iBold, $iSuper, $iUnderline ) =
          unpack( "v5c", $sWk );
        my ( $iSize, $iHigh ) = unpack( 'cc', substr( $sWk, 14, 2 ) );
        if ( $iHigh ) {
            $sFntName = substr( $sWk, 16, $iSize * 2 );
            _SwapForUnicode( \$sFntName );
            $sFntName = $oBook->{FmtClass}->TextFmt( $sFntName, 'ucs2' );
        }
        else {
            $sFntName = substr( $sWk, 16, $iSize );
            $sFntName = $oBook->{FmtClass}->TextFmt( $sFntName, '_native_' );
        }
        $bBold      = ( $iBold >= 0x2BC ) ? 1 : 0;
        $bItalic    = ( $iAttr & 0x02 )   ? 1 : 0;
        $bStrikeout = ( $iAttr & 0x08 )   ? 1 : 0;
        $bUnderline = ( $iUnderline )     ? 1 : 0;
    }
    elsif ( $oBook->{BIFFVersion} == verBIFF5 ) {
        ( $iHeight, $iAttr, $iCIdx, $iBold, $iSuper, $iUnderline ) =
          unpack( "v5c", $sWk );
        $sFntName =
          $oBook->{FmtClass}
          ->TextFmt( substr( $sWk, 15, unpack( "c", substr( $sWk, 14, 1 ) ) ),
            '_native_' );
        $bBold      = ( $iBold >= 0x2BC ) ? 1 : 0;
        $bItalic    = ( $iAttr & 0x02 )   ? 1 : 0;
        $bStrikeout = ( $iAttr & 0x08 )   ? 1 : 0;
        $bUnderline = ( $iUnderline )     ? 1 : 0;
    }
    else {
        ( $iHeight, $iAttr ) = unpack( "v2", $sWk );
        $iCIdx  = undef;
        $iSuper = 0;

        $bBold      = ( $iAttr & 0x01 ) ? 1 : 0;
        $bItalic    = ( $iAttr & 0x02 ) ? 1 : 0;
        $bUnderline = ( $iAttr & 0x04 ) ? 1 : 0;
        $bStrikeout = ( $iAttr & 0x08 ) ? 1 : 0;

        $sFntName = substr( $sWk, 5, unpack( "c", substr( $sWk, 4, 1 ) ) );
    }
    push @{ $oBook->{Font} }, Spreadsheet::ParseExcel::Font->new(
        Height         => $iHeight / 20.0,
        Attr           => $iAttr,
        Color          => $iCIdx,
        Super          => $iSuper,
        UnderlineStyle => $iUnderline,
        Name           => $sFntName,

        Bold      => $bBold,
        Italic    => $bItalic,
        Underline => $bUnderline,
        Strikeout => $bStrikeout,
    );

    #Skip Font[4]
    push @{ $oBook->{Font} }, {} if ( scalar( @{ $oBook->{Font} } ) == 4 );

}

#------------------------------------------------------------------------------
# _subBoundSheet (for Spreadsheet::ParseExcel): DK: P307
#------------------------------------------------------------------------------
sub _subBoundSheet {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my ( $iPos, $iGr, $iKind ) = unpack( "Lc2", $sWk );
    $iKind &= 0x0F;
    return if ( ( $iKind != 0x00 ) && ( $iKind != 0x01 ) );

    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        my ( $iSize, $iUni ) = unpack( "cc", substr( $sWk, 6, 2 ) );
        my $sWsName = substr( $sWk, 8 );
        if ( $iUni & 0x01 ) {
            _SwapForUnicode( \$sWsName );
            $sWsName = $oBook->{FmtClass}->TextFmt( $sWsName, 'ucs2' );
        }
        $oBook->{Worksheet}[ $oBook->{SheetCount} ] =
          Spreadsheet::ParseExcel::Worksheet->new(
            Name        => $sWsName,
            Kind        => $iKind,
            _Pos        => $iPos,
            _Book       => $oBook,
            _SheetNo    => $oBook->{SheetCount},
            SheetHidden => $iGr & 0x03
          );
    }
    else {
        $oBook->{Worksheet}[ $oBook->{SheetCount} ] =
          Spreadsheet::ParseExcel::Worksheet->new(
            Name =>
              $oBook->{FmtClass}->TextFmt( substr( $sWk, 7 ), '_native_' ),
            Kind        => $iKind,
            _Pos        => $iPos,
            _Book       => $oBook,
            _SheetNo    => $oBook->{SheetCount},
            SheetHidden => $iGr & 0x03
          );
    }
    $oBook->{SheetCount}++;
}

#------------------------------------------------------------------------------
# _subHeader (for Spreadsheet::ParseExcel) DK: P340
#------------------------------------------------------------------------------
sub _subHeader {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );
    my $sW;

    if ( !defined $sWk ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Header} = undef;
        return;
    }

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        $sW = _convBIFF8String( $oBook, $sWk );
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Header} =
          ( $sW eq "\x00" ) ? undef : $sW;
    }

    #Before BIFF8
    else {
        my ( $iLen ) = unpack( "c", $sWk );
        $sW =
          $oBook->{FmtClass}->TextFmt( substr( $sWk, 1, $iLen ), '_native_' );
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Header} =
          ( $sW eq "\x00\x00\x00" ) ? undef : $sW;
    }
}

#------------------------------------------------------------------------------
# _subFooter (for Spreadsheet::ParseExcel) DK: P335
#------------------------------------------------------------------------------
sub _subFooter {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );
    my $sW;

    if ( !defined $sWk ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Footer} = undef;
        return;
    }

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        $sW = _convBIFF8String( $oBook, $sWk );
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Footer} =
          ( $sW eq "\x00" ) ? undef : $sW;
    }

    #Before BIFF8
    else {
        my ( $iLen ) = unpack( "c", $sWk );
        $sW =
          $oBook->{FmtClass}->TextFmt( substr( $sWk, 1, $iLen ), '_native_' );
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Footer} =
          ( $sW eq "\x00\x00\x00" ) ? undef : $sW;
    }
}

#------------------------------------------------------------------------------
# _subHPageBreak (for Spreadsheet::ParseExcel) DK: P341
#------------------------------------------------------------------------------
sub _subHPageBreak {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my @aBreak;
    my $iCnt = unpack( "v", $sWk );

    return undef unless ( defined $oBook->{_CurSheet} );

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        for ( my $i = 0 ; $i < $iCnt ; $i++ ) {
            my ( $iRow, $iColB, $iColE ) =
              unpack( 'v3', substr( $sWk, 2 + $i * 6, 6 ) );

            #            push @aBreak, [$iRow, $iColB, $iColE];
            push @aBreak, $iRow;
        }
    }

    #Before BIFF8
    else {
        for ( my $i = 0 ; $i < $iCnt ; $i++ ) {
            my ( $iRow ) = unpack( 'v', substr( $sWk, 2 + $i * 2, 2 ) );
            push @aBreak, $iRow;

            #            push @aBreak, [$iRow, 0, 255];
        }
    }
    @aBreak = sort { $a <=> $b } @aBreak;
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{HPageBreak} = \@aBreak;
}

#------------------------------------------------------------------------------
# _subVPageBreak (for Spreadsheet::ParseExcel) DK: P447
#------------------------------------------------------------------------------
sub _subVPageBreak {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my @aBreak;
    my $iCnt = unpack( "v", $sWk );

    #BIFF8
    if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
        for ( my $i = 0 ; $i < $iCnt ; $i++ ) {
            my ( $iCol, $iRowB, $iRowE ) =
              unpack( 'v3', substr( $sWk, 2 + $i * 6, 6 ) );
            push @aBreak, $iCol;

            #            push @aBreak, [$iCol, $iRowB, $iRowE];
        }
    }

    #Before BIFF8
    else {
        for ( my $i = 0 ; $i < $iCnt ; $i++ ) {
            my ( $iCol ) = unpack( 'v', substr( $sWk, 2 + $i * 2, 2 ) );
            push @aBreak, $iCol;

            #            push @aBreak, [$iCol, 0, 65535];
        }
    }
    @aBreak = sort { $a <=> $b } @aBreak;
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{VPageBreak} = \@aBreak;
}

#------------------------------------------------------------------------------
# _subMargin (for Spreadsheet::ParseExcel) DK: P306, 345, 400, 440
#------------------------------------------------------------------------------
sub _subMargin {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    # The "Mergin" options are a workaround for a backward compatible typo.

    my $dWk = _convDval( substr( $sWk, 0, 8 ) );
    if ( $bOp == 0x26 ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{LeftMergin} = $dWk;
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{LeftMargin} = $dWk;
    }
    elsif ( $bOp == 0x27 ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{RightMergin} = $dWk;
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{RightMargin} = $dWk;
    }
    elsif ( $bOp == 0x28 ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{TopMergin} = $dWk;
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{TopMargin} = $dWk;
    }
    elsif ( $bOp == 0x29 ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{BottomMergin} = $dWk;
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{BottomMargin} = $dWk;
    }
}

#------------------------------------------------------------------------------
# _subHcenter (for Spreadsheet::ParseExcel) DK: P340
#------------------------------------------------------------------------------
sub _subHcenter {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my $iWk = unpack( "v", $sWk );
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{HCenter} = $iWk;

}

#------------------------------------------------------------------------------
# _subVcenter (for Spreadsheet::ParseExcel) DK: P447
#------------------------------------------------------------------------------
sub _subVcenter {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my $iWk = unpack( "v", $sWk );
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{VCenter} = $iWk;
}

#------------------------------------------------------------------------------
# _subPrintGridlines (for Spreadsheet::ParseExcel) DK: P397
#------------------------------------------------------------------------------
sub _subPrintGridlines {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my $iWk = unpack( "v", $sWk );
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{PrintGrid} = $iWk;

}

#------------------------------------------------------------------------------
# _subPrintHeaders (for Spreadsheet::ParseExcel) DK: P397
#------------------------------------------------------------------------------
sub _subPrintHeaders {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my $iWk = unpack( "v", $sWk );
    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{PrintHeaders} = $iWk;
}

#------------------------------------------------------------------------------
# _subSETUP (for Spreadsheet::ParseExcel) DK: P409
#------------------------------------------------------------------------------
sub _subSETUP {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    # Workaround for some apps and older Excels that don't write a
    # complete SETUP record.
    return undef if $bLen != 34;

    my $oWkS = $oBook->{Worksheet}[ $oBook->{_CurSheet} ];
    my $iGrBit;

    (
        $oWkS->{PaperSize}, $oWkS->{Scale},     $oWkS->{PageStart},
        $oWkS->{FitWidth},  $oWkS->{FitHeight}, $iGrBit,
        $oWkS->{Res},       $oWkS->{VRes},
    ) = unpack( 'v8', $sWk );

    $oWkS->{HeaderMargin} = _convDval( substr( $sWk, 16, 8 ) );
    $oWkS->{FooterMargin} = _convDval( substr( $sWk, 24, 8 ) );
    $oWkS->{Copis} = unpack( 'v2', substr( $sWk, 32, 2 ) );
    $oWkS->{LeftToRight} = ( ( $iGrBit & 0x01 ) ? 1 : 0 );
    $oWkS->{Landscape}   = ( ( $iGrBit & 0x02 ) ? 1 : 0 );
    $oWkS->{NoPls}       = ( ( $iGrBit & 0x04 ) ? 1 : 0 );
    $oWkS->{NoColor}     = ( ( $iGrBit & 0x08 ) ? 1 : 0 );
    $oWkS->{Draft}       = ( ( $iGrBit & 0x10 ) ? 1 : 0 );
    $oWkS->{Notes}       = ( ( $iGrBit & 0x20 ) ? 1 : 0 );
    $oWkS->{NoOrient}    = ( ( $iGrBit & 0x40 ) ? 1 : 0 );
    $oWkS->{UsePage}     = ( ( $iGrBit & 0x80 ) ? 1 : 0 );

    # The NoPls flag indicates that the values have not been taken from an
    # actual printer and thus may not be accurate.

    # Set default scale if NoPls otherwise it may be an invalid value of 0XFF.
    $oWkS->{Scale} = 100 if $oWkS->{NoPls};

    # Workaround for a backward compatible typo.
    $oWkS->{HeaderMergin} = $oWkS->{HeaderMargin};
    $oWkS->{FooterMergin} = $oWkS->{FooterMargin};

}

#------------------------------------------------------------------------------
# _subName (for Spreadsheet::ParseExcel) DK: P350
#------------------------------------------------------------------------------
sub _subName {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    my (
        $iGrBit, $cKey,    $cCh,    $iCce,   $ixAls,
        $iTab,   $cchCust, $cchDsc, $cchHep, $cchStatus
    ) = unpack( 'vc2v3c4', $sWk );

    #Builtin Name + Length == 1
    if ( ( $iGrBit & 0x20 ) && ( $cCh == 1 ) ) {

        #BIFF8
        if ( $oBook->{BIFFVersion} >= verBIFF8 ) {
            my $iName  = unpack( 'n', substr( $sWk, 14 ) );
            my $iSheet = unpack( 'v', substr( $sWk, 8 ) ) - 1;

            # Workaround for mal-formed Excel workbooks where Print_Title is
            # set as Global (i.e. itab = 0). Note, this will have to be
            # treated differently when we get around to handling global names.
            return undef if $iSheet == -1;

            if ( $iName == 6 ) {    #PrintArea
                my ( $iSheetW, $raArea ) = _ParseNameArea( substr( $sWk, 16 ) );
                $oBook->{PrintArea}[$iSheet] = $raArea;
            }
            elsif ( $iName == 7 ) {    #Title
                my ( $iSheetW, $raArea ) = _ParseNameArea( substr( $sWk, 16 ) );
                my @aTtlR = ();
                my @aTtlC = ();
                foreach my $raI ( @$raArea ) {
                    if ( $raI->[3] == 0xFF ) {    #Row Title
                        push @aTtlR, [ $raI->[0], $raI->[2] ];
                    }
                    else {                        #Col Title
                        push @aTtlC, [ $raI->[1], $raI->[3] ];
                    }
                }
                $oBook->{PrintTitle}[$iSheet] =
                  { Row => \@aTtlR, Column => \@aTtlC };
            }
        }
        else {
            my $iName = unpack( 'c', substr( $sWk, 14 ) );
            if ( $iName == 6 ) {                  #PrintArea
                my ( $iSheet, $raArea ) =
                  _ParseNameArea95( substr( $sWk, 15 ) );
                $oBook->{PrintArea}[$iSheet] = $raArea;
            }
            elsif ( $iName == 7 ) {               #Title
                my ( $iSheet, $raArea ) =
                  _ParseNameArea95( substr( $sWk, 15 ) );
                my @aTtlR = ();
                my @aTtlC = ();
                foreach my $raI ( @$raArea ) {
                    if ( $raI->[3] == 0xFF ) {    #Row Title
                        push @aTtlR, [ $raI->[0], $raI->[2] ];
                    }
                    else {                        #Col Title
                        push @aTtlC, [ $raI->[1], $raI->[3] ];
                    }
                }
                $oBook->{PrintTitle}[$iSheet] =
                  { Row => \@aTtlR, Column => \@aTtlC };
            }
        }
    }
}

#------------------------------------------------------------------------------
# ParseNameArea (for Spreadsheet::ParseExcel) DK: 494 (ptgAread3d)
#------------------------------------------------------------------------------
sub _ParseNameArea {
    my ( $sObj ) = @_;
    my ( $iOp );
    my @aRes = ();
    $iOp = unpack( 'C', $sObj );
    my $iSheet;
    if ( $iOp == 0x3b ) {
        my ( $iWkS, $iRs, $iRe, $iCs, $iCe ) =
          unpack( 'v5', substr( $sObj, 1 ) );
        $iSheet = $iWkS;
        push @aRes, [ $iRs, $iCs, $iRe, $iCe ];
    }
    elsif ( $iOp == 0x29 ) {
        my $iLen = unpack( 'v', substr( $sObj, 1, 2 ) );
        my $iSt = 0;
        while ( $iSt < $iLen ) {
            my ( $iOpW, $iWkS, $iRs, $iRe, $iCs, $iCe ) =
              unpack( 'cv5', substr( $sObj, $iSt + 3, 11 ) );

            if ( $iOpW == 0x3b ) {
                $iSheet = $iWkS;
                push @aRes, [ $iRs, $iCs, $iRe, $iCe ];
            }

            if ( $iSt == 0 ) {
                $iSt += 11;
            }
            else {
                $iSt += 12;    #Skip 1 byte;
            }
        }
    }
    return ( $iSheet, \@aRes );
}

#------------------------------------------------------------------------------
# ParseNameArea95 (for Spreadsheet::ParseExcel) DK: 494 (ptgAread3d)
#------------------------------------------------------------------------------
sub _ParseNameArea95 {
    my ( $sObj ) = @_;
    my ( $iOp );
    my @aRes = ();
    $iOp = unpack( 'C', $sObj );
    my $iSheet;
    if ( $iOp == 0x3b ) {
        $iSheet = unpack( 'v', substr( $sObj, 11, 2 ) );
        my ( $iRs, $iRe, $iCs, $iCe ) =
          unpack( 'v2C2', substr( $sObj, 15, 6 ) );
        push @aRes, [ $iRs, $iCs, $iRe, $iCe ];
    }
    elsif ( $iOp == 0x29 ) {
        my $iLen = unpack( 'v', substr( $sObj, 1, 2 ) );
        my $iSt = 0;
        while ( $iSt < $iLen ) {
            my $iOpW = unpack( 'c', substr( $sObj, $iSt + 3, 6 ) );
            $iSheet = unpack( 'v', substr( $sObj, $iSt + 14, 2 ) );
            my ( $iRs, $iRe, $iCs, $iCe ) =
              unpack( 'v2C2', substr( $sObj, $iSt + 18, 6 ) );
            push @aRes, [ $iRs, $iCs, $iRe, $iCe ] if ( $iOpW == 0x3b );

            if ( $iSt == 0 ) {
                $iSt += 21;
            }
            else {
                $iSt += 22;    #Skip 1 byte;
            }
        }
    }
    return ( $iSheet, \@aRes );
}

#------------------------------------------------------------------------------
# _subBOOL (for Spreadsheet::ParseExcel) DK: P452
#------------------------------------------------------------------------------
sub _subWSBOOL {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{PageFit} =
      ( ( unpack( 'v', $sWk ) & 0x100 ) ? 1 : 0 );
}

#------------------------------------------------------------------------------
# _subMergeArea (for Spreadsheet::ParseExcel) DK: (Not)
#------------------------------------------------------------------------------
sub _subMergeArea {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    return undef unless ( defined $oBook->{_CurSheet} );

    my $iCnt = unpack( "v", $sWk );
    my $oWkS = $oBook->{Worksheet}[ $oBook->{_CurSheet} ];
    $oWkS->{MergedArea} = [] unless ( defined $oWkS->{MergedArea} );
    for ( my $i = 0 ; $i < $iCnt ; $i++ ) {
        my ( $iRs, $iRe, $iCs, $iCe ) =
          unpack( 'v4', substr( $sWk, $i * 8 + 2, 8 ) );
        for ( my $iR = $iRs ; $iR <= $iRe ; $iR++ ) {
            for ( my $iC = $iCs ; $iC <= $iCe ; $iC++ ) {
                $oWkS->{Cells}[$iR][$iC]->{Merged} = 1
                  if ( defined $oWkS->{Cells}[$iR][$iC] );
            }
        }
        push @{ $oWkS->{MergedArea} }, [ $iRs, $iCs, $iRe, $iCe ];
    }
}

#------------------------------------------------------------------------------
# DecodeBoolErr (for Spreadsheet::ParseExcel) DK: P306
#------------------------------------------------------------------------------
sub DecodeBoolErr {
    my ( $iVal, $iFlg ) = @_;
    if ( $iFlg ) {    # ERROR
        if ( $iVal == 0x00 ) {
            return "#NULL!";
        }
        elsif ( $iVal == 0x07 ) {
            return "#DIV/0!";
        }
        elsif ( $iVal == 0x0F ) {
            return "#VALUE!";
        }
        elsif ( $iVal == 0x17 ) {
            return "#REF!";
        }
        elsif ( $iVal == 0x1D ) {
            return "#NAME?";
        }
        elsif ( $iVal == 0x24 ) {
            return "#NUM!";
        }
        elsif ( $iVal == 0x2A ) {
            return "#N/A!";
        }
        else {
            return "#ERR";
        }
    }
    else {
        return ( $iVal ) ? "TRUE" : "FALSE";
    }
}

###############################################################################
#
# _decode_rk_number()
#
# Convert an encoded RK number into a real number. The RK encoding is
# explained in some detail in the MS docs. It is a way of storing applicable
# ints and doubles in 32bits (30 data + 2 info bits) in order to save space.
#
sub _decode_rk_number {

    my $rk_number = shift;
    my $number;

    # Check the main RK type.
    if ( $rk_number & 0x02 ) {

        # RK Type 2 and 4, a packed integer.

        # Shift off the info bits.
        $number = $rk_number >> 2;

        # Convert from unsigned to signed if required.
        $number -= 0x40000000 if $number & 0x20000000;
    }
    else {

        # RK Type 1 and 3, a truncated IEEE Double.

        # Pack the RK number into the high 30 bits of an IEEE double.
        $number = pack "VV", 0x0000, $rk_number & 0xFFFFFFFC;

        # Reverse the packed IEEE double on big-endian machines.
        $number = reverse $number if $BIGENDIAN;

        # Unpack the number.
        $number = unpack "d", $number;
    }

    # RK Types 3 and 4 were multiplied by 100 prior to encoding.
    $number /= 100 if $rk_number & 0x01;

    return $number;
}

###############################################################################
#
# _subStrWk()
#
# Extract the workbook strings from the SST (Shared String Table) record and
# any following CONTINUE records.
#
# The workbook strings are initially contained in the SST block but may also
# occupy one or more CONTINUE blocks. Reading the CONTINUE blocks is made a
# little tricky by the fact that they can contain an additional initial byte
# if a string is continued from a previous block.
#
# Parsing is further complicated by the fact that the continued section of the
# string may have a different encoding (ASCII or UTF-8) from the previous
# section. Excel does this to save space.
#
sub _subStrWk {

    my ( $self, $biff_data, $is_continue ) = @_;

    if ( $is_continue ) {

        # We are reading a CONTINUE record.

        if ( $self->{_buffer} eq '' ) {

            # A CONTINUE block with no previous SST.
            $self->{_buffer} .= $biff_data;
        }
        elsif ( !defined $self->{_string_continued} ) {

            # The CONTINUE block starts with a new (non-continued) string.

            # Strip the Grbit byte and store the string data.
            $self->{_buffer} .= substr $biff_data, 1;
        }
        else {

            # A CONTINUE block that starts with a continued string.

            # The first byte (Grbit) of the CONTINUE record indicates if (0)
            # the continued string section is single bytes or (1) double bytes.
            my $grbit = ord $biff_data;

            my ( $str_position, $str_length ) = @{ $self->{_previous_info} };
            my $buff_length = length $self->{_buffer};

            if ( $buff_length >= ( $str_position + $str_length ) ) {

                # Not in a string.
                $self->{_buffer} .= $biff_data;
            }
            elsif ( ( $self->{_string_continued} & 0x01 ) == ( $grbit & 0x01 ) )
            {

                # Same encoding as the previous block of the string.
                $self->{_buffer} .= substr( $biff_data, 1 );
            }
            else {

                # Different encoding to the previous block of the string.
                if ( $grbit & 0x01 ) {

                    # Current block is UTF-16, previous was ASCII.
                    my ( undef, $cch ) = unpack 'vc', $self->{_buffer};
                    substr( $self->{_buffer}, 2, 1 ) = pack( 'C', $cch | 0x01 );

                    # Convert the previous ASCII, single character, portion of
                    # the string into a double character UTF-16 string by
                    # inserting zero bytes.
                    for (
                        my $i = ( $buff_length - $str_position ) ;
                        $i >= 1 ;
                        $i--
                      )
                    {
                        substr( $self->{_buffer}, $str_position + $i, 0 ) =
                          "\x00";
                    }

                }
                else {

                    # Current block is ASCII, previous was UTF-16.

                    # Convert the current ASCII, single character, portion of
                    # the string into a double character UTF-16 string by
                    # inserting null bytes.
                    my $change_length =
                      ( $str_position + $str_length ) - $buff_length;

                    # Length of the current CONTINUE record data.
                    my $biff_length = length $biff_data;

                    # Restrict the portion to be changed to the current block
                    # if the string extends over more than one block.
                    if ( $change_length > ( $biff_length - 1 ) * 2 ) {
                        $change_length = ( $biff_length - 1 ) * 2;
                    }

                    # Insert the null bytes.
                    for ( my $i = ( $change_length / 2 ) ; $i >= 1 ; $i-- ) {
                        substr( $biff_data, $i + 1, 0 ) = "\x00";
                    }

                }

                # Strip the Grbit byte and store the string data.
                $self->{_buffer} .= substr $biff_data, 1;
            }
        }
    }
    else {

        # Not a CONTINUE block therefore an SST block.
        $self->{_buffer} .= $biff_data;
    }

    # Reset the state variables.
    $self->{_string_continued} = undef;
    $self->{_previous_info}    = undef;

    # Extract out any full strings from the current buffer leaving behind a
    # partial string that is continued into the next block, or an empty
    # buffer is no string is continued.
    while ( length $self->{_buffer} >= 4 ) {
        my ( $str_info, $length, $str_position, $str_length ) =
          _convBIFF8String( $self, $self->{_buffer}, 1 );

        if ( defined $str_info->[0] ) {
            push @{ $self->{PkgStr} },
              {
                Text    => $str_info->[0],
                Unicode => $str_info->[1],
                Rich    => $str_info->[2],
                Ext     => $str_info->[3],
              };
            $self->{_buffer} = substr( $self->{_buffer}, $length );
        }
        else {
            $self->{_string_continued} = $str_info->[1];
            $self->{_previous_info} = [ $str_position, $str_length ];
            last;
        }
    }
}

#------------------------------------------------------------------------------
# _SwapForUnicode (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _SwapForUnicode {
    my ( $sObj ) = @_;

    #    for(my $i = 0; $i<length($$sObj); $i+=2){
    for ( my $i = 0 ; $i < ( int( length( $$sObj ) / 2 ) * 2 ) ; $i += 2 ) {
        my $sIt = substr( $$sObj, $i, 1 );
        substr( $$sObj, $i, 1 ) = substr( $$sObj, $i + 1, 1 );
        substr( $$sObj, $i + 1, 1 ) = $sIt;
    }
}

#------------------------------------------------------------------------------
# _NewCell (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub _NewCell {
    my ( $oBook, $iR, $iC, %rhKey ) = @_;
    my ( $sWk, $iLen );
    return undef unless ( defined $oBook->{_CurSheet} );

    my $FmtClass = $oBook->{FmtClass};
    $rhKey{Type} =
      $FmtClass->ChkType( $rhKey{Numeric}, $rhKey{Format}{FmtIdx} );
    my $FmtStr = $oBook->{FormatStr}{ $rhKey{Format}{FmtIdx} };

    # Set "Date" type if required for numbers in a MulRK BIFF block.
    if ( defined $FmtStr && $rhKey{Type} eq "Numeric" ) {

        # Match a range of possible date formats. Note: this isn't important
        # except for reporting. The number will still be converted to a date
        # by ExcelFmt() even if 'Type' isn't set to 'Date'.
        if ( $FmtStr =~ m{^[dmy][-\\/dmy]*$}i ) {
            $rhKey{Type} = "Date";
        }
    }

    my $oCell = Spreadsheet::ParseExcel::Cell->new(
        Val      => $rhKey{Val},
        FormatNo => $rhKey{FormatNo},
        Format   => $rhKey{Format},
        Code     => $rhKey{Code},
        Type     => $rhKey{Type},
    );
    $oCell->{_Kind} = $rhKey{Kind};
    $oCell->{_Value} = $FmtClass->ValFmt( $oCell, $oBook );
    if ( $rhKey{Rich} ) {
        my @aRich = ();
        my $sRich = $rhKey{Rich};
        for ( my $iWk = 0 ; $iWk < length( $sRich ) ; $iWk += 4 ) {
            my ( $iPos, $iFnt ) = unpack( 'v2', substr( $sRich, $iWk ) );
            push @aRich, [ $iPos, $oBook->{Font}[$iFnt] ];
        }
        $oCell->{Rich} = \@aRich;
    }

    if ( defined $oBook->{CellHandler} ) {
        if ( defined $oBook->{Object} ) {
            no strict;
            ref( $oBook->{CellHandler} ) eq "CODE"
              ? $oBook->{CellHandler}->(
                $_Object, $oBook, $oBook->{_CurSheet}, $iR, $iC, $oCell
              )
              : $oBook->{CellHandler}->callback( $_Object, $oBook, $oBook->{_CurSheet},
                $iR, $iC, $oCell );
        }
        else {
            $oBook->{CellHandler}->( $oBook, $oBook->{_CurSheet}, $iR, $iC, $oCell );
        }
    }
    unless ( $oBook->{NotSetCell} ) {
        $oBook->{Worksheet}[ $oBook->{_CurSheet} ]->{Cells}[$iR][$iC] = $oCell;
    }
    return $oCell;
}

#------------------------------------------------------------------------------
# ColorIdxToRGB (for Spreadsheet::ParseExcel)
#
# Returns for most recently opened book for compatibility, use
# Workbook::color_idx_to_rgb instead
#
#------------------------------------------------------------------------------
sub ColorIdxToRGB {
    my ( $sPkg, $iIdx ) = @_;


    unless( defined $currentbook ) {
	return ( ( defined $aColor[$iIdx] ) ? $aColor[$iIdx] : $aColor[0] );
    }

    return $currentbook->color_idx_to_rgb( $iIdx );
}


###############################################################################
#
# error().
#
# Return an error string for a failed parse().
#
sub error {

    my $self = shift;

    my $parse_error = $self->{_error_status};

    if ( exists $error_strings{$parse_error} ) {
        return $error_strings{$parse_error};
    }
    else {
        return 'Unknown parse error';
    }
}


###############################################################################
#
# error_code().
#
# Return an error code for a failed parse().
#
sub error_code {

    my $self = shift;

    return $self->{_error_status};
}


###############################################################################
#
# Mapping between legacy method names and new names.
#
{
    no warnings;    # Ignore warnings about variables used only once.
    *Parse = *parse;
}

1;

__END__

=head1 NAME

Spreadsheet::ParseExcel - Read information from an Excel file.

=head1 SYNOPSIS

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::ParseExcel;

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

    if ( !defined $workbook ) {
        die $parser->error(), ".\n";
    }

    for my $worksheet ( $workbook->worksheets() ) {

        my ( $row_min, $row_max ) = $worksheet->row_range();
        my ( $col_min, $col_max ) = $worksheet->col_range();

        for my $row ( $row_min .. $row_max ) {
            for my $col ( $col_min .. $col_max ) {

                my $cell = $worksheet->get_cell( $row, $col );
                next unless $cell;

                print "Row, Col    = ($row, $col)\n";
                print "Value       = ", $cell->value(),       "\n";
                print "Unformatted = ", $cell->unformatted(), "\n";
                print "\n";
            }
        }
    }


=head1 DESCRIPTION

The Spreadsheet::ParseExcel module can be used to read information from Excel 95-2003 binary files.

The module cannot read files in the Excel 2007 Open XML XLSX format. See the L<Spreadsheet::XLSX> module instead.

=head1 Parser

=head2 new()

The C<new()> method is used to create a new C<Spreadsheet::ParseExcel> parser object.

    my $parser = Spreadsheet::ParseExcel->new();

It is possible to pass a password to decrypt an encrypted file:

    $parser = Spreadsheet::ParseExcel->new( Password => 'secret' );

Only the default Excel encryption scheme is currently supported. See L</Decryption>.

As an advanced feature it is also possible to pass a call-back handler to the parser to control the parsing of the spreadsheet.

    $parser = Spreadsheet::ParseExcel->new(
        CellHandler => \&cell_handler,
        NotSetCell  => 1,
    );

The call-back can be used to ignore certain cells or to reduce memory usage. See the section L<Reducing the memory usage of Spreadsheet::ParseExcel> for more information.


=head2 parse($filename, $formatter)

The Parser C<parse()> method returns a L</Workbook> object.

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

If an error occurs C<parse()> returns C<undef>. In general, programs should contain a test for failed parsing as follows:

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

    if ( !defined $workbook ) {
        die $parser->error(), ".\n";
    }

The C<$filename> parameter is generally the file to be parsed. However, it can also be a filehandle or a scalar reference.

The optional C<$formatter> parameter can be an reference to a L</Formatter Class> to format the value of cells. This is useful for parsing workbooks with Unicode or Asian characters:

    my $parser    = Spreadsheet::ParseExcel->new();
    my $formatter = Spreadsheet::ParseExcel::FmtJapan->new();
    my $workbook  = $parser->parse( 'Book1.xls', $formatter );

The L<Spreadsheet::ParseExcel::FmtJapan> formatter also supports Unicode. If you encounter any encoding problems with the default formatter try that instead.


=head2 error()

The Parser C<error()> method returns an error string if a C<parse()> fails:

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

    if ( !defined $workbook ) {
        die $parser->error(), ".\n";
    }

If you wish to generate you own error string you can use the C<error_code()> method instead (see below). The C<error()> and C<error_code()> values are as follows:

    error()                         error_code()
    =======                         ============
    ''                              0
    'File not found'                1
    'No Excel data found in file'   2
    'File is encrypted'             3


The C<error_code()> method is explained below.

Spreadsheet::ParseExcel will try to decrypt an encrypted Excel file using the default password or a user supplied password passed to C<new()>, see above. If these fail the module will return the C<'File is encrypted'> error. Only the default Excel encryption scheme is currently supported, see L</Decryption>.


=head2 error_code()

The Parser C<error_code()> method returns an error code if a C<parse()> fails:

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

    if ( !defined $workbook ) {
        die "Got error code ", $parser->error_code, ".\n";
    }

This can be useful if you wish to employ you own error strings or error handling methods.


=head1 Workbook

A C<Spreadsheet::ParseExcel::Workbook> is created via the C<Spreadsheet::ParseExcel> C<parse()> method:

    my $parser   = Spreadsheet::ParseExcel->new();
    my $workbook = $parser->parse('Book1.xls');

The main methods of the Workbook class are:

    $workbook->worksheets()
    $workbook->worksheet()
    $workbook->worksheet_count()
    $workbook->get_filename()

These more commonly used methods of the Workbook class are outlined below. The other, less commonly used, methods are documented in L<Spreadsheet::ParseExcel::Worksheet>.


=head2 worksheets()

Returns an array of L</Worksheet> objects. This was most commonly used to iterate over the worksheets in a workbook:

    for my $worksheet ( $workbook->worksheets() ) {
        ...
    }


=head2 worksheet()

The C<worksheet()> method returns a single C<Worksheet> object using either its name or index:

    $worksheet = $workbook->worksheet('Sheet1');
    $worksheet = $workbook->worksheet(0);

Returns C<undef> if the sheet name or index doesn't exist.


=head2 worksheet_count()

The C<worksheet_count()> method returns the number of Worksheet objects in the Workbook.

    my $worksheet_count = $workbook->worksheet_count();


=head2 get_filename()

The C<get_filename()> method returns the name of the Excel file of C<undef> if the data was read from a filehandle rather than a file.

    my $filename = $workbook->get_filename();


=head2 Other Workbook Methods

For full documentation of the methods available via a Workbook object see L<Spreadsheet::ParseExcel::Workbook>.

=head1 Worksheet

The C<Spreadsheet::ParseExcel::Worksheet> class encapsulates the properties of an Excel worksheet.

A Worksheet object is obtained via the L</worksheets()> or L</worksheet()> methods.

    for my $worksheet ( $workbook->worksheets() ) {
        ...
    }

    # Or:

    $worksheet = $workbook->worksheet('Sheet1');
    $worksheet = $workbook->worksheet(0);

The most commonly used methods of the Worksheet class are:

    $worksheet->get_cell()
    $worksheet->row_range()
    $worksheet->col_range()
    $worksheet->get_name()

The Spreadsheet::ParseExcel::Worksheet class exposes a lot of methods but in general very few are required unless you are writing an advanced filter.

The most commonly used methods are detailed below. The others are documented in L<Spreadsheet::ParseExcel::Worksheet>.

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

The C<get_name()> method returns the name of the worksheet, such as 'Sheet1'.

    my $name = $worksheet->get_name();

=head2 Other Worksheet Methods

For other, less commonly used, Worksheet methods see L<Spreadsheet::ParseExcel::Worksheet>.

=head1 Cell

The C<Spreadsheet::ParseExcel::Cell> class has the following main methods.

    $cell->value()
    $cell->unformatted()

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

=head2 Other Cell Methods

For other, less commonly used, Worksheet methods see L<Spreadsheet::ParseExcel::Cell>.


=head1 Format

The C<Spreadsheet::ParseExcel::Format> class has the following properties:

=head2 Format properties

    $format->{Font}
    $format->{AlignH}
    $format->{AlignV}
    $format->{Indent}
    $format->{Wrap}
    $format->{Shrink}
    $format->{Rotate}
    $format->{JustLast}
    $format->{ReadDir}
    $format->{BdrStyle}
    $format->{BdrColor}
    $format->{BdrDiag}
    $format->{Fill}
    $format->{Lock}
    $format->{Hidden}
    $format->{Style}

These properties are generally only of interest to advanced users. Casual users can skip this section.

=head2 $format->{Font}

Returns the L</Font> object for the Format.

=head2 $format->{AlignH}

Returns the horizontal alignment of the format where the value has the following meaning:

    0 => No alignment
    1 => Left
    2 => Center
    3 => Right
    4 => Fill
    5 => Justify
    6 => Center across
    7 => Distributed/Equal spaced

=head2 $format->{AlignV}

Returns the vertical alignment of the format where the value has the following meaning:

    0 => Top
    1 => Center
    2 => Bottom
    3 => Justify
    4 => Distributed/Equal spaced

=head2 $format->{Indent}

Returns the indent level of the C<Left> horizontal alignment.

=head2 $format->{Wrap}

Returns true if textwrap is on.

=head2 $format->{Shrink}

Returns true if "Shrink to fit" is set for the format.

=head2 $format->{Rotate}

Returns the text rotation. In Excel97+, it returns the angle in degrees of the text rotation.

In Excel95 or earlier it returns a value as follows:

    0 => No rotation
    1 => Top down
    2 => 90 degrees anti-clockwise,
    3 => 90 clockwise

=head2 $format->{JustLast}

Return true if the "justify last" property is set for the format.

=head2 $format->{ReadDir}

Returns the direction that the text is read from.

=head2 $format->{BdrStyle}

Returns an array ref of border styles as follows:

    [ $left, $right, $top, $bottom ]

=head2 $format->{BdrColor}

Returns an array ref of border color indexes as follows:

    [ $left, $right, $top, $bottom ]

=head2 $format->{BdrDiag}

Returns an array ref of diagonal border kind, style and color index as follows:

    [$kind, $style, $color ]

Where kind is:

    0 => None
    1 => Right-Down
    2 => Right-Up
    3 => Both

=head2 $format->{Fill}

Returns an array ref of fill pattern and color indexes as follows:

    [ $pattern, $front_color, $back_color ]

=head2 $format->{Lock}

Returns true if the cell is locked.

=head2 $format->{Hidden}

Returns true if the cell is Hidden.

=head2 $format->{Style}

Returns true if the format is a Style format.




=head1 Font

I<Spreadsheet::ParseExcel::Font>

Format class has these properties:

=head1 Font Properties

    $font->{Name}
    $font->{Bold}
    $font->{Italic}
    $font->{Height}
    $font->{Underline}
    $font->{UnderlineStyle}
    $font->{Color}
    $font->{Strikeout}
    $font->{Super}

=head2 $font->{Name}

Returns the name of the font, for example 'Arial'.

=head2 $font->{Bold}

Returns true if the font is bold.

=head2 $font->{Italic}

Returns true if the font is italic.

=head2 $font->{Height}

Returns the size (height) of the font.

=head2 $font->{Underline}

Returns true if the font in underlined.

=head2 $font->{UnderlineStyle}

Returns the style of an underlined font where the value has the following meaning:

     0 => None
     1 => Single
     2 => Double
    33 => Single accounting
    34 => Double accounting

=head2 $font->{Color}

Returns the color index for the font. The mapping to an RGB color is defined by each workbook.

The index can be converted to a RGB string using the C<$workbook->ColorIdxToRGB()> Parser method.

(Older versions of C<Spreadsheet::ParseExcel> provided the C<ColorIdxToRGB> class method, which is deprecated.)


=head2 $font->{Strikeout}

Returns true if the font has the strikeout property set.

=head2 $font->{Super}

Returns one of the following values if the superscript or subscript property of the font is set:

    0 => None
    1 => Superscript
    2 => Subscript

=head1 Formatter Class

Formatters can be passed to the C<parse()> method to deal with Unicode or Asian formatting.

Spreadsheet::ParseExcel includes 2 formatter classes. C<FmtDefault> and C<FmtJapanese>. It is also possible to create a user defined formatting class.

The formatter class C<Spreadsheet::ParseExcel::Fmt*> should provide the following functions:


=head2 ChkType($self, $is_numeric, $format_index)

Method to check the type of data in the cell. Should return C<Date>, C<Numeric> or C<Text>. It is passed the following parameters:

=over

=item $self

A scalar reference to the Formatter object.

=item $is_numeric

If true, the value seems to be number.

=item $format_index

The index number for the cell Format object.

=back

=head2 TextFmt($self, $string_data, $string_encoding)

Converts the string data in the cell into the correct encoding.  It is passed the following parameters:

=over

=item $self

A scalar reference to the Formatter object.

=item $string_data

The original string/text data.

=item $string_encoding

The character encoding of original string/text.

=back

=head2 ValFmt($self, $cell, $workbook)

Convert the original unformatted cell value into the appropriate formatted value. For instance turn a number into a formatted date.  It is passed the following parameters:

=over

=item $self

A scalar reference to the Formatter object.

=item $cell

A scalar reference to the Cell object.

=item $workbook

A scalar reference to the Workbook object.

=back


=head2 FmtString($self, $cell, $workbook)

Get the format string for the Cell.  It is passed the following parameters:

=over

=item $self

A scalar reference to the Formatter object.

=item $cell

A scalar reference to the Cell object.

=item $workbook

A scalar reference to the Workbook object.

=back


=head1 Reducing the memory usage of Spreadsheet::ParseExcel

In some cases a C<Spreadsheet::ParseExcel> application may consume a lot of memory when processing a large Excel file and, as a result, may fail to complete. The following explains why this can occur and how to resolve it.

C<Spreadsheet::ParseExcel> processes an Excel file in two stages. In the first stage it extracts the Excel binary stream from the OLE container file using C<OLE::Storage_Lite>. In the second stage it parses the binary stream to read workbook, worksheet and cell data which it then stores in memory. The majority of the memory usage is required for storing cell data.

The reason for this is that as the Excel file is parsed and each cell is encountered a cell handling function creates a relatively large nested cell object that contains the cell value and all of the data that relates to the cell formatting. For large files (a 10MB Excel file on a 256MB system) this overhead can cause the system to grind to a halt.

However, in a lot of cases when an Excel file is being processed the only information that is required are the cell values. In these cases it is possible to avoid most of the memory overhead by specifying your own cell handling function and by telling Spreadsheet::ParseExcel not to store the parsed cell data. This is achieved by passing a cell handler function to C<new()> when creating the parse object. Here is an example.

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::ParseExcel;

    my $parser = Spreadsheet::ParseExcel->new(
        CellHandler => \&cell_handler,
        NotSetCell  => 1
    );

    my $workbook = $parser->parse('file.xls');

    sub cell_handler {

        my $workbook    = $_[0];
        my $sheet_index = $_[1];
        my $row         = $_[2];
        my $col         = $_[3];
        my $cell        = $_[4];

        # Do something useful with the formatted cell value
        print $cell->value(), "\n";

    }


The user specified cell handler is passed as a code reference to C<new()> along with the parameter C<NotSetCell> which tells Spreadsheet::ParseExcel not to store the parsed cell. Note, you don't have to iterate over the rows and columns, this happens automatically as part of the parsing.

The cell handler is passed 5 arguments. The first, C<$workbook>, is a reference to the C<Spreadsheet::ParseExcel::Workbook> object that represent the parsed workbook. This can be used to access any of the C<Spreadsheet::ParseExcel::Workbook> methods, see L</Workbook>. The second C<$sheet_index> is the zero-based index of the worksheet being parsed. The third and fourth, C<$row> and C<$col>, are the zero-based row and column number of the cell. The fifth, C<$cell>, is a reference to the C<Spreadsheet::ParseExcel::Cell> object. This is used to extract the data from the cell. See L</Cell> for more information.

This technique can be useful if you are writing an Excel to database filter since you can put your DB calls in the cell handler.

If you don't want all of the data in the spreadsheet you can add some control logic to the cell handler. For example we can extend the previous example so that it only prints the first 10 rows of the first two worksheets in the parsed workbook by adding some C<if()> statements to the cell handler:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::ParseExcel;

    my $parser = Spreadsheet::ParseExcel->new(
        CellHandler => \&cell_handler,
        NotSetCell  => 1
    );

    my $workbook = $parser->parse('file.xls');

    sub cell_handler {

        my $workbook    = $_[0];
        my $sheet_index = $_[1];
        my $row         = $_[2];
        my $col         = $_[3];
        my $cell        = $_[4];

        # Skip some worksheets and rows (inefficiently).
        return if $sheet_index >= 3;
        return if $row >= 10;

        # Do something with the formatted cell value
        print $cell->value(), "\n";

    }


However, this still processes the entire workbook. If you wish to save some additional processing time you can abort the parsing after you have read the data that you want, using the workbook C<ParseAbort> method:

    #!/usr/bin/perl -w

    use strict;
    use Spreadsheet::ParseExcel;

    my $parser = Spreadsheet::ParseExcel->new(
        CellHandler => \&cell_handler,
        NotSetCell  => 1
    );

    my $workbook = $parser->parse('file.xls');

    sub cell_handler {

        my $workbook    = $_[0];
        my $sheet_index = $_[1];
        my $row         = $_[2];
        my $col         = $_[3];
        my $cell        = $_[4];

        # Skip some worksheets and rows (more efficiently).
        if ( $sheet_index >= 1 and $row >= 10 ) {
            $workbook->ParseAbort(1);
            return;
        }

        # Do something with the formatted cell value
        print $cell->value(), "\n";

    }

=head1 Decryption

If a workbook is "protected" then Excel will encrypt the file whether a password is supplied or not. As of version 0.59 Spreadsheet::ParseExcel supports decrypting Excel workbooks using a default or user supplied password. However, only the following encryption scheme is supported:

    Office 97/2000 Compatible encryption

The following encryption methods are not supported:

    Weak Encryption (XOR)
    RC4, Microsoft Base Cryptographic Provider v1.0
    RC4, Microsoft Base DSS and Diffie-Hellman Cryptographic Provider
    RC4, Microsoft DH SChannel Cryptographic Provider
    RC4, Microsoft Enhanced Cryptographic Provider v1.0
    RC4, Microsoft Enhanced DSS and Diffie-Hellman Cryptographic Provider
    RC4, Microsoft Enhanced RSA and AES Cryptographic Provider
    RC4, Microsoft RSA SChannel Cryptographic Provider
    RC4, Microsoft Strong Cryptographic Provider

See the following for more information on Excel encryption: L<http://office.microsoft.com/en-us/office-2003-resource-kit/important-aspects-of-password-and-encryption-protection-HA001140311.aspx>.



=head1 KNOWN PROBLEMS

=over

=item * Issues reported by users: L<http://rt.cpan.org/Public/Dist/Display.html?Name=Spreadsheet-ParseExcel>

=item * This module cannot read the values of formulas from files created with Spreadsheet::WriteExcel unless the user specified the values when creating the file (which is generally not the case). The reason for this is that Spreadsheet::WriteExcel writes the formula but not the formula result since it isn't in a position to calculate arbitrary Excel formulas without access to Excel's formula engine.

=item * If Excel has date fields where the specified format is equal to the system-default for the short-date locale, Excel does not store the format, but defaults to an internal format which is system dependent. In these cases ParseExcel uses the date format 'yyyy-mm-dd'.

=back




=head1 REPORTING A BUG

Bugs can be reported via rt.cpan.org. See the following for instructions on bug reporting for Spreadsheet::ParseExcel

L<http://rt.cpan.org/Public/Dist/Display.html?Name=Spreadsheet-ParseExcel>




=head1 SEE ALSO

=over

=item * xls2csv by Ken Prows L<http://search.cpan.org/~ken/xls2csv-1.06/script/xls2csv>.

=item * xls2csv and xlscat by H.Merijn Brand (these utilities are part of Spreadsheet::Read, see below).

=item * excel2txt by Ken Youens-Clark, L<http://search.cpan.org/~kclark/excel2txt/excel2txt>. This is an excellent example of an Excel filter using Spreadsheet::ParseExcel. It can produce CSV, Tab delimited, Html, XML and Yaml.

=item * XLSperl by Jon Allen L<http://search.cpan.org/~jonallen/XLSperl/bin/XLSperl>. This application allows you to use Perl "one-liners" with Microsoft Excel files.

=item * Spreadsheet::XLSX L<http://search.cpan.org/~dmow/Spreadsheet-XLSX/lib/Spreadsheet/XLSX.pm> by Dmitry Ovsyanko. A module with a similar interface to Spreadsheet::ParseExcel for parsing Excel 2007 XLSX OpenXML files.

=item * Spreadsheet::Read L<http://search.cpan.org/~hmbrand/Spreadsheet-Read/Read.pm> by H.Merijn Brand. A single interface for reading several different spreadsheet formats.

=item * Spreadsheet::WriteExcel L<http://search.cpan.org/~jmcnamara/Spreadsheet-WriteExcel/lib/Spreadsheet/WriteExcel.pm>. A perl module for creating new Excel files.

=item * Spreadsheet::ParseExcel::SaveParser L<http://search.cpan.org/~jmcnamara/Spreadsheet-ParseExcel/lib/Spreadsheet/ParseExcel/SaveParser.pm>. This is a combination of Spreadsheet::ParseExcel and Spreadsheet::WriteExcel and it allows you to "rewrite" an Excel file. See the following example L<http://search.cpan.org/~jmcnamara/Spreadsheet-WriteExcel/lib/Spreadsheet/WriteExcel.pm#MODIFYING_AND_REWRITING_EXCEL_FILES>. It is part of the Spreadsheet::ParseExcel distro.

=item * Text::CSV_XS L<http://search.cpan.org/~hmbrand/Text-CSV_XS/CSV_XS.pm> by H.Merijn Brand. A fast and rigorous module for reading and writing CSV data. Don't consider rolling your own CSV handling, use this module instead.

=back




=head1 MAILING LIST

There is a Google group for discussing and asking questions about Spreadsheet::ParseExcel. This is a good place to search to see if your question has been asked before:  L<http://groups-beta.google.com/group/spreadsheet-parseexcel/>




=head1 DONATIONS

If you'd care to donate to the Spreadsheet::ParseExcel project, you can do so via PayPal: L<http://tinyurl.com/7ayes>




=head1 TODO

=over

=item * The current maintenance work is directed towards making the documentation more useful, improving and simplifying the API, and improving the maintainability of the code base. After that new features will be added.

=item * Fix open bugs and documentation for SaveParser.

=item * Add Formula support, Hyperlink support, Named Range support.

=item * Improve Spreadsheet::ParseExcel::SaveParser compatibility with Spreadsheet::WriteExcel.

=item * Improve Unicode and other encoding support. This will probably require dropping support for perls prior to 5.8+.

=back



=head1 ACKNOWLEDGEMENTS

From Kawai Takanori:

First of all, I would like to acknowledge the following valuable programs and modules:
XHTML, OLE::Storage and Spreadsheet::WriteExcel.

In no particular order: Yamaji Haruna, Simamoto Takesi, Noguchi Harumi, Ikezawa Kazuhiro, Suwazono Shugo, Hirofumi Morisada, Michael Edwards, Kim Namusk, Slaven Rezic, Grant Stevens, H.Merijn Brand and many many people + Kawai Mikako.

Alexey Mazurin added the decryption facility.



=head1 DISCLAIMER OF WARRANTY

Because this software is licensed free of charge, there is no warranty for the software, to the extent permitted by applicable law. Except when otherwise stated in writing the copyright holders and/or other parties provide the software "as is" without warranty of any kind, either expressed or implied, including, but not limited to, the implied warranties of merchantability and fitness for a particular purpose. The entire risk as to the quality and performance of the software is with you. Should the software prove defective, you assume the cost of all necessary servicing, repair, or correction.

In no event unless required by applicable law or agreed to in writing will any copyright holder, or any other party who may modify and/or redistribute the software as permitted by the above licence, be liable to you for damages, including any general, special, incidental, or consequential damages arising out of the use or inability to use the software (including but not limited to loss of data or data being rendered inaccurate or losses sustained by you or third parties or a failure of the software to operate with any other software), even if such holder or other party has been advised of the possibility of such damages.




=head1 LICENSE

Either the Perl Artistic Licence L<http://dev.perl.org/licenses/artistic.html> or the GPL L<http://www.opensource.org/licenses/gpl-license.php>




=head1 AUTHOR

Current maintainer 0.60+: Douglas Wilson dougw@cpan.org

Maintainer 0.40-0.59: John McNamara jmcnamara@cpan.org

Maintainer 0.27-0.33: Gabor Szabo szabgab@cpan.org

Original author: Kawai Takanori (Hippo2000) kwitknr@cpan.org




=head1 COPYRIGHT

Copyright (c) 2014 Douglas Wilson

Copyright (c) 2009-2013 John McNamara

Copyright (c) 2006-2008 Gabor Szabo

Copyright (c) 2000-2006 Kawai Takanori

All rights reserved. This is free software. You may distribute under the terms of either the GNU General Public License or the Artistic License.


=cut
