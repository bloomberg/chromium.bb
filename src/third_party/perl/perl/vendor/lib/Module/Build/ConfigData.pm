package Module::Build::ConfigData;
use strict;
my $arrayref = eval do {local $/; <DATA>}
  or die "Couldn't load ConfigData data: $@";
close DATA;
my ($config, $features, $auto_features) = @$arrayref;

sub config { $config->{$_[1]} }

sub set_config { $config->{$_[1]} = $_[2] }
sub set_feature { $features->{$_[1]} = 0+!!$_[2] }  # Constrain to 1 or 0

sub auto_feature_names { sort grep !exists $features->{$_}, keys %$auto_features }

sub feature_names {
  my @features = (sort keys %$features, auto_feature_names());
  @features;
}

sub config_names  { sort keys %$config }

sub write {
  my $me = __FILE__;

  # Can't use Module::Build::Dumper here because M::B is only a
  # build-time prereq of this module
  require Data::Dumper;

  my $mode_orig = (stat $me)[2] & 07777;
  chmod($mode_orig | 0222, $me); # Make it writeable
  open(my $fh, '+<', $me) or die "Can't rewrite $me: $!";
  seek($fh, 0, 0);
  while (<$fh>) {
    last if /^__DATA__$/;
  }
  die "Couldn't find __DATA__ token in $me" if eof($fh);

  seek($fh, tell($fh), 0);
  my $data = [$config, $features, $auto_features];
  print($fh 'do{ my '
	      . Data::Dumper->new([$data],['x'])->Purity(1)->Dump()
	      . '$x; }' );
  truncate($fh, tell($fh));
  close $fh;

  chmod($mode_orig, $me)
    or warn "Couldn't restore permissions on $me: $!";
}

sub feature {
  my ($package, $key) = @_;
  return $features->{$key} if exists $features->{$key};

  my $info = $auto_features->{$key} or return 0;

  require Module::Build;  # XXX should get rid of this
  foreach my $type (sort keys %$info) {
    my $prereqs = $info->{$type};
    next if $type eq 'description' || $type eq 'recommends';

    foreach my $modname (sort keys %$prereqs) {
      my $status = Module::Build->check_installed_status($modname, $prereqs->{$modname});
      if ((!$status->{ok}) xor ($type =~ /conflicts$/)) { return 0; }
      if ( ! eval "require $modname; 1" ) { return 0; }
    }
  }
  return 1;
}


=head1 NAME

Module::Build::ConfigData - Configuration for Module::Build

=head1 SYNOPSIS

  use Module::Build::ConfigData;
  $value = Module::Build::ConfigData->config('foo');
  $value = Module::Build::ConfigData->feature('bar');

  @names = Module::Build::ConfigData->config_names;
  @names = Module::Build::ConfigData->feature_names;

  Module::Build::ConfigData->set_config(foo => $new_value);
  Module::Build::ConfigData->set_feature(bar => $new_value);
  Module::Build::ConfigData->write;  # Save changes


=head1 DESCRIPTION

This module holds the configuration data for the C<Module::Build>
module.  It also provides a programmatic interface for getting or
setting that configuration data.  Note that in order to actually make
changes, you'll have to have write access to the C<Module::Build::ConfigData>
module, and you should attempt to understand the repercussions of your
actions.


=head1 METHODS

=over 4

=item config($name)

Given a string argument, returns the value of the configuration item
by that name, or C<undef> if no such item exists.

=item feature($name)

Given a string argument, returns the value of the feature by that
name, or C<undef> if no such feature exists.

=item set_config($name, $value)

Sets the configuration item with the given name to the given value.
The value may be any Perl scalar that will serialize correctly using
C<Data::Dumper>.  This includes references, objects (usually), and
complex data structures.  It probably does not include transient
things like filehandles or sockets.

=item set_feature($name, $value)

Sets the feature with the given name to the given boolean value.  The
value will be converted to 0 or 1 automatically.

=item config_names()

Returns a list of all the names of config items currently defined in
C<Module::Build::ConfigData>, or in scalar context the number of items.

=item feature_names()

Returns a list of all the names of features currently defined in
C<Module::Build::ConfigData>, or in scalar context the number of features.

=item auto_feature_names()

Returns a list of all the names of features whose availability is
dynamically determined, or in scalar context the number of such
features.  Does not include such features that have later been set to
a fixed value.

=item write()

Commits any changes from C<set_config()> and C<set_feature()> to disk.
Requires write access to the C<Module::Build::ConfigData> module.

=back


=head1 AUTHOR

C<Module::Build::ConfigData> was automatically created using C<Module::Build>.
C<Module::Build> was written by Ken Williams, but he holds no
authorship claim or copyright claim to the contents of C<Module::Build::ConfigData>.

=cut


__DATA__
do{ my $x = [
       {},
       {},
       {
         'HTML_support' => {
                             'description' => 'Create HTML documentation',
                             'requires' => {
                                             'Pod::Html' => 0
                                           }
                           },
         'PPM_support' => {
                            'description' => 'Generate PPM files for distributions'
                          },
         'dist_authoring' => {
                               'description' => 'Create new distributions',
                               'recommends' => {
                                                 'Module::Signature' => '0.21',
                                                 'Pod::Readme' => '0.04'
                                               },
                               'requires' => {
                                               'Archive::Tar' => '1.09'
                                             }
                             },
         'inc_bundling_support' => {
                                     'description' => 'Bundle Module::Build in inc/',
                                     'requires' => {
                                                     'ExtUtils::Install' => '1.54',
                                                     'ExtUtils::Installed' => '1.999',
                                                     'inc::latest' => '0.5'
                                                   }
                                   },
         'license_creation' => {
                                 'description' => 'Create licenses automatically in distributions',
                                 'requires' => {
                                                 'Software::License' => '0.103009'
                                               }
                               },
         'manpage_support' => {
                                'description' => 'Create Unix man pages',
                                'requires' => {
                                                'Pod::Man' => 0
                                              }
                              }
       }
     ];
$x; }