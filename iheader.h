#define _XOPEN_SOURCE 500 
#define MAX_NTW_NAME 1024
//The fixed size of the event buffer:
#define EVENT_SIZE (sizeof (struct inotify_event) )
//The size of the read buffer: estimate 1024 events with 16 bytes per name over and above the fixed size above
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

struct List{
	int size;
	struct ListNode *head;
};

struct TreeNode {
	char *name;
	char *path;
	char type; //d or f (dir or file)
	struct i_node_node *i_node;
	struct List children;
};

struct ListNode {
	struct TreeNode *treenode;
	struct ListNode *next;
};

struct Tree{
	struct TreeNode *root;
};

struct NameNode {
	char *name;
	struct NameNode *next;
};

struct i_node {
	ino_t i_no;
	time_t mtime;
	int file_size;
	struct NameNode *head; //files pointing to this i-node
	int links;
	int mark;
	struct i_node_node *dest_node;
};

struct i_node_node {
	struct i_node inode;
	struct i_node_node *next;
};

struct i_nodeList {
	struct i_node_node *head;
};

struct name_to_wd {
	char path[MAX_NTW_NAME];
	int wd;
	struct name_to_wd *next;
};

struct wd_list {
	struct name_to_wd *head;
};

//helper function prototypes
const char* target_type(struct inotify_event *event);
const char* target_name(struct inotify_event *event);
const char* event_name(struct inotify_event *event);
void fail(const char *message);

int in_create_dir(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*);

int in_create_file(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,struct inotify_event*,char*,char*,char*);

int in_delete(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,struct inotify_event*,char*,char*,char*);

int in_delete_self(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*);

int in_attrib(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*);

int in_modify(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*);

int in_close_write(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*);

int in_moved_from(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,char*,struct inotify_event*,char*,char*,char*);

int in_moved_to(struct Tree*,struct Tree*,struct i_nodeList*,struct i_nodeList*,
	struct wd_list*,int,struct inotify_event*,char*,char*,char*,char*);

int sychronization(struct TreeNode*,struct TreeNode*,struct i_nodeList*,struct i_nodeList*);

int dir_tree_create(struct i_nodeList*,struct TreeNode*,char*,int);

void mycopy(char*,char*);

void mycopy_p(char*,char*);

void dir_remove(char*);

int add_watch(int,char*,struct wd_list*);

int add_dir_watch(int,char*,struct wd_list*);

void get_bu_name(char name[],char[],char*);

int destroy_dir_watch(int fd,struct wd_list *wd_list);


	//wd_list functions
void wd_list_init(struct wd_list **wd_list);

struct name_to_wd* wd_list_insert(struct wd_list*,char *path,int wd);

int wd_list_remove(struct wd_list*,int);

void wd_list_search(struct wd_list*,int wd,char *path);

void wd_list_print(struct wd_list*);

void wd_list_destroy(struct wd_list**);


	//i_node functions
void i_node_list_init(struct i_nodeList **ilist) ;

void i_node_list_print(struct i_nodeList*);

struct i_node_node* i_node_list_insert(struct i_nodeList*,ino_t,time_t,int);

struct i_node_node* i_node_list_search(struct i_nodeList *ilist,ino_t i_n);

void i_node_list_dest_print(struct i_nodeList*);

void i_node_list_destroy(struct i_nodeList**);


	//i_node_node function
int i_node_remove(struct i_node_node*,char*);

void i_node_destroy(struct i_node_node*);

void i_node_increase(struct i_node_node*);

int i_node_get_numlinks(struct i_node_node*);

char* i_node_get_headname(struct i_node_node*);

struct i_node_node* i_node_get_dest_inode(struct i_node_node*);


	//NameNode functions
struct NameNode* name_insert(struct NameNode**,char*);

int name_remove(struct NameNode**,char*);

void name_print(struct NameNode*);

void name_destroy(struct NameNode**);


	//List functions
void listnode_swap(struct ListNode *,struct ListNode *);

char* listnode_get_name(struct ListNode);

char* listnode_get_path(struct ListNode);

char listnode_get_type(struct ListNode);

struct i_node_node* listnode_get_inode(struct ListNode);


	//List functions
void list_init(struct List*);

struct TreeNode* list_insert(struct List*,char*,char*,char);

struct ListNode* list_insertafter(struct List*,char*,char*,char*,char);

int list_remove(struct List*,char*);

void list_sort(struct List*);

void list_print(struct List*);

struct ListNode* list_get_head(struct List*);

struct ListNode* list_search(struct List*,char*);

void list_destroy(struct List**);


	//TreeNode functions
void treenode_init(struct TreeNode*);

struct TreeNode* treenode_insert(struct TreeNode*,char*,char*,char);

struct TreeNode* treenode_t_insert(struct TreeNode*,char*,char*,char,char*);

struct TreeNode* treenode_treeinsert(struct TreeNode*,char*,char*,char,char*);

struct TreeNode* treenode_search_(struct TreeNode*,char*);

struct TreeNode* treenode_search(struct TreeNode*,char*);

int treenode_remove(struct TreeNode*,char*);

int treenode_remove_all(struct TreeNode*);

int treenode_t_remove(struct i_nodeList*,struct TreeNode*,char*);

int treenode_treeremove(struct i_nodeList*,struct TreeNode*,char*);

int treenode_t_remove2(struct i_nodeList*,struct TreeNode*,char*);

int treenode_treeremove2(struct i_nodeList*,struct TreeNode*,char*);

void treenode_remove_name_inode(struct i_nodeList*,struct TreeNode*);

void treenode_sort(struct TreeNode*);

void treenode_print(struct TreeNode*,int);

char treenode_get_type(struct TreeNode*);

struct i_node_node* treenode_get_dest_inode(struct TreeNode*);

struct i_node_node* treenode_get_inode(struct TreeNode*);


	//Tree functions
void tree_init(struct Tree**);

void tree_print(struct Tree*); 

void tree_root_insertion(struct Tree**,char*);

void tree_destroy(struct Tree**);