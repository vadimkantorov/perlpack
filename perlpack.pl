use strict;
use warnings;
use Getopt::Long;
use File::Path;
use File::Find;
use File::Spec;
use File::Copy;
use Cwd;
my $input_path = '';
my $output_path = '';
my $prefix = '';
my $ld = 'ld';
my $include = '';
my $exclude = '';
my $exclude_executable = 0;
Getopt::Long::GetOptions(
    'input-path|i=s'      => \$input_path,
    'output-path|o=s'     => \$output_path,
    'prefix=s'            => \$prefix,
    'ld=s'                => \$ld,
    'include=s'           => \$include,
    'exclude=s'           => \$exclude,
    'exclude-executable'  => \$exclude_executable
);

die "Input path does not exist or is not a directory" unless -e $input_path && -d $input_path ;
die "Output path not specified" if $output_path eq '';

$output_path = Cwd::abs_path($output_path);
File::Path::make_path($output_path . '.o');
my (@objects, @relpaths_dirs, @safepaths, @relpaths);
    
# problem: can produce the same symbol name because of this mapping, ld maps only to _, so may need to rename the file before invoking ld
my %translate = ('.' => '_', '-' => '_', '_' => '_', '/' => '_');
my $translate_keys = join("", keys %translate);

my $oldcwd = Cwd::getcwd();
File::Find::find(sub {
    my $newcwd = Cwd::getcwd(); chdir($oldcwd); 
    my $p = $File::Find::name;
    
    my $relpath = $p;
    if (index($relpath, $input_path) == 0) { $relpath = substr($relpath, length($input_path)); }
    if (index($relpath, '/') == 0) { $relpath = substr($relpath, 1); }
    my $safepath = ''; 
    for my $char (split //, $relpath) { $safepath .= exists $translate{$char} ? $translate{$char} : $char; }

    my $include_file = 1;
    if (-d $p) {
        push @relpaths_dirs, $p;
        $include_file = 0;
    } elsif ($include ne '' and $p =~ /$include/) {
        $include_file = 1;
    } elsif ($exclude ne '' and $p =~ /$exclude/) {
        $include_file = 0;
    } elsif ($exclude_executable and (-x $p)) {
        $include_file = 0;
    }
    if ($include_file) {
        push @safepaths, $safepath;
        push @relpaths, $relpath;
        push @objects, File::Spec->catfile($output_path . '.o', $safepath . '.o');
        die "File should not end with .o" if substr($relpath, -2) eq '.o';
        
        File::Copy::copy($relpath, File::Spec->catfile($output_path + '.o', $safepath));
        chdir($output_path . '.o'); system($ld, '-r', '-b', 'binary', '-o', $objects[-1], $safepath) == 0 or die "ld command failed: $?";
        unlink($safepath);
    }
    chdir($newcwd);
}, $input_path);

open my $g, '>', $output_path . '.txt' or die;
print $g join("\n", @objects);
open my $f, '>', $output_path or die;
print $f "size_t packfs_builtin_files_num = ", scalar(@relpaths), ", packfs_builtin_dirs_num = ", scalar(@relpaths_dirs), ";\n\n";
print $f "const char* packfs_builtin_abspaths[] = {\n\"" , join("\",\n\"", map { File::Spec->catfile($prefix, $_) } @relpaths), "\"\n};\n\n";
print $f "const char* packfs_builtin_abspaths_dirs[] = {\n\"" , join("\",\n\"", map { File::Spec->catfile($prefix, $_) } @relpaths_dirs) , "\"\n};\n\n";
print $f join("\n", map { "extern char _binary_${_}_start[], _binary_${_}_end[];" } @safepaths), "\n\n";
print $f "const char* packfs_builtin_starts[] = {\n", join("\n", map { "_binary_${_}_start," } @safepaths), "\n};\n\n";
print $f "const char* packfs_builtin_ends[] = {\n", join("\n", map { "_binary_${_}_end," } @safepaths), "\n};\n\n";
