#!perl
# Copyright 2004, 2006, 2010 by Audrey Tang <cpan@audreyt.org>

use strict;
use File::Basename;
use Win32::Exe;
use Getopt::Long;

=head1 NAME

exe_update.pl - Modify windows executable files

=head1 SYNOPSIS

B<exe_update.pl> S<[ B<--gui> | B<--console> ]> S<[ B<--icon> I<iconfile> ]>
              S<[ B<--manifest> I<manifestfile> ]>
              S<[ B<--info> I<key=value;...> ]> I<exefile>

=head1 DESCRIPTION

This program rewrites PE headers in a Windows executable file.  It can
change whether the executable runs with a console window, as well as
setting the icons, manifest and version information associated with it.
In general, a PE file must have an existing resource section or you
cannot add icons, manifests or version info. However, on Win32 platforms
a new resource section will be created if none exists.

=head1 OPTIONS

Options are available in a I<short> form and a I<long> form.  For
example, the three lines below are all equivalent:

    % exe_update.pl -i new.ico input.exe
    % exe_update.pl --icon new.ico input.exe
    % exe_update.pl --icon=new.ico input.exe

=over 4

=item B<-c>, B<--console>

Set the executable to always display a console window.

=item B<-g>, B<--gui>

Set the executable so it does not have a console window.

=item B<-i>, B<--icon>=I<FILE>

Specify an icon file (in F<.ico>, F<.exe> or F<.dll> format) for the
executable.

=item B<-m>, B<--manifest>=I<FILE>

Specify a manifest file in F<.xml> format for the
executable.

=item B<-N>, B<--info>=I<KEY=VAL>

Attach version information for the executable.  The name/value pair is
joined by C<=>.  You may specify C<-N> multiple times, or use C<;> to
link several pairs.

These special C<KEY> names are recognized:

    Comments        CompanyName     FileDescription FileVersion
    InternalName    LegalCopyright  LegalTrademarks OriginalFilename
    ProductName     ProductVersion

=item B<-A>, B<--manifestargs>=I<KEY=VAL>

As an alternative to specifying a manifest file, specify manifest attributes.
The name/value pair is joined by C<=>.  You may specify C<-A> multiple times,
or use C<;> to link several pairs. This option may be preferable to using
a manifest file as these attributes will be combined with any existing
manifest that may be in the executable.

These special C<KEY> names are recognized:

    ExecutionLevel  UIAccess     ExecName   Description
    CommonControls  Version

The CommonControls key is a simple boolean value to indicate that the
Common Controls Version 6.0.0.0 dependency should be added to the manifest.

e.g

--manifestargs="ExecutionLevel=requireAdministrator;ExecName=My.App;CommonControls=1"
--manifestargs="Version=1.3.6.7895;UIAccess=false"

=back

=cut

my $Options = {};
Getopt::Long::GetOptions( $Options,
    'g|gui',            # No console window
    'c|console',        # Use console window
    'i|icon:s',         # Icon file
    'm|manifest:s',     # manifest file
    'N|info:s@',        # Executable header info
    'A|manifestargs:s@' # manifest arguments
);

my $exe = shift or die "Usage: " . basename($0) .
    " [--gui | --console] [--icon file.ico] [--manifest file.xml] [--info key=value] [--manifestargs key=value ] file.exe\n";

my $exec = Win32::Exe->new($exe) or die "Unable to open file $exe";

if(!$exec->has_resource_section) {
    die("Cannot create new resource section on this platform. Requires Win32") if $^O !~ /^mswin/i;
    $exec->create_resource_section or die("Failed to create new resource section");
    $exec = Win32::Exe->new($exe) or die "Unable to open file $exe";
}

$exec->update(
    gui	        => $Options->{g},
    console     => $Options->{c},
    icon        => $Options->{i},
    info        => $Options->{N},
    manifest    => $Options->{m},
    manifestargs => $Options->{A},
) or die "Update of $exe failed!\n";

__END__

=head1 AUTHORS

Audrey Tang E<lt>cpan@audreyt.orgE<gt>

=head1 COPYRIGHT

Copyright 2004, 2006, 2010 by Audrey Tang E<lt>cpan@audreyt.orgE<gt>.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
