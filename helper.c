/*
 *
 *
 */
#include <stdio.h>	// io
#include <string.h>	// for str*
#include <sys/types.h>	// for stat, getpwuid
#include <sys/stat.h>	// for stat
#include <unistd.h>	// for exit, access, getpwuid, execve
#include <errno.h>	// for debugging
#include <pwd.h>	// to get username for config parsing
#include <time.h>	// time
#include <syslog.h>
#include "scponly.h"

#define MAX(x,y)	( ( x > y ) ? x : y )

extern int debuglevel;
extern char username[MAX_USERNAME];
extern char homedir[FILENAME_MAX];

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
		 "username: %s(%d), IP and port info: %s", username, getuid(), ipstring);
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
			exit(-1);
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
 *	this function removes pathnames from the first
 *	part of the shell request.
 *
 *	we need to find the character after the last 
 * 	occurance of a forward slash that is also
 *	before the first whitespace, if it exists
 */
char *clean_request (char *request)
{
        char *fs,*ws,*tfs;
        int i=0;

	if (debuglevel)
		syslog(LOG_DEBUG, "check if \"%s\" needs to be cleaned of path info", request);
        if (((ws=strchr(request,' '))==NULL) && \
                ((ws=strchr(request,'\t'))==NULL))
                        ws=&request[strlen(request)];
        if (((fs=strchr(request,'/')) == NULL) || (fs > ws))
                return(request);
        ++fs;
        while (fs < ws)
        {
                tfs=strchr(fs,'/');
                if ((tfs == NULL) || (tfs > ws))
                        break;
                fs=(tfs+1);
        }
	if (debuglevel)
		syslog(LOG_DEBUG, "cleaned request is now \"%s\"\n", fs);
        return(fs);
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

