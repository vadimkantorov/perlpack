#URLPERL = https://www.cpan.org/src/5.0/perl-5.35.4.tar.gz

OBJCOPY = objcopy
LD = ld

libarchive/.libs/libarchive.a:
	cd libarchive && sh build/autogen.sh && sh configure --without-zlib --without-bz2lib  --without-libb2 --without-iconv --without-lz4  --without-zstd --without-lzma --without-cng  --without-xml2 --without-expat --without-openssl && $(MAKE)

build/libperl.a:
	mkdir -p build
	curl -L ${URLPERL} | tar -xzf - --strip-components=1 --directory=build
	cd build && sh ./Configure -sde -Dman1dir=none -Dman3dir=none -Dprefix=/mnt/perlpack -Dinstallprefix=../packfs -Aldflags=-lm -Accflags=-lm -Dusedevel -Dlibs="-lpthread -ldl -lm -lutil -lc" -Dstatic_ext="${MODULES_ext}" && cd ..
	make -C build miniperl generate_uudmap
	make -C build perl
	make -C build install

libc_perlpack.a:
	cp $(shell $(CC) -print-file-name=libc.a) $@
	$(AR) x $@  open.lo close.lo read.lo stat.lo lseek.lo access.lo fopen.lo fileno.lo
	$(OBJCOPY) --redefine-sym open=orig_open     open.lo
	$(OBJCOPY) --redefine-sym close=orig_close   close.lo
	$(OBJCOPY) --redefine-sym read=orig_read     read.lo
	$(OBJCOPY) --redefine-sym stat=orig_stat     stat.lo
	$(OBJCOPY) --redefine-sym lseek=orig_lseek   lseek.lo
	$(OBJCOPY) --redefine-sym access=orig_access access.lo
	$(OBJCOPY) --redefine-sym fopen=orig_fopen   fopen.lo
	$(OBJCOPY) --redefine-sym fileno=orig_fileno fileno.lo
	$(AR) rs $@ open.lo close.lo read.lo stat.lo lseek.lo access.lo fopen.lo fileno.lo

perlpackstatic:
	$(LD) -r -b binary -o myscript.o myscript.pl
	rm -rf packfs/man packfs/lib/*/pod/
	find packfs -name '*.pod' -o -name '*.ld' -o -name '*.a' -o -name '*.h' -delete
	python perlpack.py -i packfs -o perlpack.h --prefix=/mnt/perlpack/
	#perl perlpack.pl -i packfs -o perlpack.h --prefix=/mnt/perlpack/
	rm -rf packfs
	#cc -o perlpack perlpack.c myscript.o  -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I$PWD/build -I/usr/local/include   -Wl,-E -fstack-protector-strong -fwrapv -fno-strict-aliasing -L/usr/local/lib build/libperl.a   -lpthread -ldl -lm -lutil -lc   $MODULES_def $(printf "build/lib/auto/%s " $MODULES_a)   @perlpack.h.txt
	#./perlpack
	#./perlpack -e 'use Cwd; print(Cwd::cwd(), "\n");'
	# https://stackoverflow.com/questions/10763394/how-to-build-a-c-program-using-a-custom-version-of-glibc-and-static-linking
	zip foo.zip perlpack.c
	$(CC) -o perlpackstatic perlpack.c myscript.o -DPACKFS_STATIC -DPACKFS_LIBARCHIVE -larchive -Llibarchive/.libs -Ilibarchive -Ilibarchive/libarchive   -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I$(PWD)/build -I/usr/local/include   -Wl,-E -fstack-protector-strong -fwrapv -fno-strict-aliasing -L/usr/local/lib build/libperl.a libc_perlpack.a  -lpthread -ldl -lm -lutil --static -static -static-libstdc++ -static-libgcc  $(MODULES_def) $(shell printf "build/lib/auto/%s " $(MODULES_a))   @perlpack.h.txt

#zipsfx.zip:
#	zip -0 $@ zipsfx.c Makefile
#zipsfxzip: zipsfx zipsfx.zip
#	cat zipsfx zipsfx.zip > $@
#	chmod +x $@
