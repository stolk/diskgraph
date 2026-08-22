CC ?= cc
CFLAGS += -D_POSIX_C_SOURCE=199309L -std=c99 -Wall -Wextra
LDFLAGS += -lm

TARGET = diskgraph
MANPAGE = $(TARGET).1
SRC = diskgraph.c
OBJ = $(SRC:.c=.o)

DISTFILES=\
Makefile \
diskgraph.c \
diskgraph.1 \
README.md \
LICENSE \
images \
completions \
debian


.PHONY: all clean install uninstall tarball packageupload

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	@echo All clean

install: diskgraph
	install -d ${DESTDIR}/usr/bin
	install -m 755 diskgraph ${DESTDIR}/usr/bin/
	install -d ${DESTDIR}/usr/share/bash-completion/completions
	install -m 644 completions/bash/diskgraph ${DESTDIR}/usr/share/bash-completion/completions/
	install -d ${DESTDIR}/usr/share/zsh/vendor-completions
	install -m 644 completions/zsh/_diskgraph ${DESTDIR}/usr/share/zsh/vendor-completions/
	install -d ${DESTDIR}/usr/share/fish/vendor_completions.d
	install -m 644 completions/fish/diskgraph.fish ${DESTDIR}/usr/share/fish/vendor_completions.d/

uninstall:
	rm -f ${DESTDIR}/usr/bin/diskgraph
	rm -f ${DESTDIR}/usr/share/bash-completion/completions/diskgraph
	rm -f ${DESTDIR}/usr/share/zsh/vendor-completions/_diskgraph
	rm -f ${DESTDIR}/usr/share/fish/vendor_completions.d/diskgraph.fish

tarball:
	tar cvzf ../diskgraph_1.3.orig.tar.gz $(DISTFILES)

packageupload:
	debuild -S
	debsign ../diskgraph_1.3-1_source.changes
	dput --force ppa:b-stolk/ppa ../diskgraph_1.2-1_source.changes

