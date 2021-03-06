/*
 * Copyright (c) 1980, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
"@(#) Copyright (c) 1980, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)df.c	8.9 (Berkeley) 5/8/95";
#else
static const char rcsid[] =
  "$FreeBSD: src/bin/df/df.c,v 1.23.2.9 2002/07/01 00:14:24 iedowse Exp $";
#endif
#endif /* not lint */

#ifdef __APPLE__
#define MNT_IGNORE 0
#include <sys/types.h> 
typedef int32_t ufs_daddr_t;
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#ifdef __APPLE__
#include "get_compat.h"
#else
#define COMPAT_MODE(func, mode) 1
#endif

#define UNITS_SI 1
#define UNITS_2 2

#define KILO_SZ(n) (n)
#define MEGA_SZ(n) ((n) * (n))
#define GIGA_SZ(n) ((n) * (n) * (n))
#define TERA_SZ(n) ((n) * (n) * (n) * (n))
#define PETA_SZ(n) ((n) * (n) * (n) * (n) * (n))

#define KILO_2_SZ (KILO_SZ(1024ULL))
#define MEGA_2_SZ (MEGA_SZ(1024ULL))
#define GIGA_2_SZ (GIGA_SZ(1024ULL))
#define TERA_2_SZ (TERA_SZ(1024ULL))
#define PETA_2_SZ (PETA_SZ(1024ULL))

#define KILO_SI_SZ (KILO_SZ(1000ULL))
#define MEGA_SI_SZ (MEGA_SZ(1000ULL))
#define GIGA_SI_SZ (GIGA_SZ(1000ULL))
#define TERA_SI_SZ (TERA_SZ(1000ULL))
#define PETA_SI_SZ (PETA_SZ(1000ULL))

/* Maximum widths of various fields. */
struct maxwidths {
	int mntfrom;
	int total;
	int used;
	int avail;
	int iused;
	int ifree;
};

unsigned long long vals_si [] = {1, KILO_SI_SZ, MEGA_SI_SZ, GIGA_SI_SZ, TERA_SI_SZ, PETA_SI_SZ};
unsigned long long vals_base2[] = {1, KILO_2_SZ, MEGA_2_SZ, GIGA_2_SZ, TERA_2_SZ, PETA_2_SZ};
unsigned long long *valp;

typedef enum { NONE, KILO, MEGA, GIGA, TERA, PETA, UNIT_MAX } unit_t;

unit_t unitp [] = { NONE, KILO, MEGA, GIGA, TERA, PETA };

int	  bread(off_t, void *, int);
int	  checkvfsname(const char *, char **);
char	 *getmntpt(char *);
int	  longwidth(long long);
char	 *makenetvfslist(void);
char	**makevfslist(char *);
void	  prthuman(struct statfs *, long);
void	  prthumanval(double);
void	  prtstat(struct statfs *, struct maxwidths *);
long	  regetmntinfo(struct statfs **, long, char **);
int	  ufs_df(char *, struct maxwidths *);
unit_t	  unit_adjust(double *);
void	  update_maxwidths(struct maxwidths *, struct statfs *);
void	  usage(void);

int	aflag = 0, hflag, iflag, nflag;
struct	ufs_args mdev;

static __inline int imax(int a, int b)
{
	return (a > b ? a : b);
}

int
main(int argc, char *argv[])
{
	struct stat stbuf;
	struct statfs statfsbuf, *mntbuf;
	struct maxwidths maxwidths;
	const char *fstype;
	char *mntpath, *mntpt, **vfslist;
	long mntsize;
	int ch, i, rv, tflag = 0, kludge_tflag = 0;
	const char *options = "abgHhiklmnPt:T:";
	if (COMPAT_MODE("bin/df", "unix2003")) {
		/* Unix2003 requires -t be "include total capicity". which df
		  already does, but it conflits with the old -t so we need to
		  *not* expect a string after -t (we provide -T in both cases
		  to cover the old use of -t) */
		options = "abgHhiklmnPtT:";
	}

	fstype = "ufs";

	vfslist = NULL;
	while ((ch = getopt(argc, argv, options)) != -1)
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'b':
				/* FALLTHROUGH */
		case 'P':
			putenv("BLOCKSIZE=512");
			hflag = 0;
			break;
		case 'g':
			putenv("BLOCKSIZE=1g");
			hflag = 0;
			break;
		case 'H':
			hflag = UNITS_SI;
			valp = vals_si;
			break;
		case 'h':
			hflag = UNITS_2;
			valp = vals_base2;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'k':
			putenv("BLOCKSIZE=1k");
			hflag = 0;
			break;
		case 'l':
			if (tflag)
				errx(1, "-l and -T are mutually exclusive.");
			if (vfslist != NULL)
				break;
			vfslist = makevfslist(makenetvfslist());
			break;
		case 'm':
			putenv("BLOCKSIZE=1m");
			hflag = 0;
			break;
		case 'n':
			nflag = 1;
			break;
		case 't':
			/* Unix2003 uses -t for something we do by default */
			if (COMPAT_MODE("bin/df", "unix2003")) {
			    kludge_tflag = 1;
			    break;
			}
		case 'T':
			if (vfslist != NULL) {
				if (tflag)
					errx(1, "only one -%c option may be specified", ch);
				else
					errx(1, "-l and -%c are mutually exclusive.", ch);
			}
			tflag++;
			fstype = optarg;
			vfslist = makevfslist(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* If we are in unix2003 mode, have seen a -t but no -T and the first
	  non switch arg isn't a file, let's pretend they used -T on it.
	  This makes the Lexmark printer installer happy (PR-3918471) */
	if (tflag == 0 && kludge_tflag && *argv && stat(*argv, &stbuf) < 0
	  && errno == ENOENT) {
	    tflag = 1;
	    fstype = *argv++;
	    vfslist = makevfslist(fstype);
	}

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	bzero(&maxwidths, sizeof(maxwidths));
	for (i = 0; i < mntsize; i++)
		update_maxwidths(&maxwidths, &mntbuf[i]);

	rv = 0;
	if (!*argv) {
		mntsize = regetmntinfo(&mntbuf, mntsize, vfslist);
		bzero(&maxwidths, sizeof(maxwidths));
		for (i = 0; i < mntsize; i++)
			update_maxwidths(&maxwidths, &mntbuf[i]);
		for (i = 0; i < mntsize; i++) {
			if (aflag || (mntbuf[i].f_flags & MNT_IGNORE) == 0)
				prtstat(&mntbuf[i], &maxwidths);
		}
		exit(rv);
	}

	for (; *argv; argv++) {
		if (stat(*argv, &stbuf) < 0) {
			if ((mntpt = getmntpt(*argv)) == 0) {
				warn("%s", *argv);
				rv = 1;
				continue;
			}
		} else if (S_ISCHR(stbuf.st_mode) || S_ISBLK(stbuf.st_mode)) {
			if ((mntpt = getmntpt(*argv)) == 0) {
				mdev.fspec = *argv;
				mntpath = strdup("/tmp/df.XXXXXX");
				if (mntpath == NULL) {
					warn("strdup failed");
					rv = 1;
					continue;
				}
				mntpt = mkdtemp(mntpath);
				if (mntpt == NULL) {
					warn("mkdtemp(\"%s\") failed", mntpath);
					rv = 1;
					free(mntpath);
					continue;
				}
				if (mount(fstype, mntpt, MNT_RDONLY,
				    &mdev) != 0) {
					rv = ufs_df(*argv, &maxwidths) || rv;
					(void)rmdir(mntpt);
					free(mntpath);
					continue;
				} else if (statfs(mntpt, &statfsbuf) == 0) {
					statfsbuf.f_mntonname[0] = '\0';
					prtstat(&statfsbuf, &maxwidths);
				} else {
					warn("%s", *argv);
					rv = 1;
				}
				(void)unmount(mntpt, 0);
				(void)rmdir(mntpt);
				free(mntpath);
				continue;
			}
		} else
			mntpt = *argv;
		/*
		 * Statfs does not take a `wait' flag, so we cannot
		 * implement nflag here.
		 */
		if (statfs(mntpt, &statfsbuf) < 0) {
			warn("%s", mntpt);
			rv = 1;
			continue;
		}
		/* Check to make sure the arguments we've been
		 * given are satisfied.  Return an error if we
		 * have been asked to list a mount point that does
		 * not match the other args we've been given (-l, -t, etc.)
		 */
		if (checkvfsname(statfsbuf.f_fstypename, vfslist)) {
			rv++;
			continue;
		}
		if (argc == 1) {
			bzero(&maxwidths, sizeof(maxwidths));
			update_maxwidths(&maxwidths, &statfsbuf);
		}
		prtstat(&statfsbuf, &maxwidths);
	}
	return (rv);
}

char *
getmntpt(char *name)
{
	long mntsize, i;
	struct statfs *mntbuf;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		if (!strcmp(mntbuf[i].f_mntfromname, name))
			return (mntbuf[i].f_mntonname);
	}
	return (0);
}

/*
 * Make a pass over the filesystem info in ``mntbuf'' filtering out
 * filesystem types not in vfslist and possibly re-stating to get
 * current (not cached) info.  Returns the new count of valid statfs bufs.
 */
long
regetmntinfo(struct statfs **mntbufp, long mntsize, char **vfslist)
{
	int i, j;
	struct statfs *mntbuf;

	if (vfslist == NULL)
		return (nflag ? mntsize : getmntinfo(mntbufp, MNT_WAIT));

	mntbuf = *mntbufp;
	for (j = 0, i = 0; i < mntsize; i++) {
		if (checkvfsname(mntbuf[i].f_fstypename, vfslist))
			continue;
		if (!nflag)
			(void)statfs(mntbuf[i].f_mntonname,&mntbuf[j]);
		else if (i != j)
			mntbuf[j] = mntbuf[i];
		j++;
	}
	return (j);
}

/*
 * Output in "human-readable" format.  Uses 3 digits max and puts
 * unit suffixes at the end.  Makes output compact and easy to read,
 * especially on huge disks.
 *
 */
unit_t
unit_adjust(double *val)
{
	double abval;
	unit_t unit;
	unsigned int unit_sz;

	abval = fabs(*val);

	unit_sz = abval ? ilogb(abval) / 10 : 0;

	if (unit_sz >= UNIT_MAX) {
		unit = NONE;
	} else {
		unit = unitp[unit_sz];
		*val /= (double)valp[unit_sz];
	}

	return (unit);
}

void
prthuman(struct statfs *sfsp, long used)
{

	prthumanval((double)sfsp->f_blocks * (double)sfsp->f_bsize);
	prthumanval((double)used * (double)sfsp->f_bsize);
	prthumanval((double)sfsp->f_bavail * (double)sfsp->f_bsize);
}

void
prthumanval(double bytes)
{

	unit_t unit;
	unit = unit_adjust(&bytes);

	if (bytes == 0)
		(void)printf("     0B");
	else if (bytes > 10)
		(void)printf(" %5.0f%c", bytes, "BKMGTPE"[unit]);
	else
		(void)printf(" %5.1f%c", bytes, "BKMGTPE"[unit]);
}

/*
 * Convert statfs returned filesystem size into BLOCKSIZE units.
 * Attempts to avoid overflow for large filesystems.
 */
#define fsbtoblk(num, fsbs, bs) \
	(((fsbs) != 0 && (fsbs) < (bs)) ? \
		((off_t)((unsigned)num)) / ((unsigned)(bs) / ((off_t)((unsigned)fsbs))) : ((off_t)((unsigned)num)) * ((off_t)((unsigned)fsbs)) / (bs))

/*
 * Print out status about a filesystem.
 */
void
prtstat(struct statfs *sfsp, struct maxwidths *mwp)
{
	static long blocksize;
	static int headerlen, timesthrough;
	static const char *header;
	unsigned long used, availblks, inodes;

	if (++timesthrough == 1) {
		mwp->mntfrom = imax(mwp->mntfrom, strlen("Filesystem"));
		if (hflag) {
			header = "  Size";
			mwp->total = mwp->used = mwp->avail = strlen(header);
		} else {
			header = getbsize(&headerlen, &blocksize);
			mwp->total = imax(mwp->total, headerlen);
		}
		mwp->used = imax(mwp->used, strlen("Used"));
		mwp->avail = imax(mwp->avail, strlen("Avail"));

		(void)printf("%-*s %-*s %*s %*s Capacity", mwp->mntfrom,
		    "Filesystem", mwp->total, header, mwp->used, "Used",
		    mwp->avail, "Avail");
		if (iflag) {
			mwp->iused = imax(mwp->iused, strlen("  iused"));
			mwp->ifree = imax(mwp->ifree, strlen("ifree"));
			(void)printf(" %*s %*s %%iused", mwp->iused - 2,
			    "iused", mwp->ifree, "ifree");
		}
		(void)printf("  Mounted on\n");
	}

	(void)printf("%-*s", mwp->mntfrom, sfsp->f_mntfromname);
	used = (off_t)((unsigned)sfsp->f_blocks) - (off_t)((unsigned)sfsp->f_bfree);
	availblks = sfsp->f_bavail + used;
	if (hflag) {
		prthuman(sfsp, used);
	} else {
		(void)printf(" %*lld %*lld %*lld", mwp->total,
	            fsbtoblk(sfsp->f_blocks, sfsp->f_bsize, blocksize),
		    mwp->used, fsbtoblk(used, sfsp->f_bsize, blocksize),
	            mwp->avail, fsbtoblk(sfsp->f_bavail, sfsp->f_bsize,
		    blocksize));
	}
	(void)printf(" %5.0f%%",
	    availblks == 0 ? 100.0 : (double)used / (double)availblks * 100.0);
	if (iflag) {
		inodes = (unsigned)sfsp->f_files;
		used = inodes - sfsp->f_ffree;
		(void)printf(" %*lu %*lu %4.0f%% ", mwp->iused, used,
		    mwp->ifree, (unsigned long)sfsp->f_ffree, inodes == 0 ? 100.0 :
		    (double)used / (double)inodes * 100.0);
	} else
		(void)printf("  ");
	(void)printf("  %s\n", sfsp->f_mntonname);
}

/*
 * Update the maximum field-width information in `mwp' based on
 * the filesystem specified by `sfsp'.
 */
void
update_maxwidths(struct maxwidths *mwp, struct statfs *sfsp)
{
	static long blocksize;
	int dummy;

	if (blocksize == 0)
		getbsize(&dummy, &blocksize);

	mwp->mntfrom = imax(mwp->mntfrom, strlen(sfsp->f_mntfromname));
	mwp->total = imax(mwp->total, longwidth(fsbtoblk(sfsp->f_blocks,
	    sfsp->f_bsize, blocksize)));
	mwp->used = imax(mwp->used, longwidth(fsbtoblk(sfsp->f_blocks -
	    sfsp->f_bfree, sfsp->f_bsize, blocksize)));
	mwp->avail = imax(mwp->avail, longwidth(fsbtoblk(sfsp->f_bavail,
	    sfsp->f_bsize, blocksize)));
	mwp->iused = imax(mwp->iused, longwidth((unsigned)(sfsp->f_files -
	    sfsp->f_ffree)));
	mwp->ifree = imax(mwp->ifree, longwidth((unsigned)(sfsp->f_ffree)));
}

/* Return the width in characters of the specified long. */
int
longwidth(long long val)
{
	int len;

	len = 0;
	/* Negative or zero values require one extra digit. */
	if (val <= 0) {
		val = -val;
		len++;
	}
	while (val > 0) {
		len++;
		val /= 10;
	}

	return (len);
}

/*
 * This code constitutes the pre-system call Berkeley df code for extracting
 * information from filesystem superblocks.
 */

union {
	struct fs iu_fs;
	char dummy[SBSIZE];
} sb;
#define sblock sb.iu_fs

int	rfd;

int
ufs_df(char *file, struct maxwidths *mwp)
{
	struct statfs statfsbuf;
	struct statfs *sfsp;
	const char *mntpt;
	static int synced;

	if (synced++ == 0)
		sync();

	if ((rfd = open(file, O_RDONLY)) < 0) {
		warn("%s", file);
		return (1);
	}
	if (bread((off_t)SBOFF, &sblock, SBSIZE) == 0) {
		(void)close(rfd);
		return (1);
	}
	sfsp = &statfsbuf;
	sfsp->f_type = 1;
	strcpy(sfsp->f_fstypename, "ufs");
	sfsp->f_flags = 0;
	sfsp->f_bsize = sblock.fs_fsize;
	sfsp->f_iosize = sblock.fs_bsize;
	sfsp->f_blocks = sblock.fs_dsize;
	sfsp->f_bfree = sblock.fs_cstotal.cs_nbfree * sblock.fs_frag +
		sblock.fs_cstotal.cs_nffree;
	sfsp->f_bavail = freespace(&sblock, sblock.fs_minfree);
	sfsp->f_files =  sblock.fs_ncg * sblock.fs_ipg;
	sfsp->f_ffree = sblock.fs_cstotal.cs_nifree;
	sfsp->f_fsid.val[0] = 0;
	sfsp->f_fsid.val[1] = 0;
	if ((mntpt = getmntpt(file)) == 0)
		mntpt = "";
	memmove(&sfsp->f_mntonname[0], mntpt, (size_t)MNAMELEN);
	memmove(&sfsp->f_mntfromname[0], file, (size_t)MNAMELEN);
	prtstat(sfsp, mwp);
	(void)close(rfd);
	return (0);
}

int
bread(off_t off, void *buf, int cnt)
{
	ssize_t nr;

	(void)lseek(rfd, off, SEEK_SET);
	if ((nr = read(rfd, buf, (size_t)cnt)) != (ssize_t)cnt) {
		/* Probably a dismounted disk if errno == EIO. */
		if (errno != EIO)
			(void)fprintf(stderr, "\ndf: %lld: %s\n",
			    (long long)off, strerror(nr > 0 ? EIO : errno));
		return (0);
	}
	return (1);
}

void
usage(void)
{

	char *t_flag = COMPAT_MODE("bin/df", "unix2003") ? "[-t]" : "[-t type]";
	(void)fprintf(stderr,
	    "usage: df [-b | -H | -h | -k | -m | -P] [-ailn] [-T type] %s [file | filesystem ...]\n", t_flag);
	exit(EX_USAGE);
}

char *
makenetvfslist(void)
{
	char *str, *strptr, **listptr;
#ifndef __APPLE__
	int mib[3], maxvfsconf, cnt=0, i;
	size_t miblen;
	struct ovfsconf *ptr;
#else
	int mib[4], maxvfsconf, cnt=0, i;
	size_t miblen;
	struct vfsconf vfc;
#endif

	mib[0] = CTL_VFS; mib[1] = VFS_GENERIC; mib[2] = VFS_MAXTYPENUM;
	miblen=sizeof(maxvfsconf);
	if (sysctl(mib, (unsigned int)(sizeof(mib) / sizeof(mib[0])),
	    &maxvfsconf, &miblen, NULL, 0)) {
		warnx("sysctl failed");
		return (NULL);
	}

	if ((listptr = malloc(sizeof(char*) * maxvfsconf)) == NULL) {
		warnx("malloc failed");
		return (NULL);
	}

#ifndef __APPLE__
	for (ptr = getvfsent(); ptr; ptr = getvfsent())
		if (ptr->vfc_flags & VFCF_NETWORK) {
			listptr[cnt++] = strdup(ptr->vfc_name);
			if (listptr[cnt-1] == NULL) {
				warnx("malloc failed");
				return (NULL);
			}
		}
#else
	miblen = sizeof (struct vfsconf);
	mib[2] = VFS_CONF;
        for (i = 0; i < maxvfsconf; i++) {
		mib[3] = i;
		if (sysctl(mib, 4, &vfc, &miblen, NULL, 0) == 0) {
	                if (!(vfc.vfc_flags & MNT_LOCAL)) {
	                        listptr[cnt++] = strdup(vfc.vfc_name);
	                        if (listptr[cnt-1] == NULL) {
	                                warnx("malloc failed");
	                                return (NULL);
	                        }
	                }
		}
	}
#endif

	if (cnt == 0 ||
	    (str = malloc(sizeof(char) * (32 * cnt + cnt + 2))) == NULL) {
		if (cnt > 0)
			warnx("malloc failed");
		free(listptr);
		return (NULL);
	}

	*str = 'n'; *(str + 1) = 'o';
	for (i = 0, strptr = str + 2; i < cnt; i++, strptr++) {
		strncpy(strptr, listptr[i], 32);
		strptr += strlen(listptr[i]);
		*strptr = ',';
		free(listptr[i]);
	}
	*(--strptr) = '\0';

	free(listptr);
	return (str);
}
