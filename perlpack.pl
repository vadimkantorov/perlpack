use strict;
use warnings;
use Getopt::Long;
use File::Path;
use File::Find;
use File::Spec;
use Cwd;

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

my $oldcwd = Cwd::getcwd();
my (@objects, @files, @dirs);
File::Find::find(sub {
    my $newcwd = Cwd::getcwd(); chdir $oldcwd; 
    my $p = $File::Find::name;
    if (-d $p) {
        push @dirs, $p;
    } else {
        my $safe_path = $p;
        $safe_path =~ s/[\/.-]/_/g;
        push @files, $p;
        push @objects, File::Spec->catfile($output_path . '.o', $safe_path . '.o');
        system('ld', '-r', '-b', 'binary', '-o', $objects[-1]), $files[-1]) == 0 or die "ld command failed: $?";
    }
    chdir $newcwd;
}, $input_path);

open my $g, '>', $output_path . '.txt' or die;
print $g join("\n", @objects);

open my $f, '>', $output_path or die;
print $f "int packfsfilesnum = ", scalar(@files), ", packfsdirsnum  = ", scalar(@dirs), ";\n";
foreach my $p (@files) {
    my $safe_path = $p;
    $safe_path =~ s/[\/.-]/_/g;
    print $f "extern char _binary_${safe_path}_start[], _binary_${safe_path}_end[];\n";
}
print $f "struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {\n";
foreach my $p (@files) {
    my $safe_path = $p;
    $safe_path =~ s/[\/.-]/_/g;
    my $ppp = (split(/\//, $p, 2))[-1];
    print $f "{ \"$safe_path\", \"$prefix$ppp\", _binary_${safe_path}_start, _binary_${safe_path}_end },\n";
}
print $f "};\n";
print $f "const char* packfsdirs[] = {\n";
print $f join(",\n", map { "\"$prefix$_\"" } map { (split(/\//, $_, 2))[-1] } @dirs);
print $f "\n};\n";
