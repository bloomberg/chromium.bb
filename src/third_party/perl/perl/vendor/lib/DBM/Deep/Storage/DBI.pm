package DBM::Deep::Storage::DBI;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base 'DBM::Deep::Storage';

use DBI;

sub new {
    my $class = shift;
    my ($args) = @_;

    my $self = bless {
        autobless => 1,
        dbh       => undef,
        dbi       => undef,
    }, $class;

    # Grab the parameters we want to use
    foreach my $param ( keys %$self ) {
        next unless exists $args->{$param};
        $self->{$param} = $args->{$param};
    }

    if ( $self->{dbh} ) {
        $self->{driver} = lc $self->{dbh}->{Driver}->{Name};
    }
    else {
        $self->open;
    }

    # Foreign keys are turned off by default in SQLite3 (for now)
    #q.v.  http://search.cpan.org/~adamk/DBD-SQLite-1.27/lib/DBD/SQLite.pm#Foreign_Keys
    # for more info.
    if ( $self->driver eq 'sqlite' ) {
        $self->{dbh}->do( 'PRAGMA foreign_keys = ON' );
    }

    return $self;
}

sub open {
    my $self = shift;

    return if $self->{dbh};

    $self->{dbh} = DBI->connect(
        $self->{dbi}{dsn}, $self->{dbi}{username}, $self->{dbi}{password}, {
            AutoCommit => 1,
            PrintError => 0,
            RaiseError => 1,
            %{ $self->{dbi}{connect_args} || {} },
        },
    ) or die $DBI::error;

    # Should we use the same method as done in new() if passed a $dbh?
    (undef, $self->{driver}) = map defined($_) ? lc($_) : undef, DBI->parse_dsn( $self->{dbi}{dsn} );

    return 1;
}

sub close {
    my $self = shift;
    $self->{dbh}->disconnect if $self->{dbh};
    return 1;
}

sub DESTROY {
    my $self = shift;
    $self->close if ref $self;
}

# Is there a portable way of determining writability to a DBH?
sub is_writable {
    my $self = shift;
    return 1;
}

sub lock_exclusive {
    my $self = shift;
}

sub lock_shared {
    my $self = shift;
}

sub unlock {
    my $self = shift;
#    $self->{dbh}->commit;
}

#sub begin_work {
#    my $self = shift;
#    $self->{dbh}->begin_work;
#}
#
#sub commit {
#    my $self = shift;
#    $self->{dbh}->commit;
#}
#
#sub rollback {
#    my $self = shift;
#    $self->{dbh}->rollback;
#}

sub read_from {
    my $self = shift;
    my ($table, $cond, @cols) = @_;

    $cond = { id => $cond } unless ref $cond;

    my @keys = keys %$cond;
    my $where = join ' AND ', map { "`$_` = ?" } @keys;

    return $self->{dbh}->selectall_arrayref(
        "SELECT `@{[join '`,`', @cols ]}` FROM $table WHERE $where",
        { Slice => {} }, @{$cond}{@keys},
    );
}

sub flush {}

sub write_to {
    my $self = shift;
    my ($table, $id, %args) = @_;

    my @keys = keys %args;
    my $sql =
        "REPLACE INTO $table ( `id`, "
          . join( ',', map { "`$_`" } @keys )
      . ") VALUES ("
          . join( ',', ('?') x (@keys + 1) )
      . ")";
    $self->{dbh}->do( $sql, undef, $id, @args{@keys} );

    return $self->{dbh}->last_insert_id("", "", "", "");
}

sub delete_from {
    my $self = shift;
    my ($table, $cond) = @_;

    $cond = { id => $cond } unless ref $cond;

    my @keys = keys %$cond;
    my $where = join ' AND ', map { "`$_` = ?" } @keys;

    $self->{dbh}->do(
        "DELETE FROM $table WHERE $where", undef, @{$cond}{@keys},
    );
}

sub driver { $_[0]{driver} }

sub rand_function {
    my $self = shift;
    my $driver = $self->driver;

    $driver eq 'sqlite' and return 'random()';
    $driver eq 'mysql'  and return 'RAND()';

    die "rand_function undefined for $driver\n";
}

1;
__END__
