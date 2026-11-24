# lockbox: builds the 3 small C helpers (no dependencies beyond what CachyOS/Arch already ships).
#   make                 builds into build/
#   make install         installs into ~/.local (PREFIX=~/.local); no root needed
#   make uninstall
# Dependencies (Arch/CachyOS): bubblewrap libseccomp wayland (already there with KDE/GNOME), gcc or clang, make.
PREFIX  ?= $(HOME)/.local
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
WL      := $(shell pkg-config --cflags --libs wayland-client)
BUILD   := build

all: $(BUILD)/lockbox-seccomp $(BUILD)/lockbox-wayland $(BUILD)/lockbox-probe

$(BUILD):
	mkdir -p $@

$(BUILD)/security-context-v1-client.h: protocols/security-context-v1.xml | $(BUILD)
	wayland-scanner client-header $< $@
$(BUILD)/security-context-v1.c: protocols/security-context-v1.xml | $(BUILD)
	wayland-scanner private-code $< $@

$(BUILD)/lockbox-seccomp: src/lockbox-seccomp.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< -lseccomp
$(BUILD)/lockbox-wayland: src/lockbox-wayland.c $(BUILD)/security-context-v1.c $(BUILD)/security-context-v1-client.h
	$(CC) $(CFLAGS) -I$(BUILD) -o $@ src/lockbox-wayland.c $(BUILD)/security-context-v1.c $(WL)
$(BUILD)/lockbox-probe: src/lockbox-probe.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(WL)

install: all
	install -Dm755 bin/lockbox $(PREFIX)/bin/lockbox
	install -Dm644 lib/x11-private.sh $(PREFIX)/lib/lockbox/x11-private.sh
	install -Dm755 $(BUILD)/lockbox-seccomp $(PREFIX)/lib/lockbox/lockbox-seccomp
	install -Dm755 $(BUILD)/lockbox-wayland $(PREFIX)/lib/lockbox/lockbox-wayland
	install -Dm755 $(BUILD)/lockbox-probe $(PREFIX)/lib/lockbox/lockbox-probe

uninstall:
	rm -f $(PREFIX)/bin/lockbox
	rm -rf $(PREFIX)/lib/lockbox

clean:
	rm -rf $(BUILD)

.PHONY: all install uninstall clean
