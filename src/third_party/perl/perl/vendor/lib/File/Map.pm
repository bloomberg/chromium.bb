package File::Map;
$File::Map::VERSION = '0.66';
# This software is copyright (c) 2008, 2009, 2010, 2011, 2012 by Leon Timmermans <leont@cpan.org>.
#
# This is free software; you can redistribute it and/or modify it under
# the same terms as perl itself.

use 5.008;
use strict;
use warnings FATAL => 'all';
use subs qw{PROT_READ PROT_WRITE MAP_PRIVATE MAP_SHARED MAP_FILE MAP_ANONYMOUS};

use Sub::Exporter::Progressive 0.001005 ();
use XSLoader ();
use Carp qw/croak carp/;
use PerlIO::Layers qw/query_handle/;

XSLoader::load('File::Map', File::Map->VERSION);

my %export_data = (
	'map'     => [qw/map_handle map_file map_anonymous unmap sys_map/],
	extra     => [qw/remap sync pin unpin advise protect/],
	'lock'    => [qw/wait_until notify broadcast lock_map/],
	constants => [qw/PROT_NONE PROT_READ PROT_WRITE PROT_EXEC MAP_ANONYMOUS MAP_SHARED MAP_PRIVATE MAP_ANON MAP_FILE/]
);

{
	my (@export_ok, %export_tags);

	while (my ($category, $functions) = each %export_data) {
		for my $function (grep { defined &{$_} } @{$functions}) {
			push @export_ok, $function;
			push @{ $export_tags{$category} }, $function;
		}
	}

	Sub::Exporter::Progressive->import(-setup => { exports => \@export_ok, groups => \%export_tags });
}

my $anon_fh = -1;

sub _check_layers {
	my $fh = shift;
	croak "Can't map fake filehandle" if fileno $fh < 0;
	if (warnings::enabled('layer')) {
		carp "Shouldn't map non-binary filehandle" if not query_handle($fh, 'mappable');
	}
	return query_handle($fh, 'utf8');
}

sub _get_offset_length {
	my ($offset, $length, $fh) = @_;

	my $size = -s $fh;
	$offset ||= 0;
	$length ||= $size - $offset;
	my $end = $offset + $length;
	croak "Window ($offset,$end) is outside the file" if $offset < 0 or $end > $size and not -c _;
	return ($offset, $length);
}

## no critic (Subroutines::RequireArgUnpacking)

sub map_handle {
	my (undef, $fh, $mode, $offset, $length) = @_;
	my $utf8 = _check_layers($fh);
	($offset, $length) = _get_offset_length($offset, $length, $fh);
	_mmap_impl($_[0], $length, _protection_value($mode || '<'), MAP_SHARED | MAP_FILE, fileno $fh, $offset, $utf8);
	return;
}

sub map_file {
	my (undef, $filename, $mode, $offset, $length) = @_;
	$mode ||= '<';
	my ($minimode, $encoding) = $mode =~ / \A ([^:]+) ([:\w-]+)? \z /xms;
	$encoding = ':raw' if not defined $encoding;
	open my $fh, $minimode.$encoding, $filename or croak "Couldn't open file $filename: $!";
	my $utf8 = _check_layers($fh);
	($offset, $length) = _get_offset_length($offset, $length, $fh);
	_mmap_impl($_[0], $length, _protection_value($minimode), MAP_SHARED | MAP_FILE, fileno $fh, $offset, $utf8);
	close $fh or croak "Couldn't close $filename after mapping: $!";
	return;
}

my %flag_for = (
	private => MAP_PRIVATE,
	shared  => MAP_SHARED,
);
sub map_anonymous {
	my (undef, $length, $flag_name) = @_;
	my $flag = $flag_for{ $flag_name || 'shared' };
	croak "No such flag '$flag_name'" if not defined $flag;
	croak 'Zero length specified for anonymous map' if $length == 0;
	_mmap_impl($_[0], $length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | $flag, $anon_fh, 0);
	return;
}

sub sys_map {    ## no critic (ProhibitManyArgs)
	my (undef, $length, $protection, $flags, $fh, $offset) = @_;
	my $utf8 = _check_layers($fh);
	my $fd = ($flags & MAP_ANONYMOUS) ? $anon_fh : fileno $fh;
	$offset ||= 0;
	_mmap_impl($_[0], $length, $protection, $flags, $fd, $offset, $utf8);
	return;
}

1;

#ABSTRACT: Memory mapping made simple and safe.

__END__

=pod

=encoding UTF-8

=head1 NAME

File::Map - Memory mapping made simple and safe.

=head1 VERSION

version 0.66

=head1 SYNOPSIS

 use File::Map 'map_file';
 
 map_file my $map, $filename, '+<';
 $map =~ s/bar/quz/g;
 substr $map, 1024, 11, "Hello world";

=head1 DESCRIPTION

File::Map maps files or anonymous memory into perl variables.

=head2 Advantages of memory mapping

=over 4

=item * Unlike normal perl variables, mapped memory is (usually) shared between threads or forked processes.

=item * It is an efficient way to slurp an entire file. Unlike for example L<File::Slurp>, this module returns almost immediately, loading the pages lazily on access. This means you only 'pay' for the parts of the file you actually use.

=item * Perl usually doesn't return memory to the system while running, mapped memory can be returned.

=back

=head2 Advantages of this module over other similar modules

=over 4

=item * Safety and Speed

This module is safe yet fast. Alternatives are either fast but can cause segfaults or lose the mapping when not used correctly, or are safe but rather slow. File::Map is as fast as a normal string yet safe.

=item * Simplicity

It offers a simple interface targeted at common usage patterns

=over 4

=item * Files are mapped into a variable that can be read just like any other variable, and it can be written to using standard Perl techniques such as regexps and C<substr>.

=item * Files can be mapped using a set of simple functions. There is no need to know weird constants or the order of 6 arguments.

=item * It will automatically unmap the file when the scalar gets destroyed. This works correctly even in multi-threaded programs.

=back

=item * Portability

File::Map supports Unix and Windows.

=item * Thread synchronization

It has built-in support for thread synchronization.

=back

=head1 FUNCTIONS

=head2 Mapping

The following functions for mapping a variable are available for exportation. Note that all of these functions throw exceptions on errors, unless noted otherwise.

=head3 map_handle $lvalue, $filehandle, $mode = '<', $offset = 0, $length = -s(*handle) - $offset

Use a filehandle to map into an lvalue. $filehandle should be a scalar filehandle. $mode uses the same format as C<open> does (it currently accepts C<< < >>, C<< +< >>, C<< > >> and C<< +> >>). $offset and $length are byte positions in the file, and default to mapping the whole file.

=head3 * map_file $lvalue, $filename, $mode = '<', $offset = 0, $length = -s($filename) - $offset

Open a file and map it into an lvalue. Other than $filename, all arguments work as in map_handle.

=head3 * map_anonymous $lvalue, $length, $type

Map an anonymous piece of memory. $type can be either C<'shared'>, in which case it will be shared with child processes, or C<'private'>, which won't be shared.

=head3 * sys_map $lvalue, $length, $protection, $flags, $filehandle, $offset = 0

Low level map operation. It accepts the same constants as mmap does (except its first argument obviously). If you don't know how mmap works you probably shouldn't be using this.

=head3 * unmap $lvalue

Unmap a variable. Note that normally this is not necessary as variables are unmapped automatically at destruction, but it is included for completeness.

=head3 * remap $lvalue, $new_size

Try to remap $lvalue to a new size. This call is linux specific and not supported on other systems. For a file backed mapping a file must be long enough to hold the new size, otherwise you can expect bus faults. For an anonymous map it must be private, shared maps can not be remapped. B<Use with caution>.

=head2 Auxiliary  

=head3 * sync $lvalue, $synchronous = 1

Flush changes made to the memory map back to disk. Mappings are always flushed when unmapped, so this is usually not necessary. If $synchronous is true and your operating system supports it, the flushing will be done synchronously.

=head3 * pin $lvalue

Disable paging for this map, thus locking it in physical memory. Depending on your operating system there may be limits on pinning.

=head3 * unpin $lvalue

Unlock the map from physical memory.

=head3 * advise $lvalue, $advice

Advise a certain memory usage pattern. This is not implemented on all operating systems, and may be a no-op. The following values for $advice are always accepted:.

=over 2

=item * normal

Specifies that the application has no advice to give on its behavior with respect to the mapped variable. It is the default characteristic if no advice is given.

=item * random

Specifies that the application expects to access the mapped variable in a random order.

=item * sequential

Specifies that the application expects to access the mapped variable sequentially from start to end.

=item * willneed

Specifies that the application expects to access the mapped variable in the near future.

=item * dontneed

Specifies that the application expects that it will not access the mapped variable in the near future.

=back

On some systems there may be more values available, but this can not be relied on. Unknown values for $advice will cause a warning but are further ignored.

=head3 * protect $lvalue, $mode

Change the memory protection of the mapping. $mode takes the same format as C<open>, but also accepts sys_map style constants.

=head2 Locking

These locking functions provide locking for threads for the mapped region. The mapped region has an internal lock and condition variable. The condition variable functions(C<wait_until>, C<notify>, C<broadcast>) can only be used inside a locked block. If your perl has been compiled without thread support the condition functions will not be available.

=head3 * lock_map $lvalue

Lock $lvalue until the end of the scope. If your perl does not support threads, this will be a no-op.

=head3 * wait_until { block } $lvalue

Wait for block to become true. After every failed attempt, wait for a signal. It returns the value returned by the block.

=head3 * notify $lvalue

This will signal to one listener that the map is available.

=head3 * broadcast $lvalue

This will signal to all listeners that the map is available.

=head2 Constants

=over 4

=item PROT_NONE, PROT_READ, PROT_WRITE, PROT_EXEC, MAP_ANONYMOUS, MAP_SHARED, MAP_PRIVATE, MAP_ANON, MAP_FILE

These constants are used for sys_map. If you think you need them your mmap manpage will explain them, but in most cases you can skip sys_map altogether.

=back

=head1 EXPORTS

All previously mentioned functions are available for exportation, but none are exported by default. Some functions may not be available on your OS or your version of perl as specified above. A number of tags are defined to make importation easier.

=over 4

=item * :map

map_handle, map_file, map_anonymous, sys_map, unmap

=item * :extra

remap, sync, pin, unpin, advise, protect

=item * :lock

lock_map, wait_until, notify, broadcast

=item * :constants

PROT_NONE, PROT_READ, PROT_WRITE, PROT_EXEC, MAP_ANONYMOUS, MAP_SHARED, MAP_PRIVATE, MAP_ANON, MAP_FILE

=item * :all

All functions defined in this module.

=back

=head1 DIAGNOSTICS

=head2 Exceptions

=over 4

=item * Could not <function name>: this variable is not memory mapped

An attempt was made to C<sync>, C<remap>, C<unmap>, C<pin>, C<unpin>, C<advise> or C<lock_map> an unmapped variable.

=item * Could not <function name>: <system error>

Your OS didn't allow File::Map to do what you asked it to do for some reason.

=item * Trying to <function_name> on an unlocked map

You tried to C<wait_until>, C<notify> or C<broadcast> on an unlocked variable.

=item * Zero length not allowed for anonymous map

A zero length anonymous map is not possible (or in any way useful).

=item * Can't remap a shared mapping

An attempt was made to remap a mapping that is shared among different threads, this is not possible.

=item * Window (<start>, <end>) is outside the file

The offset and/or length you specified were invalid for this file.

=item * Can't map fake filehandle

The filehandle you provided is not real. This may mean it's a scalar string handle or a tied handle.

=item * No such flag <flag_name>

The flag given for map_anonymous isn't valid, it should either be C<shared> or C<private>.

=back

=head2 Warnings

=over 4

=item * Writing directly to a memory mapped file is not recommended

Due to the way perl works internally, it's not possible to write a mapping implementation that allows direct assignment yet performs well. As a compromise, File::Map is capable of fixing up the mess if you do it nonetheless, but it will warn you that you're doing something you shouldn't. This warning is only given when C<use warnings 'substr'> is in effect.

=item * Truncating new value to size of the memory map

This warning is additional to the previous one, warning you that you're losing data. This warning is only given when C<use warnings 'substr'> is in effect.

=item * Shouldn't mmap non-binary filehandle

You tried to to map a filehandle that has some encoding layer. Encoding layers are not supported by File::Map. This warning is only given when C<use warnings 'layer'> is in effect. Note that this may become an exception in a future version.

=item * Unknown advice '<advice>'

You gave advise an advice it didn't know. This is either a typo or a portability issue. This warning is only given when C<use warnings 'portable'> is in effect.

=item * Syncing a readonly map makes no sense

C<sync> flushes changes to the map to the filesystem. This obviously is of little use when you can't change the map. This warning is only given when C<use warnings 'io'> is in effect.

=item * Can't overwrite an empty map

Overwriting an empty map is rather nonsensical, hence a warning is given when this is tried. This warning is only given when C<use warnings 'substr'> is in effect.

=back

=head1 DEPENDENCIES

This module depends on perl 5.8, L<Sub::Exporter::Progressive> and L<PerlIO::Layers>. Perl 5.8.8 or higher is recommended because older versions can give spurious warnings.

In perl versions before 5.11.5 many string functions including C<substr> are limited to L<32bit logic|http://rt.perl.org/rt3//Public/Bug/Display.html?id=72784>, even on 64bit architectures. Effectively this means you can't use them on strings bigger than 2GB. If you are working with such large files, it is strongly recommended to upgrade to 5.12.

In perl versions before 5.17.5, there is an off-by-one bug in Perl's regexp engine, as explained L<here|http://rt.perl.org/rt3//Public/Bug/Display.html?id=73542>. If the length of the file is an exact multiple of the page size, some regexps can trigger a segmentation fault.

=head1 PITFALLS

=over 4

=item * This module doesn't do any encoding or newline transformation for you, and will reject any filehandle with such features enabled as mapping it would return a different value than reading it normally. Most importantly this means that on Windows you have to remember to use the C<:raw> open mode or L<binmode> to make your filehandles binary before mapping them, as by default it would do C<crlf> transformation. See L<PerlIO> for more information on how that works.

=item * You can map a C<:utf8> filehandle, but writing to it may be tricky. Hic sunt dracones.

=item * You probably don't want to use C<E<gt>> as a mode. This does not give you reading permissions on many architectures, resulting in segmentation faults when trying to read a variable (confusingly, it will work on some others like x86).

=back

=head1 BUGS AND LIMITATIONS

As any piece of software, bugs are likely to exist here. Bug reports are welcome.

Please report any bugs or feature requests to C<bug-file-map at rt.cpan.org>, or through
the web interface at L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=File-Map>.  I will be notified, and then you'll
automatically be notified of progress on your bug as I make changes.

Unicode file mappings are known to be buggy on perl 5.8.7 and lower.

=head1 SEE ALSO

=over 4

=item * L<Sys::Mmap>, the original Perl mmap module

=item * L<mmap(2)>, your mmap man page

=item * L<Win32::MMF>

=item * CreateFileMapping at MSDN: L<http://msdn.microsoft.com/en-us/library/aa366537(VS.85).aspx>

=back

=head1 AUTHOR

Leon Timmermans <fawaka@gmail.com>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Leon Timmermans.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
