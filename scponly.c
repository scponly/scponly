/*
 *	scponly.c
 *
 * 	http://sublimation.org/scponly
 *	rumblefish@escapeartist.com
 *	jan 28 2001
 *
 *	version 1.3: feb 5th, 2002
 */

#include <stdio.h>	// io
#include <string.h>	// for str*
#include <unistd.h>	// for exit
#include <stdlib.h>	// atexit
#include <errno.h>
#include "scponly.h"

FILE *log=NULL;
int debuglevel=0;
int chrooted=0;
char username[MAX_USERNAME];
char homedir[FILENAME_MAX];

int process_ssh_request(char *request);

void cleanup (void)
{
	if (log != NULL)
		fclose(log);
}

int main (int argc, char **argv) 
{
	FILE *debugfile;
	
	/*
	 * set debuglevel.  any nonzero number will result in debugging info to stderr
	 */
	if (NULL!=(debugfile=fopen(DEBUGFILE,"r")))
	{
		fscanf(debugfile,"%u",&debuglevel);
	// 	if (debuglevel)
			fprintf(stderr,"debuglevel set to %u\n",debuglevel);
		fflush(stderr);
		fclose(debugfile);
	}
	/*
	 *	is this a chroot'ed scponly installation?
	 */
	if (0==strncmp(argv[0],CHROOTED_NAME,FILENAME_MAX))
	{
		if (debuglevel)
			fprintf(stderr,"chrooted binary in place, will chroot()\n");
		fflush(stderr);
		chrooted=1;
	}

	if (debuglevel)
	{
		int i;
		fprintf (stderr, "%d arguments in total.\n",argc);
		for (i=0;i<argc;i++)
			fprintf (stderr, "\targ %u is %s\n",i,argv[i]);
		fflush(stderr);
	}
	if (NULL==(log=fopen(LOGFILE,"a+")))
	{
		if (debuglevel) 
		{
			fprintf (stderr,"couldn't open logfile for writing: \"%s\"\n",LOGFILE);
			perror("fopen");
		}
		exit(-1);
	}
	if (debuglevel)
		fprintf (stderr,"opened logfile %s\n",LOGFILE);
	fflush(stderr);
	atexit(cleanup);
	if (getuid()==0)
	{	
		log_stamp();
		fprintf(log,"root login denied\n");
		fprintf(stderr,"root login denied\n");
		exit(-1);
	}
	if (argc!=3)
	{
		if (debuglevel)
			fprintf (stderr,"incorrect number of args\n");
		exit(-1);
	}
	if (!get_uservar())
	{
		fprintf (stderr, "%s is misconfigured. contact sysadmin.\n",argv[0]);
		exit (-1);
	}
	if (chrooted)
	{
		if (debuglevel)
			fprintf (stderr,"chrooting to dir: \"%s\"\n",homedir);
		if (-1==(chroot(homedir)))
		{
			if (debuglevel)
			{
				fprintf (stderr,"couldn't chroot errno=%d\n",errno);
				perror("chroot");
				fflush(stderr);
			}
			log_stamp();
			fprintf(log,"couldn't chroot to %s\n",homedir);
			exit(-1);
		}
	}
	if (debuglevel)
		fprintf (stderr,"setting uid to %u\n",getuid());
	fflush(stderr);
	if (-1==(seteuid(getuid())))
	{
		fprintf (stderr,"couldn't revert to my real uid\n");
		perror("seteuid");
		exit(-1);
	}
	if (-1==process_ssh_request(argv[2]))
	{
		log_stamp();
		fprintf (log,"bad request: %s\n",argv[2]);
		fflush(log);
		exit(-1);
	}
	if (debuglevel)
		fprintf (stderr,"scponly completed\n");
	fflush(stderr);
	exit(0);
}

int exec_request(char *request)
{
	char tempbuf[MAX_REQUEST];
	int err;

	/*
	 * the following little nugget prevents
	 * attempts to overflow the cmd buf
	 */
	strncpy(tempbuf,request,MAX_REQUEST);
	tempbuf[MAX_REQUEST-1]='\0';

	/*
	 * I realize system is a lame version of
	 * execve, but I actually dont want to 
	 * handle wildcard expansion or argument
	 * parsing. leave that to /bin/sh
	 */
	if (err=system(tempbuf))
	{
		if (debuglevel)
			perror("system");	
		log_stamp();
		fprintf(log,"system() fail: %s, errno=%d\n",request,errno);
	}
	return(err);
}

int process_ssh_request(char *request)
{
	log_stamp();
	fprintf (log,"\"%s\"\n",request);
	fflush(log);

	if (debuglevel)
	{
		fprintf (stderr,"processing request: \"%s\"\n", request);
	}

	if (!valid_chars(request))
		return(-1);

	/*
	 *	some scp clients like to request a specific path for
	 *	sftp-server.  i will strip that out here and rely
	 *	upon sh's $PATH to find the binary 
	 */
	request=clean_request(request);

	/*
	 *	request must be one of a few commands.
	 *	some commands must be exact matches.
	 *	others only require the beginning of the string to match.
	 */

	if ((exact_match(request,"ls")) || 
		(NULL!=strend(request,"ls ")) || 
		(exact_match(SCP2_ARG,request)) || 
		(NULL!=strend(request,"scp ")) ||
		(exact_match(request,"pwd")) ||
		(NULL!=strend(request,"chmod ")) ||
		(NULL!=strend(request,"mkdir ")) ||
		(NULL!=strend(request,"rm ")) ||
		(NULL!=strend(request,"mv ")) ||
		(NULL!=strend(request,"rmdir "))) 
			exit(exec_request(request));

	/*
	 *	reaching this point in the code means the request isnt one of
	 *	our accepted commands
 	 */
	if (debuglevel)
	{
		fprintf (stderr,"denied request. not a supported command.\n");
		fprintf (stderr,"supported comands are: ls, scp, %s, pwd, chmod, mkdir, rm, mv, rmdir\n",SCP2_ARG);
	}
	log_stamp();
	fprintf (log,"denied request\n");
	exit(-1);
}
