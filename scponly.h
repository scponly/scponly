#include <stdio.h>	// FILENAME_MAX

/*
 * any installation customizations should be done here
 */
#define MAX_USERNAME 32
#define MAX_REQUEST (512)			// any request exceeding this is truncated
#define SCP2_ARG "sftp-server"
#define ALLOWABLE "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 ./-*\\_\'\""
#define exact_match(x,y) (0==strcmp(x,y))	// we dont want strncpy for this
#define LOGFILE "/var/log/scponly.log"
#define DEBUGFILE "/var/run/scponly.debuglevel"
#define CHROOTED_NAME "scponlyc"

/*
 * function prototypes
 */
inline char *strend (char *big, char *small);
int valid_chars(char *string);
int get_uservar(void);
void show_usage(void);
void log_stamp(void);
char *clean_request (char *request);
