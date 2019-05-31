package DBIx::Class::Storage::TxnScopeGuard;

use strict;
use warnings;
use Try::Tiny;
use Scalar::Util qw(weaken blessed refaddr);
use DBIx::Class;
use DBIx::Class::_Util qw(is_exception detected_reinvoked_destructor);
use DBIx::Class::Carp;
use namespace::clean;

sub new {
  my ($class, $storage) = @_;

  my $guard = {
    inactivated => 0,
    storage => $storage,
  };

  # we are starting with an already set $@ - in order for things to work we need to
  # be able to recognize it upon destruction - store its weakref
  # recording it before doing the txn_begin stuff
  #
  # FIXME FRAGILE - any eval that fails but *does not* rethrow between here
  # and the unwind will trample over $@ and invalidate the entire mechanism
  # There got to be a saner way of doing this...
  if (is_exception $@) {
    weaken(
      $guard->{existing_exception_ref} = (ref($@) eq '') ? \$@ : $@
    );
  }

  $storage->txn_begin;

  weaken( $guard->{dbh} = $storage->_dbh );

  bless $guard, ref $class || $class;

  $guard;
}

sub commit {
  my $self = shift;

  $self->{storage}->throw_exception("Refusing to execute multiple commits on scope guard $self")
    if $self->{inactivated};

  # FIXME - this assumption may be premature: a commit may fail and a rollback
  # *still* be necessary. Currently I am not aware of such scenarious, but I
  # also know the deferred constraint handling is *severely* undertested.
  # Making the change of "fire txn and never come back to this" in order to
  # address RT#107159, but this *MUST* be reevaluated later.
  $self->{inactivated} = 1;
  $self->{storage}->txn_commit;
}

sub DESTROY {
  return if &detected_reinvoked_destructor;

  my $self = shift;

  return if $self->{inactivated};

  # if our dbh is not ours anymore, the $dbh weakref will go undef
  $self->{storage}->_verify_pid unless DBIx::Class::_ENV_::BROKEN_FORK;
  return unless $self->{dbh};

  my $exception = $@ if (
    is_exception $@
      and
    (
      ! defined $self->{existing_exception_ref}
        or
      refaddr( ref($@) eq '' ? \$@ : $@ ) != refaddr($self->{existing_exception_ref})
    )
  );

  {
    local $@;

    carp 'A DBIx::Class::Storage::TxnScopeGuard went out of scope without explicit commit or error. Rolling back.'
      unless defined $exception;

    my $rollback_exception;
    # do minimal connectivity check due to weird shit like
    # https://rt.cpan.org/Public/Bug/Display.html?id=62370
    try { $self->{storage}->_seems_connected && $self->{storage}->txn_rollback }
    catch { $rollback_exception = shift };

    if ( $rollback_exception and (
      ! defined blessed $rollback_exception
          or
      ! $rollback_exception->isa('DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION')
    ) ) {
      # append our text - THIS IS A TEMPORARY FIXUP!
      # a real stackable exception object is in the works
      if (ref $exception eq 'DBIx::Class::Exception') {
        $exception->{msg} = "Transaction aborted: $exception->{msg} "
          ."Rollback failed: ${rollback_exception}";
      }
      elsif ($exception) {
        $exception = "Transaction aborted: ${exception} "
          ."Rollback failed: ${rollback_exception}";
      }
      else {
        carp (join ' ',
          "********************* ROLLBACK FAILED!!! ********************",
          "\nA rollback operation failed after the guard went out of scope.",
          'This is potentially a disastrous situation, check your data for',
          "consistency: $rollback_exception"
        );
      }
    }
  }

  $@ = $exception;
}

1;

__END__

=head1 NAME

DBIx::Class::Storage::TxnScopeGuard - Scope-based transaction handling

=head1 SYNOPSIS

 sub foo {
   my ($self, $schema) = @_;

   my $guard = $schema->txn_scope_guard;

   # Multiple database operations here

   $guard->commit;
 }

=head1 DESCRIPTION

An object that behaves much like L<Scope::Guard>, but hardcoded to do the
right thing with transactions in DBIx::Class.

=head1 METHODS

=head2 new

Creating an instance of this class will start a new transaction (by
implicitly calling L<DBIx::Class::Storage/txn_begin>. Expects a
L<DBIx::Class::Storage> object as its only argument.

=head2 commit

Commit the transaction, and stop guarding the scope. If this method is not
called and this object goes out of scope (e.g. an exception is thrown) then
the transaction is rolled back, via L<DBIx::Class::Storage/txn_rollback>

=cut

=head1 SEE ALSO

L<DBIx::Class::Schema/txn_scope_guard>.

L<Scope::Guard> by chocolateboy (inspiration for this module)

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
