package IO::Compress::Adapter::Lzma ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.052 qw(:Status);

use Compress::Raw::Lzma  2.052 qw(LZMA_OK LZMA_STREAM_END) ;

our ($VERSION);
$VERSION = '2.052';

sub mkCompObject
{
    my $Filter = shift ;

    my ($def, $status) =
    Compress::Raw::Lzma::AloneEncoder->new(AppendOutput => 1,
                                           Filter => $Filter);

    return (undef, "Could not create AloneEncoder object: $status", $status)
        if $status != LZMA_OK ;

    return bless {'Def'        => $def,
                  'Error'      => '',
                  'ErrorNo'    => 0,
                 }  ;     
}

sub mkRawZipCompObject
{
    my $preset = shift ;
    my $extreme = shift;
    my $filter;


    if (defined $preset) {
        $preset |= Compress::Raw::Lzma::LZMA_PRESET_EXTREME()
            if $extreme;
        $filter = Lzma::Filter::Lzma1::Preset($preset) ;
    }
    else  
      { $filter = Lzma::Filter::Lzma1 }

    my ($def, $status) =
    Compress::Raw::Lzma::RawEncoder->new(AppendOutput => 1,
                                         ForZip => 1,
                                         Filter => $filter,
                                         #Filter => Lzma::Filter::Lzma1m
                                         );

    return (undef, "Could not create RawEncoder object: $status", $status)
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

    my ($def, $status) = 
        Compress::Raw::Lzma::AloneEncoder->new(AppendOutput => 1);
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

