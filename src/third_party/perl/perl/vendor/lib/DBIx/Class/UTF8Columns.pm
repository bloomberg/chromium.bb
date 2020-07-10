package DBIx::Class::UTF8Columns;
use strict;
use warnings;
use base qw/DBIx::Class/;

__PACKAGE__->mk_classdata( '_utf8_columns' );

=head1 NAME

DBIx::Class::UTF8Columns - Force UTF8 (Unicode) flag on columns (DEPRECATED)

=head1 SYNOPSIS

    package Artist;
    use base 'DBIx::Class::Core';

    __PACKAGE__->load_components(qw/UTF8Columns/);
    __PACKAGE__->utf8_columns(qw/name description/);

    # then belows return strings with utf8 flag
    $artist->name;
    $artist->get_column('description');

=head1 DESCRIPTION

This module allows you to get and store utf8 (unicode) column data
in a database that does not natively support unicode. It ensures
that column data is correctly serialised as a byte stream when
stored and de-serialised to unicode strings on retrieval.

  THE USE OF THIS MODULE (AND ITS COUSIN DBIx::Class::ForceUTF8) IS VERY
  STRONGLY DISCOURAGED, PLEASE READ THE WARNINGS BELOW FOR AN EXPLANATION.

If you want to continue using this module and do not want to receive
further warnings set the environment variable C<DBIC_UTF8COLUMNS_OK>
to a true value.

=head2 Warning - Module does not function properly on create/insert

Recently (April 2010) a bug was found deep in the core of L<DBIx::Class>
which affects any component attempting to perform encoding/decoding by
overloading L<store_column|DBIx::Class::Row/store_column> and
L<get_columns|DBIx::Class::Row/get_columns>. As a result of this problem
L<create|DBIx::Class::ResultSet/create> sends the original column values
to the database, while L<update|DBIx::Class::ResultSet/update> sends the
encoded values. L<DBIx::Class::UTF8Columns> and L<DBIx::Class::ForceUTF8>
are both affected by this bug.

It is unclear how this bug went undetected for so long (it was
introduced in March 2006), No attempts to fix it will be made while the
implications of changing such a fundamental behavior of DBIx::Class are
being evaluated. However in this day and age you should not be using
this module anyway as Unicode is properly supported by all major
database engines, as explained below.

If you have specific questions about the integrity of your data in light
of this development - please
L<join us on IRC or the mailing list|DBIx::Class/GETTING HELP/SUPPORT>
to further discuss your concerns with the team.

=head2 Warning - Native Database Unicode Support

If your database natively supports Unicode (as does SQLite with the
C<sqlite_unicode> connect flag, MySQL with C<mysql_enable_utf8>
connect flag or Postgres with the C<pg_enable_utf8> connect flag),
then this component should B<not> be used, and will corrupt unicode
data in a subtle and unexpected manner.

It is far better to do Unicode support within the database if
possible rather than converting data to and from raw bytes on every
database round trip.

=head2 Warning - Component Overloading

Note that this module overloads L<DBIx::Class::Row/store_column> in a way
that may prevent other components overloading the same method from working
correctly. This component must be the last one before L<DBIx::Class::Row>
(which is provided by L<DBIx::Class::Core>). DBIx::Class will detect such
incorrect component order and issue an appropriate warning, advising which
components need to be loaded differently.

=head1 SEE ALSO

L<Template::Stash::ForceUTF8>, L<DBIx::Class::UUIDColumns>.

=head1 METHODS

=head2 utf8_columns

=cut

sub utf8_columns {
    my $self = shift;
    if (@_) {
        foreach my $col (@_) {
            $self->throw_exception("column $col doesn't exist")
                unless $self->has_column($col);
        }
        return $self->_utf8_columns({ map { $_ => 1 } @_ });
    } else {
        return $self->_utf8_columns;
    }
}

=head1 EXTENDED METHODS

=head2 get_column

=cut

sub get_column {
    my ( $self, $column ) = @_;
    my $value = $self->next::method($column);

    utf8::decode($value) if (
      defined $value and $self->_is_utf8_column($column) and ! utf8::is_utf8($value)
    );

    return $value;
}

=head2 get_columns

=cut

sub get_columns {
    my $self = shift;
    my %data = $self->next::method(@_);

    foreach my $col (keys %data) {
      utf8::decode($data{$col}) if (
        exists $data{$col} and defined $data{$col} and $self->_is_utf8_column($col) and ! utf8::is_utf8($data{$col})
      );
    }

    return %data;
}

=head2 store_column

=cut

sub store_column {
    my ( $self, $column, $value ) = @_;

    # the dirtiness comparison must happen on the non-encoded value
    my $copy;

    if ( defined $value and $self->_is_utf8_column($column) and utf8::is_utf8($value) ) {
      $copy = $value;
      utf8::encode($value);
    }

    $self->next::method( $column, $value );

    return $copy || $value;
}

# override this if you want to force everything to be encoded/decoded
sub _is_utf8_column {
  # my ($self, $col) = @_;
  return ($_[0]->utf8_columns || {})->{$_[1]};
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
