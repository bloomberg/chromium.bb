package IO::Uncompress::Adapter::UnLzma;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common 2.052 qw(:Status);

use Compress::Raw::Lzma 2.052 ;

our ($VERSION, @ISA);
$VERSION = '2.052';

#@ISA = qw( Compress::Raw::UnLzma );


sub mkUncompObject
{
    #my $CompressedLength = shift;
    #my $UncompressedLength = shift;
    #my $small     = shift || 0;
    #my $verbosity = shift || 0;

    my ($inflate, $status) = Compress::Raw::Lzma::AloneDecoder->new(AppendOutput => 1,
                                              ConsumeInput => 1, 
                                              LimitOutput => 1);

    return (undef, "Could not create AloneDecoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Inf'           => $inflate,
                  'CompSize'      => 0,
                  'UnCompSize'    => 0,
                  'Error'         => '',
                  'ConsumesInput' => 1,
                  #'CompressedLen' => $CompressedLength || 0,
                  #'UncompressedLen' => $UncompressedLength || 0,
                 }  ;     
    
}

sub mkUncompZipObject
{
    my $properties = shift ;
    #my $CompressedLength = shift;
    #my $UncompressedLength = shift;
    #my $small     = shift || 0;
    #my $verbosity = shift || 0;

    my ($inflate, $status) = Compress::Raw::Lzma::RawDecoder->new(AppendOutput => 1,
                                              Properties => $properties,
                                              ConsumeInput => 1, 
                                              LimitOutput => 1);

    return (undef, "Could not create RawDecoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Inf'           => $inflate,
                  'CompSize'      => 0,
                  'UnCompSize'    => 0,
                  'Error'         => '',
                  'ConsumesInput' => 1,
                  #'CompressedLen' => $CompressedLength || 0,
                  #'UncompressedLen' => $UncompressedLength || 0,
                 }  ;     
    
}

sub uncompr
{
    my $self = shift ;
    my $from = shift ;
    my $to   = shift ;
    my $eof  = shift ;

    my $inf   = $self->{Inf};
    my $status = $inf->code($from, $to);
    $self->{ErrorNo} = $status;

    if ($status != LZMA_OK && $status != LZMA_STREAM_END )
    {
        $self->{Error} = "Uncompression Error: $status";
        return STATUS_ERROR;
    }
    
    return STATUS_ENDSTREAM if $status == LZMA_STREAM_END  || 
                                ($eof && length $$from == 0);

    return STATUS_OK        if $status == LZMA_OK ;
    return STATUS_ERROR ;
}


sub reset
{
    my $self = shift ;

    my ($inf, $status) = Compress::Raw::Lzma::AloneDecoder->new(AppendOutput => 1,
                                              ConsumeInput => 1, 
                                              LimitOutput => 1);
    $self->{ErrorNo} = ($status == LZMA_OK) ? 0 : $status ;

    if ($status != LZMA_OK)
    {
        $self->{Error} = "Cannot create UnLzma object: $status"; 
        return STATUS_ERROR;
    }

    $self->{Inf} = $inf;

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
    #$self->{Inf}->crc32();
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

