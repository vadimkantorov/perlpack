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

#include "perlpack.h"
//size_t packfs_builtin_files_num, packfs_builtin_dirs_num; const char** packfs_builtin_abspaths; const char** packfs_builtin_abspaths_dirs; const char** packfs_builtin_starts; const char** packfs_builtin_ends;

extern int      __real_open(const char *path, int flags);                               
extern int      __real_close(int fd);                                                   
extern ssize_t  __real_read(int fd, void* buf, size_t count);                           
extern int      __real_access(const char *path, int flags);                             
extern off_t    __real_lseek(int fd, off_t offset, int whence);                         
extern int      __real_stat(const char *restrict path, struct stat *restrict statbuf);  
extern int      __real_fstat(int fd, struct stat * statbuf);                            
extern FILE*    __real_fopen(const char *path, const char *mode);                       
extern int      __real_fileno(FILE* stream);                                            
    
enum {
    packfs_filefd_min = 1000000000, 
    packfs_filefd_max = 1000001000, 
    packfs_filepath_max_len = 128, 
};
int packfs_enabled;
int packfs_filefd[packfs_filefd_max - packfs_filefd_min];
FILE* packfs_fileptr[packfs_filefd_max - packfs_filefd_min];
size_t packfs_filesize[packfs_filefd_max - packfs_filefd_min];

#define PACKFS_STRING_VALUE_(x) #x
#define PACKFS_STRING_VALUE(x) PACKFS_STRING_VALUE_(x)
// TODO: append / if missing
char packfs_builtin_prefix[] = PACKFS_STRING_VALUE(PACKFS_BUILTIN_PREFIX);
#undef PACKFS_STRING_VALUE
#undef PACKFS_STRING_VALUE_

const char* packfs_sanitize_path(const char* path)
{
    return (path != NULL && strlen(path) > 2 && path[0] == '.' && path[1] == '/') ? (path + 2) : path;
}

int packfs_strncmp(const char* prefix, const char* path, size_t count)
{
    return (prefix != NULL && prefix[0] != '\0' && path != NULL && path[0] != '\0') ? strncmp(prefix, path, count) : 1;
}

int packfs_open(const char* path, FILE** out)
{
    const char* path_sanitized = packfs_sanitize_path(path);

    FILE* fileptr = NULL;
    size_t filesize = 0;
    
    if(packfs_builtin_files_num > 0 && 0 == packfs_strncmp(packfs_builtin_prefix, path_sanitized, strlen(packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_builtin_abspaths[i]))
            {
                filesize = (size_t)(packfs_builtin_ends[i] - packfs_builtin_starts[i]);
                fileptr = fmemopen((void*)packfs_builtin_starts[i], filesize, "r");
                break;
            }
        }
    }

    if(out != NULL)
        *out = fileptr;

    for(size_t k = 0; fileptr != NULL && k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_filefd[k] == 0)
        {
            packfs_filefd[k] = packfs_filefd_min + k;
            packfs_fileptr[k] = fileptr;
            packfs_filesize[k] = filesize;
            return packfs_filefd[k];
        }
    }

    return -1;
}

int packfs_close(int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return -2;

    for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_filefd[k] == fd)
        {
            packfs_filefd[k] = 0;
            packfs_filesize[k] = 0;
            int res = fclose(packfs_fileptr[k]);
            packfs_fileptr[k] = NULL;
            return res;
        }
    }
    return -2;
}

void* packfs_find(int fd, FILE* ptr)
{
    if(ptr != NULL)
    {
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_fileptr[k] == ptr)
                return &packfs_filefd[k];
        }
        return NULL;
    }
    else
    {
        if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
            return NULL;
        
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_filefd[k] == fd)
                return packfs_fileptr[k];
        }
    }
    return NULL;
}

ssize_t packfs_read(int fd, void* buf, size_t count)
{
    FILE* ptr = packfs_find(fd, NULL);
    if(!ptr)
        return -1;
    return (ssize_t)fread(buf, 1, count, ptr);
}

int packfs_seek(int fd, long offset, int whence)
{
    FILE* ptr = packfs_find(fd, NULL);
    if(!ptr)
        return -1;
    return fseek(ptr, offset, whence);
}

int packfs_access(const char* path)
{
    const char* path_sanitized = packfs_sanitize_path(path);

    if(0 == packfs_strncmp(packfs_builtin_prefix, path_sanitized, strlen(packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_builtin_abspaths[i]))
                return 0;
        }
        return -1;
    }
    
    return -2;
}

int packfs_stat(const char* path, int fd, struct stat *restrict statbuf)
{
    const char* path_sanitized = packfs_sanitize_path(path);
    
    if(0 == packfs_strncmp(packfs_builtin_prefix, path_sanitized, strlen(packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_builtin_abspaths[i]))
            {
                *statbuf = (struct stat){0};
                statbuf->st_size = (off_t)(packfs_builtin_ends[i] - packfs_builtin_starts[i]);
                statbuf->st_mode = S_IFREG;
                return 0;
            }
        }
        for(size_t i = 0; i < packfs_builtin_dirs_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_builtin_abspaths_dirs[i]))
            {
                *statbuf = (struct stat){0};
                statbuf->st_size = 0;
                statbuf->st_mode = S_IFDIR;
                return 0;
            }
        }
        return -1;
    }
    
    if(fd >= 0 && packfs_filefd_min <= fd && fd < packfs_filefd_max)
    {
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_filefd[k] == fd)
            {
                *statbuf = (struct stat){0};
                statbuf->st_size = packfs_filesize[k];
                statbuf->st_mode = S_IFREG;
                return 0;
            }
        }
        return -1;
    }

    return -2;
}

///////////

FILE* __wrap_fopen(const char *path, const char *mode)
{
    if(packfs_enabled)
    {
        FILE* res = NULL;
        if(packfs_open(path, &res) >= 0)
        {
            return res;
        }
    }

    FILE* res = __real_fopen(path, mode);
    return res;
}

int __wrap_fileno(FILE *stream)
{
    int res = __real_fileno(stream);
    
    if(packfs_enabled && res < 0)
    {        
        int* ptr = packfs_find(-1, stream);
        res = ptr == NULL ? -1 : (*ptr);
    }
    
    return res;
}

int __wrap_open(const char *path, int flags, ...)
{
    if(packfs_enabled)
    {
        int res = packfs_open(path, NULL);
        if(res >= 0)
        { 
            return res;
        }
    }
    
    int res = __real_open(path, flags);
    return res;
}

int __wrap_close(int fd)
{
    if(packfs_enabled)
    {
        int res = packfs_close(fd);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = __real_close(fd);
    return res;
}


ssize_t __wrap_read(int fd, void* buf, size_t count)
{
    if(packfs_enabled)
    {
        ssize_t res = packfs_read(fd, buf, count);
        if(res >= 0)
        {
            return res;
        }
    }

    ssize_t res = __real_read(fd, buf, count);
    return res;
}

off_t __wrap_lseek(int fd, off_t offset, int whence)
{
    if(packfs_enabled)
    {
        int res = packfs_seek(fd, (long)offset, whence);
        if(res >= 0)
        {
            return res;
        }
    }

    off_t res = __real_lseek(fd, offset, whence);
    return res;
}


int __wrap_access(const char *path, int flags) 
{
    if(packfs_enabled)
    {
        int res = packfs_access(path);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = __real_access(path, flags); 
    return res;
}

int __wrap_stat(const char *restrict path, struct stat *restrict statbuf)
{
    if(packfs_enabled)
    {
        int res = packfs_stat(path, -1, statbuf);
        if(res >= -1)
        {
            return res;
        }
    }

    int res = __real_stat(path, statbuf);
    return res;
}

int __wrap_fstat(int fd, struct stat * statbuf)
{
    if(packfs_enabled)
    {
        int res = packfs_stat(NULL, fd, statbuf);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = __real_fstat(fd, statbuf);
    return res;
}

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

// #include <xsinit.c>
void xs_init(pTHX) //EXTERN_C 
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;
    
    extern void boot_DynaLoader(pTHX_ CV* cv);                  newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
   
#ifdef PERLPACK_mro
    extern void boot_mro(pTHX_ CV* cv);                         newXS("mro::bootstrap", boot_mro, file);
#endif
#ifdef PERLPACK_Devel__Peek
    extern void boot_Devel__Peek(pTHX_ CV* cv);                 newXS("Devel::Peek", boot_Devel__Peek, file);
#endif
#ifdef PERLPACK_File__DosGlob
    extern void boot_File__DosGlob(pTHX_ CV* cv);               newXS("File::DosGlob::bootstrap", boot_File__DosGlob, file);
#endif
#ifdef PERLPACK_File__Glob
    extern void boot_File__Glob(pTHX_ CV* cv);                  newXS("File::Glob::bootstrap", boot_File__Glob, file);
#endif
#ifdef PERLPACK_Sys__Syslog
    extern void boot_Sys__Syslog(pTHX_ CV* cv);                 newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
#endif
#ifdef PERLPACK_Sys__Hostname
    extern void boot_Sys__Hostname(pTHX_ CV* cv);               newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
#endif
#ifdef PERLPACK_PerlIO__via
    extern void boot_PerlIO__via(pTHX_ CV* cv);                 newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
#endif
#ifdef PERLPACK_PerlIO__mmap
    extern void boot_PerlIO__mmap(pTHX_ CV* cv);                newXS("PerlIO::mmap::bootstrap", boot_PerlIO__mmap, file);
#endif
#ifdef PERLPACK_PerlIO__encoding
    extern void boot_PerlIO__encoding(pTHX_ CV* cv);            newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
#endif
#ifdef PERLPACK_B
    extern void boot_B(pTHX_ CV* cv);                           newXS("B::bootstrap", boot_B, file);
#endif
#ifdef PERLPACK_attributes
    extern void boot_attributes(pTHX_ CV* cv);                  newXS("attributes::bootstrap", boot_attributes, file);
#endif
#ifdef PERLPACK_Unicode__Normalize
    extern void boot_Unicode__Normalize(pTHX_ CV* cv);          newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
#endif
#ifdef PERLPACK_Unicode__Collate
    extern void boot_Unicode__Collate(pTHX_ CV* cv);            newXS("Unicode::Collate::bootstrap", boot_Unicode__Collate, file);
#endif
#ifdef PERLPACK_threads
    extern void boot_threads(pTHX_ CV* cv);                     newXS("threads::bootstrap", boot_threads, file);
#endif
#ifdef PERLPACK_threads__shared
    extern void boot_threads__shared(pTHX_ CV* cv);             newXS("threads::shared::bootstrap", boot_threads__shared, file);
#endif
#ifdef PERLPACK_IPC__SysV
    extern void boot_IPC__SysV(pTHX_ CV* cv);                   newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
#endif
#ifdef PERLPACK_re
    extern void boot_re(pTHX_ CV* cv);                          newXS("re::bootstrap", boot_re, file);
#endif
#ifdef PERLPACK_Digest__MD5
    extern void boot_Digest__MD5(pTHX_ CV* cv);                 newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
#endif
#ifdef PERLPACK_Digest__SHA
    extern void boot_Digest__SHA(pTHX_ CV* cv);                 newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
#endif
#ifdef PERLPACK_SDBM_File
    extern void boot_SDBM_File(pTHX_ CV* cv);                   newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
#endif
#ifdef PERLPACK_Math__BigInt__FastCalc
    extern void boot_Math__BigInt__FastCalc(pTHX_ CV* cv);      newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
#endif
#ifdef PERLPACK_Data__Dumper
    extern void boot_Data__Dumper(pTHX_ CV* cv);                newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
#endif
#ifdef PERLPACK_I18N__Langinfo
    extern void boot_I18N__Langinfo(pTHX_ CV* cv);              newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
#endif
#ifdef PERLPACK_Time__HiRes
    extern void boot_Time__HiRes(pTHX_ CV* cv);                 newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
#endif
#ifdef PERLPACK_Time__Piece
    extern void boot_Time__Piece(pTHX_ CV* cv);                 newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
#endif
#ifdef PERLPACK_IO
    extern void boot_IO(pTHX_ CV* cv);                          newXS("IO::bootstrap", boot_IO, file);
#endif
#ifdef PERLPACK_Socket
    extern void boot_Socket(pTHX_ CV* cv);                      newXS("Socket::bootstrap", boot_Socket, file);
#endif
#ifdef PERLPACK_Hash__Util__FieldHash
    extern void boot_Hash__Util__FieldHash(pTHX_ CV* cv);       newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file); 
#endif
#ifdef PERLPACK_Hash__Util
    extern void boot_Hash__Util(pTHX_ CV* cv);                  newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
#endif
#ifdef PERLPACK_Filter__Util__Call
    extern void boot_Filter__Util__Call(pTHX_ CV* cv);          newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
#endif
#ifdef PERLPACK_POSIX
    extern void boot_POSIX(pTHX_ CV* cv);                       newXS("POSIX::bootstrap", boot_POSIX, file);
#endif
#ifdef PERLPACK_Encode__Unicode
    extern void boot_Encode__Unicode(pTHX_ CV* cv);             newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
#endif
#ifdef PERLPACK_Encode
    extern void boot_Encode(pTHX_ CV* cv);                      newXS("Encode::bootstrap", boot_Encode, file);
#endif
#ifdef PERLPACK_Encode__JP
    extern void boot_Encode__JP(pTHX_ CV* cv);                  newXS("Encode::JP::bootstrap", boot_Encode__JP, file);
#endif
#ifdef PERLPACK_Encode__KR
    extern void boot_Encode__KR(pTHX_ CV* cv);                  newXS("Encode::KR::bootstrap", boot_Encode__KR, file);
#endif
#ifdef PERLPACK_Encode__EBCDIC
    extern void boot_Encode__EBCDIC(pTHX_ CV* cv);              newXS("Encode::EBCDIC::bootstrap", boot_Encode__EBCDIC, file);
#endif
#ifdef PERLPACK_Encode__CN
    extern void boot_Encode__CN(pTHX_ CV* cv);                  newXS("Encode::CN::bootstrap", boot_Encode__CN, file);
#endif
#ifdef PERLPACK_Encode__Symbol
    extern void boot_Encode__Symbol(pTHX_ CV* cv);              newXS("Encode::Symbol::bootstrap", boot_Encode__Symbol, file);
#endif
#ifdef PERLPACK_Encode__Byte
    extern void boot_Encode__Byte(pTHX_ CV* cv);                newXS("Encode::Byte::bootstrap", boot_Encode__Byte, file);
#endif
#ifdef PERLPACK_Encode__TW
    extern void boot_Encode__TW(pTHX_ CV* cv);                  newXS("Encode::TW::bootstrap", boot_Encode__TW, file);
#endif
#ifdef PERLPACK_Compress__Raw__Zlib
    extern void boot_Compress__Raw__Zlib(pTHX_ CV* cv);         newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
#endif
#ifdef PERLPACK_Compress__Raw__Bzip2
    extern void boot_Compress__Raw__Bzip2(pTHX_ CV* cv);        newXS("Compress::Raw::Bzip2::bootstrap", boot_Compress__Raw__Bzip2, file);
#endif
#ifdef PERLPACK_MIME__Base64
    extern void boot_MIME__Base64(pTHX_ CV* cv);                newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
#endif
#ifdef PERLPACK_Cwd
    extern void boot_Cwd(pTHX_ CV* cv);                         newXS("Cwd::bootstrap", boot_Cwd, file);
#endif
#ifdef PERLPACK_Storable
    extern void boot_Storable(pTHX_ CV* cv);                    newXS("Storable::bootstrap", boot_Storable, file);
#endif
#ifdef PERLPACK_List__Util
    extern void boot_List__Util(pTHX_ CV* cv);                  newXS("List::Util::bootstrap", boot_List__Util, file);
#endif
#ifdef PERLPACK_Fcntl
    extern void boot_Fcntl(pTHX_ CV* cv);                       newXS("Fcntl::bootstrap", boot_Fcntl, file);
#endif
#ifdef PERLPACK_Opcode
    extern void boot_Opcode(pTHX_ CV* cv);                      newXS("Opcode::bootstrap", boot_Opcode, file);
#endif
}

int main(int argc, char *argv[], char* envp[])
{
    static char script[1 << 20] = "print('Hello world! Need more arguments!\n');";
    extern char _binary_myscript_pl_start[];
    extern char _binary_myscript_pl_end[];

    if(argc < 1)
        return -1;
    else if(argc > 1 && 0 == strcmp("myscript.pl", argv[1]))
    {
        size_t iSize = _binary_myscript_pl_end - _binary_myscript_pl_start;
        strncpy(script, _binary_myscript_pl_start, iSize);
        script[iSize] = '\0';
    }
    else if(argc > 2 && 0 == strcmp("-e", argv[1]))
    {
        strcpy(script, argv[2]);
    }
    
    packfs_enabled = 1;

    PERL_SYS_INIT3(&argc, &argv, &envp);
    PerlInterpreter* myperl = perl_alloc();
    if(myperl == NULL)
        return -1;

    perl_construct(myperl);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    char *myperl_argv[] = { "perlpack", "-e", script, "--", argv[2], NULL };
    int myperl_argc = sizeof(myperl_argv) / sizeof(myperl_argv[0]) - 1;
    int res = perl_parse(myperl, xs_init, myperl_argc, myperl_argv, envp);
    if(res == 0)
        res = perl_run(myperl); // error if res != 0 (or stricter in case exit(0) was called from perl code): (res & 0xFF) != 0: 

    PL_perl_destruct_level = 0;
    res = perl_destruct(myperl);
    perl_free(myperl);
    PERL_SYS_TERM();

    return res;
}
