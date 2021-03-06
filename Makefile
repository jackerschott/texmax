TARGET = vimax
INCS = -I.
LIBS = 

CFLAGS = $(INCS)
LDFLAGS = $(LIBS)

include config.mk

first: target

target: objdir main.o parse.o write.o str.o util.o
	$(CC) -o texmax obj/main.o obj/parse.o obj/write.o obj/str.o obj/util.o $(LDFLAGS)
clean:
	rm -f texmax
	rm -f test/testparse
	rm -f test/teststr
	rm -rf obj

objdir:
	mkdir -p obj

main.o: main.c
	$(CC) -g -c -o obj/main.o main.c $(CFLAGS)
parse.o: parse.c
	$(CC) -g -c -o obj/parse.o parse.c $(CFLAGS)
write.o: write.c
	$(CC) -g -c -o obj/write.o write.c $(CFLAGS)
str.o: str.c
	$(CC) -g -c -o obj/str.o str.c $(CFLAGS)
util.o: util.c
	$(CC) -g -c -o obj/util.o util.c $(CFLAGS)

install: target
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f texmax $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/texmax
	mkdir -p $(DESTDIR)$(PREFIX)/share/texmax
	cp -f init.lisp $(DESTDIR)$(PREFIX)/share/texmax
	cp -f init.mac $(DESTDIR)$(PREFIX)/share/texmax
	chmod 644 $(DESTDIR)$(PREFIX)/share/texmax/init.lisp
	chmod 644 $(DESTDIR)$(PREFIX)/share/texmax/init.mac
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/vimax
	rm -f $(DESTDIR)$(PREFIX)/share/texmax/init.lisp
	rm -f $(DESTDIR)$(PREFIX)/share/texmax/init.mac
	rmdir $(DESTDIR)$(PREFIX)/share/texmax
