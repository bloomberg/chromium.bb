package DBIx::Class::InflateColumn::File;

use strict;
use warnings;
use base 'DBIx::Class';
use File::Path;
use File::Copy;
use Path::Class;
use DBIx::Class::Carp;
use namespace::clean;

carp 'InflateColumn::File has entered a deprecation cycle. This component '
    .'has a number of architectural deficiencies that can quickly drive '
    .'your filesystem and database out of sync and is not recommended '
    .'for further use. It will be retained for backwards '
    .'compatibility, but no new functionality patches will be accepted. '
    .'Please consider using the much more mature and actively maintained '
    .'DBIx::Class::InflateColumn::FS. You can set the environment variable '
    .'DBIC_IC_FILE_NOWARN to a true value to disable  this warning.'
unless $ENV{DBIC_IC_FILE_NOWARN};



__PACKAGE__->load_components(qw/InflateColumn/);

sub register_column {
    my ($self, $column, $info, @rest) = @_;
    $self->next::method($column, $info, @rest);
    return unless defined($info->{is_file_column});

    $self->inflate_column($column => {
        inflate => sub {
            my ($value, $obj) = @_;
            $obj->_inflate_file_column($column, $value);
        },
        deflate => sub {
            my ($value, $obj) = @_;
            $obj->_save_file_column($column, $value);
        },
    });
}

sub _file_column_file {
    my ($self, $column, $filename) = @_;

    my $column_info = $self->result_source->column_info($column);

    return unless $column_info->{is_file_column};

    # DO NOT CHANGE
    # This call to id() is generally incorrect - will not DTRT on
    # multicolumn key. However changing this may introduce
    # backwards-comp regressions, thus leaving as is
    my $id = $self->id || $self->throw_exception(
        'id required for filename generation'
    );

    $filename ||= $self->$column->{filename};
    return Path::Class::file(
        $column_info->{file_column_path}, $id, $filename,
    );
}

sub delete {
    my ( $self, @rest ) = @_;

    my $colinfos = $self->result_source->columns_info;

    for ( keys %$colinfos ) {
        if ( $colinfos->{$_}{is_file_column} ) {
            rmtree( [$self->_file_column_file($_)->dir], 0, 0 );
            last; # if we've deleted one, we've deleted them all
        }
    }

    return $self->next::method(@rest);
}

sub insert {
    my $self = shift;

    # cache our file columns so we can write them to the fs
    # -after- we have a PK
    my $colinfos = $self->result_source->columns_info;

    my %file_column;
    for ( keys %$colinfos ) {
        if ( $colinfos->{$_}{is_file_column} ) {
            $file_column{$_} = $self->$_;
            $self->store_column($_ => $self->$_->{filename});
        }
    }

    $self->next::method(@_);

    # write the files to the fs
    while ( my ($col, $file) = each %file_column ) {
        $self->_save_file_column($col, $file);
    }

    return $self;
}


sub _inflate_file_column {
    my ( $self, $column, $value ) = @_;

    my $fs_file = $self->_file_column_file($column, $value);

    return { handle => $fs_file->open('r'), filename => $value };
}

sub _save_file_column {
    my ( $self, $column, $value ) = @_;

    return unless ref $value;

    my $fs_file = $self->_file_column_file($column, $value->{filename});
    mkpath [$fs_file->dir];

    # File::Copy doesn't like Path::Class (or any for that matter) objects,
    # thus ->stringify (http://rt.perl.org/rt3/Public/Bug/Display.html?id=59650)
    File::Copy::copy($value->{handle}, $fs_file->stringify);

    $self->_file_column_callback($value, $self, $column);

    return $value->{filename};
}

=head1 NAME

DBIx::Class::InflateColumn::File -  DEPRECATED (superseded by DBIx::Class::InflateColumn::FS)

=head2 Deprecation Notice

 This component has a number of architectural deficiencies that can quickly
 drive your filesystem and database out of sync and is not recommended for
 further use. It will be retained for backwards compatibility, but no new
 functionality patches will be accepted. Please consider using the much more
 mature and actively supported DBIx::Class::InflateColumn::FS. You can set
 the environment variable DBIC_IC_FILE_NOWARN to a true value to disable
 this warning.

=head1 SYNOPSIS

In your L<DBIx::Class> table class:

    use base 'DBIx::Class::Core';

    __PACKAGE__->load_components(qw/InflateColumn::File/);

    # define your columns
    __PACKAGE__->add_columns(
        "id",
        {
            data_type         => "integer",
            is_auto_increment => 1,
            is_nullable       => 0,
            size              => 4,
        },
        "filename",
        {
            data_type           => "varchar",
            is_file_column      => 1,
            file_column_path    =>'/tmp/uploaded_files',
            # or for a Catalyst application
            # file_column_path  => MyApp->path_to('root','static','files'),
            default_value       => undef,
            is_nullable         => 1,
            size                => 255,
        },
    );


In your L<Catalyst::Controller> class:

FileColumn requires a hash that contains L<IO::File> as handle and the file's
name as name.

    my $entry = $c->model('MyAppDB::Articles')->create({
        subject => 'blah',
        filename => {
            handle => $c->req->upload('myupload')->fh,
            filename => $c->req->upload('myupload')->basename
        },
        body => '....'
    });
    $c->stash->{entry}=$entry;


And Place the following in your TT template

    Article Subject: [% entry.subject %]
    Uploaded File:
    <a href="/static/files/[% entry.id %]/[% entry.filename.filename %]">File</a>
    Body: [% entry.body %]

The file will be stored on the filesystem for later retrieval.  Calling delete
on your resultset will delete the file from the filesystem.  Retrevial of the
record automatically inflates the column back to the set hash with the
IO::File handle and filename.

=head1 DESCRIPTION

InflateColumn::File

=head1 METHODS

=head2 _file_column_callback ($file,$ret,$target)

Method made to be overridden for callback purposes.

=cut

sub _file_column_callback {}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
