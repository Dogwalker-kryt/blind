CC      := gcc
VERSION := $(shell git describe --abbrev=0 --always --tags)
CFLAGS  += -DVERSION=\"$(VERSION)\"

TRG := blind
SRC := blind.c

all: $(SRC)
	$(CC) $(CFLAGS) -o $(TRG) $(SRC)

cross-compile: $(SRC)
	@command -v zig >/dev/null || { echo "zig not exists" && exit 1; }
	@for target in x86_64-linux-musl aarch64-linux-musl x86_64-macos aarch64-macos; do \
		$(MAKE) --no-print-directory CC="zig cc -target $$target" TRG=$$target || exit 1; \
		$(RM) $$target; \
	done

clean:
	$(RM) $(TRG)
