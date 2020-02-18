package DBIx::Simple::Result::RowObject;
use base 'Object::Accessor';
sub new {
    my ($class, %args) = @_;

    my $self = $class->SUPER::new;
    $self->mk_accessors(keys %args);
    $self->$_( $args{$_} ) for keys %args;

    return $self;
}

1;

=head1 NAME

DBIx::Simple::Result::RowObject - Simple result row object class

=head1 DESCRIPTION

This class is the default for the C<object> and C<objects> result object
methods. Mainly, it provides syntactic sugar at the expense of performance.

Instead of writing

    my $r = $db->query('SELECT foo, bar FROM baz')->hash;
    do_something_with $r->{foo}, $r->{bar};

you may write

    my $r = $db->query('SELECT foo, bar FROM baz')->object;
    do_something_with $r->foo, $r->bar;

This class is a subclass of Object::Accessor, which provides per-object (rather
than per-class) accessors. Your records must not have columns names like these:

    * can
    * ls_accessors
    * ls_allow
    * mk_accessor
    * mk_clone
    * mk_flush
    * mk_verify
    * new
    * register_callback
    * ___autoload
    * ___callback
    * ___debug
    * ___error
    * ___get
    * ___set

And of course DESTROY and AUTOLOAD, and anything that new versions of
Object::Accessor might add.

=head1 DBIx::Simple::OO

DBIx::Simple::OO is a third party module by Jos Boumans that provided C<object>
and C<objects> to DBIx::Simple. Similar functionality is now built in, in part
inspired by DBIx::Simple:OO.

Using DBIx::Simple 1.33 or newer together with DBIx::Simple::OO 0.01 will
result in method name clash. DBIx::Simple::Result::RowObject was written to be
compatible with DBIx::Simple::OO::Item, except for the name, so C<isa> calls
still need to be changed.

In practice, DBIx::Simple 1.33 makes DBIx::Simple::OO obsolete.

=head1 AUTHOR

Juerd Waalboer <juerd@cpan.org> <http://juerd.nl/>

=head1 SEE ALSO

L<DBIx::Simple>
