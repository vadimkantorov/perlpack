use strict;
use warnings;
use Getopt::Long;
use File::Path;
use File::Find;
use File::Spec;

my $input_path = '';
my $output_path = '';
my $prefix = '';

Getopt::Long::GetOptions(
    'input-path|i=s'  => \$input_path,
    'output-path|o=s' => \$output_path,
    'prefix=s'        => \$prefix,
);
die "Input path does not exist or is not a directory" unless -e $input_path && -d $input_path ;
die "Output path not specified" if $output_path eq '';

File::Path::make_path($output_path . '.o');
if (-d "./perlpack.h.o/") {
    print("OK!\n");
}
if (-d "./packfs/") {
    print("DIR!\n");
}

my (@objects, @files, @dirs);
File::Find::find(sub {

    if (-d $File::Find::name) {
        print($File::Find::name, "dir\n");
        push @dirs, $File::Find::name;
    } else {
        print($File::Find::name, "file\n");
        my $safe_path = $File::Find::name;
        $safe_path =~ s/[\/.-]/_/g;
        push @files, $File::Find::name;
        push @objects, File::Spec->catfile($output_path . '.o', $safe_path . '.o');
        system('ld', '-r', '-b', 'binary', '-o', $objects[-1], $files[-1]) == 0 or die "ld command failed: $?";
    }

}, $input_path);

open my $g, '>', $output_path . '.txt' or die;
print $g join("\n", @objects);

open my $f, '>', $output_path or die;
print $f "int packfsfilesnum = ", scalar(@files), ", packfsdirsnum  = ", scalar(@dirs), ";\n";
foreach my $File__Find__name (@files) {
    my $safe_path = $File__Find__name;
    $safe_path =~ s/[\/.-]/_/g;
    print $f "extern char _binary_${safe_path}_start[], _binary_${safe_path}_end[];\n";
}
print $f "struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {\n";
foreach my $File__Find__name (@files) {
    my $safe_path = $File__Find__name;
    $safe_path =~ s/[\/.-]/_/g;
    my $ppp = (split(/\//, $File__Find__name, 2))[-1];
    print $f "{ \"$safe_path\", \"$prefix$ppp\", _binary_${safe_path}_start, _binary_${safe_path}_end },\n";
}
print $f "};\n";
print $f "const char* packfsdirs[] = {\n";
print $f join(",\n", map { "\"$prefix$_\"" } map { (split(/\//, $_, 2))[-1] } @dirs);
print $f "\n};\n";
