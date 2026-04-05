all: compile strip compress

compile:
	musl-clang laim.c -o laim -std=c23 -Oz -flto -funroll-loops -fmerge-all-constants -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-ident -fno-plt -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none -Wl,-z,norelro -Wno-format -Wno-#warnings -static -Wl,--hash-style=gnu -Wl,-z,noseparate-code -Wno-c23-extensions
strip:
	sstrip laim
compress:
	upx --ultra-brute laim

.PHONY: all compile strip compress
