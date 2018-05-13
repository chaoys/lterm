
#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <gtk/gtk.h>

#define XML_STATE_INIT 0
#define XML_STATE_CONNECTION_SET 1
#define XML_STATE_FOLDER 2
#define XML_STATE_CONNECTION 3
#define XML_STATE_AUTHENTICATION 4
#define XML_STATE_AUTH_USER 5
#define XML_STATE_AUTH_PASSWORD 6
#define XML_STATE_LAST_USER 7
#define XML_STATE_DIRECTORY 8
#define XML_STATE_USER_OPTIONS 9
#define XML_STATE_NOTE 10

#define CONN_AUTH_MODE_PROMPT 0
#define CONN_AUTH_MODE_SAVE 1
#define CONN_AUTH_MODE_KEY 2

typedef struct _SSH_Options {
	int x11Forwarding;
	int agentForwarding;
	int disableStrictKeyChecking;
	int flagKeepAlive;
	int keepAliveInterval;
	int flagConnectTimeout;
	int connectTimeout;
} SSH_Options;

typedef struct _Connection {
	char name[256];
	char host[256];
	int port;
	char last_user[32];
	char user_options[256];
	int auth_mode;
	char auth_user[32];
	char auth_password[32];
	char auth_password_encrypted[64];
	char user[32];
	char password[32];
	unsigned int flags;
	char identityFile[1024];
	SSH_Options sshOptions;
} Connection;

extern GList *conn_list;

int save_connections(GList *pList, char *filename);
int load_connections();

//int add_update_connection (Connection *p_conn_model);

void connection_init (Connection *);
int choose_manage_connection (Connection *p_conn);
int conn_update_last_user (char *cname, char *last_user);

void cl_remove (GList **p_cl, char *name);
void cl_release (GList **p_cl);
int cl_count (GList *p_cl);
Connection * cl_append (GList **p_cl, Connection *p_new);
Connection *cl_insert_sorted (GList **p_cl, Connection *p_new);
Connection *cl_get_by_index (GList *p_cl, int index);
Connection *cl_get_by_name (GList *p_cl, char *name);

void connection_copy (Connection *p_dst, Connection *p_src);

#endif

