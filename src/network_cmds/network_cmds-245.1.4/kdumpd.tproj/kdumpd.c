/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

/* Mac OS X kernel core dump server, based on the BSD trivial file
 * transfer protocol server (FreeBSD distribution), with several
 * modifications. This server is *not* compatible with tftp, as the
 * protocol has changed considerably.
 */

/*
 * Based on the trivial file transfer protocol server.
 *
 * The original version included many modifications by Jim Guyton
 * <guyton@rand-unix>.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include "kdump.h"
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "kdumpsubs.h"

#define	TIMEOUT		5

int	peer;
int	rexmtval = TIMEOUT;
int	maxtimeout = 10*TIMEOUT;

#define	PKTSIZE	SEGSIZE+6
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_in from;
int	fromlen;

void	kdump __P((struct kdumphdr *, int));

/*
 * Null-terminated directory prefix list for absolute pathname requests and
 * search list for relative pathname requests.
 *
 * MAXDIRS should be at least as large as the number of arguments that
 * inetd allows (currently 20).
 */
#define MAXDIRS	20
static struct dirlist {
	char	*name;
	int	len;
} dirs[MAXDIRS+1];
static int	suppress_naks;
static int	logging = 1;
static int	ipchroot;

static char *errtomsg __P((int));
static void  nak __P((int));
static char * __P(verifyhost(struct sockaddr_in *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct kdumphdr *tp;
	register int n;
	int ch, on;
	struct sockaddr_in sin;
	char *chroot_dir = NULL;
	struct passwd *nobody;
	char *chuser = "nobody";

	openlog("kdumpd", LOG_PID | LOG_NDELAY, LOG_FTP);
	while ((ch = getopt(argc, argv, "cClns:u:")) != -1) {
		switch (ch) {
		case 'c':
			ipchroot = 1;
			break;
		case 'C':
			ipchroot = 2;
			break;
		case 'l':
			logging = 1;
			break;
		case 'n':
			suppress_naks = 1;
			break;
		case 's':
			chroot_dir = optarg;
			break;
		case 'u':
			chuser = optarg;
			break;
		default:
			syslog(LOG_WARNING, "ignoring unknown option -%c", ch);
		}
	}

	if (optind < argc) {
		struct dirlist *dirp;

		/* Get list of directory prefixes. Skip relative pathnames. */
		for (dirp = dirs; optind < argc && dirp < &dirs[MAXDIRS];
		     optind++) {
			if (argv[optind][0] == '/') {
				dirp->name = argv[optind];
				dirp->len  = strlen(dirp->name);
				dirp++;
			}
		}
	}
	else if (chroot_dir) {
		dirs->name = "/";
		dirs->len = 1;
	}
	if (ipchroot && chroot_dir == NULL) {
		syslog(LOG_ERR, "-c requires -s");
		exit(1);
	}

	on = 1;
	if (ioctl(0, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m");
		exit(1);
	}
	fromlen = sizeof (from);
	n = recvfrom(0, buf, sizeof (buf), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m");
		exit(1);
	}
	/*
	 * Now that we have read the message out of the UDP
	 * socket, we fork and exit.  Thus, inetd will go back
	 * to listening to the kdump port, and the next request
	 * to come in will start up a new instance of kdumpd.
	 *
	 * We do this so that inetd can run kdumpd in "wait" mode.
	 * The problem with kdumpd running in "nowait" mode is that
	 * inetd may get one or more successful "selects" on the
	 * kdump port before we do our receive, so more than one
	 * instance of kdumpd may be started up.  Worse, if kdumpd
	 * breaks before doing the above "recvfrom", inetd would
	 * spawn endless instances, clogging the system.
	 */
	{
		int pid;
		int i, j;

		for (i = 1; i < 20; i++) {
		    pid = fork();
		    if (pid < 0) {
				sleep(i);
				/*
				 * flush out to most recently sent request.
				 *
				 * This may drop some requests, but those
				 * will be resent by the clients when
				 * they timeout.  The positive effect of
				 * this flush is to (try to) prevent more
				 * than one kdumpd being started up to service
				 * a single request from a single client.
				 */
				j = sizeof from;
				i = recvfrom(0, buf, sizeof (buf), 0,
				    (struct sockaddr *)&from, &j);
				if (i > 0) {
					n = i;
					fromlen = j;
				}
		    } else {
				break;
		    }
		}
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
	}

	/*
	 * Since we exit here, we should do that only after the above
	 * recvfrom to keep inetd from constantly forking should there
	 * be a problem.  See the above comment about system clogging.
	 */
	if (chroot_dir) {
		if (ipchroot) {
			char tempchroot[MAXPATHLEN];
			char *tempaddr;
			struct stat sb;
			int statret;

			tempaddr = inet_ntoa(from.sin_addr);
			snprintf(tempchroot, sizeof(tempchroot), "%s/%s", chroot_dir, tempaddr);
			statret = stat(tempchroot, &sb);
			if ((sb.st_mode & S_IFDIR) &&
			    (statret == 0 || (statret == -1 && ipchroot == 1)))
				chroot_dir = tempchroot;
		}
		/* Must get this before chroot because /etc might go away */
		if ((nobody = getpwnam(chuser)) == NULL) {
			syslog(LOG_ERR, "%s: no such user", chuser);
			exit(1);
		}
		if (chroot(chroot_dir)) {
			syslog(LOG_ERR, "chroot: %s: %m", chroot_dir);
			exit(1);
		}
		chdir( "/" );
		setuid(nobody->pw_uid);
	}
	else
	  if (0 !=  chdir(dirs->name))
	    syslog(LOG_ERR, "chdir%s: %m", dirs->name);

	from.sin_family = AF_INET;
	alarm(0);
	close(0);
	close(1);
	peer = socket(AF_INET, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (bind(peer, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		syslog(LOG_ERR, "bind: %m");
		exit(1);
	}
	if (connect(peer, (struct sockaddr *)&from, sizeof(from)) < 0) {
		syslog(LOG_ERR, "connect: %m");
		exit(1);
	}
	tp = (struct kdumphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == WRQ)
		kdump(tp, n);
	exit(1);
}

struct formats;
int	validate_access __P((char **, int));

void	recvfile __P((struct formats *));

struct formats {
	char	*f_mode;
	int	(*f_validate) __P((char **, int));

	void	(*f_recv) __P((struct formats *));
	int	f_convert;
} formats[] = {
  { "netascii",	validate_access, recvfile, 1 },
  { "octet",	validate_access, recvfile, 0 },
  { 0 }
};

/*
 * Handle initial connection protocol.
 */
void
kdump(tp, size)
	struct kdumphdr *tp;
	int size;
{
	register char *cp;
	int first = 1, ecode;
	register struct formats *pf;
	char *filename, *mode = NULL;

	filename = cp = tp->th_stuff;
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		nak(EBADOP);
		exit(1);
	}
	if (first) {
		mode = ++cp;
		first = 0;
		goto again;
	}
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		nak(EBADOP);
		exit(1);
	}
	ecode = (*pf->f_validate)(&filename, tp->th_opcode);
	if (logging) {
		syslog(LOG_INFO, "%s: %s request for %s: %s", verifyhost(&from),
			tp->th_opcode == WRQ ? "write" : "read",
			filename, errtomsg(ecode));
	}
	if (ecode) {
		/*
		 * Avoid storms of naks to a RRQ broadcast for a relative
		 * bootfile pathname from a diskless Sun.
		 */
		if (suppress_naks && *filename != '/' && ecode == ENOTFOUND)
			exit(0);
		nak(ecode);
		exit(1);
	}
	if (tp->th_opcode == WRQ)
		(*pf->f_recv)(pf);

	exit(0);
}


FILE *file;

/*
 * Validate file access. We only allow storage of files that do not already
 * exist, and that do not include directory specifiers in their pathnames. 
 * This is because kernel coredump filenames should always be of the form 
 * "core-version-IP as dotted quad-random string" as in :
 * core-custom-17.202.40.204-a75b4eec
 * The file is written to the directory supplied as the first argument
 * in inetd.conf
 */

int 
validate_access(char **filep, int mode)
{
  struct stat stbuf;
  int	fd;
  char *filename = *filep;
  static char pathname[MAXPATHLEN]; 

  if (strstr(filename, "/") || strstr(filename, ".."))
    return (EACCESS);
  
  snprintf(pathname, sizeof(pathname), "./%s", filename);

  if (0 == stat(pathname, &stbuf))
    return (EEXIST);
  
  if (errno != ENOENT)
    return (errno);


  fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC);

  if (fd < 0)
    return (errno + 100);
  
  file = fdopen(fd, (mode == RRQ)? "r":"w");
  if (file == NULL) {
    return errno+100;
  }
  if (fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) < 0)
    return errno+100;

  return (0);  
}

int	timeout;
jmp_buf	timeoutbuf;

void
timer()
{

	timeout += rexmtval;
	if (timeout >= maxtimeout)
	  {
	    longjmp(timeoutbuf, 2); 
	  }
	longjmp(timeoutbuf, 1);
}

void
justquit()
{
	exit(0);
}


/*
 * Receive a file.
 */
void
recvfile(pf)
	struct formats *pf;
{
	struct kdumphdr *dp, *w_init();
	register struct kdumphdr *ap;    /* ack buffer */
	register int n, size;
	volatile unsigned int block;
	volatile unsigned int jmpval = 0;

	signal(SIGALRM, timer);
	dp = w_init();
	ap = (struct kdumphdr *)ackbuf;
	block = 0;
	do {
send_seek_ack:	timeout = 0;
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htonl((unsigned int)block);
		block++;
		jmpval = setjmp(timeoutbuf);
		if (2 == jmpval)
		  {
		    syslog (LOG_ERR, "Timing out and flushing file to disk");
		    goto flushfile;
		  }
send_ack:
		if (send(peer, ackbuf, 6 , 0) != 6) {
			syslog(LOG_ERR, "write: %m");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) {            /* really? */
				syslog(LOG_ERR, "read: %m");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohl((unsigned int)dp->th_block);
			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == KDP_EOF)
			  {
			    syslog (LOG_ERR, "Received last panic dump packet");
			    goto final_ack;
			  }
			if (dp->th_opcode == KDP_SEEK)
			  {
			    if (dp->th_block == block)
			      {
				unsigned int tempoff = 0;
				bcopy (dp->th_data, &tempoff, sizeof(unsigned int));
				lseek (fileno (file), ntohl(tempoff), SEEK_SET);
				if (errno)
				  syslog (LOG_ERR, "lseek: %m");

				goto send_seek_ack;
			      }
			    (void) synchnet(peer);
			    if (dp->th_block == (block-1))
			      {
				syslog (LOG_DAEMON|LOG_ERR, "Retransmitting seek ack - current block %hu, received block %hu", block, dp->th_block);
				goto send_ack;          /* rexmit */
			      }
			  }

			if (dp->th_opcode == DATA) {
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
				  {
				    syslog (LOG_DAEMON|LOG_ERR, "Retransmitting ack - current block %hu, received block %hu", block, dp->th_block);
				    goto send_ack;          /* rexmit */
				  }
				else
				  syslog (LOG_DAEMON|LOG_ERR, "Not retransmitting ack - current block %hu, received block %hu", block, dp->th_block);
			}
		}

		size = writeit(file, &dp, n - 6, pf->f_convert);
		if (size != (n-6)) {                    /* ahem */
			if (size < 0) nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (dp->th_opcode != KDP_EOF);

final_ack:
	ap->th_opcode = htons((u_short)ACK);    /* send the "final" ack */
	ap->th_block = htonl((unsigned int) (block));
	(void) send(peer, ackbuf, 6, 0);
flushfile:
	write_behind(file, pf->f_convert);
	(void) fclose(file);            /* close data file */
	syslog (LOG_ERR, "file closed, sending final ACK\n");

	signal(SIGALRM, justquit);      /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, sizeof (buf), 0); /* normally times out and quits */
	alarm(0);
	if (n >= 6 &&                   /* if read some data */
	    dp->th_opcode == DATA &&    /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		(void) send(peer, ackbuf, 6, 0);     /* resend final ack */
	}
abort:
	return;
}

struct errmsg {
	int	e_code;
	char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal KDUMP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		0 }
};

static char *
errtomsg(error)
	int error;
{
	static char buf[20];
	register struct errmsg *pe;
	if (error == 0)
		return "success";
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			return pe->e_msg;
	snprintf(buf, sizeof(buf), "error %d", error);
	return buf;
}

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard KDUMP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(error)
	int error;
{
	register struct kdumphdr *tp;
	int length;
	register struct errmsg *pe;

	tp = (struct kdumphdr *)buf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((unsigned int)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	strcpy(tp->th_msg, pe->e_msg);
	length = strlen(pe->e_msg);
	tp->th_msg[length] = '\0';
	length += 5;
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m");
}

static char *
verifyhost(fromp)
	struct sockaddr_in *fromp;
{
	struct hostent *hp;

	hp = gethostbyaddr((char *)&fromp->sin_addr, sizeof(fromp->sin_addr),
		fromp->sin_family);
	if(hp)
		return hp->h_name;
	else
		return inet_ntoa(fromp->sin_addr);
}
