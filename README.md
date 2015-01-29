This is a mirror of Xplage, originally at
http://www.chriskern.net/code/xplaneToGoogleEarth.html.

**Credit goes to original developer, Chris Kern.**

These notes deal with a few special issues which may affect some users
of xplage and that don't belong in the man pages.

Where to Run X-Plane, Xplage and Google Earth
=============================================

You can run X-Plane, xplage and Google Earth on the same computer if you
have sufficient CPU and graphics cycles, and sufficient monitor real
estate. Or you can run each program on a different computer as long as
each of the computers can communicate with the others through a network.
Finally, you can run any two of the programs on one computer and the
third program on a second computer. This section describes some
considerations which may help you decide where to run each of the
programs.

Google Earth
------------

A good starting point is whether you want to be able to view both
xplage's overhead map and its perspective view at the same time. As a
practical matter, a single instance of Google Earth can only display one
of these views at any given time and a single computer can only run one
instance of Google Earth. So you want to see both views simultaneously,
you will need to run Google Earth on two different computers.

Ignoring X-Plane for now, let's call the computer on which xplage is
running the "KML server" because it is the source of the KML directives
that Google Earth needs to read as a "network link." For Google Earth to
fetch a KML file from a machine other than the one on which Google Earth
is running, the KML server needs to be turned into a webserver. The
easiest way to do this on all the tested platforms is to use the Free
Software Foundation's Apache webserver software. The Apache software is
included in the operating system bundle on all the tested platforms
except Microsoft Windows.

If you're running xplage on Windows, you can download the Apache
webserver software from http://httpd.apache.org/, and install and
configure it following the instructions provided on that site. On Sun's
Solaris, FreeBSD, the Linux variants, and Apple's OS X, follow the
instructions provided by the operating system manufacturer to configure
the webserver. Once the webserver is running, set up each instance of
Google Earth to fetch either the overhead KML file or the perspective
KML file from the KML server using the HTTP protocol.

If you only want to see one of xplage's two views at any given time, you
can run Google Earth and xplage on the same computer. In that case, you
don't need to use the HTTP protocol since Google Earth can also read a
network link from the local filesystem (even though it isn't really a
"network link" in at case). Use the "Edit Network Link" configuration
screen in Google Earth to browse for the location where xplage is
depositing the file you want to view.

Xplage
------

Xplage can run on any computer that can receive the UDP telemetry from
X-Plane. It should be executed from a terminal emulation window -- all
the tested platforms supply a terminal emulator as part of the operating
system bundle -- because you need to type in the initial command line
and may want to change the height of the Google Earth "eye" while
X-Plane and xplage are running. However, once you launch xplage, you can
minimize or dock the terminal emulation window if you don't have space
for it on your computer monitor. Xplage will continue to run quite
happily even if you can't see the informational messages it is printing
on its standard output stream. The KML files it produces will still be
stored in the location you have specified or the default location for
the Apache webserver on the platform on which it is running.

X-Plane
-------

If you have multiple computers at your disposal, you probably know which
one you want to use for X-Plane: presumably the one with the fastest
CPU, the most memory, and the best graphics card. One warning, however,
if you plan to run X-Plane and xplage on the same computer, you *must*
follow the instructions contained in the section of this document
titled -- appropriately enough -- "Running X-Plane and Xplage on the
Same Computer."

Running X-Plane and Xplage on the Same Computer
===============================================

If you run X-Plane and xplage on the same computer, you must

1.  configure X-Plane to transmit UDP telemetry on a port other than the
    default port of 49000,

2.  configure X-Plane with an "IP address of data receiver" of 127.0.0.1
    (the "loopback address") for efficiency, and

3.  execute xplage with the -p <port> switch, with <port> matching the
    port you selected for X-Plane's UDP transmissions.

The reason for this is that X-Plane is hard-coded to listen as well as
transmit on port 49000. If both X-Plane and xlage are running on the
same computer, xplage will not be able to bind to that port.

To change the X-Plane UDP output port, enter another port of your choice
in the "Data Input and Output \> Inet2" configuration screen. The port
number should be greater than 1024, e.g., 40000. The IP address of the
"data receiver" is also set from this screen.

To execute xplane with a non-default port, add the -p switch to the
command line -- e.g., "xplage -p 40000" -- each time you run the
program.

This will guarantee peaceful coexistence between the two programs.

Again, you only need to do this if your are running X-Plane and xplage
on the same computer.

Using the Track2kml Utility
===========================

I wrote the track2kml utility primarily as a way to demonstrate how to
use the track.csv output file that is created when xplage is run with
the -t (track) option. Track2kml is a post-processor for the track data
file; in other words, after the track.csv file has been created by
xplage, you can use track2kml to convert tho data contained in the file
into a three-dimensional image of the flight path that can be displayed
by Google Earth.

The track2kml must be run in a terminal emulation window within a POSIX
operating environment. Sun's Solaris, FreeBSD, the tested Linux
variants, and Apple's OS X all provide a POSIX environment as part of
the manufacturer's operating system bundle. There are several ways to
add a POSIX environment to Microsoft Windows, but the easiest, and the
one I recommend, is to download and install the free Cygwin software
from http://www.cygwin.com.

On all the tested operating systems, launch track2kml on the command
line with the location (pathname) of the track.csv file produced by
xplage. On OS X, for example, the command line would be

track2kml /Library/WebServer/Documents/xp2ge/track.csv

if you are using xplage's default file output settings. For more
information about running track2kml, see the manual page in the xplage
distribution bundle.

Import the KML file produced by track2kml into Google Earth using Google
Earth's "Edit Network Link" or "File\>Open" features. (Obviously,
File\>Open will only work if xplage and Google Earth are running on the
same computer.) When Google Earth displays the flight path, you can tilt
or rotate the "viewer" to see the path from any angle, or animate the
path by clicking on the animation controls that will appear at the top
of the Google Earth display. If you see multiple airplane icons during
the animation, you can reduce them to a single icon by moving the two
vertical time-span bars of the animation control closer together.

Again, I wrote track2kml mainly as an example for other programmers who
might want to render xplage's track data into other formats. If you're a
programmer and you want to display information about an X-Plane
simulated flight path in a viewer other than Google Earth, track2kml may
be a useful starting point.

Compiling Xplage on Microsoft Windows
=====================================

Compiling xplage on Microsoft Windows turned out to be effortless,
notwithstanding that the program was designed on and for a very
different computing environment, thanks to the free Cygwin development
tools and run-time link library (http://www.cygwin.com). It may be just
as easy to build the program with Microsoft's native compiler and tools
since recent releases of Windows NT provide UNIX-like subsystems: a
supplemental "Windows Services for UNIX" package on NT 5.1 ("Windows
XP") and NT 5.2 ("Windows Server 2003"), and what apparently is embedded
POSIX support in at least some of the versions of the NT 6.0 ("Windows
Vista") release. However, my admittedly limited experience is that
someone more familiar with software development on UNIX or Linux than
with Windows can't go wrong with the Cygwin suite, and that's what
xplage has been tested with. The supplied Makefile assumes that is what
you are using.

Installing Xplage on Microsoft Windows
======================================

The Cygwin dynamic link library that provides POSIX run-time support
should be installed in the same directory as the xplage.exe binary. This
file (cygwin1.dll) is included in the binary distribution but not in the
source distribution because you will need to install the Cygwin
development suite, which includes the library, to compile the program
yourself on a Microsoft platform.

Multiple Versions of the Cygwin Link Library on Windows
=======================================================

I discovered while testing the Windows port of xplage that, unbeknownst
to me, over the years I had acquired several other open source apps that
were built against the cygwin1.dll run-time link library. I also
discovered that these were using old revisions of the library, and that
the older and current revs did not peacefully coexist. Conflicts are
announced with a diagnostic message similar to this when a program is
launched:

"\*\*\* system shared memory version mismatch detected -
0x2D1E009C/0x75BE0081. This problem is probably due to using
incompatible versions of the cygwin DLL. Search for cygwin1.dll using
the Windows Start-\>Find/Search facility and delete all but the most
recent version."

Contrary to the instructions in the diagnostic, deleting the older
cygwin1.dll files may cripple the programs that depend on them. However,
replacing them with the current revision of the library seems to work.
Again, you don't need to do anything unless you are unable to launch
xplage because of this conflict.

(Note: the cygwin1.dll file included in the xplage binary package is
current as of this writing.)

Compiling Xplage on Apple OS X
==============================

Building xplage on OS X also is effortless, but you'll need to install
the Apple's Xcode development tools. These are included in the 10.5
(Leopard) distribution disk and may be downloaded for earlier releases
from the Apple developer web site: http://developer.Apple.Com
(registration required).

