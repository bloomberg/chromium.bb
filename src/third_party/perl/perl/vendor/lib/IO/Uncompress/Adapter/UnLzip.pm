package IO::Uncompress::Adapter::UnLzip;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common 2.086 qw(:Status);

use Compress::Raw::Lzma 2.086 ;

our ($VERSION, @ISA);
$VERSION = '2.086';

#@ISA = qw( Compress::Raw::UnLzma );


sub mkUncompObject
{
    my $dictSize = shift;

    # my $properties ;
    # my $filter = Lzma::Filter::Lzma1(DictSize => $dictSize,
    #                                     #'PreserDict' => [1, 1, Parse_unsigned(), undef],
    #                                 # 'Lc'    => [1, 1, Parse_unsigned(), LZMA_LC_DEFAULT()],
    #                                 # 'Lp'    => [1, 1, Parse_unsigned(), LZMA_LP_DEFAULT()],
    #                                 # 'Pb'    => [1, 1, Parse_unsigned(), LZMA_PB_DEFAULT()],
    #                                 # 'Mode'  => [1, 1, Parse_unsigned(), LZMA_MODE_NORMAL()],
    #                                 # 'Nice'  => [1, 1, Parse_unsigned(), 64],
    #                                 # 'Mf'    => [1, 1, Parse_unsigned(), LZMA_MF_BT4()],
    #                                 # 'Depth' => [1, 1, Parse_unsigned(), 0],
    #                                 );

    # From lzip_init (in archive_read_support_filter_xz.c)
    my $properties = pack("C V", 0x5D, 1 << ($dictSize & 0x1F));

    my ($inflate, $status) = Compress::Raw::Lzma::RawDecoder->new(AppendOutput => 1,
                                              Properties => $properties,
                                            #   Filter => $filter,
                                              ConsumeInput => 1, 
                                              LimitOutput => 1);

    return (undef, "Could not create RawDecoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Inf'           => $inflate,
                  'CompSize'      => 0,
                  'UnCompSize'    => 0,
                  'Error'         => '',
                  'ErrorNo'       => 0,
                  'ConsumesInput' => 1,
                  'CRC32'         => 0,
                  'Properties'    => $properties,
                 }  ;     
    
}

sub uncompr
{
    my $self = shift ;
    my $from = shift ;
    my $to   = shift ;
    my $eof  = shift ;

    my $len = length($$to) ;
    my $inf   = $self->{Inf};
    my $status = $inf->code($from, $to);
    $self->{ErrorNo} = $status;

    if ($status != LZMA_OK && $status != LZMA_STREAM_END )
    {
        $self->{Error} = "Uncompression Error: $status";
        return STATUS_ERROR;
    }

    $self->{CRC32} = Compress::Raw::Zlib::crc32(substr($$to, $len), $self->{CRC32}) 
        if length($$to) > $len ;

    return STATUS_ENDSTREAM if $status == LZMA_STREAM_END  || 
                                ($eof && length $$from == 0);


    return STATUS_OK        if $status == LZMA_OK ;
    return STATUS_ERROR ;
}


sub reset
{
    my $self = shift ;

    my ($inf, $status) = Compress::Raw::Lzma::RawDecoder->new(AppendOutput => 1,
                                              Properties => $self->{Properties},
                                            #   Filter => $filter,
                                              ConsumeInput => 1, 
                                              LimitOutput => 1);

    # my ($inf, $status) = Compress::Raw::Lzma::RawDecoder->new(AppendOutput => 1,
    #                                           ConsumeInput => 1, 
    #                                           LimitOutput => 1);
    $self->{ErrorNo} = ($status == LZMA_OK) ? 0 : $status ;

    if ($status != LZMA_OK)
    {
        $self->{Error} = "Cannot create UnLzma object: $status"; 
        return STATUS_ERROR;
    }

    $self->{Inf} = $inf;
    $self->{CRC32} = 0 ;

    return STATUS_OK ;
}

#sub count
#{
#    my $self = shift ;
#    $self->{Inf}->inflateCount();
#}

sub compressedBytes
{
    my $self = shift ;
    $self->{Inf}->compressedBytes();
}

sub uncompressedBytes
{
    my $self = shift ;
    $self->{Inf}->uncompressedBytes();
}

sub crc32
{
    my $self = shift ;
    $self->{CRC32};
}

sub adler32
{
    my $self = shift ;
    #$self->{Inf}->adler32();
}

sub sync
{
    my $self = shift ;
    #( $self->{Inf}->inflateSync(@_) == LZMA_OK) 
    #        ? STATUS_OK 
    #        : STATUS_ERROR ;
}


1;

__END__

