use strict; use warnings;
package IO::All::DBM;

use IO::All::File -base;
use Fcntl;

field _dbm_list => [];
field '_dbm_class';
field _dbm_extra => [];

sub dbm {
    my $self = shift;
    bless $self, __PACKAGE__;
    $self->_dbm_list([@_]);
    return $self;
}

sub _assert_open {
    my $self = shift;
    return $self->tied_file
      if $self->tied_file;
    $self->open;
}

sub assert_filepath {
    my $self = shift;
    $self->SUPER::assert_filepath(@_);
    if ($self->_rdonly and not -e $self->pathname) {
        my $rdwr = $self->_rdwr;
        $self->assert(0)->rdwr(1)->rdonly(0)->open;
        $self->close;
        $self->assert(1)->rdwr($rdwr)->rdonly(1);
    }
}

sub open {
    my $self = shift;
    $self->is_open(1);
    return $self->tied_file if $self->tied_file;
    $self->assert_filepath if $self->_assert;
    my $dbm_list = $self->_dbm_list;
    my @dbm_list = @$dbm_list ? @$dbm_list :
      (qw(DB_File GDBM_File NDBM_File ODBM_File SDBM_File));
    my $dbm_class;
    for my $module (@dbm_list) {
        (my $file = "$module.pm") =~ s{::}{/}g;
        if (defined $INC{$file} || eval "eval 'use $module; 1'") {
            $self->_dbm_class($module);
            last;
        }
    }
    $self->throw("No module available for IO::All DBM operation")
      unless defined $self->_dbm_class;
    my $mode = $self->_rdonly ? O_RDONLY : O_RDWR;
    if ($self->_dbm_class eq 'DB_File::Lock') {
        $self->_dbm_class->import;
        my $type = eval '$DB_HASH'; die $@ if $@;
        # XXX Not sure about this warning
        warn "Using DB_File::Lock in IO::All without the rdonly or rdwr method\n"
          if not ($self->_rdwr or $self->_rdonly);
        my $flag = $self->_rdwr ? 'write' : 'read';
        $mode = $self->_rdwr ? O_RDWR : O_RDONLY;
        $self->_dbm_extra([$type, $flag]);
    }
    $mode |= O_CREAT if $mode & O_RDWR;
    $self->mode($mode);
    $self->perms(0666) unless defined $self->perms;
    return $self->tie_dbm;
}

sub tie_dbm {
    my $self = shift;
    my $hash;
    my $filename = $self->name;
    my $db = tie %$hash, $self->_dbm_class, $filename, $self->mode, $self->perms,
        @{$self->_dbm_extra}
      or $self->throw("Can't open '$filename' as DBM file:\n$!");
    $self->add_utf8_dbm_filter($db)
      if $self->_has_utf8;
    $self->tied_file($hash);
}

sub add_utf8_dbm_filter {
    my $self = shift;
    my $db = shift;
    $db->filter_store_key(sub { utf8::encode($_) });
    $db->filter_store_value(sub { utf8::encode($_) });
    $db->filter_fetch_key(sub { utf8::decode($_) });
    $db->filter_fetch_value(sub { utf8::decode($_) });
}

1;
