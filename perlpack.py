import os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--input-path', '-i')
parser.add_argument('--output-path', '-o')
parser.add_argument('--prefix', default = '')
args = parser.parse_args()

translate = {ord('/') : '_', ord('.') : '_', ord('-') : '_'}

assert os.path.exists(args.input_path) and os.path.isdir(args.input_path and args.output_path)

os.makedirs(args.output_path + '.o', exist_ok = True)

objects, files, dirs = [], [], []
for (dirpath, dirnames, filenames) in os.walk(args.input_path):
    dirs.extend(os.path.join(dirpath, dirname) for dirname in dirnames)
    for basename in filenames:
        p = os.path.join(dirpath, basename)
        assert not os.path.isabs(p)
        files.append(p)
        safe_path = p.translate(translate)
        objects.append(os.path.join(args.output_path + '.o', safe_path + '.o'))
        subprocess.check_call(['ld', '-r', '-b', 'binary', '-o', objects[-1], files[-1]])

# problem: can produce the same symbol name because of this mapping

f = open(args.output_path + '.txt', 'w')
print('\n'.join(objects), file = f)

f = open(args.output_path, 'w')
print(f'int packfsfilesnum = {len(files)}, packfsdirsnum  = {len(dirs)};', file=f)
print('\n'.join(f'extern char _binary_{safe_path}_start[], _binary_{safe_path}_end[];' for p in files for safe_path in [p.translate(translate)]),  file=f)
print('struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {', file=f)
for p in files:
    ppp = p.split(os.path.sep, maxsplit=1)[-1]
    safe_path = p.translate(translate)
    print('{ "' + repr(safe_path)[1:-1] + '", "' + repr(os.path.join(args.prefix, ppp ))[1:-1] + '", ', f'_binary_{safe_path}_start, ', f'_binary_{safe_path}_end ', '},', file=f)
print('};', file=f)
print('const char* packfsdirs[] = {\n', ',\n'.join('"' + repr(os.path.join(args.prefix, p.split(os.path.sep,maxsplit=1)[-1] ))[1:-1] + '"' for p in dirs), '\n};\n', file=f)
