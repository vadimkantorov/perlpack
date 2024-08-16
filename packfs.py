import os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--input-path', '-i')
parser.add_argument('--output-path', '-o')
args = parser.parse_args()

assert os.path.exists(args.input_path) and os.path.isdir(args.input_path)

objects, files, dirs = [], [], []
for (dirpath, dirnames, filenames) in list(os.walk(args.input_path)):
    dirs.extend(os.path.join(dirpath, dirname) for dirname in dirnames)
    for basename in filenames:
        if basename.endswith('.o'):
            continue
        p = os.path.join(dirpath, basename)
        files.append(p)
        objects.append(p + '.o')
        subprocess.check_call(['ld', '-r', '-b', 'binary', '-o', objects[-1], files[-1]])

translate = {ord('/') : '_', ord('.') : '_', ord('-') : '_'}
# problem: can produce the same symbol name because of this mapping

f = open(args.output_path + '.txt', 'w')
print('\n'.join(objects), file = f)

f = open(args.output_path, 'w')
print(f'int packfsfilesnum = {len(files)}, packfsdirsnum  = {len(dirs)};', file = f)

print('\n'.join(f'extern char _binary_{pp}_start[], _binary_{pp}_end[];' for p in files for pp in [p.translate(translate)]), file = f)
print('struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {', file = f)
for p in files:
    pp = p.translate(translate)
    print('{ "' + repr(pp)[1:-1] + '", "' + repr(p)[1:-1] + '", ', f'_binary_{pp}_start, ', f'_binary_{pp}_end ', '},', file = f)
print('};', file = f)
print('const char* packfsdirs[] = {\n', ',\n'.join('"' + repr(p)[1:-1] + '"' for p in dirs), '\n};\n', file = f)
#print('\nconst char* packfsfiles[] = {\n' + ',\n'.join('"' + repr(p)[1:-1] + '"' for p in files) + '\n};\n\n', file = f)

