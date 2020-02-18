package PPI::Singletons;

# exports some singleton variables to avoid aliasing magic

use strict;
use Exporter     ();

our $VERSION = '1.269'; # VERSION

our @ISA       = 'Exporter';
our @EXPORT_OK = qw{ %_PARENT %OPERATOR %MAGIC %LAYER $CURLY_SYMBOL %QUOTELIKE %KEYWORDS };

our %_PARENT; # Master Child -> Parent index

# operator index
our %OPERATOR = map { $_ => 1 } (
	qw{
	-> ++ -- ** ! ~ + -
	=~ !~ * / % x . << >>
	< > <= >= lt gt le ge
	== != <=> eq ne cmp ~~
	& | ^ && || // .. ...
	? :
	= **= += -= .= *= /= %= x= &= |= ^= <<= >>= &&= ||= //=
	=> <> <<>>
	and or xor not
	}, ',' 	# Avoids "comma in qw{}" warning
);

# Magic variables taken from perlvar.
# Several things added separately to avoid warnings.
our %MAGIC = map { $_ => 1 } qw{
	$1 $2 $3 $4 $5 $6 $7 $8 $9
	$_ $& $` $' $+ @+ %+ $* $. $/ $|
	$\\ $" $; $% $= $- @- %- $)
	$~ $^ $: $? $! %! $@ $$ $< $>
	$( $0 $[ $] @_ @*

	$^L $^A $^E $^C $^D $^F $^H
	$^I $^M $^N $^O $^P $^R $^S
	$^T $^V $^W $^X %^H

	$::|
}, '$}', '$,', '$#', '$#+', '$#-';

our %LAYER = ( 1 => [], 2 => [] );    # Registered function store

our $CURLY_SYMBOL = qr{\G\^[[:upper:]_]\w+\}};

our %QUOTELIKE = (
	'q'  => 'Quote::Literal',
	'qq' => 'Quote::Interpolate',
	'qx' => 'QuoteLike::Command',
	'qw' => 'QuoteLike::Words',
	'qr' => 'QuoteLike::Regexp',
	'm'  => 'Regexp::Match',
	's'  => 'Regexp::Substitute',
	'tr' => 'Regexp::Transliterate',
	'y'  => 'Regexp::Transliterate',
);

# List of keywords is from regen/keywords.pl in the perl source.
our %KEYWORDS = map { $_ => 1 } qw{
	abs accept alarm and atan2 bind binmode bless break caller chdir chmod
	chomp chop chown chr chroot close closedir cmp connect continue cos
	crypt dbmclose dbmopen default defined delete die do dump each else
	elsif endgrent endhostent endnetent endprotoent endpwent endservent
	eof eq eval evalbytes exec exists exit exp fc fcntl fileno flock for
	foreach fork format formline ge getc getgrent getgrgid getgrnam
	gethostbyaddr gethostbyname gethostent getlogin getnetbyaddr
	getnetbyname getnetent getpeername getpgrp getppid getpriority
	getprotobyname getprotobynumber getprotoent getpwent getpwnam
	getpwuid getservbyname getservbyport getservent getsockname
	getsockopt given glob gmtime goto grep gt hex if index int ioctl join
	keys kill last lc lcfirst le length link listen local localtime lock
	log lstat lt m map mkdir msgctl msgget msgrcv msgsnd my ne next no
	not oct open opendir or ord our pack package pipe pop pos print
	printf prototype push q qq qr quotemeta qw qx rand read readdir
	readline readlink readpipe recv redo ref rename require reset return
	reverse rewinddir rindex rmdir s say scalar seek seekdir select semctl
	semget semop send setgrent sethostent setnetent setpgrp
	setpriority setprotoent setpwent setservent setsockopt shift shmctl
	shmget shmread shmwrite shutdown sin sleep socket socketpair sort
	splice split sprintf sqrt srand stat state study sub substr symlink
	syscall sysopen sysread sysseek system syswrite tell telldir tie tied
	time times tr truncate uc ucfirst umask undef unless unlink unpack
	unshift untie until use utime values vec wait waitpid wantarray warn
	when while write x xor y
};

1;
