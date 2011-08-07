# makefile of pianobarfly

PREFIX:=/usr/local
BINDIR:=${PREFIX}/bin
LIBDIR:=${PREFIX}/lib
INCDIR:=${PREFIX}/include
MANDIR:=${PREFIX}/share/man
DYNLINK:=0

# Respect environment variables set by user; does not work with :=
ifeq (${CFLAGS},)
	CFLAGS=-O2 -DNDEBUG
endif
ifeq (${CC},cc)
	CC=c99
endif

PIANOBAR_DIR=src
PIANOBAR_SRC=\
		${PIANOBAR_DIR}/main.c \
		${PIANOBAR_DIR}/player.c \
		${PIANOBAR_DIR}/settings.c \
		${PIANOBAR_DIR}/terminal.c \
		${PIANOBAR_DIR}/ui_act.c \
		${PIANOBAR_DIR}/ui.c \
		${PIANOBAR_DIR}/ui_readline.c \
		${PIANOBAR_DIR}/ui_dispatch.c \
		${PIANOBAR_DIR}/fly.c \
		${PIANOBAR_DIR}/fly_id3.c \
		${PIANOBAR_DIR}/fly_misc.c \
		${PIANOBAR_DIR}/fly_mp4.c
PIANOBAR_HDR=\
		${PIANOBAR_DIR}/player.h \
		${PIANOBAR_DIR}/settings.h \
		${PIANOBAR_DIR}/terminal.h \
		${PIANOBAR_DIR}/ui_act.h \
		${PIANOBAR_DIR}/ui.h \
		${PIANOBAR_DIR}/ui_readline.h \
		${PIANOBAR_DIR}/main.h \
		${PIANOBAR_DIR}/config.h \
		${PIANOBAR_DIR}/fly.h \
		${PIANOBAR_DIR}/fly_id3.h \
		${PIANOBAR_DIR}/fly_misc.h \
		${PIANOBAR_DIR}/fly_mp4.h
PIANOBAR_OBJ=${PIANOBAR_SRC:.c=.o}

LIBPIANO_DIR=src/libpiano
LIBPIANO_SRC=\
		${LIBPIANO_DIR}/crypt.c \
		${LIBPIANO_DIR}/piano.c \
		${LIBPIANO_DIR}/xml.c
LIBPIANO_HDR=\
		${LIBPIANO_DIR}/config.h \
		${LIBPIANO_DIR}/crypt_key_output.h \
		${LIBPIANO_DIR}/xml.h \
		${LIBPIANO_DIR}/crypt.h \
		${LIBPIANO_DIR}/piano.h \
		${LIBPIANO_DIR}/crypt_key_input.h \
		${LIBPIANO_DIR}/piano_private.h
LIBPIANO_OBJ=${LIBPIANO_SRC:.c=.o}
LIBPIANO_RELOBJ=${LIBPIANO_SRC:.c=.lo}
LIBPIANO_INCLUDE=${LIBPIANO_DIR}

LIBWAITRESS_DIR=src/libwaitress
LIBWAITRESS_SRC=${LIBWAITRESS_DIR}/waitress.c
LIBWAITRESS_HDR=\
		${LIBWAITRESS_DIR}/config.h \
		${LIBWAITRESS_DIR}/waitress.h
LIBWAITRESS_OBJ=${LIBWAITRESS_SRC:.c=.o}
LIBWAITRESS_RELOBJ=${LIBWAITRESS_SRC:.c=.lo}
LIBWAITRESS_INCLUDE=${LIBWAITRESS_DIR}

LIBEZXML_DIR=src/libezxml
LIBEZXML_SRC=${LIBEZXML_DIR}/ezxml.c
LIBEZXML_HDR=${LIBEZXML_DIR}/ezxml.h
LIBEZXML_OBJ=${LIBEZXML_SRC:.c=.o}
LIBEZXML_RELOBJ=${LIBEZXML_SRC:.c=.lo}
LIBEZXML_INCLUDE=${LIBEZXML_DIR}

ifeq (${DISABLE_FAAD}, 1)
	LIBFAAD_CFLAGS=
	LIBFAAD_LDFLAGS=
else
	LIBFAAD_CFLAGS=-DENABLE_FAAD
	LIBFAAD_LDFLAGS=-lfaad
endif

ifeq (${DISABLE_MAD}, 1)
	LIBMAD_CFLAGS=
	LIBMAD_LDFLAGS=
else
	LIBMAD_CFLAGS=-DENABLE_MAD
	LIBMAD_LDFLAGS=-lmad
endif

ifeq (${DISABLE_ID3TAG}, 1)
	LIBID3TAG_CFLAGS=
	LIBID3TAG_LDFLAGS=
else
	LIBID3TAG_CFLAGS=-DENABLE_ID3TAG
	LIBID3TAG_LDFLAGS=-lid3tag
endif

# build pianobarfly
ifeq (${DYNLINK},1)
pianobarfly: ${PIANOBAR_OBJ} ${PIANOBAR_HDR} libpiano.so.0
	${CC} -o $@ ${PIANOBAR_OBJ} ${LDFLAGS} -lao -lpthread -lm -L. -lpiano \
			${LIBFAAD_LDFLAGS} ${LIBMAD_LDFLAGS}
else
pianobarfly: ${PIANOBAR_OBJ} ${PIANOBAR_HDR} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} \
		${LIBWAITRESS_HDR} ${LIBEZXML_OBJ} ${LIBEZXML_HDR}
	${CC} ${CFLAGS} ${LDFLAGS} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} \
			${LIBWAITRESS_OBJ} ${LIBEZXML_OBJ} -lao -lpthread -lm \
			${LIBFAAD_LDFLAGS} ${LIBMAD_LDFLAGS} ${LIBID3TAG_LDFLAGS} -o $@
endif

# build shared and static libpiano
libpiano.so.0: ${LIBPIANO_RELOBJ} ${LIBPIANO_HDR} ${LIBWAITRESS_RELOBJ} \
		${LIBWAITRESS_HDR} ${LIBEZXML_RELOBJ} ${LIBEZXML_HDR} \
		${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} ${LIBEZXML_OBJ}
	${CC} -shared -Wl,-soname,libpiano.so.0 ${CFLAGS} ${LDFLAGS} \
			-o libpiano.so.0.0.0 ${LIBPIANO_RELOBJ} \
			${LIBWAITRESS_RELOBJ} ${LIBEZXML_RELOBJ}
	ln -s libpiano.so.0.0.0 libpiano.so.0
	ln -s libpiano.so.0 libpiano.so
	${AR} rcs libpiano.a ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} ${LIBEZXML_OBJ}

%.o: %.c
	${CC} ${CFLAGS} -I ${LIBPIANO_INCLUDE} -I ${LIBWAITRESS_INCLUDE} \
			-I ${LIBEZXML_INCLUDE} ${LIBFAAD_CFLAGS} \
			${LIBMAD_CFLAGS} ${LIBID3TAG_CFLAGS} -c -o $@ $<

# create position independent code (for shared libraries)
%.lo: %.c
	${CC} ${CFLAGS} -I ${LIBPIANO_INCLUDE} -I ${LIBWAITRESS_INCLUDE} \
			-I ${LIBEZXML_INCLUDE} -c -fPIC -o $@ $<

clean:
	${RM} ${PIANOBAR_OBJ} ${LIBPIANO_OBJ} ${LIBWAITRESS_OBJ} ${LIBWAITRESS_OBJ}/test.o \
			${LIBEZXML_OBJ} ${LIBPIANO_RELOBJ} ${LIBWAITRESS_RELOBJ} \
			${LIBEZXML_RELOBJ} pianobarfly libpiano.so* libpiano.a waitress-test

all: pianobarfly

debug: pianobarfly
debug: CFLAGS=-Wall -pedantic -ggdb -DDEBUG

waitress-test: CFLAGS+= -DTEST
waitress-test: ${LIBWAITRESS_OBJ}
	${CC} ${LDFLAGS} ${LIBWAITRESS_OBJ} -o waitress-test

test: waitress-test
	./waitress-test

ifeq (${DYNLINK},1)
install: pianobarfly install-libpiano
else
install: pianobarfly
endif
	install -d ${DESTDIR}/${BINDIR}/
	install -m755 pianobarfly ${DESTDIR}/${BINDIR}/
	install -d ${DESTDIR}/${MANDIR}/man1/
	install -m644 contrib/pianobar.1 ${DESTDIR}/${MANDIR}/man1/

install-libpiano:
	install -d ${DESTDIR}/${LIBDIR}/
	install -m644 libpiano.so.0.0.0 ${DESTDIR}/${LIBDIR}/
	ln -s libpiano.so.0.0.0 ${DESTDIR}/${LIBDIR}/libpiano.so.0
	ln -s libpiano.so.0 ${DESTDIR}/${LIBDIR}/libpiano.so
	install -m644 libpiano.a ${DESTDIR}/${LIBDIR}/
	install -d ${DESTDIR}/${INCDIR}/
	install -m644 src/libpiano/piano.h ${DESTDIR}/${INCDIR}/
