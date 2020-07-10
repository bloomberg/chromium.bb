package SQL::Statement::Util;

######################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2007-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
# See below for help and copyright information (search for SYNOPSIS).
#
######################################################################

use strict;
use warnings FATAL => "all";

use vars qw($VERSION);
$VERSION = '1.412';

sub type
{
    my ($self) = @_;
    return 'function' if $self->isa('SQL::Statement::Util::Function');
    return 'column'   if $self->isa('SQL::Statement::Util::Column');
}

package SQL::Statement::Util::Column;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Util);

use Params::Util qw(_ARRAY _HASH0 _STRING);

sub new
{
    my ( $class, $col_name, $table_name, $term, $display_name, $full_orig_name, $coldef ) = @_;
    $display_name ||= $col_name;

    if ( $col_name && ( $col_name =~ m/^((?:"[^"]+")|(?:[^.]*))\.(.*)$/ ) )
    {
        $table_name = $1;
        $col_name   = $2;
    }
    elsif ( defined( _ARRAY($table_name) ) && ( scalar( @{$table_name} ) == 1 ) )
    {
        $table_name = $table_name->[0];
    }

    my %instance = (
        name           => $col_name,
        table          => $table_name,
        display_name   => $display_name,
        term           => $term,
        full_orig_name => $full_orig_name,
        coldef         => $coldef,
    );

    my $self = bless( \%instance, $class );

    return $self;
}

sub value($)         { $_[0]->{term}->value( $_[1] ); }
sub term()           { $_[0]->{term} }
sub display_name()   { $_[0]->{display_name} }
sub full_orig_name() { $_[0]->{full_orig_name} }
sub name()           { $_[0]->{name} }
sub table()          { $_[0]->{table} }
sub coldef()         { $_[0]->{coldef} }

package SQL::Statement::Util::Function;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Util);

sub new
{
    my ( $class, $name, $sub_name, $args ) = @_;
    my ( $pkg, $sub ) = $sub_name =~ /^(.*::)([^:]+$)/;
    if ( !$sub )
    {
        $pkg = 'main';
        $sub = $sub_name;
    }
    $pkg = 'main' if $pkg eq '::';
    $pkg =~ s/::$//;
    my %newfunc = (
        name     => $name,
        sub_name => $sub,
        pkg_name => $pkg,
        args     => $args,
        type     => 'function',
    );
    return bless \%newfunc, $class;
}
sub name     { shift->{name} }
sub pkg_name { shift->{pkg_name} }
sub sub_name { shift->{sub_name} }
sub args     { shift->{args} }

sub validate
{
    my ($self) = @_;
    my $pkg    = $self->pkg_name;
    my $sub    = $self->sub_name;
    $pkg =~ s,::,/,g;
    eval { require "$pkg.pm" }
      unless $pkg eq 'SQL/Statement/Functions'
      or $pkg eq 'main';
    die $@ if $@;
    $pkg =~ s,/,::,g;
    die "Can't find subroutine $pkg" . "::$sub\n" unless $pkg->can($sub);
    return 1;
}

sub run
{
    use SQL::Statement::Functions;

    my ($self) = shift;
    my $sub    = $self->sub_name;
    my $pkg    = $self->pkg_name;
    return $pkg->$sub(@_);
}

1;

=pod

=head1 NAME

SQL::Statement::Util

=head1 SYNOPSIS

  SQL::Statement::Util::Column->new($col_name, $table_name, $term, $display_name)
  SQL::Statement::Util::AggregatedColumns($col_name, $table_name, $term, $display_name)
  SQL::Statement::Util::Function($name, $sub_name, $args)

=head1 DESCRIPTION

This package contains three utility classes to handle deliverable columns.

=head1 INHERITANCE

  SQL::Statement::Util::Column
  ISA SQL::Statement::Util

  SQL::Statement::Util::AggregatedColumns
  ISA SQL::Statement::Util::Column
    ISA SQL::Statement::Util

  SQL::Statement::Util::Function
  ISA SQL::Statement::Util

=begin undocumented

=head1 METHODS

=head2 type

Returns the type of the SQL::Statement::Util instance.

=end undocumented

=head1 AUTHOR & COPYRIGHT

 This module is

 copyright (c) 2001,2005 by Jeff Zucker and
 copyright (c) 2007-2017 by Jens Rehsack.

 All rights reserved.

The module may be freely distributed under the same terms as
Perl itself using either the "GPL License" or the "Artistic
License" as specified in the Perl README file.

Jeff can be reached at: jzuckerATcpan.org
Jens can be reached at: rehsackATcpan.org or via dbi-devATperl.org

=cut
