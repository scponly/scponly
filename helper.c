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
#include "scponly.h"

#define MAX(x,y)	( ( x > y ) ? x : y )

extern FILE *log;
extern int debuglevel;
extern char username[MAX_USERNAME];
extern char homedir[FILENAME_MAX];

void log_stamp(void)
{
        time_t now=time(NULL);
	char *ipstring,bad_ip[10]="no ip?!";
	char *timebuf=ctime(&now);

	timebuf[strlen(timebuf)-1]='\0';
        fprintf (log,"%s: USER %s (",timebuf,username);
	if (NULL!=(ipstring=(char *)getenv("SSH_CLIENT")))
		fprintf (log,"%s) requesting: ",ipstring);
	else if (NULL!=(ipstring=(char *)getenv("SSH2_CLIENT")))
		fprintf (log,"%s) requesting: ",ipstring);
	else
		fprintf (log, "no IP?!?!");
	fflush(log);
}

/*
 *	if big starts with small, return the char after 
 *	the last char in small from big. ahem.
 */
inline char *strend (char *big, char *small)
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
		log_stamp();
		fprintf (log,"no knowledge of uid %d\n",getuid());
		fflush(log);
		if (debuglevel)
		{
			fprintf (stderr,"no knowledge of uid %d\n",getuid());
			perror("getpwuid");	
		}
		return 0;
	}
	if (debuglevel)
		fprintf (stderr,"retrieved home directory of \"%s\" for user \"%s\"\n",
			userinfo->pw_dir,userinfo->pw_name);
	strncpy(username,userinfo->pw_name,MAX_USERNAME);
	strncpy(homedir,userinfo->pw_dir,FILENAME_MAX);
	return 1;
}

