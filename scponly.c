/*
 *	scponly.c
 *
 * 	http://sublimation.org/scponly
 *	joe@sublimation.org
 *
 *	version 2.1: jul 5nd, 2002
 */

#include <stdio.h>	// io
#include <string.h>	// for str*
#include <unistd.h>	// for exit
#include <stdlib.h>	// atexit
#include <errno.h>
#include <syslog.h>
#include "scponly.h"

int debuglevel=0;
int winscp_mode=0;
int chrooted=0;
char username[MAX_USERNAME];
char homedir[FILENAME_MAX];

int process_ssh_request(char *request);
int winscp_regular_request(char *request);
int winscp_transit_request(char *request);
int process_winscp_requests(void);

void cleanup (void)
{
	closelog();
}

int main (int argc, char **argv) 
{
	FILE *debugfile;
	int logopts = LOG_PID|LOG_NDELAY;
	
	/*
	 * set debuglevel.  any nonzero number will result in debugging info to log
	 */
	if (NULL!=(debugfile=fopen(DEBUGFILE,"r")))
	{
		fscanf(debugfile,"%u",&debuglevel);
		fclose(debugfile);
	}
	if (debuglevel > 1) // debuglevel 1 will still log to syslog
		logopts |= LOG_PERROR;

	openlog(LOGIDENT, logopts, LOG_AUTHPRIV);

	/*
	 *	is this a chroot'ed scponly installation?
	 */
#ifdef WINSCP_COMPAT
	if ((argc==3 && (0==strncmp(argv[0],CHROOTED_NAME,FILENAME_MAX)) ) || 
		( argc==1 && (0==strncmp(&argv[0][1],CHROOTED_NAME,FILENAME_MAX ))))

#else
	if (0==strncmp(argv[0],CHROOTED_NAME,FILENAME_MAX))
#endif
	{
		if (debuglevel)
			syslog(LOG_INFO, "chrooted binary in place, will chroot()");
		chrooted=1;
	}

	if (debuglevel)
	{
		int i;
		syslog(LOG_DEBUG, "%d arguments in total.", argc);
		for (i=0;i<argc;i++)
			syslog(LOG_DEBUG, "\targ %u is %s", i, argv[i]);
	}
	if (debuglevel)
		syslog(LOG_DEBUG, "opened log at LOG_AUTHPRIV, opts 0x%08x", logopts);
	atexit(cleanup);

	if (getuid()==0)
	{	
		syslog(LOG_ERR, "root login denied [%s]", logstamp());
		exit(-1);
	}

#ifdef WINSCP_COMPAT
	if ((argc!=3) && (argc!=1))
#else
	if (argc!=3)
#endif
	{
		if (debuglevel)
			syslog (LOG_ERR, "incorrect number of args");
		exit(-1);
	}
	if (!get_uservar())
	{
		syslog (LOG_ERR, "%s is misconfigured. contact sysadmin.", argv[0]);
		exit (-1);
	}

	if (chrooted)
	{
		if (debuglevel)
			syslog (LOG_DEBUG, "chrooting to dir: \"%s\"", homedir);
		if (-1==(chroot(homedir)))
		{
			if (debuglevel)
			{
				syslog (LOG_ERR, "chroot: %m");
			}
			syslog (LOG_ERR, "couldn't chroot to %s [%s]", homedir, logstamp());
			exit(-1);
		}
	}
	if (debuglevel)
		syslog (LOG_DEBUG, "setting uid to %u", getuid());
	if (-1==(seteuid(getuid())))
	{
		syslog (LOG_ERR, "couldn't revert to my real uid. seteuid: %m");
		exit(-1);
	}
#ifdef WINSCP_COMPAT
	if (argc==1)
	{
		if (debuglevel)
		{
			syslog (LOG_DEBUG, "entering WinSCP compatibility mode [%s]",logstamp());
			printf ("entering WinSCP compatibility mode\n");
		}
		if (-1==process_winscp_requests())
		{
			syslog(LOG_ERR, "failed WinSCP compatibility mode [%s]", logstamp());
			exit(-1);
		}
	}
#else
	if (0)	{}	// placeholder
#endif
	else if (-1==process_ssh_request(argv[2]))
	{
		syslog(LOG_ERR, "bad request: %s [%s]", argv[2], logstamp());
		exit(-1);
	}
	if (debuglevel)
		syslog(LOG_DEBUG, "scponly completed");
	exit(0);
}

#ifdef WINSCP_COMPAT

int winscp_transit_request(char *request)
{
	char *new_request=NULL;

	/*
	 * for scp -t and scp -f commands, winscp preceeds each
	 * command with a "begin-of-file" marker we need to
	 * provide, everything after that can be handled by
	 * winscp_regular_request.
	 */

	new_request=strbeg(request,WINSCP_BOF_REQ);
	if (NULL == new_request)
	{
		return(-1);	 // improper transit cmd
	}
	printf ("%s\n",WINSCP_BOF); // start transfer
	fflush(stdout);
	return(winscp_regular_request(new_request));
}

int winscp_regular_request(char *request)
{
	/*
	 * winscp uses one of two requests to terminate each command.
	 * we must determine which (if any) is terminating this request. fun!
	 */
	char *new_request=NULL;
	int retval=0;
	int retzero=1;
	new_request=strend(request, WINSCP_EOF_REQ_ZERO);
	if (NULL == new_request)
	{
		retzero=0;
		new_request=strend(request, WINSCP_EOF_REQ_RETVAL);
		if (NULL == new_request)
		{
			printf ("command wasn't terminated with %s or %s\n",
				WINSCP_EOF_REQ_RETVAL, WINSCP_EOF_REQ_ZERO);
			return(-1);	// bogus termination
		}
	}
	/*
	 *  ignore unalias and unset commands
	 */
//	fprintf (stderr, "new_request: %s\n",new_request);
	if ((NULL!=strbeg(new_request,"unset ")) ||
		(NULL!=strbeg(new_request,"unalias ")))
	{
		retval=0;
	}
	else
	{
		retval=process_ssh_request(new_request);
		if (retzero)		// ignore actual retval if winscp wants us to
			retval=0;
	}
	free(new_request);
	return(retval);
}

int process_winscp_requests(void)
{
        char    linebuf[MAX_REQUEST];
	int	count=0;		// num of semicolons in cmd
	int	ack=0;

	winscp_mode=1;

	//printf ("%s0\n",WINSCP_EOF); // reply to initial echo request
	fflush(stdout);

	/*
	 *	now process commands interactively
 	 */
        while (fgets(&linebuf[0],sizeof(linebuf), stdin) != NULL)
	{
		ack=0;

		if (strlen(linebuf)==0)
			return(-1);

		linebuf[strlen(linebuf)-1]=0;	// drop the trailing CR
		count=cntchr(linebuf,';');

		if (count==1)	// regular cmd
		{
			ack=winscp_regular_request(linebuf);
		}
		else if (count==2)	 // transit command
		{
			ack=winscp_transit_request(linebuf);
		}
		else
			ack=0; 	// winscp always sends 2 or 3 cmds at once

		printf ("%s%d\n",WINSCP_EOF,ack); // respond to client
		fflush(stdout);
	}
	return 0;
}

#endif

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
	syslog(LOG_INFO, "running: %s (%s)",tempbuf, logstamp());

	if (err=system(tempbuf))
	{
		if (debuglevel)
			syslog(LOG_ERR, "system: %m");
		syslog(LOG_ERR, "system() fail: %s, %s errno=%d [%s]\n",
		       request,strerror(errno),errno, logstamp());
	}
	return err;
}

int process_ssh_request(char *request)
{
	if (debuglevel)
		syslog(LOG_DEBUG, "processing request: \"%s\"\n", request);

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

	if (NULL!=strbeg(request,"cd "))
	{
#ifdef WINSCP_COMPAT
		int retval;
		char *destdir=NULL;
		int reqlen=strlen(request);
		destdir=(char *)malloc(reqlen);
		if (destdir == NULL)
		{
			perror("malloc");
			exit(-1);
		}
		
		/*
		 * well, now that scponly is a persistent shell
		 * i have to maintain a $PWD.  damn.
		 * we're going to INSIST upon a double quote
		 * encapsulated new directory to change to.
		 */
		if (request[(reqlen-1)]=='"')
		{
			if (request[3]=='"') // check first doublequote
			{	
				bzero(destdir,reqlen);
				strncpy(destdir,&request[4],reqlen-5);
				retval=chdir(destdir);
				free(destdir);
				return(retval);
			}
		}


#endif
	}
	else if ((exact_match(request,"ls")) || 
		(exact_match(SCP2_ARG,request)) || 
#ifdef WINSCP_COMPAT
		(exact_match(request,"pwd")) ||
		(exact_match(request,"groups")) ||
		(NULL!=strbeg(request,"echo ")) ||
#endif
		(NULL!=strbeg(request,"ls ")) || 
		(NULL!=strbeg(request,"scp ")) ||
		(NULL!=strbeg(request,"rm ")) ||
		(NULL!=strbeg(request,"ln ")) ||
		(NULL!=strbeg(request,"mv ")) ||
		(NULL!=strbeg(request,"chmod ")) ||
		(NULL!=strbeg(request,"chown ")) ||
		(NULL!=strbeg(request,"mkdir ")) ||
		(NULL!=strbeg(request,"rmdir "))) 
			return(exec_request(request));

	/*
	 *	reaching this point in the code means the request isnt one of
	 *	our accepted commands
 	 */
	if (debuglevel)
	{
		syslog (LOG_DEBUG, "denied request. not a supported command.\
supported comands are: ls, scp, %s, pwd, chmod, mkdir, rm, mv, rmdir", SCP2_ARG);
	}
	syslog (LOG_ERR, "denied request [%s]", logstamp());

#ifdef WINSCP_COMPAT
	if (winscp_mode)
	{
		printf ("command not permitted by scponly\n");
		return(-1);
	}
	else
#endif 
	{
		exit(-1);
	}
}
