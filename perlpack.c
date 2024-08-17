// https://perldoc.perl.org/perlembed
// https://perldoc.perl.org/perlguts
// https://www.perlmonks.org/?node_id=385469
// https://github.com/Perl/perl5/commit/0301e899536a22752f40481d8a1d141b7a7dda82
// https://github.com/Perl/perl5/issues/16565
// https://www.postgresql.org/message-id/23260.1527026547%40sss.pgh.pa.us

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

static char script[1 << 20] = "print('Hello world! Need more arguments!\n');";

extern char _binary_myscript_pl_start[];
extern char _binary_myscript_pl_end[];

#ifdef PACKFS_DYNAMIC

#include "perlpack.h"

#define packfs_prefix "/mnt/perlpack/"
#define packfs_filefd_min 1000000000
#define packfs_filefd_max 1000001000
#define packfs_filefd_cnt (packfs_filefd_max - packfs_filefd_min)

int packfs_filefd[packfs_filefd_cnt];
FILE* packfs_fileptr[packfs_filefd_cnt];

const char* packfs_sanitize_path(const char* path)
{
    return (path != NULL && strlen(path) > 2 && path[0] == '.' && path[1] == '/') ? (path + 2) : path;
}

int packfs_open(const char* path, FILE** out)
{
    path = packfs_sanitize_path(path);
    
    if(strncmp(packfs_prefix, path, strlen(packfs_prefix)) != 0)
        return -1;

    for(int i = 0; i < packfsfilesnum; i++)
    {
        if(0 == strcmp(path, packfsinfos[i].path))
        {
            FILE* res = fmemopen((void*)packfsinfos[i].start, (size_t)(packfsinfos[i].end - packfsinfos[i].start), "r");
            
            for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
            {
                if(packfs_filefd[k] == 0)
                {
                    packfs_filefd[k] = packfs_filefd_min + k;
                    packfs_fileptr[k] = res;
                    if(out != NULL)
                        *out = packfs_fileptr[k];
                    return packfs_filefd[k];
                }
            }
        }
    }

    return -1;
}

int packfs_close(int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return -2;

    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_filefd[k] == fd)
        {
            packfs_filefd[k] = 0;
            int res = fclose(packfs_fileptr[k]);
            packfs_fileptr[k] = NULL;
            return res;
        }
    }
    return -2;
}

FILE* packfs_findptr(int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return NULL;
    
    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_filefd[k] == fd)
            return packfs_fileptr[k];
    }
    return NULL;
}

int packfs_findfd(FILE* ptr)
{
    for(int k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_fileptr[k] == ptr)
            return packfs_filefd[k];
    }
    return -1;
}

ssize_t packfs_read(int fd, void* buf, size_t count)
{
    FILE* ptr = packfs_findptr(fd);
    if(!ptr)
        return -1;
    return (ssize_t)fread(buf, 1, count, ptr);
}

int packfs_seek(int fd, long offset, int whence)
{
    FILE* ptr = packfs_findptr(fd);
    if(!ptr)
        return -1;
    return fseek(ptr, offset, whence);
}

int packfs_access(const char* path)
{
    path = packfs_sanitize_path(path);

    if(strncmp(packfs_prefix, path, strlen(packfs_prefix)) == 0)
    {
        for(int i = 0; i < packfsfilesnum; i++)
        {
            if(0 == strcmp(path, packfsinfos[i].path))
            {
                return 0;
            }
        }
        return -1;
    }
    return -2;
}

int packfs_stat(const char* path, struct stat *restrict statbuf)
{
    path = packfs_sanitize_path(path);
    
    if(strncmp(packfs_prefix, path, strlen(packfs_prefix)) == 0)
    {
        for(int i = 0; i < packfsfilesnum; i++)
        {
            if(0 == strcmp(path, packfsinfos[i].path))
            {
                *statbuf = (struct stat){0};
                statbuf->st_size = (off_t)(packfsinfos[i].end - packfsinfos[i].start);
                statbuf->st_mode = S_IFREG;
                return 0;
            }
        }
        for(int i = 0; i < packfsdirsnum; i++)
        {
            if(0 == strcmp(path, packfsdirs[i]))
            {
                *statbuf = (struct stat){0};
                statbuf->st_mode = S_IFDIR;
                return 0;
            }
        }
        return -1;
    }
    
    return -2;
}

FILE* fopen(const char *path, const char *mode)
{
    typedef FILE* (*orig_fopen_func_type)(const char *path, const char *mode);
    orig_fopen_func_type orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen");
    
    FILE* res = NULL;
    if(packfs_open(path, &res) >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Fopen(\"%s\", \"%s\") == %p\n", path, mode, (void*)res);
#endif
        return res;
    }

    res = orig_func(path, mode);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: fopen(\"%s\", \"%s\") == %p\n", path, mode, (void*)res);
#endif
    return res;
}

int fileno(FILE *stream)
{
    typedef int (*orig_func_type)(FILE* stream);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "fileno");
    
    if(!stream) return -1;
    
    int res = orig_func(stream);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: fileno(%p) == %d\n", (void*)stream, res);
#endif
    
    if(res < 0)
    {        
        res = packfs_findfd(stream);
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Fileno(%p) == %d\n", (void*)stream, res);
#endif
    }
    
    return res;
}


typedef int (*orig_func_type_open)(const char *path, int flags);

typedef struct
{
    bool initialized;
    orig_func_type_open orig_open;
} packfs_context;

packfs_context packfs_ensure_context()
{
    static packfs_context packfs_ctx = {0};
    if(!packfs_ctx.initialized)
    {
        packfs_ctx.orig_open = dlsym(RTLD_NEXT, "open"); //(orig_func_type_open)
        packfs.initialized = true;
    }
    return packfs_ctx;
}

int open(const char *path, int flags, ...)
{
    packfs_context packfs_ctx = packfs_ensure_context();

    int res = packfs_open(path, NULL);
    if(res >= 0)
    { 
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Open(\"%s\", %d) == %d\n", path, flags, res);
#endif
        return res;
    }
    
    res = packfs_ctx.orig_open(path, flags);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: open(\"%s\", %d) == %d\n", path, flags, res);
#endif
    return res;
}

int close(int fd)
{
    typedef int (*orig_func_type)(int fd);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "close");

    int res = packfs_close(fd);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Close(%d) == %d\n", fd, res);
#endif
        return res;
    }
    
    res = orig_func(fd);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: close(%d) == %d\n", fd, res);
#endif
    return res;
}


// https://en.cppreference.com/w/c/io/fread
ssize_t read(int fd, void* buf, size_t count)
{
    typedef ssize_t (*orig_func_type)(int fd, void* buf, size_t count);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "read");

    ssize_t res = packfs_read(fd, buf, count);
    if(res >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Read(%d, %p, %zu) == %d\n", fd, buf, count, (int)res);
#endif
        return res;
    }

    res = orig_func(fd, buf, count);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: read(%d, %p, %zu) == %d\n", fd, buf, count, (int)res);
#endif
    return res;
}

off_t lseek(int fd, off_t offset, int whence)
{
    typedef off_t (*orig_func_type)(int fd, off_t offset, int whence);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "lseek");
    
    int res = packfs_seek(fd, (long)offset, whence);
    if(res >= 0)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Seek(%d, %d, %d) == %d\n", fd, (int)offset, whence, (int)res);
#endif
        return res;
    }

    res = orig_func(fd, offset, whence);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: seek(%d, %d, %d) == %d\n", fd, (int)offset, whence, (int)res);
#endif
    return res;
}


int access(const char *path, int flags) 
{
    typedef int (*orig_func_type)(const char *path, int flags);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "access");
   
    int res = packfs_access(path);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Access(\"%s\", %d) == 0\n", path, flags);
#endif
        return res;
    }
    
    res = orig_func(path, flags); 
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: access(\"%s\", %d) == %d\n", path, flags, res);
#endif
    return res;
}

int stat(const char *restrict path, struct stat *restrict statbuf)
{
    typedef int (*orig_func_type)(const char *restrict path, struct stat *restrict statbuf);
    orig_func_type orig_func = (orig_func_type)dlsym(RTLD_NEXT, "stat");
    
    int res = packfs_stat(path, statbuf);
    if(res >= -1)
    {
#ifdef PACKFS_LOG
        fprintf(stderr, "packfs: Stat(\"%s\", %p) == -1\n", path, (void*)statbuf);
#endif
        return res;
    }

    res = orig_func(path, statbuf);
#ifdef PACKFS_LOG
    fprintf(stderr, "packfs: stat(\"%s\", %p) == %d\n", path, (void*)statbuf, res);
#endif
    return res;
}

#endif

///////////////////////////////////////
// #include <xsinit.c>

extern void boot_DynaLoader (pTHX_ CV* cv);

#ifdef PERLPACK_mro
extern void boot_mro(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Devel__Peek
extern void boot_Devel__Peek(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_File__DosGlob
extern void boot_File__DosGlob(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_File__Glob
extern void boot_File__Glob(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Sys__Syslog
extern void boot_Sys__Syslog(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Sys__Hostname
extern void boot_Sys__Hostname(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_PerlIO__via
extern void boot_PerlIO__via(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_PerlIO__mmap
extern void boot_PerlIO__mmap(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_PerlIO__encoding
extern void boot_PerlIO__encoding(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_PerlIO__scalar
extern void boot_PerlIO__scalar(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_B
extern void boot_B(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_attributes
extern void boot_attributes(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Unicode__Normalize
extern void boot_Unicode__Normalize(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Unicode__Collate
extern void boot_Unicode__Collate(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_threads
extern void boot_threads(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_threads__shared
extern void boot_threads__shared(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_IPC__SysV
extern void boot_IPC__SysV(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_re
extern void boot_re(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Digest__MD5
extern void boot_Digest__MD5(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Digest__SHA
extern void boot_Digest__SHA(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_SDBM_File
extern void boot_SDBM_File(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Math__BigInt__FastCalc
extern void boot_Math__BigInt__FastCalc(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Data__Dumper
extern void boot_Data__Dumper(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_I18N__Langinfo
extern void boot_I18N__Langinfo(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Time__HiRes
extern void boot_Time__HiRes(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Time__Piece
extern void boot_Time__Piece(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_IO
extern void boot_IO(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Socket
extern void boot_Socket(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Hash__Util__FieldHash
extern void boot_Hash__Util__FieldHash(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Hash__Util
extern void boot_Hash__Util(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Filter__Util__Call
extern void boot_Filter__Util__Call(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_POSIX
extern void boot_POSIX(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__Unicode
extern void boot_Encode__Unicode(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode
extern void boot_Encode(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__JP
extern void boot_Encode__JP(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__KR
extern void boot_Encode__KR(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__EBCDIC
extern void boot_Encode__EBCDIC(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__CN
extern void boot_Encode__CN(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__Symbol
extern void boot_Encode__Symbol(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__Byte
extern void boot_Encode__Byte(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Encode__TW
extern void boot_Encode__TW(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Compress__Raw__Zlib
extern void boot_Compress__Raw__Zlib(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Compress__Raw__Bzip2
extern void boot_Compress__Raw__Bzip2(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_MIME__Base64
extern void boot_MIME__Base64(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Cwd
extern void boot_Cwd(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Storable
extern void boot_Storable(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_List__Util
extern void boot_List__Util(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Fcntl
extern void boot_Fcntl(pTHX_ CV* cv);
#endif
#ifdef PERLPACK_Opcode
extern void boot_Opcode(pTHX_ CV* cv);
#endif




//EXTERN_C void xs_init         (pTHX);
void xs_init         (pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;
    
    // https://medium.com/booking-com-development/native-extensions-for-perl-without-smoke-and-mirrors-40479999dfc8
    // https://github.com/xsawyerx/xs-fun
    // https://perldoc.perl.org/perlxs
    // https://perldoc.perl.org/perlapi
   
#ifdef PERLPACK_mro
    newXS("mro::bootstrap", boot_mro, file);
#endif
#ifdef PERLPACK_Devel__Peek
    newXS("Devel::Peek", boot_Devel__Peek, file);
#endif
#ifdef PERLPACK_File__DosGlob
    newXS("File::DosGlob::bootstrap", boot_File__DosGlob, file);
#endif
#ifdef PERLPACK_File__Glob
    newXS("File::Glob::bootstrap", boot_File__Glob, file);
#endif
#ifdef PERLPACK_Sys__Syslog
    newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
#endif
#ifdef PERLPACK_Sys__Hostname
    newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
#endif
#ifdef PERLPACK_PerlIO__via
    newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
#endif
#ifdef PERLPACK_PerlIO__mmap
    newXS("PerlIO::mmap::bootstrap", boot_PerlIO__mmap, file);
#endif
#ifdef PERLPACK_PerlIO__encoding
    newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
#endif
#ifdef PERLPACK_PerlIO__scalar
    newXS("PerlIO::scalar::bootstrap", boot_PerlIO__scalar, file);
#endif
#ifdef PERLPACK_B
    newXS("B::bootstrap", boot_B, file);
#endif
#ifdef PERLPACK_attributes
    newXS("attributes::bootstrap", boot_attributes, file);
#endif
#ifdef PERLPACK_Unicode__Normalize
    newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
#endif
#ifdef PERLPACK_Unicode__Collate
    newXS("Unicode::Collate::bootstrap", boot_Unicode__Collate, file);
#endif
#ifdef PERLPACK_threads
    newXS("threads::bootstrap", boot_threads, file);
#endif
#ifdef PERLPACK_threads__shared
    newXS("threads::shared::bootstrap", boot_threads__shared, file);
#endif
#ifdef PERLPACK_IPC__SysV
    newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
#endif
#ifdef PERLPACK_re
    newXS("re::bootstrap", boot_re, file);
#endif
#ifdef PERLPACK_Digest__MD5
    newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
#endif
#ifdef PERLPACK_Digest__SHA
    newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
#endif
#ifdef PERLPACK_SDBM_File
    newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
#endif
#ifdef PERLPACK_Math__BigInt__FastCalc
    newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
#endif
#ifdef PERLPACK_Data__Dumper
    newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
#endif
#ifdef PERLPACK_I18N__Langinfo
    newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
#endif
#ifdef PERLPACK_Time__HiRes
    newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
#endif
#ifdef PERLPACK_Time__Piece
    newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
#endif
#ifdef PERLPACK_IO
    newXS("IO::bootstrap", boot_IO, file);
#endif
#ifdef PERLPACK_Socket
    newXS("Socket::bootstrap", boot_Socket, file);
#endif
#ifdef PERLPACK_Hash__Util__FieldHash
    newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file); 
#endif
#ifdef PERLPACK_Hash__Util
    newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
#endif
#ifdef PERLPACK_Filter__Util__Call
    newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
#endif
#ifdef PERLPACK_POSIX
    newXS("POSIX::bootstrap", boot_POSIX, file);
#endif
#ifdef PERLPACK_Encode__Unicode
    newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
#endif
#ifdef PERLPACK_Encode
    newXS("Encode::bootstrap", boot_Encode, file);
#endif
#ifdef PERLPACK_Encode__JP
    newXS("Encode::JP::bootstrap", boot_Encode__JP, file);
#endif
#ifdef PERLPACK_Encode__KR
    newXS("Encode::KR::bootstrap", boot_Encode__KR, file);
#endif
#ifdef PERLPACK_Encode__EBCDIC
    newXS("Encode::EBCDIC::bootstrap", boot_Encode__EBCDIC, file);
#endif
#ifdef PERLPACK_Encode__CN
    newXS("Encode::CN::bootstrap", boot_Encode__CN, file);
#endif
#ifdef PERLPACK_Encode__Symbol
    newXS("Encode::Symbol::bootstrap", boot_Encode__Symbol, file);
#endif
#ifdef PERLPACK_Encode__Byte
    newXS("Encode::Byte::bootstrap", boot_Encode__Byte, file);
#endif
#ifdef PERLPACK_Encode__TW
    newXS("Encode::TW::bootstrap", boot_Encode__TW, file);
#endif
#ifdef PERLPACK_Compress__Raw__Zlib
    newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
#endif
#ifdef PERLPACK_Compress__Raw__Bzip2
    newXS("Compress::Raw::Bzip2::bootstrap", boot_Compress__Raw__Bzip2, file);
#endif
#ifdef PERLPACK_MIME__Base64
    newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
#endif
#ifdef PERLPACK_Cwd
    newXS("Cwd::bootstrap", boot_Cwd, file);
#endif
#ifdef PERLPACK_Storable
    newXS("Storable::bootstrap", boot_Storable, file);
#endif
#ifdef PERLPACK_List__Util
    newXS("List::Util::bootstrap", boot_List__Util, file);
#endif
#ifdef PERLPACK_Fcntl
    newXS("Fcntl::bootstrap", boot_Fcntl, file);
#endif
#ifdef PERLPACK_Opcode
    newXS("Opcode::bootstrap", boot_Opcode, file);
#endif
    
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}
///////////////////////////////////////

int main(int argc, char *argv[], char* envp[])
{
    if(argc > 1 && 0 == strcmp("myscript.pl", argv[1]))
    {
        int iSize = (int)(_binary_myscript_pl_end - _binary_myscript_pl_start);
        strncpy(script,    _binary_myscript_pl_start, iSize);
        script[iSize] = '\0';
    }
    else if(argc > 2 && 0 == strcmp("-e", argv[1]))
    {
        strcpy(script, argv[2]);
    }
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
