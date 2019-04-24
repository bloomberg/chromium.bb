package IO::Compress::Adapter::Xz ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.052 qw(:Status);

use Compress::Raw::Lzma  2.052 qw(LZMA_OK LZMA_STREAM_END LZMA_PRESET_DEFAULT LZMA_CHECK_CRC32) ;

our ($VERSION);
$VERSION = '2.052';

sub mkCompObject
{
    my $Preset = shift ;
    my $Extreme = shift ;
    my $Check  = shift ;

    my ($def, $status) = Compress::Raw::Lzma::EasyEncoder->new(AppendOutput => 1, 
                                                               Preset => $Preset,
                                                               Extreme => $Extreme,
                                                               Check => $Check);

    return (undef, "Could not create EasyEncoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Def'        => $def,
                  'Error'      => '',
                  'ErrorNo'    => 0,
                 }  ;     
}

sub compr
{
    my $self = shift ;

    my $def   = $self->{Def};

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

    my ($def, $status) = Compress::Raw::Lzma->lzma_easy_encoder();
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
#
#sub crc32
#{
#    my $self = shift ;
#    $self->{Def}->crc32();
#}
#
#sub adler32
#{
#    my $self = shift ;
#    $self->{Def}->adler32();
#}


1;

__END__

