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

#zipsfx.zip:
#	zip -0 $@ zipsfx.c Makefile

#zipsfxzip: zipsfx zipsfx.zip
#	cat zipsfx zipsfx.zip > $@
#	chmod +x $@
