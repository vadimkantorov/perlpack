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
// #include <xsinit.c>

extern void boot_DynaLoader (pTHX_ CV* cv);

extern void boot_mro(pTHX_ CV* cv);
extern void boot_Devel__Peek(pTHX_ CV* cv);
extern void boot_File__DosGlob(pTHX_ CV* cv);
extern void boot_File__Glob(pTHX_ CV* cv);
extern void boot_Sys__Syslog(pTHX_ CV* cv);
extern void boot_Sys__Hostname(pTHX_ CV* cv);
extern void boot_PerlIO__via(pTHX_ CV* cv);
extern void boot_PerlIO__mmap(pTHX_ CV* cv);
extern void boot_PerlIO__encoding(pTHX_ CV* cv);
extern void boot_PerlIO__scalar(pTHX_ CV* cv);
extern void boot_B(pTHX_ CV* cv);
extern void boot_attributes(pTHX_ CV* cv);
extern void boot_Unicode__Normalize(pTHX_ CV* cv);
extern void boot_Unicode__Collate(pTHX_ CV* cv);
extern void boot_threads(pTHX_ CV* cv);
extern void boot_threads__shared(pTHX_ CV* cv);
extern void boot_IPC__SysV(pTHX_ CV* cv);
extern void boot_re(pTHX_ CV* cv);
extern void boot_Digest__MD5(pTHX_ CV* cv);
extern void boot_Digest__SHA(pTHX_ CV* cv);
extern void boot_SDBM_File(pTHX_ CV* cv);
extern void boot_Math__BigInt__FastCalc(pTHX_ CV* cv);
extern void boot_Data__Dumper(pTHX_ CV* cv);
extern void boot_I18N__Langinfo(pTHX_ CV* cv);
extern void boot_Time__HiRes(pTHX_ CV* cv);
extern void boot_Time__Piece(pTHX_ CV* cv);
extern void boot_IO(pTHX_ CV* cv);
extern void boot_Socket(pTHX_ CV* cv);
extern void boot_Hash__Util__FieldHash(pTHX_ CV* cv);
extern void boot_Hash__Util(pTHX_ CV* cv);
extern void boot_Filter__Util__Call(pTHX_ CV* cv);
extern void boot_POSIX(pTHX_ CV* cv);
extern void boot_Encode__Unicode(pTHX_ CV* cv);
extern void boot_Encode(pTHX_ CV* cv);
extern void boot_Encode__JP(pTHX_ CV* cv);
extern void boot_Encode__KR(pTHX_ CV* cv);
extern void boot_Encode__EBCDIC(pTHX_ CV* cv);
extern void boot_Encode__CN(pTHX_ CV* cv);
extern void boot_Encode__Symbol(pTHX_ CV* cv);
extern void boot_Encode__Byte(pTHX_ CV* cv);
extern void boot_Encode__TW(pTHX_ CV* cv);
extern void boot_Compress__Raw__Zlib(pTHX_ CV* cv);
extern void boot_Compress__Raw__Bzip2(pTHX_ CV* cv);
extern void boot_MIME__Base64(pTHX_ CV* cv);
extern void boot_Cwd(pTHX_ CV* cv);
extern void boot_Storable(pTHX_ CV* cv);
extern void boot_List__Util(pTHX_ CV* cv);
extern void boot_Fcntl(pTHX_ CV* cv);
extern void boot_Opcode(pTHX_ CV* cv);




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
    newXS("Fcntl::bootstrap", boot_Fcntl, file);
    newXS("IO::bootstrap", boot_IO, file);
    newXS("Cwd::bootstrap", boot_Cwd, file);
    newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
    

    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}
///////////////////////////////////////


static char script[1 << 20] = "print('Hello world! Need more arguments!');";

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
