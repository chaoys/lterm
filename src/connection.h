
#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <gtk/gtk.h>
#include "grouptree.h"

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

typedef struct Connection {
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

	struct Connection *next;
} SConnection;

struct Connection_List {
	struct Connection *head;
	struct Connection *tail;
};

struct Connection *get_connection (struct Connection_List *p_cl, char *name);
struct Connection *get_connection_by_index (int index);
struct Connection *get_connection_by_name (char *name);
struct Connection *get_connection_by_host (char *host);

int load_connections_from_file_xml (char *filename);
GList *load_connection_list_from_file_xml (char *filename);
int save_connections_to_file (char *, struct Connection_List *, int);
int save_connections_to_file_xml_from_glist (GList *pList, char *filename);
int save_connections_to_file_xml (char *filename);
int load_connections ();

#define ERR_VALIDATE_MISSING_VALUES 1
#define ERR_VALIDATE_EXISTING_CONNECTION 2
#define ERR_VALIDATE_EXISTING_ITEM_LEVEL 3

char *get_validation_error_string (int error_code);
int validate_name (struct GroupNode *p_parent, struct GroupNode *p_node_upd, struct Connection *p_conn, char *item_name);
struct GroupNode *add_update_connection (struct GroupNode *p_node, struct Connection *p_conn_model);
struct GroupNode *add_update_folder (struct GroupNode *p_node);

void connection_init (SConnection *);
void connection_init_stuff ();
void rebuild_tree_store ();
GtkWidget *create_entry_control (char *label, GtkWidget *entry);
void refresh_connection_tree_view (GtkTreeView *tree_view);
int count_current_connections ();
int choose_manage_connection (struct Connection *p_conn);
int conn_update_last_user (char *cname, char *last_user);


void cl_init (struct Connection_List *p_cl);
void cl_remove (struct Connection_List *p_cl, char *name);
void cl_release (struct Connection_List *p_cl);
int cl_count (struct Connection_List *p_cl);
struct Connection * cl_append (struct Connection_List *p_cl, struct Connection *p_new);
struct Connection *cl_insert_sorted (struct Connection_List *p_cl, struct Connection *p_new);
struct Connection *cl_host_search (struct Connection_List *p_cl, char *host, char *skip_this);
struct Connection *cl_get_by_index (struct Connection_List *p_cl, int index);
struct Connection *cl_get_by_name (struct Connection_List *p_cl, char *name);

void connection_copy (struct Connection *p_dst, struct Connection *p_src);

#endif

