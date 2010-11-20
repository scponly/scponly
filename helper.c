/*
 *	helper functions for scponly
 */
#include <stdio.h>	/* io */
#include <string.h>	/* for str* */
#include <sys/types.h>	/* for stat, getpwuid */
#include <sys/stat.h>	/* for stat */
#include <unistd.h>	/* for exit, access, getpwuid, execve, getopt */
#include <errno.h>	/* for debugging */
#include <pwd.h>	/* to get username for config parsing */
#include <time.h>	/* time */
#include <libgen.h>	/* basename */
#include <stdlib.h>	/* realloc */
#include <syslog.h>

#include "config.h"
#include "scponly.h" /* includes getopt */

#ifdef HAVE_GLOB
#include <glob.h>	/* for glob() */
#else
#ifdef HAVE_WORDEXP
#include <wordexp.h>	/* for wordexp() */
#endif
#endif

#ifdef RSYNC_COMPAT
#define RSYNC_ARG_SERVER 0x01
#define RSYNC_ARG_EXECUTE 0x02
#endif

#define MAX(x,y)	( ( x > y ) ? x : y )
#define MIN(x,y)	( ( x < y ) ? x : y )

extern int debuglevel;
extern char username[MAX_USERNAME];
extern char homedir[FILENAME_MAX];
extern cmd_t commands[];
extern cmd_arg_t dangerous_args[];
extern char * allowed_env_vars[];
extern char * safeenv[MAX_ENV];
extern void (*debug)(int priority, const char* format, ...);
extern int (*scponly_getopt_long)( int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);

#ifdef HAVE_GETOPT
extern char *optarg;
extern int optind;
#ifdef HAVE_OPTRESET
extern int optreset;
#endif
#endif

void noop_syslog(int priority, const char* format, ...)
{
	/* purposefully does nothing, used when debuglevel <= 0 */
}

#ifdef UNIX_COMPAT
char* solaris_needs_strsep(char** str, char* delims)
{
    char* tmpstr;

    if (*str==NULL) {
	return NULL;
    }

    tmpstr=*str;
    while (**str!='\0') {
	if (strchr(delims,**str)!=NULL) {
	    **str='\0';
	    (*str)++;
	    return tmpstr;
	}
	(*str)++;
    }
    *str=NULL;
    return tmpstr;
}
#endif

/*
 * perform a deep copy of an argument vector, ignoring all vector elements which begin with "--"
 */
char **strip_vector(char **sav)
{
	char 	**tmpptr=sav;  
	char 	**dav=(char **)malloc(MAX_ARGC * (sizeof(char *)));
	int		dac=0;

	bzero(dav,sizeof(dav));
    while (*tmpptr!=NULL)
	{
		if(NULL == strbeg(*tmpptr, "--"))
			dav[dac++]=strdup(*tmpptr);
		tmpptr++;
	}
	return dav;
}

void discard_vector(char **av)
{
	discard_child_vectors(av);
	free(av);
}

void discard_child_vectors(char **av)
{
	char **tmpptr=av;	
	while (*tmpptr!=NULL)
		free(*tmpptr++);
}

char *flatten_vector(char **av)
{
	char **tmpptr=av;	
	char *temp=NULL;
	char *crptr=NULL;
	char *outbuf=NULL;
	int len=0,newlen=0;

	while (*tmpptr!=NULL)
	{
		if (NULL != (crptr=strchr(*tmpptr, '\n')))
		{
			*crptr='\0';
		}
		if (outbuf!=NULL)
		{
			len = strlen(outbuf);
			newlen=len + strlen(*tmpptr)+1;
		}
		else
		{
			len = 0;
			newlen=strlen(*tmpptr);
		}
		if (NULL == (temp = realloc(outbuf, newlen + 1)))
		{
			perror("realloc");
			if (outbuf)
				free(outbuf);
			exit(EXIT_FAILURE);
		}
		outbuf=temp;
		temp=NULL;
		if (len)
		{
			outbuf[len]=' ';
			strcpy(&outbuf[len+1],*tmpptr);	
		}
		else
			strcpy(outbuf,*tmpptr);	
		tmpptr++;
	}
	return (outbuf);
}

/*
 *	since some programs support invoking other programs for their encryption
 *	(dropins to replace ssh), we must refuse to support these arguments
 *
 *	RETURN: 1 means reject this command, 0 means it is safe.
 */
int check_dangerous_args(char **av)
{
	cmd_arg_t	*cmdarg=dangerous_args;
	char		**tmpptr=av;
	int			ch;
	int			ac=0;
	int			longopt_index = 0;
#ifdef RSYNC_COMPAT
	/* 
	 * bitwise flag: 0x01 = server, 0x02 = -e.
	 * Thus 0x03 is allowed and 0x01 is allowed, but 0x02 is not allowed
	 */
	int			rsync_flags = 0; 
#endif /* RSYNC_COMPAT */

	while (cmdarg != NULL)
	{
		if (cmdarg->name == NULL)
			return 0;
		if (exact_match(cmdarg->name,av[0]))
		{
			/*
			 *	the command matches one of our dangerous commands
			 *	check the rest of the vector for dangerous command
			 *	line arguments
			 *
			 *	if the "getoptflag" is set for this command, use getopt
			 *	to determine if the flag is present, otherwise
			 *	to a string match on each element of the argument vector
			 */
			if (1 == cmdarg->getoptflag)
			{
				debug(LOG_DEBUG, "Using getopt processing for cmd %s\n (%s)", cmdarg->name, logstamp());
				/*	
				 *	first count the arguments in the vector
				 */
				tmpptr=av;
				while (*tmpptr!=NULL)
				{	
					tmpptr++;
					ac++;
				}
				/* 
				 *	now use getopt to look for our problem option
				 */
#ifdef HAVE_GETOPT
#ifdef HAVE_OPTRESET
				/*
				 * if we have optreset, use it to reset the global variables
				 */
				optreset=1;
				optind=1;
#else
				/*
				 *	otherwise, try a glibc-style reset of the global getopt vars
				 */
				optind=0;
#endif /* HAVE_OPTRESET */
				/*
				 *	tell getopt to only be strict if the 'opts' is well defined
				 */
				opterr=cmdarg->strict;
				while ((ch = scponly_getopt_long(ac, av, cmdarg->opts, cmdarg->longopts, &longopt_index)) != -1) {
					
					debug(LOG_DEBUG, "getopt processing returned '%c' (%s)", ch, logstamp());
					
#ifdef RSYNC_COMPAT
					if (exact_match(cmdarg->name, PROG_RSYNC) && (ch == 's' || ch == 'e')) {
						if (ch == 's')
							rsync_flags |= RSYNC_ARG_SERVER;
						else
							/* -e */
							rsync_flags |= RSYNC_ARG_EXECUTE;
						debug(LOG_DEBUG, "rsync_flags are now set to: %0x", rsync_flags);
					}
					else
#endif /* RSYNC_COMPAT */

					/* if the character is found in badarg, then it's not a permitted option */
					if (cmdarg->badarg != NULL && (strchr(cmdarg->badarg, ch) != NULL))
					{
						syslog(LOG_ERR, "option '%c' or a related long option is not permitted for use with %s (arg was %s) (%s))", 
							ch, cmdarg->name, (optarg!=NULL ? optarg : "<NULL>"), logstamp());
						return 1;
					}
					else if (cmdarg->strict && ch == '?')
					{
						syslog(LOG_ERR, "an unrecognized option was encountered while processing cmd %s (arg was %s) (%s))", 
							cmdarg->name, (optarg!=NULL ? optarg : "<NULL>"), logstamp());
						return 1;
					}
				}
#ifdef RSYNC_COMPAT
				/* it's not safe if the execute flag was set and server was not set */
				if ((rsync_flags & RSYNC_ARG_EXECUTE) != 0 && (rsync_flags & RSYNC_ARG_SERVER) == 0) {
						syslog(LOG_ERR, "option 'e' is not allowed unless '--server' is also set with cmd %s (%s)", 
							PROG_RSYNC, logstamp());
						return 1;
				}
#endif /* RSYNC_COMPAT */

#else /* HAVE_GETOPT */
				/*
				 * make sure that processing doesn't continue if we can't validate a rsync check
				 * and if the getopt flag is set.
				 */
				syslog(LOG_ERR, "a getopt() argument check could not be performed for %s, recompile scponly without support for %s or rebuild scponly with getopt", av[0], av[0]);
				return 1;
#endif /* HAVE_GETOPT */
			}
			else
			/*
			 * command does not require getopt processing
			 */
			{
				debug(LOG_DEBUG, "Not using getopt processing on cmd %s (%s)", cmdarg->name, logstamp());

				tmpptr=av;
				tmpptr++;
				while (*tmpptr!=NULL)
				{
					debug(LOG_DEBUG, "Verifying that %s is an allowed option (%s)", *tmpptr, logstamp());
						
					if(strbeg(*tmpptr, cmdarg->badarg))
					{
						syslog(LOG_ERR, "%s is not permitted for use with %s (%s))", 
							cmdarg->badarg, cmdarg->name, logstamp());
						return 1;
					}
					tmpptr++;
				}
			}
		}
		cmdarg++;
	}
	return 0;
}

int valid_arg_vector(char **av)
{
	cmd_t	*cmd=commands;

	while (cmd != NULL)
	{
		if (cmd->name == NULL)
			return 0;
		if (exact_match(cmd->name,av[0]))
		{
			if ((cmd->argflag == 0) && (av[1]!=NULL))
			{
				return 0;
			}
			return 1;
		}
		cmd++;
	}
	return 0;
}

char *substitute_known_path(char *request)
{
	cmd_t	*cmd=commands;
	char *stripped_req=strdup(basename(request));
	while (cmd != NULL)
	{
		if (cmd->name == NULL)
			break;
		if (exact_match(basename(cmd->name),stripped_req))
		{
			free(stripped_req); /* discard old pathname */
			return (strdup(cmd->name));
		}
		cmd++;
	}
	return (stripped_req);
}

char **build_arg_vector(char *request)
{
	/*
	 * i strdup vector elements so i know they are
	 * mine to free later.
	 */
	char **ap, *argv[MAX_ARGC], *inputstring, *tmpstring, *freeme;
	char **ap2,**av=(char **)malloc(MAX_ARGC * (sizeof(char *)));

	ap=argv;
	freeme=inputstring=strdup(request); /* make a local copy  */

	while (ap < &argv[(MAX_ARGC-1)]) 
	{
		if (inputstring && (*inputstring=='"'))
		{
			if (NULL != (tmpstring=strchr((inputstring+1),'"')))
			{
				*tmpstring++='\0';
				*ap=(inputstring+1);
				
#ifdef UNIX_COMPAT
				if (solaris_needs_strsep(&tmpstring, WHITE) == NULL)
#else
				if (strsep(&tmpstring, WHITE) == NULL)
#endif
					break;
				inputstring=tmpstring;
		
       				if (**ap != '\0')
					ap++;
				continue;
			}
		}
		
#ifdef UNIX_COMPAT
		if ((*ap = solaris_needs_strsep(&inputstring, WHITE)) == NULL)
#else
		if ((*ap = strsep(&inputstring, WHITE)) == NULL)
#endif
			break;
		
		if (**ap != '\0')
			ap++;
	}
	*ap = NULL;
	ap=argv;
	ap2=av;
	while (*ap != NULL)
	{
		*ap2=strdup(*ap);
		ap2++;
		ap++;
	}
	*ap2 = NULL;
	free(freeme);
	
	return (av);	
}

char **expand_wildcards(char **av_old)
#ifdef HAVE_GLOB
{
	char		**av_new=(char **)malloc(MAX_ARGC * (sizeof(char *)));
	glob_t g;
	int c_old,c_new,c;	/* argument counters */
#ifdef UNIX_COMPAT
	int flags = GLOB_NOCHECK;
#else
	int flags = GLOB_NOCHECK | GLOB_TILDE;
#endif

	g.gl_offs = c_new = c_old = 0;

	while(av_old[c_old] != NULL )
	{
		if (0 == glob(av_old[c_old++],flags,NULL,&g))
		{
			c=0;
			while((g.gl_pathv[c] != NULL) && (c_new < (MAX_ARGC-1)))
				av_new[c_new++]=strdup(g.gl_pathv[c++]);
			globfree(&g);
		}
	}
	av_new[c_new]=NULL;
	discard_vector(av_old);
	return av_new;
}

#else 
#ifdef HAVE_WORDEXP
{
	return NULL;
}
#endif
#endif

int cntchr(char *buf, char x)
{
	int count=0;
	while (*buf!=0)
		if (*buf++==x)
			count++;
	return count;
}

char *logstamp ()
{
	/* Time and pid are handled for us by syslog(3). */
	static char ret_buf[255];
	static const char bad_ip[10] = "no ip?!";
	char *ipstring = NULL;
	
	ipstring = (char *)getenv("SSH_CLIENT");
	if (!ipstring)
		ipstring = (char *)getenv("SSH2_CLIENT");
	if (!ipstring)
		ipstring = (char *)bad_ip;
	snprintf(ret_buf, sizeof(ret_buf)-1,
		 "username: %s(%d), IP/port: %s", username, getuid(), ipstring);
	return ret_buf;
}

/*
 *	if big ends with small, return big without
 *	small in a new buf, else NULL
 */
char *strend (char *big, char *small)
{
	int blen,slen;
	slen=strlen(small);
	blen=strlen(big);
	if ((blen==0) || (slen==0) || (blen < slen))
	{
		return NULL;
	}
	if (0 == strcmp(&big[(blen-slen)],small))
	{
		char *tempbuf=NULL;
		tempbuf=(char *)malloc(blen-slen+1);
		if (tempbuf==NULL)
		{
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		bzero(tempbuf,(blen-slen+1));
		strncpy(tempbuf, big, blen-slen);
		return tempbuf;
	}
	return NULL;
}

/*
 *	if big starts with small, return the char after 
 *	the last char in small from big. ahem.
 */
char *strbeg(char *big, char *small)
{
	if (strlen(big) <= strlen(small))
		return NULL;
	if (0==strncmp(big,small,strlen(small)))
		return (big+strlen(small));
	return NULL;
}

/*
 *	if any chars in string dont appear in ALLOWABLE
 *	then fail (return 0)
 */
int valid_chars(char *string)
{
	int count;
	if ((count=strspn(string,ALLOWABLE))==strlen(string))
		return 1;
	else
	{
		fprintf (stderr, "invalid characters in scp command!\n");
		fprintf (stderr, "here:%s\n",string+count);
		fprintf (stderr, "try using a wildcard to match this file/directory\n");
		return 0;
	}
}

/*
 * retrieves username and home directory from passwd database
 */
int get_uservar(void)
{
	struct passwd *userinfo;

	char *user = (char *)getenv("USER");
	if (user) {
		if (NULL==(userinfo=getpwnam(user)))
		{
			syslog(LOG_WARNING, "no knowledge of username %s [%s]", user, logstamp());
			return 0;
		}
		if (userinfo->pw_uid != getuid())
		{
			syslog(LOG_WARNING, "%s's uid doesn't match getuid(): %d [%s]", 
                                              user, getuid(), logstamp());
			return 0;
		}
		debug(LOG_DEBUG, "determined USER is \"%s\" from environment", user);
	} else {
		if (NULL==(userinfo=getpwuid(getuid())))
		{
			syslog (LOG_WARNING, "no knowledge of uid %d [%s]", getuid(), logstamp());
			return 0;
		}
	}
	debug(
		LOG_DEBUG,
		"retrieved home directory of \"%s\" for user \"%s\"",
		userinfo->pw_dir, userinfo->pw_name
		);
	strncpy(username,userinfo->pw_name,MAX_USERNAME);
	strncpy(homedir,userinfo->pw_dir,FILENAME_MAX);
	return 1;
}

/*
 * look through safeenv for the "name" environment variable and replace
 * it's value with "value".
 */
int replace_env_entry(const char* name, const char* value) {
	
	char** base = safeenv;
	char buf[257]; /* make sure I don't overflow */
	snprintf(buf, 255, "%s=", name);
	while (*base != NULL) {
		debug(LOG_DEBUG, "Looking for '%s' in '%s'", buf, *base);
		if (strbeg(*base, buf) != NULL) {
			strcpy(*base + strlen(buf), value);
			debug(LOG_DEBUG, "'%s' env entry now reads '%s'", name, *base);
			return 0;
		}
		base++;
	}
	return 1;
}

int mysetenv(const char *name, const char *value) {
	/* from: http://www.onlamp.com/pub/a/onlamp/excerpt/PUIS3_chap16/index3.html */
	static char count = 0;
	char buff[255];
	
	if (count == 0)
		/* in case nothing ever gets put in here... */
		safeenv[0] = NULL;
	if (count == MAX_ENV)
			return 0;
	if (!name || !value)
			return 0;
	if (snprintf(buff, sizeof(buff), "%s=%s", name, value) < 0)
			return 0;
	if ((safeenv[count] = strdup(buff))) {
		safeenv[++count] = NULL;
		return 1;
	}
	return 0;
}

void filter_allowed_env_vars() {
	
	char *p_env;
	char **p_valid = allowed_env_vars;
	
	/* check each allowed variable */
	while (NULL != *p_valid) {
		
		p_env = (char*)getenv(*p_valid);
		if (NULL != p_env) {
			debug(LOG_DEBUG, "Found \"%s\" and setting it to \"%s\"", *p_valid, p_env);
			if (!mysetenv(*p_valid, p_env))
				syslog(LOG_ERR, "Unable to set environment var \"%s\" to \"%s\"", *p_valid, p_env);
		} else {
			debug(LOG_DEBUG, "Unable to find \"%s\" in the environment", *p_valid);
		}
		p_valid++;
	}
}

/* vim: set noet sw=4 ts=4: */
