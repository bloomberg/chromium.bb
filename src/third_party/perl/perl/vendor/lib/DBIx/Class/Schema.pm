package DBIx::Class::Schema;

use strict;
use warnings;

use base 'DBIx::Class';

use DBIx::Class::Carp;
use Try::Tiny;
use Scalar::Util qw/weaken blessed/;
use DBIx::Class::_Util qw(refcount quote_sub is_exception scope_guard);
use Devel::GlobalDestruction;
use namespace::clean;

__PACKAGE__->mk_classdata('class_mappings' => {});
__PACKAGE__->mk_classdata('source_registrations' => {});
__PACKAGE__->mk_classdata('storage_type' => '::DBI');
__PACKAGE__->mk_classdata('storage');
__PACKAGE__->mk_classdata('exception_action');
__PACKAGE__->mk_classdata('stacktrace' => $ENV{DBIC_TRACE} || 0);
__PACKAGE__->mk_classdata('default_resultset_attributes' => {});

=head1 NAME

DBIx::Class::Schema - composable schemas

=head1 SYNOPSIS

  package Library::Schema;
  use base qw/DBIx::Class::Schema/;

  # load all Result classes in Library/Schema/Result/
  __PACKAGE__->load_namespaces();

  package Library::Schema::Result::CD;
  use base qw/DBIx::Class::Core/;

  __PACKAGE__->load_components(qw/InflateColumn::DateTime/); # for example
  __PACKAGE__->table('cd');

  # Elsewhere in your code:
  my $schema1 = Library::Schema->connect(
    $dsn,
    $user,
    $password,
    { AutoCommit => 1 },
  );

  my $schema2 = Library::Schema->connect($coderef_returning_dbh);

  # fetch objects using Library::Schema::Result::DVD
  my $resultset = $schema1->resultset('DVD')->search( ... );
  my @dvd_objects = $schema2->resultset('DVD')->search( ... );

=head1 DESCRIPTION

Creates database classes based on a schema. This is the recommended way to
use L<DBIx::Class> and allows you to use more than one concurrent connection
with your classes.

NB: If you're used to L<Class::DBI> it's worth reading the L</SYNOPSIS>
carefully, as DBIx::Class does things a little differently. Note in
particular which module inherits off which.

=head1 SETUP METHODS

=head2 load_namespaces

=over 4

=item Arguments: %options?

=back

  package MyApp::Schema;
  __PACKAGE__->load_namespaces();

  __PACKAGE__->load_namespaces(
     result_namespace => 'Res',
     resultset_namespace => 'RSet',
     default_resultset_class => '+MyApp::Othernamespace::RSet',
  );

With no arguments, this method uses L<Module::Find> to load all of the
Result and ResultSet classes under the namespace of the schema from
which it is called.  For example, C<My::Schema> will by default find
and load Result classes named C<My::Schema::Result::*> and ResultSet
classes named C<My::Schema::ResultSet::*>.

ResultSet classes are associated with Result class of the same name.
For example, C<My::Schema::Result::CD> will get the ResultSet class
C<My::Schema::ResultSet::CD> if it is present.

Both Result and ResultSet namespaces are configurable via the
C<result_namespace> and C<resultset_namespace> options.

Another option, C<default_resultset_class> specifies a custom default
ResultSet class for Result classes with no corresponding ResultSet.

All of the namespace and classname options are by default relative to
the schema classname.  To specify a fully-qualified name, prefix it
with a literal C<+>.  For example, C<+Other::NameSpace::Result>.

=head3 Warnings

You will be warned if ResultSet classes are discovered for which there
are no matching Result classes like this:

  load_namespaces found ResultSet class $classname with no corresponding Result class

If a ResultSource instance is found to already have a ResultSet class set
using L<resultset_class|DBIx::Class::ResultSource/resultset_class> to some
other class, you will be warned like this:

  We found ResultSet class '$rs_class' for '$result_class', but it seems
  that you had already set '$result_class' to use '$rs_set' instead

=head3 Examples

  # load My::Schema::Result::CD, My::Schema::Result::Artist,
  #    My::Schema::ResultSet::CD, etc...
  My::Schema->load_namespaces;

  # Override everything to use ugly names.
  # In this example, if there is a My::Schema::Res::Foo, but no matching
  #   My::Schema::RSets::Foo, then Foo will have its
  #   resultset_class set to My::Schema::RSetBase
  My::Schema->load_namespaces(
    result_namespace => 'Res',
    resultset_namespace => 'RSets',
    default_resultset_class => 'RSetBase',
  );

  # Put things in other namespaces
  My::Schema->load_namespaces(
    result_namespace => '+Some::Place::Results',
    resultset_namespace => '+Another::Place::RSets',
  );

To search multiple namespaces for either Result or ResultSet classes,
use an arrayref of namespaces for that option.  In the case that the
same result (or resultset) class exists in multiple namespaces, later
entries in the list of namespaces will override earlier ones.

  My::Schema->load_namespaces(
    # My::Schema::Results_C::Foo takes precedence over My::Schema::Results_B::Foo :
    result_namespace => [ 'Results_A', 'Results_B', 'Results_C' ],
    resultset_namespace => [ '+Some::Place::RSets', 'RSets' ],
  );

=cut

# Pre-pends our classname to the given relative classname or
#   class namespace, unless there is a '+' prefix, which will
#   be stripped.
sub _expand_relative_name {
  my ($class, $name) = @_;
  $name =~ s/^\+// or $name = "${class}::${name}";
  return $name;
}

# Finds all modules in the supplied namespace, or if omitted in the
# namespace of $class. Untaints all findings as they can be assumed
# to be safe
sub _findallmod {
  require Module::Find;
  return map
    { $_ =~ /(.+)/ }   # untaint result
    Module::Find::findallmod( $_[1] || ref $_[0] || $_[0] )
  ;
}

# returns a hash of $shortname => $fullname for every package
# found in the given namespaces ($shortname is with the $fullname's
# namespace stripped off)
sub _map_namespaces {
  my ($me, $namespaces) = @_;

  my %res;
  for my $ns (@$namespaces) {
    $res{ substr($_, length "${ns}::") } = $_
      for $me->_findallmod($ns);
  }

  \%res;
}

# returns the result_source_instance for the passed class/object,
# or dies with an informative message (used by load_namespaces)
sub _ns_get_rsrc_instance {
  my $me = shift;
  my $rs_class = ref ($_[0]) || $_[0];

  return try {
    $rs_class->result_source_instance
  } catch {
    $me->throw_exception (
      "Attempt to load_namespaces() class $rs_class failed - are you sure this is a real Result Class?: $_"
    );
  };
}

sub load_namespaces {
  my ($class, %args) = @_;

  my $result_namespace = delete $args{result_namespace} || 'Result';
  my $resultset_namespace = delete $args{resultset_namespace} || 'ResultSet';

  my $default_resultset_class = delete $args{default_resultset_class};

  $default_resultset_class = $class->_expand_relative_name($default_resultset_class)
    if $default_resultset_class;

  $class->throw_exception('load_namespaces: unknown option(s): '
    . join(q{,}, map { qq{'$_'} } keys %args))
      if scalar keys %args;

  for my $arg ($result_namespace, $resultset_namespace) {
    $arg = [ $arg ] if ( $arg and ! ref $arg );

    $class->throw_exception('load_namespaces: namespace arguments must be '
      . 'a simple string or an arrayref')
        if ref($arg) ne 'ARRAY';

    $_ = $class->_expand_relative_name($_) for (@$arg);
  }

  my $results_by_source_name = $class->_map_namespaces($result_namespace);
  my $resultsets_by_source_name = $class->_map_namespaces($resultset_namespace);

  my @to_register;
  {
    no warnings qw/redefine/;
    local *Class::C3::reinitialize = sub { } if DBIx::Class::_ENV_::OLD_MRO;
    use warnings qw/redefine/;

    # ensure classes are loaded and attached in inheritance order
    for my $result_class (values %$results_by_source_name) {
      $class->ensure_class_loaded($result_class);
    }
    my %inh_idx;
    my @source_names_by_subclass_last = sort {

      ($inh_idx{$a} ||=
        scalar @{mro::get_linear_isa( $results_by_source_name->{$a} )}
      )

          <=>

      ($inh_idx{$b} ||=
        scalar @{mro::get_linear_isa( $results_by_source_name->{$b} )}
      )

    } keys(%$results_by_source_name);

    foreach my $source_name (@source_names_by_subclass_last) {
      my $result_class = $results_by_source_name->{$source_name};

      my $preset_resultset_class = $class->_ns_get_rsrc_instance ($result_class)->resultset_class;
      my $found_resultset_class = delete $resultsets_by_source_name->{$source_name};

      if($preset_resultset_class && $preset_resultset_class ne 'DBIx::Class::ResultSet') {
        if($found_resultset_class && $found_resultset_class ne $preset_resultset_class) {
          carp "We found ResultSet class '$found_resultset_class' matching '$results_by_source_name->{$source_name}', but it seems "
             . "that you had already set the '$results_by_source_name->{$source_name}' resultet to '$preset_resultset_class' instead";
        }
      }
      # elsif - there may be *no* default_resultset_class, in which case we fallback to
      # DBIx::Class::Resultset and there is nothing to check
      elsif($found_resultset_class ||= $default_resultset_class) {
        $class->ensure_class_loaded($found_resultset_class);
        if(!$found_resultset_class->isa("DBIx::Class::ResultSet")) {
            carp "load_namespaces found ResultSet class '$found_resultset_class' that does not subclass DBIx::Class::ResultSet";
        }

        $class->_ns_get_rsrc_instance ($result_class)->resultset_class($found_resultset_class);
      }

      my $source_name = $class->_ns_get_rsrc_instance ($result_class)->source_name || $source_name;

      push(@to_register, [ $source_name, $result_class ]);
    }
  }

  foreach (sort keys %$resultsets_by_source_name) {
    carp "load_namespaces found ResultSet class '$resultsets_by_source_name->{$_}' "
        .'with no corresponding Result class';
  }

  Class::C3->reinitialize if DBIx::Class::_ENV_::OLD_MRO;

  $class->register_class(@$_) for (@to_register);

  return;
}

=head2 load_classes

=over 4

=item Arguments: @classes?, { $namespace => [ @classes ] }+

=back

L</load_classes> is an alternative method to L</load_namespaces>, both of
which serve similar purposes, each with different advantages and disadvantages.
In the general case you should use L</load_namespaces>, unless you need to
be able to specify that only specific classes are loaded at runtime.

With no arguments, this method uses L<Module::Find> to find all classes under
the schema's namespace. Otherwise, this method loads the classes you specify
(using L<use>), and registers them (using L</"register_class">).

It is possible to comment out classes with a leading C<#>, but note that perl
will think it's a mistake (trying to use a comment in a qw list), so you'll
need to add C<no warnings 'qw';> before your load_classes call.

If any classes found do not appear to be Result class files, you will
get the following warning:

   Failed to load $comp_class. Can't find source_name method. Is
   $comp_class really a full DBIC result class? Fix it, move it elsewhere,
   or make your load_classes call more specific.

Example:

  My::Schema->load_classes(); # loads My::Schema::CD, My::Schema::Artist,
                              # etc. (anything under the My::Schema namespace)

  # loads My::Schema::CD, My::Schema::Artist, Other::Namespace::Producer but
  # not Other::Namespace::LinerNotes nor My::Schema::Track
  My::Schema->load_classes(qw/ CD Artist #Track /, {
    Other::Namespace => [qw/ Producer #LinerNotes /],
  });

=cut

sub load_classes {
  my ($class, @params) = @_;

  my %comps_for;

  if (@params) {
    foreach my $param (@params) {
      if (ref $param eq 'ARRAY') {
        # filter out commented entries
        my @modules = grep { $_ !~ /^#/ } @$param;

        push (@{$comps_for{$class}}, @modules);
      }
      elsif (ref $param eq 'HASH') {
        # more than one namespace possible
        for my $comp ( keys %$param ) {
          # filter out commented entries
          my @modules = grep { $_ !~ /^#/ } @{$param->{$comp}};

          push (@{$comps_for{$comp}}, @modules);
        }
      }
      else {
        # filter out commented entries
        push (@{$comps_for{$class}}, $param) if $param !~ /^#/;
      }
    }
  } else {
    my @comp = map { substr $_, length "${class}::"  }
                 $class->_findallmod($class);
    $comps_for{$class} = \@comp;
  }

  my @to_register;
  {
    no warnings qw/redefine/;
    local *Class::C3::reinitialize = sub { } if DBIx::Class::_ENV_::OLD_MRO;
    use warnings qw/redefine/;

    foreach my $prefix (keys %comps_for) {
      foreach my $comp (@{$comps_for{$prefix}||[]}) {
        my $comp_class = "${prefix}::${comp}";
        $class->ensure_class_loaded($comp_class);

        my $snsub = $comp_class->can('source_name');
        if(! $snsub ) {
          carp "Failed to load $comp_class. Can't find source_name method. Is $comp_class really a full DBIC result class? Fix it, move it elsewhere, or make your load_classes call more specific.";
          next;
        }
        $comp = $snsub->($comp_class) || $comp;

        push(@to_register, [ $comp, $comp_class ]);
      }
    }
  }
  Class::C3->reinitialize if DBIx::Class::_ENV_::OLD_MRO;

  foreach my $to (@to_register) {
    $class->register_class(@$to);
  }
}

=head2 storage_type

=over 4

=item Arguments: $storage_type|{$storage_type, \%args}

=item Return Value: $storage_type|{$storage_type, \%args}

=item Default value: DBIx::Class::Storage::DBI

=back

Set the storage class that will be instantiated when L</connect> is called.
If the classname starts with C<::>, the prefix C<DBIx::Class::Storage> is
assumed by L</connect>.

You want to use this to set subclasses of L<DBIx::Class::Storage::DBI>
in cases where the appropriate subclass is not autodetected.

If your storage type requires instantiation arguments, those are
defined as a second argument in the form of a hashref and the entire
value needs to be wrapped into an arrayref or a hashref.  We support
both types of refs here in order to play nice with your
Config::[class] or your choice. See
L<DBIx::Class::Storage::DBI::Replicated> for an example of this.

=head2 exception_action

=over 4

=item Arguments: $code_reference

=item Return Value: $code_reference

=item Default value: None

=back

When L</throw_exception> is invoked and L</exception_action> is set to a code
reference, this reference will be called instead of
L<DBIx::Class::Exception/throw>, with the exception message passed as the only
argument.

Your custom throw code B<must> rethrow the exception, as L</throw_exception> is
an integral part of DBIC's internal execution control flow.

Example:

   package My::Schema;
   use base qw/DBIx::Class::Schema/;
   use My::ExceptionClass;
   __PACKAGE__->exception_action(sub { My::ExceptionClass->throw(@_) });
   __PACKAGE__->load_classes;

   # or:
   my $schema_obj = My::Schema->connect( .... );
   $schema_obj->exception_action(sub { My::ExceptionClass->throw(@_) });

=head2 stacktrace

=over 4

=item Arguments: boolean

=back

Whether L</throw_exception> should include stack trace information.
Defaults to false normally, but defaults to true if C<$ENV{DBIC_TRACE}>
is true.

=head2 sqlt_deploy_hook

=over

=item Arguments: $sqlt_schema

=back

An optional sub which you can declare in your own Schema class that will get
passed the L<SQL::Translator::Schema> object when you deploy the schema via
L</create_ddl_dir> or L</deploy>.

For an example of what you can do with this, see
L<DBIx::Class::Manual::Cookbook/Adding Indexes And Functions To Your SQL>.

Note that sqlt_deploy_hook is called by L</deployment_statements>, which in turn
is called before L</deploy>. Therefore the hook can be used only to manipulate
the L<SQL::Translator::Schema> object before it is turned into SQL fed to the
database. If you want to execute post-deploy statements which can not be generated
by L<SQL::Translator>, the currently suggested method is to overload L</deploy>
and use L<dbh_do|DBIx::Class::Storage::DBI/dbh_do>.

=head1 METHODS

=head2 connect

=over 4

=item Arguments: @connectinfo

=item Return Value: $new_schema

=back

Creates and returns a new Schema object. The connection info set on it
is used to create a new instance of the storage backend and set it on
the Schema object.

See L<DBIx::Class::Storage::DBI/"connect_info"> for DBI-specific
syntax on the C<@connectinfo> argument, or L<DBIx::Class::Storage> in
general.

Note that C<connect_info> expects an arrayref of arguments, but
C<connect> does not. C<connect> wraps its arguments in an arrayref
before passing them to C<connect_info>.

=head3 Overloading

C<connect> is a convenience method. It is equivalent to calling
$schema->clone->connection(@connectinfo). To write your own overloaded
version, overload L</connection> instead.

=cut

sub connect { shift->clone->connection(@_) }

=head2 resultset

=over 4

=item Arguments: L<$source_name|DBIx::Class::ResultSource/source_name>

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

  my $rs = $schema->resultset('DVD');

Returns the L<DBIx::Class::ResultSet> object for the registered source
name.

=cut

sub resultset {
  my ($self, $source_name) = @_;
  $self->throw_exception('resultset() expects a source name')
    unless defined $source_name;
  return $self->source($source_name)->resultset;
}

=head2 sources

=over 4

=item Return Value: L<@source_names|DBIx::Class::ResultSource/source_name>

=back

  my @source_names = $schema->sources;

Lists names of all the sources registered on this Schema object.

=cut

sub sources { keys %{shift->source_registrations} }

=head2 source

=over 4

=item Arguments: L<$source_name|DBIx::Class::ResultSource/source_name>

=item Return Value: L<$result_source|DBIx::Class::ResultSource>

=back

  my $source = $schema->source('Book');

Returns the L<DBIx::Class::ResultSource> object for the registered
source name.

=cut

sub source {
  my $self = shift;

  $self->throw_exception("source() expects a source name")
    unless @_;

  my $source_name = shift;

  my $sreg = $self->source_registrations;
  return $sreg->{$source_name} if exists $sreg->{$source_name};

  # if we got here, they probably passed a full class name
  my $mapped = $self->class_mappings->{$source_name};
  $self->throw_exception("Can't find source for ${source_name}")
    unless $mapped && exists $sreg->{$mapped};
  return $sreg->{$mapped};
}

=head2 class

=over 4

=item Arguments: L<$source_name|DBIx::Class::ResultSource/source_name>

=item Return Value: $classname

=back

  my $class = $schema->class('CD');

Retrieves the Result class name for the given source name.

=cut

sub class {
  return shift->source(shift)->result_class;
}

=head2 txn_do

=over 4

=item Arguments: C<$coderef>, @coderef_args?

=item Return Value: The return value of $coderef

=back

Executes C<$coderef> with (optional) arguments C<@coderef_args> atomically,
returning its result (if any). Equivalent to calling $schema->storage->txn_do.
See L<DBIx::Class::Storage/"txn_do"> for more information.

This interface is preferred over using the individual methods L</txn_begin>,
L</txn_commit>, and L</txn_rollback> below.

WARNING: If you are connected with C<< AutoCommit => 0 >> the transaction is
considered nested, and you will still need to call L</txn_commit> to write your
changes when appropriate. You will also want to connect with C<< auto_savepoint =>
1 >> to get partial rollback to work, if the storage driver for your database
supports it.

Connecting with C<< AutoCommit => 1 >> is recommended.

=cut

sub txn_do {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_do called on $schema without storage');

  $self->storage->txn_do(@_);
}

=head2 txn_scope_guard

Runs C<txn_scope_guard> on the schema's storage. See
L<DBIx::Class::Storage/txn_scope_guard>.

=cut

sub txn_scope_guard {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_scope_guard called on $schema without storage');

  $self->storage->txn_scope_guard(@_);
}

=head2 txn_begin

Begins a transaction (does nothing if AutoCommit is off). Equivalent to
calling $schema->storage->txn_begin. See
L<DBIx::Class::Storage/"txn_begin"> for more information.

=cut

sub txn_begin {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_begin called on $schema without storage');

  $self->storage->txn_begin;
}

=head2 txn_commit

Commits the current transaction. Equivalent to calling
$schema->storage->txn_commit. See L<DBIx::Class::Storage/"txn_commit">
for more information.

=cut

sub txn_commit {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_commit called on $schema without storage');

  $self->storage->txn_commit;
}

=head2 txn_rollback

Rolls back the current transaction. Equivalent to calling
$schema->storage->txn_rollback. See
L<DBIx::Class::Storage/"txn_rollback"> for more information.

=cut

sub txn_rollback {
  my $self = shift;

  $self->storage or $self->throw_exception
    ('txn_rollback called on $schema without storage');

  $self->storage->txn_rollback;
}

=head2 storage

  my $storage = $schema->storage;

Returns the L<DBIx::Class::Storage> object for this Schema. Grab this
if you want to turn on SQL statement debugging at runtime, or set the
quote character. For the default storage, the documentation can be
found in L<DBIx::Class::Storage::DBI>.

=head2 populate

=over 4

=item Arguments: L<$source_name|DBIx::Class::ResultSource/source_name>, [ \@column_list, \@row_values+ ] | [ \%col_data+ ]

=item Return Value: L<\@result_objects|DBIx::Class::Manual::ResultClass> (scalar context) | L<@result_objects|DBIx::Class::Manual::ResultClass> (list context)

=back

A convenience shortcut to L<DBIx::Class::ResultSet/populate>. Equivalent to:

 $schema->resultset($source_name)->populate([...]);

=over 4

=item NOTE

The context of this method call has an important effect on what is
submitted to storage. In void context data is fed directly to fastpath
insertion routines provided by the underlying storage (most often
L<DBI/execute_for_fetch>), bypassing the L<new|DBIx::Class::Row/new> and
L<insert|DBIx::Class::Row/insert> calls on the
L<Result|DBIx::Class::Manual::ResultClass> class, including any
augmentation of these methods provided by components. For example if you
are using something like L<DBIx::Class::UUIDColumns> to create primary
keys for you, you will find that your PKs are empty.  In this case you
will have to explicitly force scalar or list context in order to create
those values.

=back

=cut

sub populate {
  my ($self, $name, $data) = @_;
  my $rs = $self->resultset($name)
    or $self->throw_exception("'$name' is not a resultset");

  return $rs->populate($data);
}

=head2 connection

=over 4

=item Arguments: @args

=item Return Value: $new_schema

=back

Similar to L</connect> except sets the storage object and connection
data in-place on the Schema class. You should probably be calling
L</connect> to get a proper Schema object instead.

=head3 Overloading

Overload C<connection> to change the behaviour of C<connect>.

=cut

sub connection {
  my ($self, @info) = @_;
  return $self if !@info && $self->storage;

  my ($storage_class, $args) = ref $self->storage_type
    ? $self->_normalize_storage_type($self->storage_type)
    : $self->storage_type
  ;

  $storage_class =~ s/^::/DBIx::Class::Storage::/;

  try {
    $self->ensure_class_loaded ($storage_class);
  }
  catch {
    $self->throw_exception(
      "Unable to load storage class ${storage_class}: $_"
    );
  };

  my $storage = $storage_class->new( $self => $args||{} );
  $storage->connect_info(\@info);
  $self->storage($storage);
  return $self;
}

sub _normalize_storage_type {
  my ($self, $storage_type) = @_;
  if(ref $storage_type eq 'ARRAY') {
    return @$storage_type;
  } elsif(ref $storage_type eq 'HASH') {
    return %$storage_type;
  } else {
    $self->throw_exception('Unsupported REFTYPE given: '. ref $storage_type);
  }
}

=head2 compose_namespace

=over 4

=item Arguments: $target_namespace, $additional_base_class?

=item Return Value: $new_schema

=back

For each L<DBIx::Class::ResultSource> in the schema, this method creates a
class in the target namespace (e.g. $target_namespace::CD,
$target_namespace::Artist) that inherits from the corresponding classes
attached to the current schema.

It also attaches a corresponding L<DBIx::Class::ResultSource> object to the
new $schema object. If C<$additional_base_class> is given, the new composed
classes will inherit from first the corresponding class from the current
schema then the base class.

For example, for a schema with My::Schema::CD and My::Schema::Artist classes,

  $schema->compose_namespace('My::DB', 'Base::Class');
  print join (', ', @My::DB::CD::ISA) . "\n";
  print join (', ', @My::DB::Artist::ISA) ."\n";

will produce the output

  My::Schema::CD, Base::Class
  My::Schema::Artist, Base::Class

=cut

# this might be oversimplified
# sub compose_namespace {
#   my ($self, $target, $base) = @_;

#   my $schema = $self->clone;
#   foreach my $source_name ($schema->sources) {
#     my $source = $schema->source($source_name);
#     my $target_class = "${target}::${source_name}";
#     $self->inject_base(
#       $target_class => $source->result_class, ($base ? $base : ())
#     );
#     $source->result_class($target_class);
#     $target_class->result_source_instance($source)
#       if $target_class->can('result_source_instance');
#     $schema->register_source($source_name, $source);
#   }
#   return $schema;
# }

sub compose_namespace {
  my ($self, $target, $base) = @_;

  my $schema = $self->clone;

  $schema->source_registrations({});

  # the original class-mappings must remain - otherwise
  # reverse_relationship_info will not work
  #$schema->class_mappings({});

  {
    no warnings qw/redefine/;
    local *Class::C3::reinitialize = sub { } if DBIx::Class::_ENV_::OLD_MRO;
    use warnings qw/redefine/;

    foreach my $source_name ($self->sources) {
      my $orig_source = $self->source($source_name);

      my $target_class = "${target}::${source_name}";
      $self->inject_base($target_class, $orig_source->result_class, ($base || ()) );

      # register_source examines result_class, and then returns us a clone
      my $new_source = $schema->register_source($source_name, bless
        { %$orig_source, result_class => $target_class },
        ref $orig_source,
      );

      if ($target_class->can('result_source_instance')) {
        # give the class a schema-less source copy
        $target_class->result_source_instance( bless
          { %$new_source, schema => ref $new_source->{schema} || $new_source->{schema} },
          ref $new_source,
        );
      }
    }

    quote_sub "${target}::${_}" => "shift->schema->$_(\@_)"
      for qw(class source resultset);
  }

  Class::C3->reinitialize() if DBIx::Class::_ENV_::OLD_MRO;

  return $schema;
}

sub setup_connection_class {
  my ($class, $target, @info) = @_;
  $class->inject_base($target => 'DBIx::Class::DB');
  #$target->load_components('DB');
  $target->connection(@info);
}

=head2 svp_begin

Creates a new savepoint (does nothing outside a transaction).
Equivalent to calling $schema->storage->svp_begin.  See
L<DBIx::Class::Storage/"svp_begin"> for more information.

=cut

sub svp_begin {
  my ($self, $name) = @_;

  $self->storage or $self->throw_exception
    ('svp_begin called on $schema without storage');

  $self->storage->svp_begin($name);
}

=head2 svp_release

Releases a savepoint (does nothing outside a transaction).
Equivalent to calling $schema->storage->svp_release.  See
L<DBIx::Class::Storage/"svp_release"> for more information.

=cut

sub svp_release {
  my ($self, $name) = @_;

  $self->storage or $self->throw_exception
    ('svp_release called on $schema without storage');

  $self->storage->svp_release($name);
}

=head2 svp_rollback

Rollback to a savepoint (does nothing outside a transaction).
Equivalent to calling $schema->storage->svp_rollback.  See
L<DBIx::Class::Storage/"svp_rollback"> for more information.

=cut

sub svp_rollback {
  my ($self, $name) = @_;

  $self->storage or $self->throw_exception
    ('svp_rollback called on $schema without storage');

  $self->storage->svp_rollback($name);
}

=head2 clone

=over 4

=item Arguments: %attrs?

=item Return Value: $new_schema

=back

Clones the schema and its associated result_source objects and returns the
copy. The resulting copy will have the same attributes as the source schema,
except for those attributes explicitly overridden by the provided C<%attrs>.

=cut

sub clone {
  my $self = shift;

  my $clone = {
      (ref $self ? %$self : ()),
      (@_ == 1 && ref $_[0] eq 'HASH' ? %{ $_[0] } : @_),
  };
  bless $clone, (ref $self || $self);

  $clone->$_(undef) for qw/class_mappings source_registrations storage/;

  $clone->_copy_state_from($self);

  return $clone;
}

# Needed in Schema::Loader - if you refactor, please make a compatibility shim
# -- Caelum
sub _copy_state_from {
  my ($self, $from) = @_;

  $self->class_mappings({ %{$from->class_mappings} });
  $self->source_registrations({ %{$from->source_registrations} });

  foreach my $source_name ($from->sources) {
    my $source = $from->source($source_name);
    my $new = $source->new($source);
    # we use extra here as we want to leave the class_mappings as they are
    # but overwrite the source_registrations entry with the new source
    $self->register_extra_source($source_name => $new);
  }

  if ($from->storage) {
    $self->storage($from->storage);
    $self->storage->set_schema($self);
  }
}

=head2 throw_exception

=over 4

=item Arguments: $message

=back

Throws an exception. Obeys the exemption rules of L<DBIx::Class::Carp> to report
errors from outer-user's perspective. See L</exception_action> for details on overriding
this method's behavior.  If L</stacktrace> is turned on, C<throw_exception>'s
default behavior will provide a detailed stack trace.

=cut

sub throw_exception {
  my ($self, @args) = @_;

  if (my $act = $self->exception_action) {

    my $guard_disarmed;

    my $guard = scope_guard {
      return if $guard_disarmed;
      local $SIG{__WARN__};
      Carp::cluck("
                    !!! DBIx::Class INTERNAL PANIC !!!

The exception_action() handler installed on '$self'
aborted the stacktrace below via a longjmp (either via Return::Multilevel or
plain goto, or Scope::Upper or something equally nefarious). There currently
is nothing safe DBIx::Class can do, aside from displaying this error. A future
version ( 0.082900, when available ) will reduce the cases in which the
handler is invoked, but this is neither a complete solution, nor can it do
anything for other software that might be affected by a similar problem.

                      !!! FIX YOUR ERROR HANDLING !!!

This guard was activated beginning"
      );
    };

    eval {
      # if it throws - good, we'll go down to the do{} below
      # if it doesn't - do different things depending on RV truthiness
      if( $act->(@args) ) {
        $args[0] = (
          "Invocation of the exception_action handler installed on $self did *not*"
        .' result in an exception. DBIx::Class is unable to function without a reliable'
        .' exception mechanism, ensure that exception_action does not hide exceptions'
        ." (original error: $args[0])"
        );
      }
      else {
        carp_unique (
          "The exception_action handler installed on $self returned false instead"
        .' of throwing an exception. This behavior has been deprecated, adjust your'
        .' handler to always rethrow the supplied error'
        );
      }

      $guard_disarmed = 1;
    }

      or

    do {
      # We call this to get the necessary warnings emitted and disregard the RV
      # as it's definitely an exception if we got as far as this do{} block
      is_exception($@);

      $guard_disarmed = 1;
      $args[0] = $@;
    };
  }

  DBIx::Class::Exception->throw($args[0], $self->stacktrace);
}

=head2 deploy

=over 4

=item Arguments: \%sqlt_args, $dir

=back

Attempts to deploy the schema to the current storage using L<SQL::Translator>.

See L<SQL::Translator/METHODS> for a list of values for C<\%sqlt_args>.
The most common value for this would be C<< { add_drop_table => 1 } >>
to have the SQL produced include a C<DROP TABLE> statement for each table
created. For quoting purposes supply C<quote_identifiers>.

Additionally, the DBIx::Class parser accepts a C<sources> parameter as a hash
ref or an array ref, containing a list of source to deploy. If present, then
only the sources listed will get deployed. Furthermore, you can use the
C<add_fk_index> parser parameter to prevent the parser from creating an index for each
FK.

=cut

sub deploy {
  my ($self, $sqltargs, $dir) = @_;
  $self->throw_exception("Can't deploy without storage") unless $self->storage;
  $self->storage->deploy($self, undef, $sqltargs, $dir);
}

=head2 deployment_statements

=over 4

=item Arguments: See L<DBIx::Class::Storage::DBI/deployment_statements>

=item Return Value: $listofstatements

=back

A convenient shortcut to
C<< $self->storage->deployment_statements($self, @args) >>.
Returns the statements used by L</deploy> and
L<DBIx::Class::Storage/deploy>.

=cut

sub deployment_statements {
  my $self = shift;

  $self->throw_exception("Can't generate deployment statements without a storage")
    if not $self->storage;

  $self->storage->deployment_statements($self, @_);
}

=head2 create_ddl_dir

=over 4

=item Arguments: See L<DBIx::Class::Storage::DBI/create_ddl_dir>

=back

A convenient shortcut to
C<< $self->storage->create_ddl_dir($self, @args) >>.

Creates an SQL file based on the Schema, for each of the specified
database types, in the given directory.

=cut

sub create_ddl_dir {
  my $self = shift;

  $self->throw_exception("Can't create_ddl_dir without storage") unless $self->storage;
  $self->storage->create_ddl_dir($self, @_);
}

=head2 ddl_filename

=over 4

=item Arguments: $database-type, $version, $directory, $preversion

=item Return Value: $normalised_filename

=back

  my $filename = $table->ddl_filename($type, $version, $dir, $preversion)

This method is called by C<create_ddl_dir> to compose a file name out of
the supplied directory, database type and version number. The default file
name format is: C<$dir$schema-$version-$type.sql>.

You may override this method in your schema if you wish to use a different
format.

 WARNING

 Prior to DBIx::Class version 0.08100 this method had a different signature:

    my $filename = $table->ddl_filename($type, $dir, $version, $preversion)

 In recent versions variables $dir and $version were reversed in order to
 bring the signature in line with other Schema/Storage methods. If you
 really need to maintain backward compatibility, you can do the following
 in any overriding methods:

    ($dir, $version) = ($version, $dir) if ($DBIx::Class::VERSION < 0.08100);

=cut

sub ddl_filename {
  my ($self, $type, $version, $dir, $preversion) = @_;

  require File::Spec;

  $version = "$preversion-$version" if $preversion;

  my $class = blessed($self) || $self;
  $class =~ s/::/-/g;

  return File::Spec->catfile($dir, "$class-$version-$type.sql");
}

=head2 thaw

Provided as the recommended way of thawing schema objects. You can call
C<Storable::thaw> directly if you wish, but the thawed objects will not have a
reference to any schema, so are rather useless.

=cut

sub thaw {
  my ($self, $obj) = @_;
  local $DBIx::Class::ResultSourceHandle::thaw_schema = $self;
  return Storable::thaw($obj);
}

=head2 freeze

This doesn't actually do anything beyond calling L<nfreeze|Storable/SYNOPSIS>,
it is just provided here for symmetry.

=cut

sub freeze {
  return Storable::nfreeze($_[1]);
}

=head2 dclone

=over 4

=item Arguments: $object

=item Return Value: dcloned $object

=back

Recommended way of dcloning L<DBIx::Class::Row> and L<DBIx::Class::ResultSet>
objects so their references to the schema object
(which itself is B<not> cloned) are properly maintained.

=cut

sub dclone {
  my ($self, $obj) = @_;
  local $DBIx::Class::ResultSourceHandle::thaw_schema = $self;
  return Storable::dclone($obj);
}

=head2 schema_version

Returns the current schema class' $VERSION in a normalised way.

=cut

sub schema_version {
  my ($self) = @_;
  my $class = ref($self)||$self;

  # does -not- use $schema->VERSION
  # since that varies in results depending on if version.pm is installed, and if
  # so the perl or XS versions. If you want this to change, bug the version.pm
  # author to make vpp and vxs behave the same.

  my $version;
  {
    no strict 'refs';
    $version = ${"${class}::VERSION"};
  }
  return $version;
}


=head2 register_class

=over 4

=item Arguments: $source_name, $component_class

=back

This method is called by L</load_namespaces> and L</load_classes> to install the found classes into your Schema. You should be using those instead of this one.

You will only need this method if you have your Result classes in
files which are not named after the packages (or all in the same
file). You may also need it to register classes at runtime.

Registers a class which isa DBIx::Class::ResultSourceProxy. Equivalent to
calling:

  $schema->register_source($source_name, $component_class->result_source_instance);

=cut

sub register_class {
  my ($self, $source_name, $to_register) = @_;
  $self->register_source($source_name => $to_register->result_source_instance);
}

=head2 register_source

=over 4

=item Arguments: $source_name, L<$result_source|DBIx::Class::ResultSource>

=back

This method is called by L</register_class>.

Registers the L<DBIx::Class::ResultSource> in the schema with the given
source name.

=cut

sub register_source { shift->_register_source(@_) }

=head2 unregister_source

=over 4

=item Arguments: $source_name

=back

Removes the L<DBIx::Class::ResultSource> from the schema for the given source name.

=cut

sub unregister_source { shift->_unregister_source(@_) }

=head2 register_extra_source

=over 4

=item Arguments: $source_name, L<$result_source|DBIx::Class::ResultSource>

=back

As L</register_source> but should be used if the result class already
has a source and you want to register an extra one.

=cut

sub register_extra_source { shift->_register_source(@_, { extra => 1 }) }

sub _register_source {
  my ($self, $source_name, $source, $params) = @_;

  $source = $source->new({ %$source, source_name => $source_name });

  $source->schema($self);
  weaken $source->{schema} if ref($self);

  my %reg = %{$self->source_registrations};
  $reg{$source_name} = $source;
  $self->source_registrations(\%reg);

  return $source if $params->{extra};

  my $rs_class = $source->result_class;
  if ($rs_class and my $rsrc = try { $rs_class->result_source_instance } ) {
    my %map = %{$self->class_mappings};
    if (
      exists $map{$rs_class}
        and
      $map{$rs_class} ne $source_name
        and
      $rsrc ne $_[2]  # orig_source
    ) {
      carp
        "$rs_class already had a registered source which was replaced by this call. "
      . 'Perhaps you wanted register_extra_source(), though it is more likely you did '
      . 'something wrong.'
      ;
    }

    $map{$rs_class} = $source_name;
    $self->class_mappings(\%map);
  }

  return $source;
}

my $global_phase_destroy;
sub DESTROY {
  ### NO detected_reinvoked_destructor check
  ### This code very much relies on being called multuple times

  return if $global_phase_destroy ||= in_global_destruction;

  my $self = shift;
  my $srcs = $self->source_registrations;

  for my $source_name (keys %$srcs) {
    # find first source that is not about to be GCed (someone other than $self
    # holds a reference to it) and reattach to it, weakening our own link
    #
    # during global destruction (if we have not yet bailed out) this should throw
    # which will serve as a signal to not try doing anything else
    # however beware - on older perls the exception seems randomly untrappable
    # due to some weird race condition during thread joining :(((
    if (length ref $srcs->{$source_name} and refcount($srcs->{$source_name}) > 1) {
      local $@;
      eval {
        $srcs->{$source_name}->schema($self);
        weaken $srcs->{$source_name};
        1;
      } or do {
        $global_phase_destroy = 1;
      };

      last;
    }
  }
}

sub _unregister_source {
    my ($self, $source_name) = @_;
    my %reg = %{$self->source_registrations};

    my $source = delete $reg{$source_name};
    $self->source_registrations(\%reg);
    if ($source->result_class) {
        my %map = %{$self->class_mappings};
        delete $map{$source->result_class};
        $self->class_mappings(\%map);
    }
}


=head2 compose_connection (DEPRECATED)

=over 4

=item Arguments: $target_namespace, @db_info

=item Return Value: $new_schema

=back

DEPRECATED. You probably wanted compose_namespace.

Actually, you probably just wanted to call connect.

=begin hidden

(hidden due to deprecation)

Calls L<DBIx::Class::Schema/"compose_namespace"> to the target namespace,
calls L<DBIx::Class::Schema/connection> with @db_info on the new schema,
then injects the L<DBix::Class::ResultSetProxy> component and a
resultset_instance classdata entry on all the new classes, in order to support
$target_namespaces::$class->search(...) method calls.

This is primarily useful when you have a specific need for class method access
to a connection. In normal usage it is preferred to call
L<DBIx::Class::Schema/connect> and use the resulting schema object to operate
on L<DBIx::Class::ResultSet> objects with L<DBIx::Class::Schema/resultset> for
more information.

=end hidden

=cut

sub compose_connection {
  my ($self, $target, @info) = @_;

  carp_once "compose_connection deprecated as of 0.08000"
    unless $INC{"DBIx/Class/CDBICompat.pm"};

  my $base = 'DBIx::Class::ResultSetProxy';
  try {
    eval "require ${base};"
  }
  catch {
    $self->throw_exception
      ("No arguments to load_classes and couldn't load ${base} ($_)")
  };

  if ($self eq $target) {
    # Pathological case, largely caused by the docs on early C::M::DBIC::Plain
    foreach my $source_name ($self->sources) {
      my $source = $self->source($source_name);
      my $class = $source->result_class;
      $self->inject_base($class, $base);
      $class->mk_classdata(resultset_instance => $source->resultset);
      $class->mk_classdata(class_resolver => $self);
    }
    $self->connection(@info);
    return $self;
  }

  my $schema = $self->compose_namespace($target, $base);
  quote_sub "${target}::schema", '$s', { '$s' => \$schema };

  $schema->connection(@info);
  foreach my $source_name ($schema->sources) {
    my $source = $schema->source($source_name);
    my $class = $source->result_class;
    #warn "$source_name $class $source ".$source->storage;
    $class->mk_classdata(result_source_instance => $source);
    $class->mk_classdata(resultset_instance => $source->resultset);
    $class->mk_classdata(class_resolver => $schema);
  }
  return $schema;
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
