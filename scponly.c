/*
 *	scponly.c
 *
 * 	http://sublimation.org/scponly
 *	joe@sublimation.org
 *
 *	see CONTRIB for additional credits
 */
 
#include <stdio.h>	/* io */
#include <string.h>	/* for str* */
#include <sys/types.h>	/* for fork, wait, stat */
#include <sys/stat.h> /* for stat */
#include <sys/wait.h>	/* for wait */
#include <unistd.h>	/* for exit, fork */
#include <stdlib.h>	/* EXIT_* */
#include <errno.h>
#include <syslog.h>
#include "scponly.h"

int debuglevel=0;
int winscp_mode=0;
int chrooted=0;
char username[MAX_USERNAME];
char homedir[FILENAME_MAX];
char chrootdir[FILENAME_MAX];
char *safeenv[MAX_ENV];

/* will point to syslog or a noop */
void (*debug)(int priority, const char* format, ...);
int (*scponly_getopt_long)(
		int argc,
		char * const argv[],
		const char *optstring,
		const struct option *longopts,
		int *longindex
		);

cmd_t commands[] =
{ 
#ifdef ENABLE_SFTP
	{ PROG_SFTP_SERVER, 1 },
#endif /*ENABLE_SFTP*/
#ifdef ENABLE_SCP2
	{ PROG_LS, 1 }, 
	{ PROG_CHMOD, 1 },
	{ PROG_CHOWN, 1 },
	{ PROG_CHGRP, 1 },
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

#ifdef UNISON_COMPAT
	{ PROG_UNISON, 1 },
#endif /*ENABLE_UNISON*/

#ifdef RSYNC_COMPAT
	{ PROG_RSYNC, 1 },
#endif /*ENABLE_RSYNC*/

#ifdef PASSWD_COMPAT
	{ PROG_PASSWD, 1 },
#endif /*ENABLE_PASSWD*/

#ifdef QUOTA_COMPAT
	{ PROG_QUOTA, 1 },
#endif /*QUOTA_COMPAT*/

#ifdef SVN_COMPAT
	{ PROG_SVN, 1 },
#endif /*ENABLE_SVN*/

#ifdef SVNSERV_COMPAT
	{ PROG_SVNSERV, 1 },
#endif /*ENABLE_SVNSERV*/
	
	{ NULL }
};

/*
 * The array of longopts to be used for validation
 * longopts := (name, has_args, *flag, val)
 */
struct option empty_longopts[] = {
	{ NULL,			0,	NULL,	0 },
	};

#ifdef RSYNC_COMPAT
struct option rsync_longopts[] = {
	/* options we need to know about that are safe */
	{"server",			0,	0,		(int)'s'},
	/* the following options have behaviors we don't want to see */
	{"rsh", 			1,	0,		(int)'r'},
	/* the following are disabled because they use daemon mode */
	{"daemon",			0,	0,		(int)'d'},
	{"rsync-path",		1,	0,		(int)'d'},
	{"address",			1,	0,		(int)'d'},
	{"port",			1,	0,		(int)'d'},
	{"sockopts",		1,	0,		(int)'d'},
	{"config",			1,	0,		(int)'d'},
	{"no-detach",		0,	0,		(int)'d'},
	{ NULL,				0,	NULL,	0 },
	};
#endif

#ifdef SVNSERV_COMPAT
struct option svnserv_longopts[] = {
	/* bad */
	{"daemon",		0,	NULL,	(int)'d' },
	{"listen-port",	1,	NULL,	(int)'d' },
	{"listen-host",	1,	NULL,	(int)'d' },
	{"foreground",	0,	NULL, 	(int)'d' },
	{"inetd",		0,	NULL, 	(int)'i' },
	{"threads",		0,	NULL,	(int)'T' },
	{"listen-once",	0,	NULL,	(int)'X' },
	/* good */
	{"read-only",	0,	NULL,	(int)'R' },
	{"help",		0,	NULL,	(int)'h' },
	{"root",		0,	NULL,	(int)'r' },
	{"tunnel",		0,	NULL,	(int)'t' },
	{"tunnel-user",	1,	NULL,	0 },
	{ NULL,			0,	NULL,	0 },
	};
#endif

#ifdef SVN_COMPAT
struct option svn_longopts[] = {
	/* bad */
	{"editor-cmd",	1,	NULL,	(int)'X' },
	{"diff-cmd",	1,	NULL,	(int)'X' },
	{"diff3-cmd",	1,	NULL,	(int)'X' },
	{"config-dir", 	1,	NULL,	(int)'X' },
	{ NULL,			0,	NULL,	0 },
	};
#endif

/*
 *	several binaries support arbitrary command execution in their arguments
 *	to prevent this we have to check the arguments for these binaries before
 *	invoking them.  
 */
cmd_arg_t dangerous_args[] =
{
	/*
	 *	'oplist' only neccesary where 'use getopt' is 1
	 *	'strict optlist' only applicable where 'use getopt?' is 1
	 *	'badarg' is a string to look for if not in rsync mode, if in rsync mode a list of invalid options
	 *
	 * program name		use getopt?		strict optlist?	badarg			optlist			longopts\n
	 */
#ifdef ENABLE_SFTP
	{ PROG_SFTP_SERVER,	1,				1,				NULL,			"f:l:u:",		empty_longopts },
#endif
#ifdef ENABLE_SCP2
	{ PROG_SCP, 		1, 				1,				"SoF",			"dfl:prtvBCc:i:P:q1246S:o:F:", empty_longopts },
#endif
#ifdef RSYNC_COMPAT
	{ PROG_RSYNC, 		1, 				0,				"rde",			"e::",			rsync_longopts },
#endif	
#ifdef UNISON_COMPAT	
	{ PROG_UNISON, 		0, 				0,				"-rshcmd",		NULL, 			empty_longopts },
	{ PROG_UNISON, 		0, 				0,				"-sshcmd",		NULL, 			empty_longopts },
	{ PROG_UNISON, 		0, 				0,				"-servercmd",	NULL, 			empty_longopts },
	{ PROG_UNISON, 		0, 				0,				"-addversionno",NULL, 			empty_longopts },
#endif
#ifdef SVNSERV_COMPAT
	{ PROG_SVNSERV,		1, 				1,				"diTX",			"dihr:RtTX",	svnserv_longopts },
#endif
#ifdef SVN_COMPAT
	{ PROG_SVN,			1, 				0,				"Xx",			"NvxuRr:qm:F:",		svn_longopts },
#endif
#ifdef QUOTA_COMPAT
	{ PROG_QUOTA,		1,				1,				NULL,			"-F:guvsilqQ",	empty_longopts },
#endif
	{ NULL }
};

/*
 *	SFTP logging requires that the following environment variables be
 *	defined in order to work:
 *
 *	LOG_SFTP
 *	USER
 *	SFTP_UMASK
 *	SFTP_PERMIT_CHMOD
 *	SFTP_PERMIT_CHOWN
 *	SFTP_LOG_LEVEL
 *	SFTP_LOG_FACILITY
 */
char * allowed_env_vars[] =
{
#ifdef SFTP_LOGGING
	"LOG_SFTP",
	"USER",
	"SFTP_UMASK",
	"SFTP_PERMIT_CHMOD",
	"SFTP_PERMIT_CHOWN",
	"SFTP_LOG_LEVEL",
	"SFTP_LOG_FACILITY",
#endif
#ifdef UNISON_COMPAT
	"HOME",
#endif
	NULL
};

int process_ssh_request(char *request);
int process_pre_chroot_request(char **av);
int winscp_regular_request(char *request);
int winscp_transit_request(char *request);
int process_winscp_requests(void);

int main (int argc, char **argv) 
{
	FILE *debugfile;
	int logopts = LOG_PID|LOG_NDELAY;
	int chars_read = 0;
#ifdef CHROOT_CHECKDIR
	struct stat	homedirstat;
#endif

	/*
	 * set debuglevel.  any nonzero number will result in debugging info to log
	 */
	if (NULL!=(debugfile=fopen(DEBUGFILE,"r")))
	{
		chars_read = fscanf(debugfile,"%d",&debuglevel);
		if (chars_read < 1)
			debuglevel = 0;
		fclose(debugfile);
	}
#ifndef UNIX_COMPAT
	if (debuglevel > 1) /* debuglevel 1 will still log to syslog */
		logopts |= LOG_PERROR;
#endif

#ifdef UNIX_COMPAT 
	openlog(PACKAGE_NAME, logopts, LOG_AUTH);
#elif IRIX_COMPAT
	openlog(PACKAGE_NAME, logopts, LOG_AUTH);
#else
	if (debuglevel > 1) /* debuglevel 1 will still log to syslog */
		logopts |= LOG_PERROR;
	openlog(PACKAGE_NAME, logopts, LOG_AUTHPRIV);
#endif

	if (debuglevel > 0)
		debug = syslog;
	else
		debug = noop_syslog;

#ifdef HAVE_GETOPT_H
	scponly_getopt_long = getopt_long;
#else
	debug(LOG_INFO, "using netbsd's bundled getopt_long");
	scponly_getopt_long = netbsd_getopt_long;
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
		debug(LOG_INFO, "chrooted binary in place, will chroot()");
		chrooted=1;
	}
#endif /* CHROOTED_NAME */

	if (debuglevel)
	{
		int i;
		syslog(LOG_DEBUG, "%d arguments in total.", argc);
		for (i=0;i<argc;i++)
			syslog(LOG_DEBUG, "\targ %u is %s", i, argv[i]);
	}

#ifdef UNIX_COMPAT
	debug(LOG_DEBUG, "opened log at LOG_AUTH, opts 0x%08x", logopts);
#else
	debug(LOG_DEBUG, "opened log at LOG_AUTHPRIV, opts 0x%08x", logopts);
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
		debug(LOG_ERR, "incorrect number of args");
		exit(EXIT_FAILURE);
	}
	if (!get_uservar())
	{
		syslog(LOG_ERR, "%s is misconfigured. contact sysadmin.", argv[0]);
		exit (EXIT_FAILURE);
	}

#ifdef CHROOTED_NAME
	if (chrooted)
	{
		char **av = NULL;
		char *tmprequest = NULL;
		char *root_dir = chrootdir;
		char chdir_path[FILENAME_MAX];
		

		strcpy(chrootdir, homedir);
		strcpy(chdir_path, "/");
		while((root_dir = strchr(root_dir, '/')) != NULL) 
		{
			if (strncmp(root_dir, "//", 2) == 0) 
			{
				snprintf(chdir_path, FILENAME_MAX, "%s", root_dir + 1);
				/* make sure HOME will be set to something correct if used*/
				debug(LOG_DEBUG, "Setting homedir to %s", chdir_path);
				strcpy(homedir, chdir_path);
				*root_dir = '\0';
				break;
			}
			root_dir++;
		}
#ifdef CHROOT_CHECKDIR
		bzero(&homedirstat, sizeof(struct stat));
		if (-1 == stat(chrootdir, &homedirstat))
		{
			syslog (LOG_ERR, "couldnt stat chroot dir: %s with errno %u", chrootdir, errno);
			exit(EXIT_FAILURE);
		}
		if (0 == (homedirstat.st_mode | S_IFDIR))
		{
			syslog (LOG_ERR, "chroot dir is not a directory: %s", chrootdir);
			exit(EXIT_FAILURE);
		}
		if (homedirstat.st_uid != 0)
		{
			syslog (LOG_ERR, "chroot dir not owned by root: %s", chrootdir);
			exit(EXIT_FAILURE);
		}
		if (0 != (homedirstat.st_mode & S_IWOTH))
		{
			syslog (LOG_ERR, "chroot dir writable by other: %s", chrootdir);
			exit(EXIT_FAILURE);
		}
		if (0 != (homedirstat.st_mode & S_IWGRP))
		{
			syslog (LOG_ERR, "chroot dir writable by group: %s", chrootdir);
			exit(EXIT_FAILURE);
		}
#endif

/* already within CHROOTED_NAME block */
#if defined(PASSWD_COMPAT) || defined(QUOTA_COMPAT)
		
		/*
		 * perhaps we need to refactor so we don't have to exit right
		 * in the middle of the code, but we can't chroot and expect to be
		 * able to change the password and have it be of any use unless
		 * there is some additional process that scponly is unaware of
		 * happening on the back end.
		 */
		tmprequest = strdup(argv[2]);
		av = build_arg_vector(tmprequest);
		free(tmprequest);
		if (
#ifdef PASSWD_COMPAT
			(exact_match(av[0],"passwd"))
			|| (exact_match(av[0],PROG_PASSWD))
#else
			0
#endif
#ifdef QUOTA_COMPAT
			|| (exact_match(av[0],"quota"))
			|| (exact_match(av[0],PROG_QUOTA))
#endif
		) {
			int status = process_pre_chroot_request(av);
			discard_vector(av);
			
			if (status) {
				syslog(LOG_ERR, "process_pre_chroot_request(%s) failed with code %i [%s]",
					argv[2],WEXITSTATUS(status),logstamp());
				exit(EXIT_FAILURE);
			}
			debug(LOG_DEBUG, "scponly completed");
			exit(EXIT_SUCCESS);
		} else {
			discard_vector(av);
		}

#endif /* passwd or quota */

		debug(LOG_DEBUG, "chrooting to dir: \"%s\"", chrootdir);
		if (-1==(chroot(chrootdir)))
		{
			debug(LOG_ERR, "chroot: %m");
			syslog(LOG_ERR, "couldn't chroot to %s [%s]", chrootdir, logstamp());
			exit(EXIT_FAILURE);
		}
		
		debug(LOG_DEBUG, "chdiring to dir: \"%s\"", chdir_path);					     
		if (-1==(chdir(chdir_path)))										   
		{													      
			debug(LOG_ERR, "chdir: %m");								 
			syslog (LOG_ERR, "couldn't chdir to %s [%s]", chdir_path, logstamp());				      
			exit(EXIT_FAILURE);     
		}
	}
#endif /* CHROOTED_NAME */

	debug(LOG_DEBUG, "setting uid to %u", getuid());
	if (-1==(seteuid(getuid())))
	{
		syslog(LOG_ERR, "couldn't revert to my real uid. seteuid: %m");
		exit(EXIT_FAILURE);
	}

#ifdef WINSCP_COMPAT
	if (argc==1)
	{
		debug(LOG_DEBUG, "entering WinSCP compatibility mode [%s]",logstamp());
		if (-1==process_winscp_requests())
		{
			syslog(LOG_ERR, "failed WinSCP compatibility mode [%s]", logstamp());
			exit(EXIT_FAILURE);
		}
	}
#else
	if (0)	{}	/*  placeholder */
#endif
	else if (-1==process_ssh_request(argv[2]))
	{
		syslog(LOG_ERR, "bad request: %s [%s]", argv[2], logstamp());
		exit(EXIT_FAILURE);
	}
	debug(LOG_DEBUG, "scponly completed");
	exit(EXIT_SUCCESS);
}

#ifdef CHROOTED_NAME
#if defined(PASSWD_COMPAT) || defined(QUOTA_COMPAT)

int process_pre_chroot_request(char ** av) {

	char *flat_request = NULL;
	char *env[2] = { NULL, NULL };
	int retval = -1;
	
	/* revert to real user so I'm not changing the passwd as root */
	debug(LOG_DEBUG, "handling pre-chroot request");
	debug(LOG_DEBUG, "setting uid to %u", getuid());

	if (-1==(seteuid(getuid())))
	{
		syslog (LOG_ERR, "couldn't revert to my real uid. seteuid: %m");
		exit(EXIT_FAILURE);
	}

	av[0] = substitute_known_path(av[0]);
	flat_request = flatten_vector(av);
	
	/* 
	 * sanity check, substitute_known_path should have given the exact path,
	 * ONLY execute if an exact match was found
	 */
	if (
#ifdef PASSWD_COMPAT
		(!exact_match(av[0], PROG_PASSWD))
#else
		1
#endif
#ifdef QUOTA_COMPAT
		&& (!exact_match(av[0], PROG_QUOTA))
#endif
	) {
		syslog(LOG_ERR, "Invalid pre-chroot request attempted: '%s' [%s]", av[0], logstamp());
		exit(EXIT_FAILURE);
	}
	
	if(check_dangerous_args(av))
	{
		syslog(LOG_ERR, "requested command (%s) tried to use disallowed argument [%s])", 
			flat_request, logstamp());
		exit(EXIT_FAILURE);
	}

	if (valid_arg_vector(av))
	{
		int status = 0;
		
		debug(LOG_DEBUG, "about to fork (%s) [%s]",flat_request,logstamp());
		
		if (fork() == 0)
			retval=execve(av[0],av,env);
		else
		{
			wait(&status);
			fflush(stdout);
			fflush(stderr);
			free(flat_request);
			
			debug(LOG_DEBUG, "forked child returned... [%s]",logstamp());
			
			return WEXITSTATUS(status);
		}

	} else {
		syslog(LOG_ERR, "invalid argument vector (%s) [%s]", 
			flat_request,logstamp());
		free(flat_request);
		
		exit(EXIT_FAILURE);
	}
	return retval;
}

#endif /* passwd or quota support */
#endif /* chrooted support */

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
		return(-1);	 /* improper transit cmd */
	}
	printf ("%s\n",WINSCP_BOF); /* start transfer */
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
				return(-1);	/* bogus termination */
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
	/*
	 *  ignore unalias and unset commands
	 */
	else if ((NULL!=strbeg(new_request,"unset ")) ||
		(NULL!=strbeg(new_request,"unalias ")))
		retval=0;
	else
	{
		retval=process_ssh_request(new_request);
		if (retzero)		/* ignore actual retval if winscp wants us to */
			retval=0;
	}
	free(new_request);
	return(retval);
}

int process_winscp_requests(void)
{
	char    linebuf[MAX_REQUEST];
	int	count=0;		/* num of semicolons in cmd */
	int	ack=0;

	winscp_mode=1;

	fflush(stdout);

#ifdef ENABLE_DEFAULT_CHDIR
	syslog(LOG_INFO, "changing initial directory to %s", DEFAULT_CHDIR);
	chdir(DEFAULT_CHDIR);
#endif

	/*
	 *	now process commands interactively
 	 */
	while (fgets(&linebuf[0],MAX_REQUEST, stdin) != NULL)
	{
		ack=0;

		if (strlen(linebuf)==0)
			return(-1);

		linebuf[strlen(linebuf)-1]=0;	/* drop the trailing CR */
		count=cntchr(linebuf,';');

		if (count==1)	/* regular cmd */
		{
			ack=winscp_regular_request(linebuf);
		}
		else if (count==2)	 /* transit command */
		{
			ack=winscp_transit_request(linebuf);
		}
		else
			ack=0; 	/* winscp always sends 2 or 3 cmds at once */

		printf ("%s%d\n",WINSCP_EOF,ack); /* respond to client */
		fflush(stdout);
	}
	return 0;
}

#endif

int process_ssh_request(char *request)
{
	char **av, **tmp_av, **tenv;
	char *flat_request,*tmpstring, *tmprequest;
	char bad_winscp3str[] = "test -x /usr/lib/sftp-server && exec /usr/lib/sftp-server test -x /usr/local/lib/sftp-server && exec /usr/local/lib/sftp-server exec sftp-server";
	int retval;
	int reqlen=strlen(request);
	char **env = NULL;

	debug(LOG_DEBUG, "processing request: \"%s\"\n", request);

	tmprequest=strdup(request);

#ifdef WINSCP_COMPAT			

	bad_winscp3str[57]=10;
	bad_winscp3str[127]=10;
			
	if(strcmp(request,bad_winscp3str)==0)
	{
	    /*
		 * switch out the command to use, winscp wont know the difference
	 	 */
		free(tmprequest);
		tmprequest=strdup(PROG_SFTP_SERVER);
	    syslog(LOG_DEBUG, "winscp3 compat correcting to: \"[%s]\"\n", PROG_SFTP_SERVER);
	}
#endif

	
#ifdef GFTP_COMPAT 
	/*
	 *	gFTP compatibility hack
	 */
	if (NULL != (tmpstring=strbeg(request, "echo -n xsftp ; ")))
	{
		free(tmprequest);
		tmprequest=strdup(tmpstring);
		printf("xsftp");
		fflush(stdout);
	}
#endif

#ifdef RESTRICTIVE_FILENAMES
	/*
	 * we flat out reject special chars
	 */
	if (!valid_chars(tmprequest))
	{
		debug(LOG_DEBUG, "rejected because of invalid chars (%s)", logstamp());
		free(tmprequest);
		return(-1);
	}
#endif

#ifdef WINSCP_COMPAT
	if (strbeg(tmprequest,PROG_CD))
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
		if ((tmprequest[(reqlen-1)]=='"') && (tmprequest[3]=='"'))
		{
			bzero(destdir,reqlen);
			strncpy(destdir,&tmprequest[4],reqlen-5);
			debug(LOG_INFO, "chdir: %s (%s)", tmprequest, logstamp());
			retval=chdir(destdir);
			free(destdir);
			free(tmprequest);
			return(retval);
		}
		syslog(LOG_ERR, "bogus chdir request: %s (%s)", tmprequest, logstamp());
		free(tmprequest);
		return(-1);
	}
#endif

	/*
	 * convert request string to an arg_vector
	 */
	av = build_arg_vector(tmprequest);

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

/*
 *	check for a compile time chdir configuration
 */
#ifdef ENABLE_DEFAULT_CHDIR
	if (exact_match(av[0],PROG_SFTP_SERVER))
	{
		syslog(LOG_INFO, "changing initial directory to %s", DEFAULT_CHDIR);
		chdir(DEFAULT_CHDIR);
	}
#endif
	

	flat_request = flatten_vector(av);

	/* 
	 * Use a temp arg vector since getopt will permute the command line arguments
	 * for anything that it does not know about.  If all rsync options are well
	 * defined this isn't necessary.
	 */
	tmp_av = build_arg_vector(flat_request);
	if(check_dangerous_args(tmp_av))
	{
		syslog(LOG_ERR, "requested command (%s) tried to use disallowed argument (%s))", 
			flat_request, logstamp());
		exit(EXIT_FAILURE);
	}
	discard_vector(tmp_av);

	if (valid_arg_vector(av))
	{

/*														   
 * Unison needs the HOME environment variable be set to the directory						  
 * where the .unison directory resides.										
 */														    
#ifdef USE_SAFE_ENVIRONMENT
		safeenv[0] = NULL;
		filter_allowed_env_vars();
		tenv = safeenv;
		if (debuglevel) {
			while (NULL != *tenv) {
				syslog(LOG_DEBUG, "Environment contains \"%s\"", *tenv++);
			}
		}
		env = safeenv;
#endif

#ifdef UNISON_COMPAT
		/* the HOME environment variable should have been set above, but I need to make sure
		 * that it's value as read from the environment is replaced with the actual value
		 * as it exists within the chroot, which is what the applications will expect to see.
		 */
		if (replace_env_entry("HOME",homedir) && (((strlen(homedir) + 6 ) > FILENAME_MAX) || !mysetenv("HOME",homedir)))
		{
			syslog(LOG_ERR, "could not set HOME environment variable (%s)", logstamp());
			exit(EXIT_FAILURE);
		}
		debug(LOG_DEBUG, "set non-chrooted HOME environment variable to %s (%s)", homedir, logstamp());
#endif 
		syslog(LOG_INFO, "running: %s (%s)", flat_request, logstamp());

#ifdef WINSCP_COMPAT
		if (winscp_mode)
		{
			int status=0;
			if (fork() == 0)
				retval=execve(av[0],av,env);
			else
			{
				wait(&status);
				fflush(stdout);
				fflush(stderr);
				discard_vector(av);
#ifdef USE_SAFE_ENVIRONMENT											    
				discard_child_vectors(safeenv);
#endif
				free(flat_request);
				free(tmprequest);
				return(WEXITSTATUS(status));
			}
		}
		else
#endif
		{
			debug(LOG_DEBUG, "about to exec \"%s\" (%s)", av[0], logstamp());
			retval=execve(av[0],av,env);
		}
		syslog(LOG_ERR, "failed: %s with error %s(%u) (%s)", flat_request, strerror(errno), errno, logstamp());
		free(flat_request);
		discard_vector(av);
#ifdef USE_SAFE_ENVIRONMENT
		discard_child_vectors(safeenv);
#endif
#ifdef WINSCP_COMPAT
		if (winscp_mode)
		{
			free(tmprequest);
			return(-1);
		}
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
		if (exact_match(flat_request,tmprequest))
			syslog (LOG_ERR, "denied request: %s [%s]", tmprequest, logstamp());
		else
			syslog (LOG_ERR, "denied request: %s (resolved to: %s) [%s]", tmprequest, flat_request, logstamp());
	}
	free(flat_request); 
#ifdef WINSCP_COMPAT
	if (winscp_mode)
	{
		printf ("command not permitted by scponly\n");
		free(tmprequest);
		return(-1);
	}
	else
#endif 
		exit(EXIT_FAILURE);
}

/* vim: set noet sw=4 ts=4: */
