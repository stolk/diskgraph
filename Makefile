CC ?= cc
CFLAGS += -D_POSIX_C_SOURCE=199309L -std=c99 -Wall -Wextra
LDFLAGS +=

TARGET = diskgraph
SRC = diskgraph.c
OBJ = $(SRC:.c=.o)

DISTFILES=\
Makefile \
diskgraph.c \
diskgraph.1 \
README.md \
LICENSE \
images \
debian

all:	$(TARGET)

$(TARGET):	$(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o $(TARGET)
	@echo All clean

install: diskgraph
	install -d ${DESTDIR}/usr/bin
	install -m 755 diskgraph ${DESTDIR}/usr/bin/

uninstall:
	rm -f ${DESTDIR}/usr/bin/distgraph

tarball:
	tar cvzf ../diskgraph_1.1.orig.tar.gz $(DISTFILES)

packageupload:
	debuild -S
	debsign ../diskgraph_1.1-1_source.changes
	dput ppa:b-stolk/ppa ../diskgraph_1.1-1_source.changes

