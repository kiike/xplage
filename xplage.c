/*
	Copyright (c) 2011 Chris Kern

	This program may be distributed in accordance with the terms of Version 2 of the GNU General Public License
	published by the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA,
	and is made available by the author without any warranty, including any implied warranty of merchantability
	or fitness for a particular purpose.
*/


static const char id[] = "Last edit: 02-Dec-11 by CK";


#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>


/* constants */

#define FEETPERMETER	3.2808399						/* conversion divisor */
#define ICONCOLOR	"ff0000ff"						/* KML opaque bright red (format: aabbggrr) */
#define MAXLINE		     1024						/* standard input line buffer length */
#define MAXDGRAM	     8192						/* largest UDP datagram expected */
#define TILT		       75						/* degrees of tilt offset for perspective view */
#define TRUE			1
#define FALSE			0


/* X-Plane UDP format: version-independent index location in stream, size and offsets for data elements */

#define CTLOCTET		6						/* UDP octet for control index */
#define DSIZE		       36						/* size of data contents */
#define CTLDATUM1		4						/* position of first four-octet control datum */
#define CTLDATUM2		8						/* position of second datum */
#define CTLDATUM3	       12						/* position of third datum */
#define MAXINDEX	      256						/* sanity check for byte-order test */


/* X-Plane UDP format: version-dependent index values */

#define V860DIRCTL	       16						/* directional heading */
#define V860LOCCTL	       18						/* geographical coordinates */

#define V900DIRCTL	       18						/* directional heading */
#define V900LOCCTL	       20						/* geographical coordinates */

#define V100DIRCTL	       17						/* directional heading */
#define V100LOCCTL	       20						/* geographical coordinates */


/* defaults for command line arguments */

#define GEYEHIGH	   100000						/* add to alt for high Google Earth "eye" */
#define GEYELOW		    20000						/* add to alt for low "eye" height */
#define GEYEMED		    60000						/* add to alt for medium "eye" height */
#define SRCPORT		    49000						/* UDP port to listen on */
#define WRITINT			1						/* file write interval in seconds */

#define XP2GE			"/var/apache/htdocs/xp2ge/"			/* default directory for KML output files */
#define OVERHEAD		"overhead.kml"					/* overhead view filename */
#define PERSPECTIVE		"perspective.kml"				/* perspective view filename */
#define TRACK			"track.csv"					/* track data filename */


/* internal prototypes */

static void *tprocudpdata(void *);						/* UDP listener and converter thread */
static int intsanity(unsigned char *, unsigned int);				/* index integer plausibility check */
static void deserialize(int, void *restrict, unsigned char *);			/* capture 32-bit network datum */

static void *tprocoutfiles(void *);						/* output file processing thread */
static void writekml(void);							/* write KML files for Google Earth */
static void writetrack(void);							/* write track information for post-processing */

static void *tprocinterrupt(void *);						/* interrupt processing thread */
static int getintupdate(void);							/* update parameters at user's request */


/* internal uninitialized globals */

static int sfd;									/* socket file descriptor */
static int dirctl, locctl;							/* index values for direction, coordinates */
static float alt, eye, hdg, lat, lon, pitch, roll;				/* coordinates and orientation */
static char opath[FILENAME_MAX], vpath[FILENAME_MAX], tpath[FILENAME_MAX];	/* output file pathnames */
static char prog[FILENAME_MAX];							/* program name to use in error diagnostics */
static pthread_mutex_t dgramutex;						/* for dgram update synchronization */
static sigset_t sigs;								/* system list of signals */


/* internal initialized globals */

static int xpver = 10;								/* default X-Plane version to listen for */
static int offset = GEYEMED;							/* default Google Earth "eye" offset */
static int silent = FALSE;							/* print status on standard output by default */
static int testxpver = TRUE;							/* test for X-Plane version by default */
static int track = FALSE;							/* omit output track by default */
static int writint = WRITINT;							/* default seconds between file writes */
static unsigned int ndgrams = 0, ndirdgrams = 0, nlocdgrams = 0;		/* UDP datagram counters */




int main(int argc, char *argv[])						/* collect X-Plane datagrams and emit KML files */
{
	int c;
	unsigned int port = SRCPORT;						/* listen on SRCPORT by default */
	char dirpath[FILENAME_MAX];
	struct sockaddr_in xpdata;
	pthread_attr_t attr;
	pthread_t tidudpdata, tidinterrupt, tidoutfile;
	extern int optind, optopt;
	extern char *optarg;
	DIR *dirp;

	(void) strcpy(dirpath, XP2GE);
	(void) strcpy(prog, basename(argv[0]));

	while ((c = getopt(argc, argv, ":f:hlmi:o:p:rstv:")) != -1) {

		switch(c) {
		case 'f':
			(void) sscanf(optarg, "%d", &offset);			/* -f: eye offset in feet */
			if (offset < 0)
				offset = GEYEMED;
			break;
		case 'h':
			offset = GEYEHIGH;					/* -h: eye offset high */
			break;
		case 'l':
			offset = GEYELOW;					/* -l: eye offset low */
			break;
		case 'm':
			offset = GEYEMED;					/* -m: eye offset medium */
			break;
		case 'i':
			(void) sscanf(optarg, "%d", &writint);			/* -i: file write interval */
			if (writint <= 0)
				writint = WRITINT;
			break;
		case 'o':
			(void) sscanf(optarg, "%s", dirpath);			/* -o: output directory for KML, track files */
			break;
		case 'p':
			(void) sscanf(optarg, "%u", &port);			/* -p: port to listen on */
			break;
		case 'r':
			(void) fprintf(stderr, "%s: %s\n", prog, id);		/* -r: report version and exit (undocumented) */
			exit(0);
		case 's':
			silent = TRUE;						/* -s: suppress chatter to standard output */
			break;
		case 't':
			track = TRUE;						/* -t: emit track data */
			break;
		case 'v':							/* -v: set X-Plane version */
			(void) sscanf(optarg, "%d", &xpver);
			testxpver = FALSE;
			break;
		case '?':
			(void) fprintf(stderr, "%s: invalid option: -%c\n", prog, optopt);
			exit(1);
		case ':':
			(void) fprintf(stderr, "%s: missing operand for -%c option\n", prog, optopt);
			exit(1);
		}
	}

	if (argc > optind)
		(void) fprintf(stderr, "%s: ignoring junk on command line\n", prog);

	switch (xpver) {
		case 8:								/* use X-Plane version 8.6X semantics */
			dirctl = V860DIRCTL;
			locctl = V860LOCCTL;
			break;
		case 9:								/* use X-Plane version 9.0X semantics */
			dirctl = V900DIRCTL;
			locctl = V900LOCCTL;
			break;
		case 10:							/* use X-Plane version 10.0X semantics */
			dirctl = V100DIRCTL;
			locctl = V100LOCCTL;
			break;
		default:
			(void) fprintf(stderr, "%s: version %d is not supported\n", prog, xpver);
			exit(1);
	}

	if ((dirp = opendir(dirpath)) != NULL)					/* see if directory exists or try to create it */
		(void) closedir(dirp);
	else {
		if (mkdir(dirpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
			(void) fprintf(stderr, "%s: can't create directory %s\n", prog, dirpath);
			exit(1);
		}
	}

	if (dirpath[strlen(dirpath) - 1] != '/')
		(void) strlcat(dirpath, "/", sizeof(dirpath));
	(void) strcpy(opath, dirpath);
	(void) strlcat(opath, OVERHEAD, sizeof(opath));
	(void) strcpy(vpath, dirpath);
	(void) strlcat(vpath, PERSPECTIVE, sizeof(vpath));
	(void) strcpy(tpath, dirpath);
	(void) strlcat(tpath, TRACK, sizeof(tpath));

	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		(void) fprintf(stderr, "%s: can't create socket\n", prog);
		exit(1);
	}

	xpdata.sin_family = AF_INET;
	xpdata.sin_port = htons(port);
	xpdata.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sfd, (struct sockaddr *) &xpdata, sizeof(xpdata)) < 0) {
		(void) fprintf(stderr, "%s: bind failed, aborting\n", prog);
		exit(1);
	}

	if (silent == FALSE)
		(void) printf("Port: %u, Directory: %s, Interval: %u\n", port, dirpath, writint);

	if (track == TRUE)
		(void) unlink(tpath);						/* don't append to old file */

	(void) sigfillset(&sigs);
	(void) pthread_sigmask(SIG_BLOCK, &sigs, NULL);				/* block signals in all threads by default */

	if (pthread_create(&tidudpdata, NULL, tprocudpdata, NULL) != 0) {	/* create thread to read UDP data */
		(void) fprintf(stderr, "%s: can't create thread to process incoming UDP datagrams, aborting\n", prog);
		exit(1);
	}

	if (pthread_create(&tidoutfile, NULL, tprocoutfiles, NULL) != 0) {	/* create thread to write output files */
		(void) fprintf(stderr, "%s: can't create thread to process output files, aborting\n", prog);
		exit(1);
	}

	if (pthread_attr_init(&attr) != 0 || pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		(void) fprintf(stderr, "%s: can't set joinable attribute to process interrupt requests, aborting\n", prog);
		exit(1);
	}

	if (pthread_create(&tidinterrupt, &attr, tprocinterrupt, NULL) != 0) {	/* create joinable thread to process interrupts */
		(void) fprintf(stderr, "%s: can't create thread to process interrupt requests, aborting\n", prog);
		exit(1);
	}

	(void) pthread_attr_destroy(&attr);

	if (pthread_join(tidinterrupt, NULL) != 0) {				/* wait for interrupt thread to terminate */
		(void) fprintf(stderr, "%s: error joining thread to process interrupt requests, aborting\n", prog);
		exit(1);
	}

	(void) fputs("Terminated.\n", stdout);					/* interrupt thread processed user exit request */
	return 0;
}




static void *tprocudpdata(void *threadid)					/* convert UDP datagrams to numeric values */
{
	int octet, noctets, reorder;
	unsigned int index;
	unsigned char msg[MAXDGRAM], *bufptr;

	for ( ;; ) {

		if ((noctets = recv(sfd, msg, MAXDGRAM, 0)) > 0) {
			for (octet = CTLOCTET, bufptr = msg + (CTLOCTET - 1); octet <= noctets; octet += DSIZE, bufptr += DSIZE) {
				reorder = intsanity(bufptr, MAXINDEX);
				deserialize(reorder, &index, bufptr);
				if (index == dirctl) {
					deserialize(reorder, &pitch, bufptr + CTLDATUM1);
					deserialize(reorder, &roll, bufptr + CTLDATUM2);
					deserialize(reorder, &hdg, bufptr + CTLDATUM3);
					(void) pthread_mutex_lock(&dgramutex);
					ndirdgrams++;
					(void) pthread_mutex_unlock(&dgramutex);
				}
				else if (index == locctl) {
					deserialize(reorder, &lat, bufptr + CTLDATUM1);
					deserialize(reorder, &lon, bufptr + CTLDATUM2);
					deserialize(reorder, &alt, bufptr + CTLDATUM3);
					eye = (alt + offset)/FEETPERMETER;	/* KML needs height of "eye" in meters */
					(void) pthread_mutex_lock(&dgramutex);
					nlocdgrams++;
					(void) pthread_mutex_unlock(&dgramutex);
				}
			}
			(void) pthread_mutex_lock(&dgramutex);
			ndgrams++;
			(void) pthread_mutex_unlock(&dgramutex);
		}
	}

	/* NOTREACHED */
}




static int intsanity(unsigned char *octets, unsigned int max)			/* test whether index int exceeds expectations */
{
	unsigned int test;

	(void) memcpy(&test, octets, 4);
	return test > max ? TRUE : FALSE;
}




static void deserialize(int flip, void *restrict datum, unsigned char *octets)	/* deserialize 32-bit network datum */
{
	int i;
	unsigned char buf[4];

	if (flip == FALSE)							/* okay as is */
		(void) memcpy(datum, octets, 4);
	else {									/* flip byte order */
		for (i = 3; i >= 0; i--, octets++)
			buf[i] = *octets;
		(void) memcpy(datum, buf, 4);
	}
}




static void *tprocoutfiles(void *threadid)					/* output files at specified interval */
{
	int skipreport;
	unsigned int elapsed, nsecs;

	for (elapsed = 0, nsecs = time(0); ; ) {

		skipreport = FALSE;

		if (ndgrams > 0) {
			if (ndirdgrams == 0 || nlocdgrams == 0) {
				if (testxpver == TRUE) {			/* check for different version */
					if (++xpver > 10)
						xpver = 8;
					switch (xpver) {
						case 8:				/* use X-Plane version 8.6X semantics */
							dirctl = V860DIRCTL;
							locctl = V860LOCCTL;
							break;
						case 9:				/* use X-Plane version 9.0X semantics */
							dirctl = V900DIRCTL;
							locctl = V900LOCCTL;
							break;
						case 10:			/* use X-Plane version 10.0X semantics */
							dirctl = V100DIRCTL;
							locctl = V100LOCCTL;
							break;
					}

					if (silent == FALSE) {
						(void) printf("\nNow listening for X-Plane version %d:\n", xpver);
						skipreport = TRUE;
					}
				}
			}
			else {
				writekml();
				if (track == TRUE)
					writetrack();
			}
		}

		if (silent == FALSE && skipreport == FALSE) {
			elapsed = time(0) - nsecs;
			nsecs = time(0);					/* update for next interval */
			(void) printf(elapsed > 0 ? "dgrams: %3u, %6.2f/sec" : "dgrams: %3u", ndgrams, (float) ndgrams/elapsed);
			(void) printf(ndgrams > 0 ? ",  lat: %f, lon: %f, alt: %.0f, hdg: %03.0f\n" : "\n", lat, lon, alt, hdg);
		}

		(void) pthread_mutex_lock(&dgramutex);
		ndgrams = ndirdgrams = nlocdgrams = 0;
		(void) pthread_mutex_unlock(&dgramutex);

		(void) sleep(writint);
	}

	/* NOTREACHED */
}




static void writekml(void)							/* emit KML file updates from global variables */
{
	FILE *fp;

	if ((fp = fopen(opath, "w")) == NULL) {					/* overhead view file */
		(void) fprintf(stderr, "%s: can't open %s\n", prog, opath);
		exit(1);
	}
	else {
		(void) fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		(void) fprintf(fp, "<kml xmlns=\"http://earth.google.com/kml/2.2\">\n");
		(void) fprintf(fp, "<Document>\n");
		(void) fprintf(fp, "\t<Style id=\"sn_icon56\">\n");
		(void) fprintf(fp, "\t\t<IconStyle>\n");
		(void) fprintf(fp, "\t\t\t<Icon>\n");
		(void) fprintf(fp, "\t\t\t\t<href>http://maps.google.com/mapfiles/kml/pal2/icon56.png</href>\n");
		(void) fprintf(fp, "\t\t\t</Icon>\n");
		(void) fprintf(fp, "\t\t</IconStyle>\n");
		(void) fprintf(fp, "\t</Style>\n");
		(void) fprintf(fp, "\t<Camera>\n");
		(void) fprintf(fp, "\t\t<longitude>%f</longitude>\n", lon);
		(void) fprintf(fp, "\t\t<latitude>%f</latitude>\n", lat);
		(void) fprintf(fp, "\t\t<altitude>%f</altitude>\n", eye);
		(void) fprintf(fp, "\t</Camera>\n");
		(void) fprintf(fp, "\t<Placemark>\n");
		(void) fprintf(fp, "\t\t<Style>\n");
		(void) fprintf(fp, "\t\t\t<IconStyle>\n");
		(void) fprintf(fp, "\t\t\t\t<color>%s</color>\n", ICONCOLOR);	/* format: aabbggrr */
		(void) fprintf(fp, "\t\t\t\t<heading>%03.0f</heading>\n", hdg);	/* format: NNN */
		(void) fprintf(fp, "\t\t\t</IconStyle>\n");
		(void) fprintf(fp, "\t\t</Style>\n");
		(void) fprintf(fp, "\t\t<styleUrl>#sn_icon56</styleUrl>\n");
		(void) fprintf(fp, "\t\t<Point>\n");
		(void) fprintf(fp, "\t\t\t<coordinates>%f,%f,%.0f</coordinates>\n", lon, lat, alt/FEETPERMETER);
		(void) fprintf(fp, "\t\t</Point>\n");
		(void) fprintf(fp, "\t</Placemark>\n");
		(void) fprintf(fp, "</Document>\n");
		(void) fprintf(fp, "</kml>\n");
		(void) fclose(fp);
	}

	if ((fp = fopen(vpath, "w")) == NULL) {					/* perspective view file */
		(void) fprintf(stderr, "%s: can't open %s\n", prog, vpath);
		exit(1);
	}
	else {
		(void) fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		(void) fprintf(fp, "<kml xmlns=\"http://earth.google.com/kml/2.2\">\n");
		(void) fprintf(fp, "<Document>\n");
		(void) fprintf(fp, "\t<Camera>\n");
		(void) fprintf(fp, "\t\t<altitudeMode>absolute</altitudeMode>\n");
		(void) fprintf(fp, "\t\t<longitude>%f</longitude>\n", lon);
		(void) fprintf(fp, "\t\t<latitude>%f</latitude>\n", lat);
		(void) fprintf(fp, "\t\t<altitude>%f</altitude>\n", alt/FEETPERMETER);
		(void) fprintf(fp, "\t\t<heading>%f</heading>\n", hdg);
		(void) fprintf(fp, "\t\t<roll>%f</roll>\n", -1 * roll);		/* invert aircraft roll to produce camera roll */
		(void) fprintf(fp, "\t\t<tilt>%f</tilt>\n", TILT + pitch);	/* point viewer slightly downwards */
		(void) fprintf(fp, "\t</Camera>\n");
		(void) fprintf(fp, "</Document>\n");
		(void) fprintf(fp, "</kml>\n");
		(void) fclose(fp);
	}
}




static void writetrack(void)							/* emit CSV track update from global variables */
{
	char xmltime[MAXLINE];
	time_t now = time(0);
	struct tm *t = gmtime(&now);
	FILE *fp;

	if ((fp = fopen(tpath, "a")) == NULL) {					/* track data file */
		(void) fprintf(stderr, "%s: can't open %s\n", prog, tpath);
		exit(1);
	}
	else {
		(void) sprintf(xmltime, "%d-%02d-%02dT%02d:%02d:%02dZ",
					t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

		(void) fprintf(fp, "%s,%f,%f,%f,%f,%f,%f\n", xmltime, lon, lat, alt, hdg, roll, pitch);
		(void) fclose(fp);
	}
}




static void *tprocinterrupt(void *threadid)					/* process user interrupt request */
{
	int signum, sival = silent;

	for ( ;; ) {

		if (sigwait(&sigs, &signum) != 0 || signum != SIGINT)
			abort();						/* should never happen */
		else {
			silent = TRUE;						/* squelch activity on stdout for duration */
			if ((offset = getintupdate()) < 0)
				pthread_exit((void *) 0);
			eye = (alt + offset)/FEETPERMETER;
			silent = sival;						/* restore value of silence flag */
		}
	}

	/* NOTREACHED */
}




static int getintupdate(void)							/* interactively update parameters on interrupt */
{
	int nentries;
	unsigned int feet;
	char c, s[MAXLINE];

	(void) fputs("\nSelect \"eye\" offset of h, m, l, f to specify height in feet, or e to exit: ", stdout);
	(void) fgets(s, MAXLINE, stdin);

	if ((nentries = sscanf(s, "%c%u", &c, &feet)) > 0) {
		switch(tolower(c)) {
			case 'h':
				return GEYEHIGH;
			case 'l':
				return GEYELOW;
			case 'm':
				return GEYEMED;
			case 'f':
				if (nentries == 1) {
					(void) fputs("Enter offset in feet for \"eye\": ", stdout);
					(void) fgets(s, MAXLINE, stdin);
					if ((nentries = sscanf(s, "%u", &feet)) < 1) {
						(void) fputs("Invalid number, setting \"feet\" to 0\n", stdout);
						feet = 0;
					}
				}
				return feet;
			case 'e':
				return -1;
		}
	}

	(void) fputs("Unrecognized command, setting \"eye\" to \"m\"\n", stdout);
	return GEYEMED;
}
