TARGET = vimax
INCS = -I.
LIBS = 

include config.mk

first: target

target: objdir main.o parse.o str.o util.o
	gcc -o $(TARGET) obj/main.o obj/parse.o obj/str.o obj/util.o $(LIBS)

objdir:
	mkdir -p obj

main.o: main.c
	gcc -g -c -o obj/main.o main.c $(INCS)
parse.o: parse.c
	gcc -g -c -o obj/parse.o parse.c $(INCS)
str.o: str.c
	gcc -g -c -o obj/str.o str.c $(INCS)
util.o: util.c
	gcc -g -c -o obj/util.o util.c $(INCS)

clean:
	rm -f vimax
	rm -rf obj/*
	rmdir obj

install: target
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f vimax $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/vimax

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/vimax
