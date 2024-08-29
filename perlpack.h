int packfsfilesnum = 2, packfsdirsnum  = 2;
extern char _binary_packfs_foo_txt_start[], _binary_packfs_foo_txt_end[];
extern char _binary_packfs_foo_bar_txt_start[], _binary_packfs_foo_bar_txt_end[];
struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {
{ "packfs_foo_txt", "/mnt/perlpack/foo.txt", _binary_packfs_foo_txt_start, _binary_packfs_foo_txt_end },
{ "packfs_foo_bar_txt", "/mnt/perlpack/foo/bar.txt", _binary_packfs_foo_bar_txt_start, _binary_packfs_foo_bar_txt_end },
};
const char* packfsdirs[] = {
"/mnt/perlpack/packfs",
"/mnt/perlpack/foo"
};
