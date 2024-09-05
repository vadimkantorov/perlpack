import os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--input-path', '-i')
parser.add_argument('--output-path', '-o')
parser.add_argument('--prefix', default = '')
args = parser.parse_args()

translate = {ord('/') : '_', ord('.') : '_', ord('-') : '_'}

assert os.path.exists(args.input_path) and os.path.isdir(args.input_path) and args.output_path

os.makedirs(args.output_path + '.o', exist_ok = True)

objects, files, dirs, safepaths, relpaths = [], [], [], [], []
for (dirpath, dirnames, filenames) in os.walk(args.input_path):
    dirs.extend(os.path.join(dirpath, dirname) for dirname in dirnames)
    for basename in filenames:
        p = os.path.join(dirpath, basename)
        safepath = p.translate(translate)
        relpath = p.split(os.path.sep, maxsplit=1)[-1]
        assert not os.path.isabs(p)
        files.append(p)
        safepaths.append(safepath)
        relpaths.append(relpath)
        objects.append(os.path.join(args.output_path + '.o', safepath + '.o'))
        subprocess.check_call(['ld', '-r', '-b', 'binary', '-o', objects[-1], files[-1]])

# problem: can produce the same symbol name because of this mapping

f = open(args.output_path + '.txt', 'w')
print('\n'.join(objects), file = f)

f = open(args.output_path, 'w')
print(f'int packfsinfosnum = {len(files)};', file=f)# + {len(dirs)}
print('\n'.join(f'extern char _binary_{safepath}_start[], _binary_{safepath}_end[];' for safepath in safepaths),  file=f)

print('struct { const char* safepath; const char *path; const char* start; const char* end; int isdir; } packfsinfos[] = {', file=f)
for p, relpath, safepath in zip(files, relpaths, safepaths):
    print('{ "' + repr(safepath)[1:-1] + '", "' + repr(os.path.join(args.prefix, relpath))[1:-1] + f'", _binary_{safepath}_start, _binary_{safepath}_end', ', 0 },', file=f)

#for p in dirs:
#    relpath = p.split(os.path.sep, maxsplit=1)[-1]
#    print('{ NULL, "' + repr(os.path.join(args.prefix, relpath))[1:-1] + '", NULL, NULL', ', 1 },', file=f)
print('};', file=f)

fmtstrlist = lambda arr: repr(arr).replace("'", '"').replace('[', '{').replace(']', '}').replace(' ', '\n')

print('const char* packfs_safepaths[] =', fmtstrlist(safepaths), ';', file=f)
print('const char* packfs_abspaths[] =' , fmtstrlist([os.path.join(args.prefix, relpath) for relpath in safepaths]), ';', file=f)
print('const char* packfs_starts[] = {', ',\n'.join(f'_binary_{safepath}_start' for safepath in safepaths), '} ;', file=f)
print('const char* packfs_ends[] = {', ',\n'.join(f'_binary_{safepath}_end' for safepath in safepaths), '} ;', file=f)
