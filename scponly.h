#include <stdio.h>	// FILENAME_MAX

/*
 * any installation customizations should be done here
 */
#define MAX_USERNAME 32
#define MAX_REQUEST (1024)		// any request exceeding this is truncated
#define SCP2_ARG "sftp-server"
#define ALLOWABLE "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 ./-*\\_\'\":$?"
#define exact_match(x,y) (0==strcmp(x,y))	// we dont want strncpy for this
#define LOGIDENT "scponly"
#define DEBUGFILE "/var/run/scponly.debuglevel"
#define CHROOTED_NAME "scponlyc"

#define WINSCP_EOF_REQ_RETVAL	" ; echo \"WinSCP: this is end-of-file:$?\""
#define WINSCP_EOF_REQ_ZERO	" ; echo \"WinSCP: this is end-of-file:0\""
#define WINSCP_BOF_REQ		"echo \"WinSCP: this is begin-of-file\" ; "

#define WINSCP_EOF      "WinSCP: this is end-of-file:"
#define WINSCP_BOF      "WinSCP: this is begin-of-file"


/*
 * function prototypes
 */
inline char *strbeg (char *big, char *small);
inline char *strend (char *big, char *small);
int valid_chars(char *string);
int get_uservar(void);
void show_usage(void);
// void log_stamp(void);
char *logstamp();
int cntchr(char *, char);
char *clean_request (char *request);
