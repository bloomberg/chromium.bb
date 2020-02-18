package DBIx::Class::CDBICompat;

use strict;
use warnings;
use base qw/DBIx::Class::Core DBIx::Class::DB/;

# Modules CDBICompat needs that DBIx::Class does not.
my @Extra_Modules = qw(
    Class::Trigger
    DBIx::ContextualFetch
    Clone
);

my @didnt_load;
for my $module (@Extra_Modules) {
    push @didnt_load, $module unless eval qq{require $module};
}
__PACKAGE__->throw_exception("@{[ join ', ', @didnt_load ]} are missing and are required for CDBICompat")
    if @didnt_load;


__PACKAGE__->load_own_components(qw/
  Constraints
  Triggers
  ReadOnly
  LiveObjectIndex
  AttributeAPI
  Stringify
  DestroyWarning
  Constructor
  AccessorMapping
  ColumnCase
  Relationships
  Copy
  LazyLoading
  AutoUpdate
  TempColumns
  GetSet
  Retrieve
  Pager
  ColumnGroups
  ColumnsAsHash
  AbstractSearch
  ImaDBI
  Iterator
/);

1;

__END__

=head1 NAME

DBIx::Class::CDBICompat - Class::DBI Compatibility layer.

=head1 SYNOPSIS

  package My::CDBI;
  use base qw/DBIx::Class::CDBICompat/;

  ...continue as Class::DBI...

=head1 DESCRIPTION

DBIx::Class features a fully featured compatibility layer with L<Class::DBI>
and some common plugins to ease transition for existing CDBI users.

This is not a wrapper or subclass of DBIx::Class but rather a series of plugins.  The result being that even though you're using the Class::DBI emulation layer you are still getting DBIx::Class objects.  You can use all DBIx::Class features and methods via CDBICompat.  This allows you to take advantage of DBIx::Class features without having to rewrite your CDBI code.


=head2 Plugins

CDBICompat is good enough that many CDBI plugins will work with CDBICompat, but many of the plugin features are better done with DBIx::Class methods.

=head3 Class::DBI::AbstractSearch

C<search_where()> is fully emulated using DBIC's search.  Aside from emulation there's no reason to use C<search_where()>.

=head3 Class::DBI::Plugin::NoCache

C<nocache> is fully emulated.

=head3 Class::DBI::Sweet

The features of CDBI::Sweet are better done using DBIC methods which are almost exactly the same.  It even uses L<Data::Page>.

=head3 Class::DBI::Plugin::DeepAbstractSearch

This plugin will work, but it is more efficiently done using DBIC's native search facilities.  The major difference is that DBIC will not infer the join for you, you have to tell it the join tables.


=head2 Choosing Features

In fact, this class is just a recipe containing all the features emulated.
If you like, you can choose which features to emulate by building your
own class and loading it like this:

  package My::DB;
  __PACKAGE__->load_own_components(qw/CDBICompat/);

this will automatically load the features included in My::DB::CDBICompat,
provided it looks something like this:

  package My::DB::CDBICompat;
  __PACKAGE__->load_components(qw/
    CDBICompat::ColumnGroups
    CDBICompat::Retrieve
    CDBICompat::HasA
    CDBICompat::HasMany
    CDBICompat::MightHave
  /);


=head1 LIMITATIONS

=head2 Unimplemented

The following methods and classes are not emulated, maybe in the future.

=over 4

=item Class::DBI::Query

Deprecated in Class::DBI.

=item Class::DBI::Column

Not documented in Class::DBI.  CDBICompat's columns() returns a plain string, not an object.

=item data_type()

Undocumented CDBI method.

=back

=head2 Limited Support

The following elements of Class::DBI have limited support.

=over 4

=item Class::DBI::Relationship

The semi-documented Class::DBI::Relationship objects returned by C<meta_info($type, $col)> are mostly emulated except for their C<args> method.

=item Relationships

Relationships between tables (has_a, has_many...) must be declared after all tables in the relationship have been declared.  Thus the usual CDBI idiom of declaring columns and relationships for each class together will not work.  They must instead be done like so:

    package Foo;
    use base qw(Class::DBI);

    Foo->table("foo");
    Foo->columns( All => qw(this that bar) );

    package Bar;
    use base qw(Class::DBI);

    Bar->table("bar");
    Bar->columns( All => qw(up down) );

    # Now that Foo and Bar are declared it is safe to declare a
    # relationship between them
    Foo->has_a( bar => "Bar" );


=back

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
