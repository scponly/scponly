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

extern FILE *log;
extern char username[MAX_USERNAME];
extern char homedir[FILENAME_MAX];

void log_stamp(void)
{
        time_t now=time(NULL);
	char *ipstring,bad_ip[10]="no ip?!";
	char *timebuf=ctime(&now);

	timebuf[strlen(timebuf)-1]='\0';
        fprintf (log,"%s: USER %s (",timebuf,username);
	if (NULL!=(ipstring=getenv("SSH_CLIENT")))
		fprintf (log,"%s) requesting: ",ipstring);
	else if (NULL!=(ipstring=getenv("SSH2_CLIENT")))
		fprintf (log,"%s) requesting: ",ipstring);
	else
		fprintf (log, "no IP?!?!");
	fflush(log);
}

void show_usage(void)
{
	fprintf (stderr, "\nscponly: incorrect usage\n");
	fprintf (stderr, "\n\tscp username@whatever.org:/some/path/whatever.zip .\n");
	fprintf (stderr, "the path must contain no characters other than:\n\t%s\n\n", ALLOWABLE);
	return;
}
/*
 *	if big starts with small, return the char after 
 *	the last char in small from big
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
#ifdef DEBUG
		perror("getpwuid");	
#endif
		log_stamp();
		fprintf (log,"no knowledge of uid %d\n",getuid());
		fflush(log);
		return 0;
	}
	strncpy(username,userinfo->pw_name,MAX_USERNAME);
	strncpy(homedir,userinfo->pw_dir,FILENAME_MAX);
	return 1;
}

