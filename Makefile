#zipsfx: libarchive/.libs/libarchive.a
#	cc -static -Wall -o $@ zipsfx.c -D ZIPSFX_USE_MMAP -larchive -Llibarchive/.libs -Ilibarchive -Ilibarchive/libarchive

#URLPERL = https://www.cpan.org/src/5.0/perl-5.35.4.tar.gz

libarchive/.libs/libarchive.a:
	cd libarchive && sh build/autogen.sh && sh configure --without-zlib --without-bz2lib  --without-libb2 --without-iconv --without-lz4  --without-zstd --without-lzma --without-cng  --without-xml2 --without-expat --without-openssl && $(MAKE)

build/libperl.a:
	mkdir -p build
	wget -nc ${URLPERL}
	tar -xf $(shell basename ${URLPERL}) --strip-components=1 --directory=build
	cd build && sh ./Configure -sde -Dman1dir=none -Dman3dir=none -Dprefix=/mnt/perlpack -Dinstallprefix=../packfs -Aldflags=-lm -Accflags=-lm -Dusedevel -Dlibs="-lpthread -ldl -lm -lutil -lc" -Dstatic_ext="${MODULES_ext}" && cd ..
	make -C build miniperl generate_uudmap
	make -C build perl
	make -C build install

libc_perlpack.a:
	cp $(shell cc -print-file-name=libc.a) libc_perlpack.a
	ar x libc_perlpack.a  open.lo close.lo read.lo stat.lo lseek.lo access.lo fopen.lo fileno.lo
	objcopy --redefine-sym open=orig_open     open.lo
	objcopy --redefine-sym close=orig_close   close.lo
	objcopy --redefine-sym read=orig_read     read.lo
	objcopy --redefine-sym stat=orig_stat     stat.lo
	objcopy --redefine-sym lseek=orig_lseek   lseek.lo
	objcopy --redefine-sym access=orig_access access.lo
	objcopy --redefine-sym fopen=orig_fopen   fopen.lo
	objcopy --redefine-sym fileno=orig_fileno fileno.lo
	ar rs libc_perlpack.a open.lo close.lo read.lo stat.lo lseek.lo access.lo fopen.lo fileno.lo

#zipsfx.zip:
#	zip -0 $@ zipsfx.c Makefile

#zipsfxzip: zipsfx zipsfx.zip
#	cat zipsfx zipsfx.zip > $@
#	chmod +x $@
