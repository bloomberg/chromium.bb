package IO::Compress::Adapter::Lzip ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.086 qw(:Status);

use Compress::Raw::Lzma  2.086 qw(LZMA_OK LZMA_STREAM_END) ;
use Compress::Raw::Zlib 2.086 qw() ;

our ($VERSION);
$VERSION = '2.086';



sub mkCompObject
{
    my $dictSize = shift ;

    my $filter = Lzma::Filter::Lzma1(DictSize => $dictSize);

    my ($def, $status) =
    Compress::Raw::Lzma::RawEncoder->new(AppendOutput => 1,
                                         ForZip => 0,
                                         Filter => $filter,
                                         #Filter => Lzma::Filter::Lzma1m
                                         );

    return (undef, "Could not create RawEncoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Def'        => $def,
                  'Error'      => '',
                  'ErrorNo'    => 0,
                  'CRC32'      => 0,
                 }  ;     
}

sub compr
{
    my $self = shift ;

    my $def   = $self->{Def};

    $self->{CRC32} = Compress::Raw::Zlib::crc32($_[0], $self->{CRC32}) ;

    my $status = $def->code($_[0], $_[1]) ;
    $self->{ErrorNo} = $status;

    if ($status != LZMA_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[1] } .= $out if defined $out;

    return STATUS_OK;    
}

sub flush
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $status = $def->flush($_[0]);
    $self->{ErrorNo} = $status;

    if ($status != LZMA_STREAM_END)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[0] } .= $out if defined $out ;
    return STATUS_OK;    
    
}

sub close
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $status = $def->flush($_[0]);
    $self->{ErrorNo} = $status;

    if ($status != LZMA_STREAM_END)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[0] } .= $out if defined $out ;
    return STATUS_OK;    
    
}


sub reset
{
    my $self = shift ;

    my $outer = $self->{Outer};

    my ($def, $status) = 
        Compress::Raw::Lzma::RawEncoder->new(AppendOutput => 1);
    $self->{ErrorNo} = ($status == LZMA_OK) ? 0 : $status ;

    if ($status != LZMA_OK)
    {
        $self->{Error} = "Cannot create Deflate object: $status"; 
        return STATUS_ERROR;
    }

    $self->{Def} = $def;

    return STATUS_OK;    
}

sub compressedBytes
{
    my $self = shift ;
    $self->{Def}->compressedBytes();
}

sub uncompressedBytes
{
    my $self = shift ;
    $self->{Def}->uncompressedBytes();
}

#sub total_out
#{
#    my $self = shift ;
#    0;
#}
#

#sub total_in
#{
#    my $self = shift ;
#    $self->{Def}->total_in();
#}

sub crc32
{
   my $self = shift ;
   return $self->{CRC32} ;
}



1;

__END__




