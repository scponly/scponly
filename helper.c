/*
 *	helper functions for scponly
 */
#include <stdio.h>	/* io */
#include <string.h>	/* for str* */
#include <sys/types.h>	/* for stat, getpwuid */
#include <sys/stat.h>	/* for stat */
#include <unistd.h>	/* for exit, access, getpwuid, execve */
#include <errno.h>	/* for debugging */
#include <pwd.h>	/* to get username for config parsing */
#include <time.h>	/* time */
#include <libgen.h>	/* basename */
#include <stdlib.h>	/* realloc */
#include <syslog.h>
#include "scponly.h"
#include "config.h"

#ifdef HAVE_GLOB
#include <glob.h>	/* for glob() */
#else
#ifdef HAVE_WORDEXP
#include <wordexp.h>	/* for wordexp() */
#endif
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

void discard_vector(char **av)
{
	char **tmpptr=av;	
	while (*tmpptr!=NULL)
		free(*tmpptr++);
	free(av);
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
		if (NULL == (temp = realloc (outbuf, newlen + 1)))
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
		*tmpptr++;
	}
	return (outbuf);
}

/*
 *	since some programs support invoking other programs for their encryption
 *	(dropings to replace ssh), we must refuse to support these arguments
 *
 *	RETURN: 1 means reject this command, 0 means it is safe.
 */
int check_dangerous_args(char **av)
{
	cmd_arg_t	*cmdarg=dangerous_args;
	char		**tmpptr=av;

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
			 */
			tmpptr=av;
			*tmpptr++;
			while (*tmpptr!=NULL)
			{
				if(exact_match(*tmpptr, cmdarg->badarg))
				{
					syslog(LOG_ERR, "%s is not permitted for use with %s (%s))", 
						cmdarg->badarg, cmdarg->name, logstamp());
					return 1;
				}
				*tmpptr++;
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
	 *  	i strdup vector elements so i know they are
	 * 	mine to free later.
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
inline char *strend (char *big, char *small)
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
inline char *strbeg (char *big, char *small)
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
	
	if (NULL==(userinfo=getpwuid(getuid())))
	{
		syslog (LOG_WARNING, "no knowledge of uid %d [%s]", getuid(), logstamp());
		if (debuglevel)
		{
			fprintf (stderr, "no knowledge of uid %d\n", getuid());
			perror ("getpwuid");
		}
		return 0;
	}
	if (debuglevel)
		syslog(LOG_DEBUG, "retrieved home directory of \"%s\" for user \"%s\"",
		       userinfo->pw_dir, userinfo->pw_name);
	strncpy(username,userinfo->pw_name,MAX_USERNAME);
	strncpy(homedir,userinfo->pw_dir,FILENAME_MAX);
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
	if (safeenv[count] = strdup(buff)) {
		safeenv[++count] = NULL;
		return 1;
	}
	return 0;
}


void filter_allowed_env_vars() {
	
	int slen = 0;
	char *p_env, *p_str;
	char **p_valid = allowed_env_vars;
	
	/* check each allowed variable */
	while (NULL != *p_valid) {
		
		p_env = (char*)getenv(*p_valid);
		if (NULL != p_env) {
			if (debuglevel)
				syslog(LOG_DEBUG, "Found \"%s\" and setting it to \"%s\"", *p_valid, p_env);
			if (!mysetenv(*p_valid, p_env))
				syslog(LOG_ERR, "Unable to set environment var \"%s\" to \"%s\"", *p_valid, p_env);
		} else if (debuglevel) {
			syslog(LOG_DEBUG, "Unable to find \"%s\" in the environment", *p_valid);
		}
		p_valid++;
	}
}

