/*
 *	scponly.c
 *
 * 	http://sublimation.org/scponly
 *	joe@sublimation.org
 */

#include <stdio.h>	// io
#include <string.h>	// for str*
#include <sys/types.h>	// for fork, wait
#include <sys/wait.h>	// for wait
#include <unistd.h>	// for exit, fork
#include <stdlib.h>	// EXIT_*
#include <errno.h>
#include <syslog.h>
#include "scponly.h"

int debuglevel=0;
int winscp_mode=0;
int chrooted=0;
char username[MAX_USERNAME];
char homedir[FILENAME_MAX];
char chrootdir[FILENAME_MAX];

cmd_t commands[] =
{ 
#ifdef ENABLE_SFTP
	{ PROG_SFTP_SERVER, 0 },
#endif /*ENABLE_SFTP*/

#ifdef ENABLE_SCP2
	{ PROG_LS, 1 }, 
	{ PROG_CHMOD, 1 },
	{ PROG_CHOWN, 1 },
	{ PROG_MKDIR, 1 },
	{ PROG_RMDIR, 1 },
	{ PROG_SCP, 1 },
	{ PROG_LN, 1 },
	{ PROG_MV, 1 },
	{ PROG_RM, 1 },
	{ PROG_CD, 1 },
#endif /*ENABLE_SCP2*/

#ifdef WINSCP_COMPAT
	{ PROG_GROUPS, 0 },
	{ PROG_PWD, 0 },
	{ PROG_ECHO, 1 },
#endif /*WINSCP_COMPAT*/

#ifdef RSYNC_COMPAT
	{ PROG_RSYNC, 1 },
#endif /*ENABLE_RSYNC*/

	NULL
};

int process_ssh_request(char *request);
int winscp_regular_request(char *request);
int winscp_transit_request(char *request);
int process_winscp_requests(void);

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
#ifndef SOLARIS_COMPAT
	if (debuglevel > 1) // debuglevel 1 will still log to syslog
		logopts |= LOG_PERROR;
#endif

#ifdef SOLARIS_COMPAT 
        openlog(PACKAGE_NAME, logopts, LOG_AUTH);
#elif IRIX_COMPAT
        openlog(PACKAGE_NAME, logopts, LOG_AUTH);
#else
        if (debuglevel > 1) // debuglevel 1 will still log to syslog
                logopts |= LOG_PERROR;
        openlog(PACKAGE_NAME, logopts, LOG_AUTHPRIV);
#endif

#ifdef CHROOTED_NAME
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
#endif //CHROOTED_NAME

	if (debuglevel)
	{
		int i;
		syslog(LOG_DEBUG, "%d arguments in total.", argc);
		for (i=0;i<argc;i++)
			syslog(LOG_DEBUG, "\targ %u is %s", i, argv[i]);
	}
        if (debuglevel)
#ifdef SOLARIS_COMPAT
                syslog(LOG_DEBUG, "opened log at LOG_AUTH, opts 0x%08x", logopts);
#else
                syslog(LOG_DEBUG, "opened log at LOG_AUTHPRIV, opts 0x%08x", logopts);
#endif

	if (getuid()==0)
	{	
		syslog(LOG_ERR, "root login denied [%s]", logstamp());
		exit(EXIT_FAILURE);
	}

#ifdef WINSCP_COMPAT
	if ((argc!=3) && (argc!=1))
#else
	if (argc!=3)
#endif
	{
		if (debuglevel)
			syslog (LOG_ERR, "incorrect number of args");
		exit(EXIT_FAILURE);
	}
	if (!get_uservar())
	{
		syslog (LOG_ERR, "%s is misconfigured. contact sysadmin.", argv[0]);
		exit (EXIT_FAILURE);
	}

#ifdef CHROOTED_NAME
	if (chrooted)
	{
		char *root_dir = chrootdir;

		strcpy(chrootdir, homedir);
		while((root_dir = strchr(root_dir, '/')) != NULL) 
		{
			if (strncmp(root_dir, "//", 2) == 0) 
			{
				*root_dir = '\0';
				break;
			}
			root_dir++;
		}
		if (debuglevel)
			syslog (LOG_DEBUG, "chrooting to dir: \"%s\"", chrootdir);
		if (-1==(chroot(chrootdir)))
		{
			if (debuglevel)
			{
				syslog (LOG_ERR, "chroot: %m");
			}
			syslog (LOG_ERR, "couldn't chroot to %s [%s]", chrootdir, logstamp());
			exit(EXIT_FAILURE);
		}
	}
#endif //CHROOTED_NAME

	if (debuglevel)
		syslog (LOG_DEBUG, "setting uid to %u", getuid());
	if (-1==(seteuid(getuid())))
	{
		syslog (LOG_ERR, "couldn't revert to my real uid. seteuid: %m");
		exit(EXIT_FAILURE);
	}

#ifdef WINSCP_COMPAT
	if (argc==1)
	{
		if (debuglevel)
			syslog (LOG_DEBUG, "entering WinSCP compatibility mode [%s]",logstamp());
		if (-1==process_winscp_requests())
		{
			syslog(LOG_ERR, "failed WinSCP compatibility mode [%s]", logstamp());
			exit(EXIT_FAILURE);
		}
	}
#else
	if (0)	{}	// placeholder
#endif
	else if (-1==process_ssh_request(argv[2]))
	{
		syslog(LOG_ERR, "bad request: %s [%s]", argv[2], logstamp());
		exit(EXIT_FAILURE);
	}
	if (debuglevel)
		syslog(LOG_DEBUG, "scponly completed");
	exit(EXIT_SUCCESS);
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
		new_request=strend(request, WINSCP_EOF_REQ_STATUS);
		if (NULL == new_request)
		{
			new_request=strend(request, WINSCP_EOF_REQ_RETVAL);
			if (NULL == new_request)
			{
				printf ("command wasn't terminated with %s, %s or %s\n",
					WINSCP_EOF_REQ_RETVAL, WINSCP_EOF_REQ_ZERO, WINSCP_EOF_REQ_STATUS);
				return(-1);	// bogus termination
			}
		}
	}
	/*
	 *	here is where we fool winscp clients into believing we are a real shell
	 */
	if ((exact_match(new_request, "echo \"$status\"")) ||
		(exact_match(new_request, "echo \"$?\"")))
	{
		printf ("0\n");
		fflush(stdout);
	}
#if 0
	else if (exact_match(new_request, "groups"))
	{
		printf("joe wheel php\n");
		fflush(stdout);
	}
#endif
	/*
	 *  ignore unalias and unset commands
	 */
	else if ((NULL!=strbeg(new_request,"unset ")) ||
		(NULL!=strbeg(new_request,"unalias ")))
		retval=0;
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

	fflush(stdout);

	/*
	 *	now process commands interactively
 	 */
        while (fgets(&linebuf[0],MAX_REQUEST, stdin) != NULL)
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

int process_ssh_request(char *request)
{
	char **av;
	char *flat_request,*tmpstring;
	int retval;
        int reqlen=strlen(request);

	if (debuglevel)
		syslog(LOG_DEBUG, "processing request: \"%s\"\n", request);

#ifdef GFTP_COMPAT 
	/*
	 *	gFTP compatibility hack
	 */
	if (NULL != (tmpstring=strbeg(request, "echo -n xsftp ; ")))
	{
		request=tmpstring;
		printf("xsftp");
		fflush(stdout);
	}
#endif

	/*
	 * we flat out reject special chars
	 */
	if (!valid_chars(request))
		return(-1);

#ifdef WINSCP_COMPAT
        if (strbeg(request,PROG_CD))
        {
                char *destdir=(char *)malloc(reqlen);
                if (destdir == NULL)
                {
                        perror("malloc");
                        exit(EXIT_FAILURE);
                }

                /*
                 * well, now that scponly is a persistent shell
                 * i have to maintain a $PWD.  damn.
                 * we're going to INSIST upon a double quote
                 * encapsulated new directory to change to.
                 */
                if ((request[(reqlen-1)]=='"') && (request[3]=='"'))
                {
                        bzero(destdir,reqlen);
                        strncpy(destdir,&request[4],reqlen-5);
                        if (debuglevel)
                                syslog(LOG_INFO, "chdir: %s (%s)", request, logstamp());
                        retval=chdir(destdir);
                        free(destdir);
                        return(retval);
                }
		syslog(LOG_ERR, "bogus chdir request: %s (%s)", request, logstamp());
		return(-1);
	}
#endif

	/*
	 * convert request string to an arg_vector
	 */
	av = build_arg_vector(request);

	/*
	 * clean any path info from request and substitute our known pathnames
	 */
	av[0] = substitute_known_path(av[0]);

	/*
	 * we only process wildcards for scp commands
	 */
#ifdef ENABLE_WILDCARDS
#ifdef ENABLE_SCP2
	if (exact_match(av[0],PROG_SCP))
		av = expand_wildcards(av);
#endif
#endif

	flat_request = flatten_vector(av);

	if (valid_arg_vector(av))
	{
		syslog(LOG_INFO, "running: %s (%s)", flat_request, logstamp());
#ifdef WINSCP_COMPAT
		if (winscp_mode)
		{
			int status=0;
			if (fork() == 0)
				retval=execve(av[0],av,NULL);
			else
			{
				wait(&status);
				fflush(stdout);
				fflush(stderr);
				discard_vector(av);
				free(flat_request);
				return(WEXITSTATUS(status));
			}
		}
		else
#endif
		{
			retval=execve(av[0],av,NULL);
		}
		syslog(LOG_ERR, "failed: %s with error %s(%u) (%s)", flat_request, strerror(errno), errno, logstamp());
		free(flat_request);
		discard_vector(av);
#ifdef WINSCP_COMPAT
		if (winscp_mode)
			return(-1);
		else
#endif 
			exit(errno);
	}

	/*
	 *	reaching this point in the code means the request isnt one of
	 *	our accepted commands
 	 */
	if (debuglevel)
	{
		if (exact_match(flat_request,request))
			syslog (LOG_ERR, "denied request: %s [%s]", request, logstamp());
		else
			syslog (LOG_ERR, "denied request: %s (resolved to: %s) [%s]", request, flat_request, logstamp());
	}
	free(flat_request); 
#ifdef WINSCP_COMPAT
	if (winscp_mode)
	{
		printf ("command not permitted by scponly\n");
		return(-1);
	}
	else
#endif 
		exit(EXIT_FAILURE);
}

