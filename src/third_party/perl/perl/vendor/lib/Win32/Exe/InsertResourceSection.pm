#########################################################################################
# Package       Win32::Exe::InsertResourceSection
# Description:  Insert Resource Section
# Created       Sun May 02 17:32:55 2010
# SVN Id        $Id: InsertResourceSection.pm 2 2010-11-30 16:40:31Z mark.dootson $
# Copyright:    Copyright (c) 2010 Mark Dootson
# Licence:      This program is free software; you can redistribute it 
#               and/or modify it under the same terms as Perl itself
#########################################################################################

package Win32::Exe::InsertResourceSection;

#########################################################################################

use strict;
use warnings;
use Exporter;
use base qw( Exporter );
use Carp;
use Win32::Exe;

our $VERSION = '0.17';
our @EXPORT = qw( insert_pe_resource_section );

sub _is_win { ($^O =~ /^mswin/i) }

if (_is_win()) {
    require XSLoader;
    XSLoader::load('Win32::Exe::InsertResourceSection', $VERSION);
}

sub create_resource_section {
    my $filename = shift;
    
    croak('Invalid filename') if $filename !~ /\.(dll|exe)$/i;
    croak('Filename not found') if !-f $filename;
    
    if(!_is_win()) {
        warn 'Cannot add resource section to PE files on this platform. Requires MSWin';
        return undef;
    }
    
    my $replacecode;
    if($filename =~ /\.(dll|exe)$/i) {
        #VFT_APP = 0x1
        #VFT_DLL = 0x2
        #VFT_DRV = 0x3
        #VFT_FONT = 0x4
        #VFT_VXD = 0x5
        #VFT_STATIC_LIB = 0x7
        if(lc($1) eq 'exe') {
            $replacecode = '01';
        } elsif(lc($1) eq 'dll') {
            $replacecode = '02';
        } else {
            croak('Invalid filename');
        }
    } else {
        croak('Invalid filename');
    }

    my @verdata = qw(
        400234000000560053005F0056004500
        5200530049004F004E005F0049004E00
        46004F0000000000BD04EFFE00000100
        00000000000000000000000000000000
        3F0000000000000004000400XX000000
        000000000000000000000000A0010000
        010053007400720069006E0067004600
        69006C00650049006E0066006F000000
        7C010000010030003000300030003000
        34004200300000002400020001004300
        6F006D00700061006E0079004E006100
        6D00650000000000200000002C000200
        0100460069006C006500440065007300
        6300720069007000740069006F006E00
        00000000200000002400020001004600
        69006C00650056006500720073006900
        6F006E00000000002000000024000200
        010049006E007400650072006E006100
        6C004E0061006D006500000020000000
        2800020001004C006500670061006C00
        43006F00700079007200690067006800
        74000000200000002C00020001004C00
        6500670061006C005400720061006400
        65006D00610072006B00730000000000
        200000002C00020001004F0072006900
        670069006E0061006C00460069006C00
        65006E0061006D006500000020000000
        240002000100500072006F0064007500
        630074004E0061006D00650000000000
        20000000280002000100500072006F00
        64007500630074005600650072007300
        69006F006E0000002000000044000000
        0100560061007200460069006C006500
        49006E0066006F000000000024000400
        00005400720061006E0073006C006100
        740069006F006E00000000000000B004
    );   
    
    my $verdatahex = join('', @verdata);
    $verdatahex =~ s/XX/$replacecode/;
    
    my $verdataraw = pack('H*', $verdatahex);
    my $verlen = length($verdataraw);
    _insert_resource_section($filename, $verdataraw, $verlen);
}

sub insert_pe_resource_section {
    my $filename = shift;
    if(create_resource_section($filename)) {
        # basic version info resource has been created
        # we now have to replace the language and original
        # filename / filename
        my $exe = Win32::Exe->new($filename);
        return ($exe->update( info => [ "FileVersion=0.0.0.0" ] )) ? $exe : undef;
    } else {
        return undef;
    }
}

1;
