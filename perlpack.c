#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
//#include <fcntl.h>
#include <stdarg.h>

#include <sys/stat.h>

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

///////////////////////////////////////
#include "perlpack.c"
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
    //newXS("Fcntl::bootstrap", boot_Fcntl, file);
    //newXS("IO::bootstrap", boot_IO, file);
    //newXS("Cwd::bootstrap", boot_Cwd, file);
    //newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
   
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


static char script[1 << 20] = "print('Hello world! Need more arguments!\n');";

extern char _binary_myscript_pl_start[];
extern char _binary_myscript_pl_end[];

int main(int argc, char **argv)
{
    PERL_SYS_INIT3(&argc, &argv, NULL);
    PerlInterpreter* my_perl = perl_alloc();
    perl_construct(my_perl);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

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

    char *one_args[] = { "perlpack", "-e", script, "--", argv[2], NULL };
    perl_parse(my_perl, xs_init, 5, one_args, (char **)NULL);
    perl_run(my_perl);
    perl_destruct(my_perl);
    perl_free(my_perl);
    PERL_SYS_TERM();

    return 0;
}
