#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifdef PACKFSLIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
// define required for #include <archive_read_private.h>
#define __LIBARCHIVE_BUILD
#include <archive_read_private.h>
void* last_file_buff; size_t last_file_block_size; size_t last_file_offset; archive_read_callback* old_file_read; archive_seek_callback* old_file_seek;
static ssize_t new_file_read(struct archive *a, void *client_data, const void **buff)
{
    // struct read_file_data copied from https://github.com/libarchive/libarchive/blob/master/libarchive/archive_read_open_filename.c
    struct read_file_data {int fd; size_t block_size; void* buffer;} *mine = client_data;
    last_file_buff = mine->buffer;
    last_file_block_size = mine->block_size;
    last_file_offset = old_file_seek(a, client_data, 0, SEEK_CUR);
    return old_file_read(a, client_data, buff);
}
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

static char script[1 << 20] = "print('Hello world! Need more arguments!\n');";
extern char _binary_myscript_pl_start[];
extern char _binary_myscript_pl_end[];

#include "perlpack.h"

enum {
    packfs_filefd_min = 1000000000, 
    packfs_filefd_max = 1000001000, 
    packfs_index_filenames_num = 1024,  
    packfs_index_filenames_len = 128, 
    packfs_archive_use_mmap = 0
};
struct packfs_context
{
    int initialized;
    
    char packfs_prefix_builtin[packfs_index_filenames_len], packfs_prefix_archive[packfs_index_filenames_len];

    int (*orig_open)(const char *path, int flags);
    int (*orig_close)(int fd);
    ssize_t (*orig_read)(int fd, void* buf, size_t count);
    int (*orig_access)(const char *path, int flags);
    off_t (*orig_lseek)(int fd, off_t offset, int whence);
    int (*orig_stat)(const char *restrict path, struct stat *restrict statbuf);
    FILE* (*orig_fopen)(const char *path, const char *mode);
    int (*orig_fileno)(FILE* stream);
    
    int packfsinfosnum;
    struct { const char* safe_path; const char *path; const char* start; const char* end; int isdir; }* packfsinfos;
    int packfs_filefd[packfs_filefd_max - packfs_filefd_min];
    FILE* packfs_fileptr[packfs_filefd_max - packfs_filefd_min];
    
    void* fileptr; size_t mmapsize;
    char filenames[packfs_index_filenames_num * packfs_index_filenames_len];
    size_t files_num, filenames_lens[packfs_index_filenames_num], offsets[packfs_index_filenames_num], sizes[packfs_index_filenames_num];
};

struct packfs_context* packfs_ensure_context()
{
    static struct packfs_context packfs_ctx = {0};

    if(packfs_ctx.initialized != 1)
    {
#ifdef PACKFS_STATIC
        extern int orig_open(const char *path, int flags); packfs_ctx.orig_open = orig_open;
        extern int orig_close(int fd); packfs_ctx.orig_close = orig_close;
        extern ssize_t orig_read(int fd, void* buf, size_t count); packfs_ctx.orig_read = orig_read;
        extern int orig_access(const char *path, int flags); packfs_ctx.orig_access = orig_access;
        extern off_t orig_lseek(int fd, off_t offset, int whence); packfs_ctx.orig_lseek = orig_lseek;
        extern int orig_stat(const char *restrict path, struct stat *restrict statbuf); packfs_ctx.orig_stat = orig_stat;
        extern FILE* orig_fopen(const char *path, const char *mode); packfs_ctx.orig_fopen = orig_fopen;
        extern int orig_fileno(FILE* stream); packfs_ctx.orig_fileno = orig_fileno;
#else
        #include <dlfcn.h>
        packfs_ctx.orig_open   = dlsym(RTLD_NEXT, "open");
        packfs_ctx.orig_close  = dlsym(RTLD_NEXT, "close");
        packfs_ctx.orig_read   = dlsym(RTLD_NEXT, "read");
        packfs_ctx.orig_access = dlsym(RTLD_NEXT, "access");
        packfs_ctx.orig_lseek  = dlsym(RTLD_NEXT, "lseek");
        packfs_ctx.orig_stat   = dlsym(RTLD_NEXT, "stat");
        packfs_ctx.orig_fopen  = dlsym(RTLD_NEXT, "fopen");
        packfs_ctx.orig_fileno = dlsym(RTLD_NEXT, "fileno");
#endif
        strcpy(packfs_ctx.packfs_prefix_builtin, "/mnt/perlpack/");
        strcpy(packfs_ctx.packfs_prefix_archive, "/mnt/perlpackarchive/");
        // TODO: disable this assignment if prefix unset
        packfs_ctx.packfsinfosnum = packfsinfosnum;
        packfs_ctx.packfsinfos = packfsinfos;
        packfs_ctx.files_num = 0;
        packfs_ctx.mmapsize = 0;
        packfs_ctx.fileptr = NULL;
        packfs_ctx.initialized = 1;
        
#ifdef PACKFSLIBARCHIVE
        struct archive *a = archive_read_new();
        archive_read_support_format_zip(a);
        struct archive_entry *entry;
        const char* packfs_archive_filename = getenv("PACKFSLIBARCHIVE");
        do
        {
            if(packfs_archive_filename == NULL || strlen(packfs_archive_filename) == 0 || strncmp(packfs_ctx.packfs_prefix_archive, packfs_archive_filename, strlen(packfs_ctx.packfs_prefix_archive)) == 0)
                break;
            
            int fd = open(packfs_archive_filename, O_RDONLY);
            struct stat file_info = {0}; 
            if(fd >= 0 && fstat(fd, &file_info) >= 0)
            {
                if(packfs_archive_use_mmap)
                {
                    packfs_ctx.mmapsize = file_info.st_size;
                    packfs_ctx.fileptr = mmap(NULL, packfs_ctx.mmapsize, PROT_READ, MAP_PRIVATE, fd, 0);
                }
                else
                {
                    packfs_ctx.mmapsize = 0;
                    packfs_ctx.fileptr = fopen(packfs_archive_filename, "rb");
                }
            }
            close(fd);

            if(packfs_ctx.fileptr == NULL)
                break;

            if(archive_read_open_filename(a, packfs_archive_filename, 10240) != ARCHIVE_OK)
                break;
            
            // struct archive_read in https://github.com/libarchive/libarchive/blob/master/libarchive/archive_read_private.h
            struct archive_read *_a = ((struct archive_read *)a);
            old_file_read = _a->client.reader;
            old_file_seek = _a->client.seeker;
            a->state = ARCHIVE_STATE_NEW;
            archive_read_set_read_callback(a, new_file_read);

            if(archive_read_open1(a) != ARCHIVE_OK)
                break;
            
            size_t filenames_lens_total = 0;
            while(1)
            {
                int r = archive_read_next_header(a, &entry);
                if (r == ARCHIVE_EOF)
                    break;
                if (r != ARCHIVE_OK)
                    break; //fprintf(stderr, "%s\n", archive_error_string(a));

                const void* firstblock_buff;
                size_t firstblock_len;
                int64_t firstblock_offset;
                r = archive_read_data_block(a, &firstblock_buff, &firstblock_len, &firstblock_offset);
                int filetype = archive_entry_filetype(entry);
                if(filetype == AE_IFREG && archive_entry_size_is_set(entry) != 0 && last_file_buff != NULL && last_file_buff <= firstblock_buff && firstblock_buff < last_file_buff + last_file_block_size)
                {
                    size_t entry_byte_size = (size_t)archive_entry_size(entry);
                    size_t entry_byte_offset = last_file_offset + (size_t)(firstblock_buff - last_file_buff);
                    const char* entryname = archive_entry_pathname(entry);
                    strcpy(packfs_ctx.filenames + filenames_lens_total, entryname);
                    packfs_ctx.filenames_lens[packfs_ctx.files_num] = strlen(entryname);
                    packfs_ctx.offsets[packfs_ctx.files_num] = entry_byte_offset;
                    packfs_ctx.sizes[packfs_ctx.files_num] = entry_byte_size;
                    filenames_lens_total += packfs_ctx.filenames_lens[packfs_ctx.files_num] + 1;
                    packfs_ctx.files_num++;
                }
                    
                r = archive_read_data_skip(a);
                if (r == ARCHIVE_EOF)
                    break;
                if (r != ARCHIVE_OK)
                    break; //fprintf(stderr, "%s\n", archive_error_string(a));
            }

            //size_t filenames_start = 0;
            //for(size_t i = 0; i < packfs_ctx.files_num; i++)
            //{
            //    fprintf(stderr, "ZIP: %s\n", packfs_ctx.filenames + filenames_start);
            //    filenames_start += packfs_ctx.filenames_lens[i] + 1;
            //}

        }
        while(0);
        archive_read_close(a);
        archive_read_free(a);
#endif
    }

    return &packfs_ctx;
}


const char* packfs_sanitize_path(const char* path)
{
    return (path != NULL && strlen(path) > 2 && path[0] == '.' && path[1] == '/') ? (path + 2) : path;
}

int packfs_open(struct packfs_context* packfs_ctx, const char* path, FILE** out)
{
    path = packfs_sanitize_path(path);

    FILE* fileptr = NULL;
    
    if(packfs_ctx->packfsinfosnum > 0 && strncmp(packfs_ctx->packfs_prefix_builtin, path, strlen(packfs_ctx->packfs_prefix_builtin)) == 0)
    {
        for(size_t i = 0; i < packfs_ctx->packfsinfosnum; i++)
        {
            if(0 == strcmp(path, packfs_ctx->packfsinfos[i].path))
            {
                fileptr = fmemopen((void*)packfs_ctx->packfsinfos[i].start, (size_t)(packfs_ctx->packfsinfos[i].end - packfs_ctx->packfsinfos[i].start), "r");
                break;
            }
        }
    }

#if PACKFSLIBARCHIVE
    else if(packfs_ctx->files_num > 0 && strncmp(packfs_ctx->packfs_prefix_archive, path, strlen(packfs_ctx->packfs_prefix_archive)) == 0)
    {
        const char* path_without_prefix = path + strlen(packfs_ctx->packfs_prefix_archive);
        size_t filenames_start = 0;
        for(size_t i = 0; i < packfs_ctx->files_num; i++)
        {
            if(0 == strcmp(packfs_ctx->filenames + filenames_start, path_without_prefix))
            {
                size_t offset = packfs_ctx->offsets[i], size = packfs_ctx->sizes[i];
                if(packfs_ctx->mmapsize != 0)
                {
                    fileptr = fmemopen((char*)packfs_ctx->fileptr + offset, size, "rb");
                }
                else
                {
                    fileptr = fmemopen(NULL, size, "rb+");
                    fseek((FILE*)packfs_ctx->fileptr, offset, SEEK_SET);
                    char buf[8192];
                    while(size > 0)
                    {
                        size_t len = fread(buf, 1, sizeof(buf) <= size ? sizeof(buf) : size, (FILE*)packfs_ctx->fileptr);
                        fwrite(buf, 1, len, fileptr);
                        size -= len;
                    }
                    fseek(fileptr, 0, SEEK_SET);
                }
                break;
            }
            filenames_start += packfs_ctx->filenames_lens[i] + 1;
        }
    }
#endif
    
    if(out != NULL)
        *out = fileptr;

    for(size_t k = 0; fileptr != NULL && k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_filefd[k] == 0)
        {
            packfs_ctx->packfs_filefd[k] = packfs_filefd_min + k;
            packfs_ctx->packfs_fileptr[k] = fileptr;
            return packfs_ctx->packfs_filefd[k];
        }
    }

    return -1;
}

int packfs_close(struct packfs_context* packfs_ctx, int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return -2;

    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_filefd[k] == fd)
        {
            packfs_ctx->packfs_filefd[k] = 0;
            int res = fclose(packfs_ctx->packfs_fileptr[k]);
            packfs_ctx->packfs_fileptr[k] = NULL;
            return res;
        }
    }
    return -2;
}

FILE* packfs_findptr(struct packfs_context* packfs_ctx, int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return NULL;
    
    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_filefd[k] == fd)
            return packfs_ctx->packfs_fileptr[k];
    }
    return NULL;
}

int packfs_findfd(struct packfs_context* packfs_ctx, FILE* ptr)
{
    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_fileptr[k] == ptr)
            return packfs_ctx->packfs_filefd[k];
    }
    return -1;
}

ssize_t packfs_read(struct packfs_context* packfs_ctx, int fd, void* buf, size_t count)
{
    FILE* ptr = packfs_findptr(packfs_ctx, fd);
    if(!ptr)
        return -1;
    return (ssize_t)fread(buf, 1, count, ptr);
}

int packfs_seek(struct packfs_context* packfs_ctx, int fd, long offset, int whence)
{
    FILE* ptr = packfs_findptr(packfs_ctx, fd);
    if(!ptr)
        return -1;
    return fseek(ptr, offset, whence);
}

int packfs_access(struct packfs_context* packfs_ctx, const char* path)
{
    path = packfs_sanitize_path(path);

    if(strncmp(packfs_ctx->packfs_prefix_builtin, path, strlen(packfs_ctx->packfs_prefix_builtin)) == 0)
    {
        for(int i = 0; i < packfs_ctx->packfsinfosnum; i++)
        {
            if(0 == strcmp(path, packfs_ctx->packfsinfos[i].path))
            {
                return 0;
            }
        }
        return -1;
    }
    return -2;
}

int packfs_stat(struct packfs_context* packfs_ctx, const char* path, struct stat *restrict statbuf)
{
    path = packfs_sanitize_path(path);
    
    if(strncmp(packfs_ctx->packfs_prefix_builtin, path, strlen(packfs_ctx->packfs_prefix_builtin)) == 0)
    {
        for(int i = 0; i < packfs_ctx->packfsinfosnum; i++)
        {
            if(0 == strcmp(path, packfs_ctx->packfsinfos[i].path))
            {
                *statbuf = (struct stat){0};
                if(packfsinfos[i].isdir)
                {
                    statbuf->st_size = 0;
                    statbuf->st_mode = S_IFDIR;
                }
                else
                {
                    statbuf->st_size = (off_t)(packfsinfos[i].end - packfsinfos[i].start);
                    statbuf->st_mode = S_IFREG;
                }
                return 0;
            }
        }
        return -1;
    }
    
    return -2;
}

FILE* fopen(const char *path, const char *mode)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    FILE* res = NULL;
    if(packfs_open(packfs_ctx, path, &res) >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Fopen(\"%s\", \"%s\") == %p\n", path, mode, (void*)res);
#endif
        return res;
    }

    res = packfs_ctx->orig_fopen(path, mode);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: fopen(\"%s\", \"%s\") == %p\n", path, mode, (void*)res);
#endif
    return res;
}

int fileno(FILE *stream)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    if(!stream) return -1;
    
    int res = packfs_ctx->orig_fileno(stream);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: fileno(%p) == %d\n", (void*)stream, res);
#endif
    
    if(res < 0)
    {        
        res = packfs_findfd(packfs_ctx, stream);
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Fileno(%p) == %d\n", (void*)stream, res);
#endif
    }
    
    return res;
}


int open(const char *path, int flags, ...)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();

    int res = packfs_open(packfs_ctx, path, NULL);
    if(res >= 0)
    { 
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Open(\"%s\", %d) == %d\n", path, flags, res);
#endif
        return res;
    }
    
    res = packfs_ctx->orig_open(path, flags);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: open(\"%s\", %d) == %d\n", path, flags, res);
#endif
    return res;
}

int close(int fd)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    int res = packfs_close(packfs_ctx, fd);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Close(%d) == %d\n", fd, res);
#endif
        return res;
    }
    
    res = packfs_ctx->orig_close(fd);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: close(%d) == %d\n", fd, res);
#endif
    return res;
}


ssize_t read(int fd, void* buf, size_t count)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    ssize_t res = packfs_read(packfs_ctx, fd, buf, count);
    if(res >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Read(%d, %p, %zu) == %d\n", fd, buf, count, (int)res);
#endif
        return res;
    }

    res = packfs_ctx->orig_read(fd, buf, count);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: read(%d, %p, %zu) == %d\n", fd, buf, count, (int)res);
#endif
    return res;
}

off_t lseek(int fd, off_t offset, int whence)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    int res = packfs_seek(packfs_ctx, fd, (long)offset, whence);
    if(res >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Seek(%d, %d, %d) == %d\n", fd, (int)offset, whence, (int)res);
#endif
        return res;
    }

    res = packfs_ctx->orig_lseek(fd, offset, whence);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: seek(%d, %d, %d) == %d\n", fd, (int)offset, whence, (int)res);
#endif
    return res;
}


int access(const char *path, int flags) 
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    int res = packfs_access(packfs_ctx, path);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Access(\"%s\", %d) == 0\n", path, flags);
#endif
        return res;
    }
    
    res = packfs_ctx->orig_access(path, flags); 
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: access(\"%s\", %d) == %d\n", path, flags, res);
#endif
    return res;
}

int stat(const char *restrict path, struct stat *restrict statbuf)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    int res = packfs_stat(packfs_ctx, path, statbuf);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Stat(\"%s\", %p) == -1\n", path, (void*)statbuf);
#endif
        return res;
    }

    res = packfs_ctx->orig_stat(path, statbuf);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: stat(\"%s\", %p) == %d\n", path, (void*)statbuf, res);
#endif
    return res;
}

// #include <xsinit.c>
void xs_init(pTHX) //EXTERN_C 
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;
    
    extern void boot_DynaLoader(pTHX_ CV* cv);
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
   
#ifdef PERLPACK_mro
    extern void boot_mro(pTHX_ CV* cv);
    newXS("mro::bootstrap", boot_mro, file);
#endif
#ifdef PERLPACK_Devel__Peek
    extern void boot_Devel__Peek(pTHX_ CV* cv);
    newXS("Devel::Peek", boot_Devel__Peek, file);
#endif
#ifdef PERLPACK_File__DosGlob
    extern void boot_File__DosGlob(pTHX_ CV* cv);
    newXS("File::DosGlob::bootstrap", boot_File__DosGlob, file);
#endif
#ifdef PERLPACK_File__Glob
    extern void boot_File__Glob(pTHX_ CV* cv);
    newXS("File::Glob::bootstrap", boot_File__Glob, file);
#endif
#ifdef PERLPACK_Sys__Syslog
    extern void boot_Sys__Syslog(pTHX_ CV* cv);
    newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
#endif
#ifdef PERLPACK_Sys__Hostname
    extern void boot_Sys__Hostname(pTHX_ CV* cv);
    newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
#endif
#ifdef PERLPACK_PerlIO__via
    extern void boot_PerlIO__via(pTHX_ CV* cv);
    newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
#endif
#ifdef PERLPACK_PerlIO__mmap
    extern void boot_PerlIO__mmap(pTHX_ CV* cv);
    newXS("PerlIO::mmap::bootstrap", boot_PerlIO__mmap, file);
#endif
#ifdef PERLPACK_PerlIO__encoding
    extern void boot_PerlIO__encoding(pTHX_ CV* cv);
    newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
#endif
#ifdef PERLPACK_PerlIO__scalar
    extern void boot_PerlIO__scalar(pTHX_ CV* cv);
    newXS("PerlIO::scalar::bootstrap", boot_PerlIO__scalar, file);
#endif
#ifdef PERLPACK_B
    extern void boot_B(pTHX_ CV* cv);
    newXS("B::bootstrap", boot_B, file);
#endif
#ifdef PERLPACK_attributes
    extern void boot_attributes(pTHX_ CV* cv);
    newXS("attributes::bootstrap", boot_attributes, file);
#endif
#ifdef PERLPACK_Unicode__Normalize
    extern void boot_Unicode__Normalize(pTHX_ CV* cv);
    newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
#endif
#ifdef PERLPACK_Unicode__Collate
    extern void boot_Unicode__Collate(pTHX_ CV* cv);
    newXS("Unicode::Collate::bootstrap", boot_Unicode__Collate, file);
#endif
#ifdef PERLPACK_threads
    extern void boot_threads(pTHX_ CV* cv);
    newXS("threads::bootstrap", boot_threads, file);
#endif
#ifdef PERLPACK_threads__shared
    extern void boot_threads__shared(pTHX_ CV* cv);
    newXS("threads::shared::bootstrap", boot_threads__shared, file);
#endif
#ifdef PERLPACK_IPC__SysV
    extern void boot_IPC__SysV(pTHX_ CV* cv);
    newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
#endif
#ifdef PERLPACK_re
    extern void boot_re(pTHX_ CV* cv);
    newXS("re::bootstrap", boot_re, file);
#endif
#ifdef PERLPACK_Digest__MD5
    extern void boot_Digest__MD5(pTHX_ CV* cv);
    newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
#endif
#ifdef PERLPACK_Digest__SHA
    extern void boot_Digest__SHA(pTHX_ CV* cv);
    newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
#endif
#ifdef PERLPACK_SDBM_File
    extern void boot_SDBM_File(pTHX_ CV* cv);
    newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
#endif
#ifdef PERLPACK_Math__BigInt__FastCalc
    extern void boot_Math__BigInt__FastCalc(pTHX_ CV* cv);
    newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
#endif
#ifdef PERLPACK_Data__Dumper
    extern void boot_Data__Dumper(pTHX_ CV* cv);
    newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
#endif
#ifdef PERLPACK_I18N__Langinfo
    extern void boot_I18N__Langinfo(pTHX_ CV* cv);
    newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
#endif
#ifdef PERLPACK_Time__HiRes
    extern void boot_Time__HiRes(pTHX_ CV* cv);
    newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
#endif
#ifdef PERLPACK_Time__Piece
    extern void boot_Time__Piece(pTHX_ CV* cv);
    newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
#endif
#ifdef PERLPACK_IO
    extern void boot_IO(pTHX_ CV* cv);
    newXS("IO::bootstrap", boot_IO, file);
#endif
#ifdef PERLPACK_Socket
    extern void boot_Socket(pTHX_ CV* cv);
    newXS("Socket::bootstrap", boot_Socket, file);
#endif
#ifdef PERLPACK_Hash__Util__FieldHash
    extern void boot_Hash__Util__FieldHash(pTHX_ CV* cv);
    newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file); 
#endif
#ifdef PERLPACK_Hash__Util
    extern void boot_Hash__Util(pTHX_ CV* cv);
    newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
#endif
#ifdef PERLPACK_Filter__Util__Call
    extern void boot_Filter__Util__Call(pTHX_ CV* cv);
    newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
#endif
#ifdef PERLPACK_POSIX
    extern void boot_POSIX(pTHX_ CV* cv);
    newXS("POSIX::bootstrap", boot_POSIX, file);
#endif
#ifdef PERLPACK_Encode__Unicode
    extern void boot_Encode__Unicode(pTHX_ CV* cv);
    newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
#endif
#ifdef PERLPACK_Encode
    extern void boot_Encode(pTHX_ CV* cv);
    newXS("Encode::bootstrap", boot_Encode, file);
#endif
#ifdef PERLPACK_Encode__JP
    extern void boot_Encode__JP(pTHX_ CV* cv);
    newXS("Encode::JP::bootstrap", boot_Encode__JP, file);
#endif
#ifdef PERLPACK_Encode__KR
    extern void boot_Encode__KR(pTHX_ CV* cv);
    newXS("Encode::KR::bootstrap", boot_Encode__KR, file);
#endif
#ifdef PERLPACK_Encode__EBCDIC
    extern void boot_Encode__EBCDIC(pTHX_ CV* cv);
    newXS("Encode::EBCDIC::bootstrap", boot_Encode__EBCDIC, file);
#endif
#ifdef PERLPACK_Encode__CN
    extern void boot_Encode__CN(pTHX_ CV* cv);
    newXS("Encode::CN::bootstrap", boot_Encode__CN, file);
#endif
#ifdef PERLPACK_Encode__Symbol
    extern void boot_Encode__Symbol(pTHX_ CV* cv);
    newXS("Encode::Symbol::bootstrap", boot_Encode__Symbol, file);
#endif
#ifdef PERLPACK_Encode__Byte
    extern void boot_Encode__Byte(pTHX_ CV* cv);
    newXS("Encode::Byte::bootstrap", boot_Encode__Byte, file);
#endif
#ifdef PERLPACK_Encode__TW
    extern void boot_Encode__TW(pTHX_ CV* cv);
    newXS("Encode::TW::bootstrap", boot_Encode__TW, file);
#endif
#ifdef PERLPACK_Compress__Raw__Zlib
    extern void boot_Compress__Raw__Zlib(pTHX_ CV* cv);
    newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
#endif
#ifdef PERLPACK_Compress__Raw__Bzip2
    extern void boot_Compress__Raw__Bzip2(pTHX_ CV* cv);
    newXS("Compress::Raw::Bzip2::bootstrap", boot_Compress__Raw__Bzip2, file);
#endif
#ifdef PERLPACK_MIME__Base64
    extern void boot_MIME__Base64(pTHX_ CV* cv);
    newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
#endif
#ifdef PERLPACK_Cwd
    extern void boot_Cwd(pTHX_ CV* cv);
    newXS("Cwd::bootstrap", boot_Cwd, file);
#endif
#ifdef PERLPACK_Storable
    extern void boot_Storable(pTHX_ CV* cv);
    newXS("Storable::bootstrap", boot_Storable, file);
#endif
#ifdef PERLPACK_List__Util
    extern void boot_List__Util(pTHX_ CV* cv);
    newXS("List::Util::bootstrap", boot_List__Util, file);
#endif
#ifdef PERLPACK_Fcntl
    extern void boot_Fcntl(pTHX_ CV* cv);
    newXS("Fcntl::bootstrap", boot_Fcntl, file);
#endif
#ifdef PERLPACK_Opcode
    extern void boot_Opcode(pTHX_ CV* cv);
    newXS("Opcode::bootstrap", boot_Opcode, file);
#endif
}

int main(int argc, char *argv[], char* envp[])
{
    if(argc > 1 && 0 == strcmp("myscript.pl", argv[1]))
    {
        size_t iSize = _binary_myscript_pl_end - _binary_myscript_pl_start;
        strncpy(script, _binary_myscript_pl_start, iSize);
        script[iSize] = '\0';
    }
    else if(argc > 2 && 0 == strcmp("-e", argv[1]))
    {
        strcpy(script, argv[2]);
    }
    
    packfs_ensure_context();

    PERL_SYS_INIT3(&argc, &argv, &envp);
    PerlInterpreter* myperl = perl_alloc();
    if(!myperl) return -1;

    perl_construct(myperl);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    char *myperl_argv[] = { "perlpack", "-e", script, "--", argv[2], NULL };
    int myperl_argc = sizeof(myperl_argv) / sizeof(myperl_argv[0]) - 1;
    int res = perl_parse(myperl, xs_init, myperl_argc, myperl_argv, envp);
    if(res == 0) res = perl_run(myperl); // error if res != 0 (or stricter in case exit(0) was called from perl code): (res & 0xFF) != 0: 

    PL_perl_destruct_level = 0;
    res = perl_destruct(myperl);
    perl_free(myperl);
    PERL_SYS_TERM();

    return res;
}
