TITLE := PS4 Direct PKG Diagnostic
VERSION := 0.70
TITLE_ID := BREW00974
CONTENT_ID := IV0000-BREW00974_00-PS4DIRECTDIAG070
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
LIBS := -lc -lkernel -lSceNet -lSceSsl -lSceHttp -lSceSysmodule -lSceCommonDialog -lSceMsgDialog
CFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
SDKTOOLS := $(TOOLCHAIN)/bin/linux
.PHONY: all clean
all: $(CONTENT_ID).pkg
build/main.o: native/main.c
	mkdir -p build
	clang $(CFLAGS) -o $@ $<
build/ps4direct.elf: build/main.o
	ld.lld $< -o $@ $(LDFLAGS)
eboot.bin: build/ps4direct.elf
	$(SDKTOOLS)/create-fself -in=$< -out=build/ps4direct.oelf --eboot $@ --paid 0x3800000000000011
sce_sys/param.sfo: Makefile
	$(SDKTOOLS)/PkgTool.Core sfo_new $@
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value $(VERSION)
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value gd
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value $(CONTENT_ID)
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value $(TITLE_ID)
	$(SDKTOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value $(VERSION)
pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx
	$(SDKTOOLS)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"
$(CONTENT_ID).pkg: pkg.gp4
	$(SDKTOOLS)/PkgTool.Core pkg_build $< .
clean:
	rm -rf build eboot.bin pkg.gp4 sce_sys/param.sfo $(CONTENT_ID).pkg
