# -*- perl -*-
#
#   DBD::Mem - A DBI driver for in-memory tables
#
#  This module is currently maintained by
#
#      Jens Rehsack
#
#  Copyright (C) 2016,2017 by Jens Rehsack
#
#  All rights reserved.
#
#  You may distribute this module under the terms of either the GNU
#  General Public License or the Artistic License, as specified in
#  the Perl README file.

require 5.008;
use strict;

#################
package DBD::Mem;
#################
use base qw( DBI::DBD::SqlEngine );
use vars qw($VERSION $ATTRIBUTION $drh);
$VERSION     = '0.001';
$ATTRIBUTION = 'DBD::Mem by Jens Rehsack';

# no need to have driver() unless you need private methods
#
sub driver ($;$)
{
    my ( $class, $attr ) = @_;
    return $drh if ($drh);

    # do the real work in DBI::DBD::SqlEngine
    #
    $attr->{Attribution} = 'DBD::Mem by Jens Rehsack';
    $drh = $class->SUPER::driver($attr);

    return $drh;
}

sub CLONE
{
    undef $drh;
}

#####################
package DBD::Mem::dr;
#####################
$DBD::Mem::dr::imp_data_size = 0;
@DBD::Mem::dr::ISA           = qw(DBI::DBD::SqlEngine::dr);

# you could put some :dr private methods here

# you may need to over-ride some DBI::DBD::SqlEngine::dr methods here
# but you can probably get away with just letting it do the work
# in most cases

#####################
package DBD::Mem::db;
#####################
$DBD::Mem::db::imp_data_size = 0;
@DBD::Mem::db::ISA           = qw(DBI::DBD::SqlEngine::db);

use Carp qw/carp/;

sub set_versions
{
    my $this = $_[0];
    $this->{mem_version} = $DBD::Mem::VERSION;
    return $this->SUPER::set_versions();
}

sub init_valid_attributes
{
    my $dbh = shift;

    # define valid private attributes
    #
    # attempts to set non-valid attrs in connect() or
    # with $dbh->{attr} will throw errors
    #
    # the attrs here *must* start with mem_ or foo_
    #
    # see the STORE methods below for how to check these attrs
    #
    $dbh->{mem_valid_attrs} = {
        mem_version        => 1,    # verbose DBD::Mem version
        mem_valid_attrs    => 1,    # DBD::Mem::db valid attrs
        mem_readonly_attrs => 1,    # DBD::Mem::db r/o attrs
        mem_meta           => 1,    # DBD::Mem public access for f_meta
        mem_tables         => 1,    # DBD::Mem public access for f_meta
    };
    $dbh->{mem_readonly_attrs} = {
        mem_version        => 1,    # verbose DBD::Mem version
        mem_valid_attrs    => 1,    # DBD::Mem::db valid attrs
        mem_readonly_attrs => 1,    # DBD::Mem::db r/o attrs
        mem_meta           => 1,    # DBD::Mem public access for f_meta
    };

    $dbh->{mem_meta} = "mem_tables";

    return $dbh->SUPER::init_valid_attributes();
}

sub get_mem_versions
{
    my ( $dbh, $table ) = @_;
    $table ||= '';

    my $meta;
    my $class = $dbh->{ImplementorClass};
    $class =~ s/::db$/::Table/;
    $table and ( undef, $meta ) = $class->get_table_meta( $dbh, $table, 1 );
    $meta or ( $meta = {} and $class->bootstrap_table_meta( $dbh, $meta, $table ) );

    return sprintf( "%s using %s", $dbh->{mem_version}, $AnyData2::VERSION );
}

package DBD::Mem::st;

use strict;
use warnings;

our $imp_data_size = 0;
our @ISA           = qw(DBI::DBD::SqlEngine::st);

############################
package DBD::Mem::Statement;
############################

@DBD::Mem::Statement::ISA = qw(DBI::DBD::SqlEngine::Statement);


sub open_table ($$$$$)
{
    my ( $self, $data, $table, $createMode, $lockMode ) = @_;

    my $class = ref $self;
    $class =~ s/::Statement/::Table/;

    my $flags = {
                  createMode => $createMode,
                  lockMode   => $lockMode,
                };
    if( defined( $data->{Database}->{mem_table_data}->{$table} ) && $data->{Database}->{mem_table_data}->{$table})
    {
        my $t = $data->{Database}->{mem_tables}->{$table};
        $t->seek( $data, 0, 0 );
        return $t;
    }

    return $self->SUPER::open_table($data, $table, $createMode, $lockMode);
}

# ====== DataSource ============================================================

package DBD::Mem::DataSource;

use strict;
use warnings;

use Carp;

@DBD::Mem::DataSource::ISA = "DBI::DBD::SqlEngine::DataSource";

sub complete_table_name ($$;$)
{
    my ( $self, $meta, $table, $respect_case ) = @_;
    $table;
}

sub open_data ($)
{
    my ( $self, $meta, $attrs, $flags ) = @_;
    defined $meta->{data_tbl} or $meta->{data_tbl} = [];
}

########################
package DBD::Mem::Table;
########################

# shamelessly stolen from SQL::Statement::RAM

use Carp qw/croak/;

@DBD::Mem::Table::ISA = qw(DBI::DBD::SqlEngine::Table);

use Carp qw(croak);

sub new
{
    #my ( $class, $tname, $col_names, $data_tbl ) = @_;
    my ( $class, $data, $attrs, $flags ) = @_;
    my $self = $class->SUPER::new($data, $attrs, $flags);

    my $meta = $self->{meta};
    $self->{records} = $meta->{data_tbl};
    $self->{index} = 0;

    $self;
}

sub bootstrap_table_meta
{
    my ( $self, $dbh, $meta, $table ) = @_;

    defined $meta->{sql_data_source} or $meta->{sql_data_source} = "DBD::Mem::DataSource";

    $meta;
}

sub fetch_row
{
    my ( $self, $data ) = @_;

    return $self->{row} =
        ( $self->{records} and ( $self->{index} < scalar( @{ $self->{records} } ) ) )
      ? [ @{ $self->{records}->[ $self->{index}++ ] } ]
      : undef;
}

sub push_row
{
    my ( $self, $data, $fields ) = @_;
    my $currentRow = $self->{index};
    $self->{index} = $currentRow + 1;
    $self->{records}->[$currentRow] = $fields;
    return 1;
}

sub truncate
{
    my $self = shift;
    return splice @{ $self->{records} }, $self->{index}, 1;
}

sub push_names
{
    my ( $self, $data, $names ) = @_;
    my $meta = $self->{meta};
    $meta->{col_names} = $self->{col_names}     = $names;
    $self->{org_col_names} = [ @{$names} ];
    $self->{col_nums} = {};
    $self->{col_nums}{ $names->[$_] } = $_ for ( 0 .. scalar @$names - 1 );
}

sub drop ($)
{
    my ($self, $data) = @_;
    delete $data->{Database}{sql_meta}{$self->{table}};
    return 1;
} # drop

sub seek
{
    my ( $self, $data, $pos, $whence ) = @_;
    return unless defined $self->{records};

    my ($currentRow) = $self->{index};
    if ( $whence == 0 )
    {
        $currentRow = $pos;
    }
    elsif ( $whence == 1 )
    {
        $currentRow += $pos;
    }
    elsif ( $whence == 2 )
    {
        $currentRow = @{ $self->{records} } + $pos;
    }
    else
    {
        croak $self . "->seek: Illegal whence argument ($whence)";
    }

    $currentRow < 0 and
        croak "Illegal row number: $currentRow";
    $self->{index} = $currentRow;
}

1;

=head1 NAME

DBD::Mem - a DBI driver for Mem & MLMem files

=head1 SYNOPSIS

 use DBI;
 $dbh = DBI->connect('dbi:Mem:', undef, undef, {});
 $dbh = DBI->connect('dbi:Mem:', undef, undef, {RaiseError => 1});

 # or
 $dbh = DBI->connect('dbi:Mem:');
 $dbh = DBI->connect('DBI:Mem(RaiseError=1):');

and other variations on connect() as shown in the L<DBI> docs and 
<DBI::DBD::SqlEngine metadata|DBI::DBD::SqlEngine/Metadata>.

Use standard DBI prepare, execute, fetch, placeholders, etc.,
see L<QUICK START> for an example.

=head1 DESCRIPTION

DBD::Mem is a database management system that works right out of the box.
If you have a standard installation of Perl and DBI you can begin creating,
accessing, and modifying simple database tables without any further modules.
You can add other modules (e.g., SQL::Statement) for improved functionality.

DBD::Mem doesn't store any data persistently - all data has the lifetime of
the instantiated C<$dbh>. The main reason to use DBD::Mem is to use extended
features of L<SQL::Statement> where temporary tables are required. One can
use DBD::Mem to simulate C<VIEWS> or sub-queries.

Bundling C<DBD::Mem> with L<DBI> will allow us further compatibility checks
of L<DBI::DBD::SqlEngine> beyond the capabilities of L<DBD::File> and
L<DBD::DBM>. This will ensure DBI provided basis for drivers like
L<DBD::AnyData2> or L<DBD::Amazon> are better prepared and tested for
not-file based backends.

=head2 Metadata

There're no new meta data introduced by C<DBD::Mem>. See
L<DBI::DBD::SqlEngine/Metadata> for full description.

=head1 GETTING HELP, MAKING SUGGESTIONS, AND REPORTING BUGS

If you need help installing or using DBD::Mem, please write to the DBI
users mailing list at L<mailto:dbi-users@perl.org> or to the
comp.lang.perl.modules newsgroup on usenet.  I cannot always answer
every question quickly but there are many on the mailing list or in
the newsgroup who can.

DBD developers for DBD's which rely on DBI::DBD::SqlEngine or DBD::Mem or
use one of them as an example are suggested to join the DBI developers
mailing list at L<mailto:dbi-dev@perl.org> and strongly encouraged to join our
IRC channel at L<irc://irc.perl.org/dbi>.

If you have suggestions, ideas for improvements, or bugs to report, please
report a bug as described in DBI. Do not mail any of the authors directly,
you might not get an answer.

When reporting bugs, please send the output of C<< $dbh->mem_versions($table) >>
for a table that exhibits the bug and as small a sample as you can make of
the code that produces the bug.  And of course, patches are welcome, too
:-).

If you need enhancements quickly, you can get commercial support as
described at L<http://dbi.perl.org/support/> or you can contact Jens Rehsack
at rehsack@cpan.org for commercial support.

=head1 AUTHOR AND COPYRIGHT

This module is written by Jens Rehsack < rehsack AT cpan.org >.

 Copyright (c) 2016- by Jens Rehsack, all rights reserved.

You may freely distribute and/or modify this module under the terms of
either the GNU General Public License (GPL) or the Artistic License, as
specified in the Perl README file.

=head1 SEE ALSO

L<DBI> for the Database interface of the Perl Programming Language.

L<SQL::Statement> and L<DBI::SQL::Nano> for the available SQL engines.

L<SQL::Statement::RAM> where the implementation is shamelessly stolen from
to allow DBI bundled Pure-Perl drivers increase the test coverage.

L<DBD::SQLite> using C<dbname=:memory:> for an incredible fast in-memory database engine.

=cut
