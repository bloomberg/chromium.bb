package Win32::Exe::Resource::Manifest;

use strict;
use base 'Win32::Exe::Resource';
use constant FORMAT => (
    Data        => 'a*',
);

sub get_manifest {
    my ($self ) = @_;
    return $self->dump;    
}

sub get_manifest_id {
    my $self = shift;
    my ($type, $id, @rest);
    eval{ ($type, $id, @rest) = $self->path; };
    $id or return 1;
    $id =~ s/^#//;
    return( $id =~ /^(1|2|3)$/) ? $id : 1;
}

sub set_manifest {
    my ( $self, $xmltext, $mid ) = @_;
    $mid ||= 1;
    $mid = ( $mid =~ /^(1|2|3)$/ ) ? $mid : 1; 
    $self->SetData( $self->encode_manifest($xmltext) );
    my $rsrc = $self->first_parent('Resources');
    $rsrc->remove("/#RT_MANIFEST");
    $rsrc->insert('/#RT_MANIFEST/#' . $mid . '/#0' => $self);
    $rsrc->refresh;
}

sub update_manifest {
    my ( $self, $xmltext ) = @_;
    $self->SetData( $self->encode_manifest($xmltext) );
}

sub encode_manifest {
    my ($self, $string) = @_;
    use bytes;
    return pack("a*", $string);
}

sub default_manifest {
    my ( $self ) = @_;
    my $defman =  <<'W32EXEDEFAULTMANIFEST'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
    <assemblyIdentity type="win32" version="0.0.0.0" name="Perl.Win32.Application" />
    <description>Perl.Win32.Application</description>
    <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
        <security>
            <requestedPrivileges>
                <requestedExecutionLevel level="asInvoker" uiAccess="false" />
            </requestedPrivileges>
        </security>
    </trustInfo>
    <dependency>
        <dependentAssembly>
            <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" publicKeyToken="6595b64144ccf1df" language="*" processorArchitecture="*" />
        </dependentAssembly>
    </dependency>
</assembly>
W32EXEDEFAULTMANIFEST
;
    return $defman;
}

1;
