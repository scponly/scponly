/*
 *	scponly.c
 *
 * 	http://sublimation.org/scponly
 *	rumblefish@escapeartist.com
 *	jan 28 2001
 */

#include <stdio.h>	// io
#include <string.h>	// for str*
#include <unistd.h>	// for exit
#include <stdlib.h>	// atexit
#include <errno.h>
#include "scponly.h"

FILE *log=NULL;
char username[MAX_USERNAME];
char homedir[FILENAME_MAX];

int process_ssh_request(char *request);

void close_logfile (void)
{
	fclose(log);
}

int main (int argc, char **argv) 
{

#ifdef DEBUG
	int i;

	fprintf (stderr, "%d arguments in total.\n",argc);
	for (i=0;i<argc;i++)
		fprintf (stderr, "\targ %u is %s\n",i,argv[i]);
#endif
	if (NULL==(log=fopen(LOGFILE,"a+")))
	{
#ifdef DEBUG
		fprintf (stderr,"couldn't open logfile %s\n",LOGFILE);
#endif
		perror("fopen");
		exit(-1);
	}
#ifdef DEBUG
	fprintf (stderr,"opened logfile %s\n",LOGFILE);
#endif
	atexit(close_logfile);
	if (getuid()==0)
	{	
		log_stamp();
		fprintf(log,"root login denied\n");
		fprintf(stderr,"root login denied\n");
		exit(-1);
	}
	if (!get_uservar())
	{
		fprintf (stderr, "%s is misconfigured. contact sysadmin.\n",argv[0]);
		exit (-1);
	}
	if (argc!=3)
	{
		show_usage();
		exit(-1);
	}
#ifdef CHROOT
#ifdef DEBUG
        fprintf (stderr,"trying chroot\n");
#endif
        if (-1==(chroot(homedir)))
        {
#ifdef DEBUG
                fprintf (stderr,"couldn't chroot errno=%d\n",errno);
                fflush(stderr);
                perror("chroot");
#endif
                log_stamp();
                fprintf(log,"couldn't chroot to %s\n",homedir);
                exit(-1);
        }
#endif
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
#ifdef DEBUG
	fprintf (stderr,"scponly completed\n");
#endif
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
#ifdef DEBUG
		perror("system");	
#endif
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

	if (!valid_chars(request))
		return(-1);

	if ((exact_match(request,"ls")) || 
	   (NULL!=strend(request,"ls ")) || 
	   (exact_match(SCP2_ARG,request)) || 
	   (NULL!=strend(request,"scp ")) ||
	   (exact_match(request,"pwd")) ||
          (NULL!=strend(request,"chmod ")) ||
           (NULL!=strend(request,"mkdir ")) ||
           (NULL!=strend(request,"rm ")) ||
          (NULL!=strend(request,"rmdir "))) 
		exit(exec_request(request));

	show_usage();
	log_stamp();
	fprintf (log,"denied request: %s\n",request);
	fflush(log);
	exit(-1);
}
