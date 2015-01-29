# Makefile for xplage
#
# Last edit: 02-Dec-11 by CK
#
# Default is to build for Solaris with Sun's Forte compiler
#
# gcc		build for Solaris with gcc
# linux		build for Linux with gcc
# nt		build for Windows NT with Cygwin gcc
# osx		build for Apple OS X with gcc
#

xplage:		xplage.c
			cc -O -D_POSIX_PTHREAD_SEMANTICS -lsocket -lnsl -mt -s -o xplage xplage.c

gcc:		xplage.c
			gcc -std=c99 -O2 -D_POSIX_PTHREAD_SEMANTICS -lsocket -lnsl -pthreads -s -o xplage xplage.c

linux:		linux-xplage.c strlcat.c
			-gcc -O2 -pthread -s -o xplage linux-xplage.c strlcat.c
			@rm linux-xplage.c

linux-xplage.c:	xplage.c
			@./linux-mk >linux-xplage.c

nt:		nt-xplage.c
			-gcc -std=c99 -O2 -s -o xplage nt-xplage.c
			@rm nt-xplage.c

nt-xplage.c:	xplage.c
			@./nt-mk >nt-xplage.c

osx:		osx-xplage.c
			-gcc -arch ppc -arch i386 -mmacosx-version-min=10.1 -std=c99 -O2 -o xplage osx-xplage.c
			@rm osx-xplage.c

osx-xplage.c:	xplage.c
			@./osx-mk >osx-xplage.c

lint:		xplage.c
			lint -D_POSIX_PTHREAD_SEMANTICS xplage.c

pdfmans:	xplage1.pdf track2kml1.pdf xplage4.pdf

xplage1.pdf:	man/man1/xplage.1
			TCAT=/usr/lib/lp/postscript/dpost man -s1 -t -M `pwd`/man/ xplage | ps2pdf14 - xplage.1.pdf

xplage4.pdf:	man/man4/xplage.4
			TCAT=/usr/lib/lp/postscript/dpost man -s4 -t -M `pwd`/man/ xplage | ps2pdf14 - xplage.4.pdf

track2kml1.pdf:	man/man1/track2kml.1
			TCAT=/usr/lib/lp/postscript/dpost man -s1 -t -M `pwd`/man/ track2kml | ps2pdf14 - track2kml.1.pdf
