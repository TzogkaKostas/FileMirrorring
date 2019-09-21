#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "iheader.h"

int in_create_dir(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	int wd;
	struct name_to_wd *temp; //auxiliary pointers
	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2; //auxiliary pointers
	struct i_node_node *i_node_n; //auxiliary pointers
	struct TreeNode *treenode_ptr; //auxiliary pointers
	struct stat statbuf;
	char parent_path[1000],*ret,type;

	type = 'd';

	wd_list_search(wd_list,event->wd,full_path);//find the name of directory having this wd
	strcpy(bu_full_path,full_path);

	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);
	
	if (stat(full_path, &statbuf) == 0) {
		i_node_node_ptr = i_node_list_insert(inode_list,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
		name_insert(&i_node_node_ptr->inode.head,full_path);
		i_node_increase(i_node_node_ptr);

		treenode_ptr = treenode_treeinsert(tree->root,event->name,full_path,type,parent_path);
		treenode_ptr->i_node = i_node_node_ptr;
	}
	else 
		{printf("in_create_dir error: stat error(orig)\n");return -1;}

	//create the full path of the directory of the backup structure
	get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
	strcpy(parent_path,bu_full_path); 
	strcat(bu_full_path,"/");strcat(bu_full_path,event->name);
	//create the directory to backup hierarchy
	if (mkdir(bu_full_path,0700) == -1) {printf("main error: mkdir\n");return -1;}

	//insert to the backup tree structure
	if (stat(bu_full_path, &statbuf) == 0) {
		i_node_node_ptr2 = i_node_list_insert(bu_inode_list,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
		name_insert(&i_node_node_ptr2->inode.head,bu_full_path);
		i_node_increase(i_node_node_ptr2);

		treenode_ptr = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
		treenode_ptr->i_node = i_node_node_ptr2;
	}
	else 
		{printf("in_create_dir error: stat error(backup)\n");return -1;}

	//source treenode points to the dest i-node
	i_node_node_ptr->inode.dest_node = i_node_node_ptr2;

	//add the created directory for monitoring
	if((wd = inotify_add_watch(fd,full_path,IN_ALL_EVENTS)) == -1 ) {
		printf("in_create_dir error: inotify_add_watch error\n");return-1;
	}
	else {
		printf("added %d to watch %s\n",wd,full_path);
	}
	//insert path and WD in the wd_list
	wd_list_insert(wd_list,full_path,wd);

	return 0;
}

int in_create_file(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	struct TreeNode *treenode_ptr,*treenode_ptr2;
	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2,*i_node_n;
	struct stat statbuf;
	char parent_path[1000],*ret,type;

	wd_list_search(wd_list,event->wd,bu_full_path);

	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	if(stat(full_path, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr = i_node_list_search(inode_list,statbuf.st_ino);
	}
	else 
		{printf("in_create_file: stat error(orig)\n");return -1;
	}

	//there exists a copy
	if (i_node_node_ptr != NULL) {  //so we link it
		//insertion to the original tree
		name_insert(&i_node_node_ptr->inode.head,full_path);
		i_node_increase(i_node_node_ptr);
	
		treenode_ptr = treenode_treeinsert(tree->root,event->name,full_path,type,parent_path);
		treenode_ptr->i_node = i_node_node_ptr;

		//creation of the full path of the directory for the backup structure
		get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
		strcpy(parent_path,bu_full_path); 
		strcat(bu_full_path,"/");strcat(bu_full_path,event->name);

		//link it
		if (link(i_node_get_headname(i_node_node_ptr->inode.dest_node),bu_full_path) == -1) {
			printf("in_create_file: link error(%d)\n",errno); return -1;
		}
		//insertion to the backup tree
		if(stat(i_node_get_headname(i_node_get_dest_inode(i_node_node_ptr)),&statbuf) == 0) { //find the source i-node
			name_insert(&i_node_node_ptr->inode.dest_node->inode.head,bu_full_path);
			i_node_increase(i_node_node_ptr->inode.dest_node);
			treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
			treenode_ptr2->i_node = i_node_node_ptr->inode.dest_node;
		}
		else 
			{printf("in_create_file: stat error(orig)\n");return -1;
		}
		//source treenode points to dest i-node
		i_node_node_ptr->inode.dest_node = i_node_node_ptr2;
	}
	else { //it doesnt exist,so we create a new one
		//insertion to the original tree
		i_node_node_ptr = i_node_list_insert(inode_list,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
		name_insert(&i_node_node_ptr->inode.head,full_path);
		i_node_increase(i_node_node_ptr);
		treenode_ptr = treenode_treeinsert(tree->root,event->name,full_path,type,parent_path);
		treenode_ptr->i_node = i_node_node_ptr;

		//creation of the full path of the directory for the backup structure
		get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
		strcpy(parent_path,bu_full_path); 
		mycopy(full_path,bu_full_path); //create the file to the backup directory
		strcat(bu_full_path,"/");strcat(bu_full_path,event->name);

		//insertion to the backup tree
		if(stat(bu_full_path, &statbuf) == 0) { //find the source i-node
			i_node_node_ptr2 = i_node_list_insert(bu_inode_list,statbuf.st_ino,i_node_node_ptr->inode.mtime,statbuf.st_size);
			name_insert(&i_node_node_ptr2->inode.head,bu_full_path);
			i_node_increase(i_node_node_ptr2);

			treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
			treenode_ptr2->i_node = i_node_node_ptr2;
		}
		else 
			{printf("in_create_file: stat error(orig)\n");return -1;
		}
			//source treenode points to dest i-node
		i_node_node_ptr->inode.dest_node = i_node_node_ptr2;
	}
}

int in_delete(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){	

	char parent_path[1024];

	wd_list_search(wd_list,event->wd,bu_full_path);
	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	//creation of the full path of the directory for the backup structure
	get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
	strcpy(parent_path,bu_full_path); 
	strcat(bu_full_path,"/");strcat(bu_full_path,event->name);

	if (unlink(bu_full_path) == -1 ) {
		printf("in_delete: unlink error\n");return -1;
	}

	treenode_treeremove(inode_list,tree->root,full_path);
	treenode_treeremove(bu_inode_list,bu_tree->root,bu_full_path);
	return 0;
}

int in_delete_self(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	char parent_path[1024];
	
	wd_list_search(wd_list,event->wd,bu_full_path);
	
	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	//creation of the full path of the directory for the backup structure
	get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
	strcpy(parent_path,bu_full_path); 
	strcat(bu_full_path,"/");strcat(bu_full_path,event->name);
	bu_full_path[strlen(bu_full_path)-2] = 0; // a/b/c/ --> a/b/c
	full_path[strlen(full_path)-2] = 0;

	dir_remove(bu_full_path);

	treenode_treeremove(inode_list,tree->root,full_path);
	treenode_treeremove(bu_inode_list,bu_tree->root,bu_full_path);

	wd_list_remove(wd_list,event->wd);
	inotify_rm_watch(fd,event->wd);
	return 0;
}

int in_attrib(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	struct name_to_wd *temp;
	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2;
	struct i_node_node *i_node_n;
	struct TreeNode *treenode_ptr,*treenode_ptr2;
	struct stat statbuf;
	char parent_path[1000],*ret,type;
	int links;
	
	wd_list_search(wd_list,event->wd,bu_full_path);
	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	if(stat(full_path, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr = i_node_list_search(inode_list,statbuf.st_ino);
	}
	else {
		printf("in_attrib error: stat0 error(%d)\n",errno);return -1;
	}

	if (statbuf.st_mtime != i_node_node_ptr->inode.mtime) {
		//creation of the full path of the directory for the backup structure
		get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
		strcpy(parent_path,bu_full_path);
		strcat(bu_full_path,"/");strcat(bu_full_path,event->name);

		//so we unlink it
		if (unlink(bu_full_path) == -1 ) {
			printf("in_attrib error: unlink error(%d)\n",errno);return -1;
		}
		links = i_node_node_ptr->inode.dest_node->inode.links;
		//we remove it from the tree structure and remove its name from the i-node
		treenode_treeremove(bu_inode_list,bu_tree->root,bu_full_path);
		printf("%d\n",links);
		
		if (links > 1) {//"hardlink" case
			//link it
			if (link(i_node_get_headname(i_node_node_ptr->inode.dest_node),bu_full_path) == -1) {
				printf("in_attrib error: link error(%d)\n",errno); return -1;
			}
			mycopy_p(full_path,parent_path); //create the file to the backup directory

			//insertion to the backup tree
			if(stat(i_node_get_headname(i_node_get_dest_inode(i_node_node_ptr)),&statbuf) == 0) { //find the source i-node
				name_insert(&i_node_node_ptr->inode.dest_node->inode.head,bu_full_path);
				i_node_increase(i_node_node_ptr->inode.dest_node);

				treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
				treenode_ptr2->i_node = i_node_node_ptr->inode.dest_node;
			}
			else {
				printf("in_attrib error: stat1 error(%d)\n",errno);return -1;
			}
		}
		else { //it doesnt exist,so we create a new one
			mycopy_p(full_path,parent_path); //create the file to the backup directory
			//insertion to the backup tree
			if(stat(bu_full_path, &statbuf) == 0) { 
				//find the source i-node
				i_node_node_ptr2 = i_node_list_insert(bu_inode_list,statbuf.st_ino,i_node_node_ptr->inode.mtime,statbuf.st_size);
				name_insert(&i_node_node_ptr2->inode.head,bu_full_path);
				i_node_increase(i_node_node_ptr2);

				treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
				treenode_ptr2->i_node = i_node_node_ptr2;
			}
			else 
				{printf("in_attrib error: stat2 error(%d)\n",errno);return -1;
			}
				//source treenode points to dest i-node
			i_node_node_ptr->inode.dest_node = i_node_node_ptr2;
		}
	}
	else {
		printf("222\n");getchar();
	}
}

int in_modify(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	char parent_path[1024];
	struct stat statbuf;
	struct i_node_node *i_node_node_ptr;

	wd_list_search(wd_list,event->wd,bu_full_path);
	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	if(stat(full_path, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr = i_node_list_search(inode_list,statbuf.st_ino);
		//mark this i-node as modified
		i_node_node_ptr->inode.mark = 1;
	}
	else {
		printf("in_modify error: stat0 error(%d)\n",errno);return -1;
	}
	return 0;
}

int in_close_write(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2;
	struct i_node_node *i_node_n;
	struct TreeNode *treenode_ptr,*treenode_ptr2;
	struct stat statbuf;
	char parent_path[1000],*ret,type;
	int links;


	wd_list_search(wd_list,event->wd,bu_full_path);
	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	if(stat(full_path, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr = i_node_list_search(inode_list,statbuf.st_ino);
	}
	else {
		printf("in_close_write error: stat0 error(%d)\n",errno);return -1;
	}
	if (i_node_node_ptr->inode.mark == 1) { //file was marked as modified 
		//creation of the full path of the directory for the backup structure
		get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
		strcpy(parent_path,bu_full_path);
		strcat(bu_full_path,"/");strcat(bu_full_path,event->name);

		//so we unlink it
		if (unlink(bu_full_path) == -1 ) {
			printf("in_close_write error: unlink error(%d)\n",errno);return -1;
		}
		links = i_node_node_ptr->inode.dest_node->inode.links; //links to the dest i-node
		//we remove it from the tree structure and remove its name from i-node
		treenode_treeremove(bu_inode_list,bu_tree->root,bu_full_path);
		printf("%d\n",links);
		
		if (links > 1) {//"hardlink" case
			//link it
			if (link(i_node_get_headname(i_node_node_ptr->inode.dest_node),bu_full_path) == -1) {
				printf("in_close_write error: link error(%d)\n",errno); return -1;
			}
			mycopy_p(full_path,parent_path); //create the file to the backup directory

			//insertion to the backup tree
			if(stat(i_node_get_headname(i_node_get_dest_inode(i_node_node_ptr)),&statbuf) == 0) { //find the source i-node
				name_insert(&i_node_node_ptr->inode.dest_node->inode.head,bu_full_path);
				i_node_increase(i_node_node_ptr->inode.dest_node);

				treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
				treenode_ptr2->i_node = i_node_node_ptr->inode.dest_node;
			}
			else {
				printf("in_close_write error: stat1 error(%d)\n",errno);return -1;
			}
		}
		else { //it doesnt exist,so we create a new one
			mycopy_p(full_path,parent_path); //create the file to the backup directory
			//insertion to the backup tree
			if(stat(bu_full_path, &statbuf) == 0) { 
				//find the source i-node
				i_node_node_ptr2 = i_node_list_insert(bu_inode_list,statbuf.st_ino,i_node_node_ptr->inode.mtime,statbuf.st_size);
				name_insert(&i_node_node_ptr2->inode.head,bu_full_path);
				i_node_increase(i_node_node_ptr2);

				treenode_ptr2 = treenode_treeinsert(bu_tree->root,event->name,bu_full_path,type,parent_path);
				treenode_ptr2->i_node = i_node_node_ptr2;
			}
			else 
				{printf("in_close_write error: stat2 error(%d)\n",errno);return -1;
			}
				//source treenode points to dest i-node
			i_node_node_ptr->inode.dest_node = i_node_node_ptr2;
		}
	}
	else {

	}
	return 0;
}

int in_moved_from(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,char *moved_out,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path){

	char parent_path[1024];
	struct stat statbuf;
	struct i_node_node *i_node_node_ptr;

	strcpy(full_path,moved_out);//path of the source file

	get_bu_name(moved_out,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1

	if (unlink(bu_full_path) == -1 ) {
		printf("in_moved_from error: unlink error\n");return -1;
	}

	treenode_treeremove(inode_list,tree->root,full_path);
	treenode_treeremove(bu_inode_list,bu_tree->root,bu_full_path);
	return 0;
}

int in_moved_to(struct Tree *tree,struct Tree *bu_tree,struct i_nodeList *inode_list,
	struct i_nodeList *bu_inode_list,struct wd_list *wd_list,int fd,
	struct inotify_event *event,char *backup_name,char *full_path,char *bu_full_path,
	char *moved_from){

	char parent_path[1024],temp[1024],orig_new_parent[1024],orig_new[1024];
	char orig_old_parent[1024],orig_old[1024],bu_new_parent[1024],bu_new[1024];
	char bu_old_parent[1024],bu_old[1024],type='f';
	struct stat statbuf;
	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2;
	struct TreeNode *treenode_ptr;

	wd_list_search(wd_list,event->wd,bu_full_path);
	strcpy(orig_new_parent,bu_full_path);
	strcpy(orig_new,orig_new_parent);strcat(orig_new,"/");strcat(orig_new,event->name);

	strcpy(full_path,bu_full_path);
	strcpy(parent_path,full_path); 
	strcat(full_path,"/");strcat(full_path,event->name);

	//creation of the full path of the directory for the backup structure
	get_bu_name(bu_full_path,backup_name,bu_full_path);//e.g.converts dir/d1/f1 to backup/d1/f1
	strcpy(parent_path,bu_full_path);
	strcat(bu_full_path,"/");strcat(bu_full_path,event->name);
	strcpy(orig_old_parent,moved_from);
	strcpy(temp,moved_from);strcat(temp,"/");strcat(temp,event->name);
	strcpy(orig_old,temp);//

	get_bu_name(orig_new_parent,backup_name,bu_new_parent);
	get_bu_name(orig_new,backup_name,bu_new);
	get_bu_name(orig_old_parent,backup_name,bu_old_parent);
	get_bu_name(orig_old,backup_name,bu_old);
	
	if(stat(orig_new, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr = i_node_list_search(inode_list,statbuf.st_ino);
	}
	else {
		printf("in_moved_to error: stat0 error(%s)\n",strerror(errno));return -1;
	}

	if(stat(bu_old, &statbuf) == 0) { //find the source i-node
		i_node_node_ptr2 = i_node_list_search(bu_inode_list,statbuf.st_ino);
	}
	else {
		printf("in_moved_to: stat0 error(%s)\n",strerror(errno));return -1;
	}

	if (rename(bu_old,bu_new) == -1 ) {
		printf("in_moved_to error: rename error(%s)\n",strerror(errno));return -1;
	}
	//delete old file in original tree
	treenode_treeremove2(inode_list,tree->root,orig_old);
	//delete old file in backup tree
	treenode_treeremove2(bu_inode_list,bu_tree->root,bu_old);
	
	//insert new file in original tree		
	treenode_ptr = treenode_treeinsert(tree->root,event->name,orig_new,type,orig_new_parent);
	treenode_ptr->i_node = i_node_node_ptr;
	name_insert(&i_node_node_ptr->inode.head,orig_new);
	i_node_increase(i_node_node_ptr);

	//insert new file in backup tree		
	treenode_ptr = treenode_treeinsert(bu_tree->root,event->name,bu_new,type,bu_new_parent);
	treenode_ptr->i_node = i_node_node_ptr2;
	name_insert(&i_node_node_ptr2->inode.head,bu_new);
	i_node_increase(i_node_node_ptr2);
	return 0;
}	

int sychronization(struct TreeNode *t1,struct TreeNode *t2,
	struct i_nodeList *i_list1,struct i_nodeList *i_list2) {
	// 			input:
	//t1 = treenode of the source tree
	//t2 = treenode of the backup tree
	//i_list1 = list with inodes of the source hierarchy
	//i_list2 = list with inodes of the backup hierarchy
	
	//recursively traverse the source and backup tree
	
	int i,pid,flag;
	char temp_name[100],temp_path[100],path[1024];
	struct stat statbuf;
	struct i_node_node *i_node_n,*i_node_n2,*i_node_node_ptr;//auxiliary pointers
	struct TreeNode *treenode_ptr;//auxiliary pointer
	struct ListNode *listnode,*bu_listnode,*found_node;
	struct List *list,*bu_list; //source list,destination(backup) list

	list = &t1->children; //children(a list of treenodes) nodes of t1
	listnode = t1->children.head; //first child of the list
	bu_list = &t2->children; //children(a list of treenodes) nodes of t2
	bu_listnode = t2->children.head;//first child of the list

	//checks if a folder or file exists in destination but not in source tree
	//traversing the children(list) of a node in the backup tree
	while(bu_listnode != NULL) {
		strcpy(temp_name,listnode_get_name(*bu_listnode));//get its name
		strcpy(temp_path,listnode_get_path(*bu_listnode));//get its path
		found_node = list_search(list,temp_name); //search for this name in the original tree
		if (found_node == NULL) { //d == directory
			if (listnode_get_type(*bu_listnode) == 'd') { //case b
				dir_remove(temp_path); //remove the directory
			} //f == file
			else if (listnode_get_type(*bu_listnode) == 'f') { //case d
				unlink(temp_path); //unlink the file
			}
			treenode_remove_name_inode(i_list2,bu_listnode->treenode); //remove the inode 
			list_remove(bu_list,temp_name); //delete a tree node (and the whole subtree of it)
		}

		bu_listnode = bu_listnode->next;
	}

	//checks if a folder or file exists in source but not in destination tree
	//traversing the children(list) of a node in the original tree
	while (listnode != NULL) {
		flag = -1; //if searching is successfull flag will be equal to 1
		strcpy(temp_name,listnode_get_name(*listnode));//get its name
		strcpy(temp_path,listnode_get_path(*listnode));//get its path
		//pointer to the found node or NULL
		found_node = list_search(bu_list,temp_name); 
		//if the type of the source node is d(directory)
		if (listnode_get_type(*listnode) == 'd') { //case a
			//if a backup node was found
			if (found_node != NULL) {
				//if its type is 'f'(file)
				//therefore we found a file with same name as the source node
				if (listnode_get_type(*found_node) == 'f') {
					//unlink it
					if (unlink(listnode_get_path(*found_node)) == -1 ) {
						printf("synch error: unlink error(116)\n");}
					//remove it from the list of name pointing some i-node
					treenode_remove_name_inode(i_list2,found_node->treenode);
					//remove it from the children list
					list_remove(bu_list,temp_name);
					flag = 1;
				}
			}
			else {//if a backup node was not found
				flag = 1;
			}
			//flag is equal to 1 if the source directory didn't exist to the backup tree
			//or a file with the same name to the source node was found
			if (flag == 1) {
				sprintf(path,"%s/%s",t2->path,temp_name);
				//create that directory to the backup tree
				if (mkdir(path,0700) == -1) {printf("main error: mkdir\n");return -1;}
				//take its info
				if (stat(path, &statbuf) == 0) { //create the i-node node in the i-node list
					i_node_node_ptr = i_node_list_insert(i_list2,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
					name_insert(&i_node_node_ptr->inode.head,path); //insert the name
					i_node_increase(i_node_node_ptr); //increase the number of files point that inode
				}
				else {printf("sychn error: stat error(131)\n");return -1;}

				treenode_ptr = list_insert(bu_list,temp_name,path,'d'); //insert to the backup tree
				treenode_ptr->i_node = i_node_node_ptr; //bu treenode points to the i-node node

				i_node_n = treenode_get_inode(listnode->treenode);//get that i-node of this treenode
				i_node_n->inode.dest_node = i_node_node_ptr; //source treenode points to dest i-node
			}
			else {
				//if the directory was already to the backup tree
				//just connect the treenode with backup inode
				i_node_n = treenode_get_inode(listnode->treenode);//source treenode points to dest i-node
				i_node_n->inode.dest_node = treenode_get_inode(found_node->treenode);//get that i-node of this treenode
			}
		}
		else if (listnode_get_type(*listnode) == 'f') { // case c
			//if a backup node was not found
			if (found_node == NULL) {
				//get the backup inode of this treenode
				i_node_n = treenode_get_dest_inode(listnode->treenode);
				//if it exists,we are dealing with a hardlink
				if (i_node_n != NULL) { //case of "hardlink"
					//find tha path to the backup file
					sprintf(path,"%s/%s",t2->path,temp_name);
					//link it with the the right file to the backup hierarchy 
					if (link(i_node_get_headname(i_node_n),path) == -1) {
						printf("synch_error: link error(%d)\n",errno); return -1;}
					//insert this file to the backup children list
					treenode_ptr = list_insert(bu_list,temp_name,path,'f');
					//make the treenode point to its inode
					treenode_ptr->i_node = i_node_n;
					//make its name point to the inode
					name_insert(&i_node_n->inode.head,path);
					i_node_increase(i_node_n);
				}
				else {//if it doesn't exist, we need to create a new file
					mycopy_p(temp_path,t2->path);//copy the file,but keep the same metadata
					//path of the file that was just created
					sprintf(path,"%s/%s",t2->path,temp_name);
					if (stat(path, &statbuf) == 0) {
						i_node_n = treenode_get_inode(listnode->treenode);
						i_node_node_ptr = i_node_list_insert(i_list2,statbuf.st_ino,i_node_n->inode.mtime,statbuf.st_size);
						name_insert(&i_node_node_ptr->inode.head,path);
						i_node_increase(i_node_node_ptr);
						i_node_list_print(i_list1);i_node_list_print(i_list2);
					}
					else {printf("sychn error: stat error(157)\n");return -1;}

					treenode_ptr = list_insert(bu_list,temp_name,path,'f');
					treenode_ptr->i_node = i_node_node_ptr;
					i_node_n = treenode_get_inode(listnode->treenode);
					i_node_n->inode.dest_node = i_node_node_ptr;
				}
			}//if a backup node was found
			else { //case e
				//get the inode of the source treenode
				i_node_n =  treenode_get_inode((listnode->treenode));
				//get the inode of the backup treenode
				i_node_n2 = treenode_get_inode((found_node->treenode));

				//check if file size and modification date are the same
				if (i_node_n->inode.mtime != i_node_n2->inode.mtime || 
					i_node_n->inode.file_size != i_node_n2->inode.file_size) {
					//if not ,do as in case c

					//unlink it
					if (unlink(listnode_get_path(*found_node)) == -1 ) {
						printf("synch error: unlink error(196)\n");
					}
					//we remove it from the tree structure and remove its i-node
					treenode_remove_name_inode(i_list2,found_node->treenode);
					list_remove(bu_list,listnode_get_path(*found_node));

					i_node_n = treenode_get_dest_inode(listnode->treenode);
					if (i_node_n != NULL) { //case of "hardlink"
						sprintf(path,"%s/%s",t2->path,temp_name);
						if (link(i_node_get_headname(i_node_n),path) == -1) {
							printf("synch_error: link error\n"); return -1;}
						treenode_ptr = list_insert(bu_list,temp_name,path,'f');
						treenode_ptr->i_node = i_node_n;
						name_insert(&i_node_n->inode.head,path);
						i_node_increase(i_node_n);
					}
					else {
						mycopy_p(temp_path,t2->path);
						sprintf(path,"%s/%s",t2->path,temp_name);
						if (stat(path, &statbuf) == 0) {
							i_node_n = treenode_get_inode(listnode->treenode);
							i_node_node_ptr = i_node_list_insert(i_list2,statbuf.st_ino,i_node_n->inode.mtime,statbuf.st_size);
							name_insert(&i_node_node_ptr->inode.head,path);
							i_node_increase(i_node_node_ptr);
						}
						else {printf("sychn error: stat error(157)\n");return -1;}

						treenode_ptr = list_insert(bu_list,temp_name,path,'f');
						treenode_ptr->i_node = i_node_node_ptr;
						i_node_n = treenode_get_inode(listnode->treenode);
						i_node_n->inode.dest_node = i_node_node_ptr;
					}
				}
				else {
					i_node_n = treenode_get_inode(listnode->treenode);
					i_node_n->inode.dest_node = treenode_get_inode(found_node->treenode);
				}
			}
		}
		listnode = listnode->next;
	}
	//sort the children of the source treenode
	list_sort(list);
	//sort the children of the backup treenode
	list_sort(bu_list);

	listnode = t1->children.head;//get again the first child of the source treenode
	bu_listnode = t2->children.head;//get again the first child of the source treenode
	
	while (listnode != NULL) {
		//visit every child of the source and backup treenode
		sychronization(listnode->treenode,bu_listnode->treenode,i_list1,i_list2);
		listnode = listnode->next;
		bu_listnode = bu_listnode->next;
	}
	return 0;
}

int dir_tree_create(struct i_nodeList *inodeList,struct TreeNode *treenode,char *name, int indent) {
	DIR *dir;
	char path[1024];
	struct stat statbuf;
	struct i_node inode;
	struct dirent *entry;
	struct TreeNode *treenode_ptr;
	struct i_node_node *i_node_node_ptr;

	if (!(dir = opendir(name))){
		return 0;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;}
		sprintf(path,"%s/%s", name, entry->d_name);


		if (entry->d_type == DT_DIR) {
			//printf("%*s[%s]\n", indent, "", entry->d_name);
			if(stat(path, &statbuf) == 0) {
				i_node_node_ptr = i_node_list_insert(inodeList,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
				name_insert(&i_node_node_ptr->inode.head,path);
				i_node_increase(i_node_node_ptr);
			}
			else 
				{printf("dir_tree create error: stat error\n");return 0;}

			treenode_ptr = treenode_insert(treenode,entry->d_name,path,'d');
			dir_tree_create(inodeList,treenode_ptr,path, indent + 2);
		}
		else {
			if(stat(path, &statbuf) == 0) {
				i_node_node_ptr = i_node_list_search(inodeList,statbuf.st_ino);
				if (i_node_node_ptr == NULL) {
					i_node_node_ptr = i_node_list_insert(inodeList,statbuf.st_ino,statbuf.st_mtime,statbuf.st_size);
				}
				name_insert(&i_node_node_ptr->inode.head,path);
				i_node_increase(i_node_node_ptr);
			}
			else 
				{printf("dir_tree create error: stat error\n");return 0;}
			treenode_ptr = treenode_insert(treenode,entry->d_name,path,'f');
			//printf("%*s- %s\n", indent, "", entry->d_name);
		}
		treenode_ptr->i_node = i_node_node_ptr;
	}
	closedir(dir);
	return 0;
}

void mycopy(char *path1,char *path2) {
	int pid;
	if ( (pid = fork()) == -1 ) {printf("mycopy_error Fork1\n");exit(-5);}
	if (pid == 0) {
		execlp("cp","cp",path1,path2,NULL);
		printf("mycopy_error1 execlp-cp\n");
	}
	else {
		wait(NULL);
	}
	return;
}

void mycopy_p(char *path1,char *path2) {
	int pid;
	if ( (pid = fork()) == -1 ) {printf("mycopy_p_error Fork1\n");exit(-5);}
	if (pid == 0) {
		execlp("cp","cp","--preserve",path1,path2,NULL);
		printf("mycopy_p_error1 execlp-cp\n");
	}
	else {
		wait(NULL);
	}
	return;
}

void dir_remove(char *dirname) {
	DIR *dir;
	char path[1024];
	struct stat s;
	struct dirent *entry;
	if(stat(dirname,&s) == 0) {
		if(s.st_mode & S_IFREG) {
			unlink(dirname);
			return;
		}
	}
	else {printf("dir_remove error: stat error\n");return;}

	if (!(dir = opendir(dirname))) {printf("dir_remove error:opendir error\n");return;}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;}
		sprintf(path,"%s/%s", dirname, entry->d_name);
		dir_remove(path);
	}
	if (rmdir(dirname) != 0) {printf("dir_remove error: rmdir error\n");}
	closedir(dir);
	return;
}

int add_watch(int fd,char *name,struct wd_list *wd_list) {
	DIR *dir;
	struct name_to_wd *temp;
	char path[1024];
	int wd;
	struct dirent *entry;
	struct stat statbuf;

	if (!(dir = opendir(name))){
		return 0;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;}
		sprintf(path,"%s/%s", name, entry->d_name);

		if (entry->d_type == DT_DIR) {
			if((wd = inotify_add_watch(fd,path,IN_ALL_EVENTS)) == -1 ) {
				printf("add_watch error: inotify_add_watch error\n");return-1;
			}

			temp = (struct name_to_wd*)malloc(sizeof(struct name_to_wd));
			temp->wd = wd;
			strcpy(temp->path,path);
			temp->next = wd_list->head;
			wd_list->head = temp;

			//printf("[%s]\n",entry->d_name);
			add_watch(fd,path,wd_list);
		}
		else {
			//printf("- %s\n",entry->d_name);
		}
	}
	closedir(dir);
	return 0;
}

int add_dir_watch(int fd,char *name,struct wd_list *wd_list) {
	int wd;
	struct name_to_wd *temp;
	if((wd = inotify_add_watch(fd,name,IN_ALL_EVENTS)) == -1 ) {
		printf("add_dir watch error: inotify_add_watch error\n");return-1;
	}
	//add the root of ther directory hierarchy to the "name-wd" structure
	temp = (struct name_to_wd*)malloc(sizeof(struct name_to_wd));
	temp->wd = wd;
	strcpy(temp->path,name);
	temp->next = NULL;
	wd_list->head = temp;
	add_watch(fd,name,wd_list);
	return 0;
}

int destroy_dir_watch(int fd,struct wd_list *wd_list) {
	struct name_to_wd *temp;

	while(wd_list->head != NULL) {
		temp = wd_list->head;
		wd_list->head = wd_list->head->next;
		inotify_rm_watch(fd,temp->wd);
		free(temp);
	}
	free(wd_list->head);
	return 0;
}

void fail(const char *message) {
	perror(message);
	exit(1);
}

const char* target_type(struct inotify_event *event) {
	if( event->len == 0 )
		return "";
	else
		return event->mask & IN_ISDIR ? "directory" : "file";
}

const char* target_name(struct inotify_event *event) {
	return event->len > 0 ? event->name : NULL;
}

const char* event_name(struct inotify_event *event) {
	if (event->mask & IN_ACCESS)
		return "access";
	else if (event->mask & IN_ATTRIB)
		return "attrib";
	else if (event->mask & IN_CLOSE_WRITE)
		return "close write";
	else if (event->mask & IN_CLOSE_NOWRITE)
		return "close nowrite";
	else if (event->mask & IN_CREATE)
		return "create";
	else if (event->mask & IN_DELETE)
		return "delete";
	else if (event->mask & IN_DELETE_SELF)
		return "watch target deleted";
	else if (event->mask & IN_MODIFY)
		return "modify";
	else if (event->mask & IN_MOVE_SELF)
		return "watch target moved";
	else if (event->mask & IN_MOVED_FROM)
		return "moved out";
	else if (event->mask & IN_MOVED_TO)
		return "moved into";
	else if (event->mask & IN_OPEN)
		return "open";
	else
		return "unknown event";
}

void get_bu_name(char name[1000],char backup[1000],char *path) {
	char *ret,temp[1000];
	ret = strchr(name,'/');
	if (ret == NULL) {
		strcpy(path,backup);
		return;
	}
	strcpy(temp,backup);
	strcat(temp,ret);
	strcpy(path,temp);
	return;
}


	//wd_list functions
void wd_list_init(struct wd_list **wd_list) {
	*wd_list = (struct wd_list*)malloc(sizeof(struct wd_list));
	(*wd_list)->head = NULL;
	return;
}

struct name_to_wd* wd_list_insert(struct wd_list *wd_list,char *path,int wd) {
	struct name_to_wd *temp;
	temp = (struct name_to_wd*)malloc(sizeof(struct name_to_wd));
	temp->wd = wd;
	strcpy(temp->path,path);
	temp->next = wd_list->head;
	wd_list->head = temp;
	return temp;
}

int wd_list_remove(struct wd_list *wd_list,int wd) {
	struct name_to_wd *cur,*prev;
	cur = wd_list->head;
	if (wd_list->head == NULL) return -1; //empty list

	if (cur->wd == wd) { //in case we delete the first node.
		wd_list->head = wd_list->head->next;
		free(cur);
		return 0;
	}

	while (cur != NULL) {
		if (cur->wd == wd) {
			prev->next = cur->next;
			free(cur);
			return 0;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}
	return -1;
}

void wd_list_search(struct wd_list *wd_list,int wd,char *path) {
	struct name_to_wd *temp;
	temp = wd_list->head;
	while(temp != NULL) {
		if (temp->wd == wd) {
			strcpy(path,temp->path);
			return;
		}
		temp = temp->next;
	}
	return;
}

void wd_list_print(struct wd_list *wd_list) {
	struct name_to_wd *temp;
	temp = wd_list->head;
	while(temp != NULL) {
		printf("%d-->%s\n",temp->wd,temp->path);
		temp = temp->next;
	}
	return;
}

void wd_list_destroy(struct wd_list **wd_list) {
	struct name_to_wd *temp;
	while((*wd_list)->head!=NULL) {
		temp = (*wd_list)->head;
		(*wd_list)->head = (*wd_list)->head->next;
		free(temp);
	}
	free(*wd_list);
	return;
}


	//i_nodeList functions
void i_node_list_init(struct i_nodeList **ilist) {
	*ilist = (struct i_nodeList*)malloc(sizeof(struct i_nodeList));
	(*ilist)->head = NULL;
	return;
}

void i_node_list_print(struct i_nodeList *ilist) {
	struct i_node_node *temp;
	temp = ilist->head;
	while (temp != NULL) {
		name_print(temp->inode.head);
		temp = temp->next;
	}
	printf("\n");
	return;
}

struct i_node_node* i_node_list_insert(struct i_nodeList *ilist,ino_t i_n,time_t mt,int f_size) {
	struct i_node_node *temp;

	temp = (struct i_node_node*)malloc(sizeof(struct i_node_node));

	temp->inode.i_no = i_n;
	temp->inode.mtime = mt;
	temp->inode.file_size = f_size;
	temp->inode.head = NULL;
	temp->inode.links = 0;
	temp->inode.mark = -1;
	temp->inode.dest_node = NULL;

	temp->next = ilist->head;
	ilist->head = temp;
	return temp;
}

struct i_node_node* i_node_list_search(struct i_nodeList *ilist,ino_t i_n) {
	struct i_node_node *temp;
	temp = ilist->head;
	while (temp != NULL) {
		if (temp->inode.i_no == i_n) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

int i_node_list_remove(struct i_nodeList *ilist,ino_t i_n) {
	struct i_node_node *cur,*prev;
	cur = ilist->head;
	if (ilist->head == NULL) return -1; //empty list

	if (cur->inode.i_no == i_n) { //in case we delete the first node.
		ilist->head= ilist->head->next;
		name_destroy(&cur->inode.head);
		free(cur);
		return 0;
	}

	while (cur != NULL) {
		if (cur->inode.i_no == i_n) {
			prev->next = cur->next;
			name_destroy(&cur->inode.head);
			free(cur);
			return 0;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}
	return -1;
}

void i_node_list_dest_print(struct i_nodeList *ilist ) {
	struct i_node_node *temp;
	temp = ilist->head;
	while (temp != NULL) {
		if (temp->inode.dest_node != NULL) {
			name_print(temp->inode.dest_node->inode.head);
		}
		temp = temp->next;
	}
	printf("\n");
	return;
}

void i_node_list_destroy(struct i_nodeList **ilist) {
	struct i_node_node *temp;

	while((*ilist)->head != NULL) {
		temp = (*ilist)->head;
		(*ilist)->head = (*ilist)->head->next;
		name_destroy(&temp->inode.head);
		free(temp);
	}
	free(*ilist);
	return;
}


	//i_node_node function
int i_node_remove(struct i_node_node *i_node_n,char *name) {
	name_remove(&(i_node_n->inode.head),name);
	if (i_node_n->inode.head == NULL) {
		return 0;//no file or directory points to this i-node
	}			//so it should be deleted
	else {
		return 1;
	}
}

void i_node_destroy(struct i_node_node *head) {
	struct i_node_node *temp;

	while(head!=NULL) {
		temp = head;
		head = head->next;
		name_destroy(&(temp->inode.head));
		free(temp);
	}
	return;
}

void i_node_increase(struct i_node_node *i_node_n) {
	(i_node_n->inode.links)++;
	return;
}

int i_node_get_numlinks(struct i_node_node *i_node_n) {
	return i_node_n->inode.links;
}

char* i_node_get_headname(struct i_node_node *i_node_n) {
	return i_node_n != NULL ? i_node_n->inode.head->name : NULL;
}

struct i_node_node* i_node_get_dest_inode(struct i_node_node* i_node_n) {
	return i_node_n != NULL ? i_node_n->inode.dest_node : NULL;
}


	//NameNode functions
struct NameNode* name_insert(struct NameNode **head,char *n) {
	struct NameNode *temp;
	temp = (struct NameNode*)malloc(sizeof(struct NameNode));

	temp->name = (char*)malloc((strlen(n)+1)*sizeof(char));
	strcpy(temp->name,n);
	temp->next = *head;

	*head = temp;
	return temp;
}

int name_remove(struct NameNode **head,char *name) {
	struct NameNode *cur,*prev;
	cur = *head;
	if (*head == NULL) return -1; //empty list

	if (strcmp(cur->name,name) == 0) { //in case we delete the first node.
		*head = (*head)->next;
		free(cur->name);
		free(cur);
		return 0;
	}

	while (cur != NULL) {
		if (strcmp(cur->name,name) == 0) {
			prev->next = cur->next;
			free(cur->name);
			free(cur);
			return 0;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}
	return -1;
}

void name_print(struct NameNode *head) {
	struct NameNode *temp;
	temp = head;
	while (temp != NULL) {
		printf("%s, ",temp->name);
		temp = temp->next;
	}
	printf("\n");
	return;
}

void name_destroy(struct NameNode **head) {
	struct NameNode *temp;

	while((*head)!=NULL) {
		temp = *head;
		*head = (*head)->next;
		free(temp->name);
		free(temp);
	}
	return;
}


	//ListNode function
void listnode_swap(struct ListNode *p1,struct ListNode *p2) {
	struct ListNode *temp;
	temp->treenode = p1->treenode;
	p1->treenode = p2->treenode;
	p2->treenode = temp->treenode;
	free(temp);
	return;
}

char* listnode_get_name(struct ListNode listnode) {
	return listnode.treenode->name;
}

char* listnode_get_path(struct ListNode listnode) {
	return listnode.treenode->path;
}

char listnode_get_type(struct ListNode listnode) {
	return treenode_get_type(listnode.treenode);
}

struct i_node_node* listnode_get_inode(struct ListNode listnode) {
	return listnode.treenode->i_node;
}


	//list function
void list_init(struct List *list) {
	list->head = NULL;
	list->head = 0;
	return;
}

struct TreeNode* list_insert(struct List *list,char* name,char* path,char type) {
	struct ListNode *temp;

	temp = (struct ListNode*)malloc(sizeof(struct ListNode));

	temp->next = list->head;
	temp->treenode = (struct TreeNode*)malloc(sizeof(struct TreeNode));
	temp->treenode->name = (char*)malloc((strlen(name)+1)*sizeof(char));
	strcpy(temp->treenode->name,name);
	temp->treenode->path = (char*)malloc((strlen(path)+1)*sizeof(char));
	strcpy(temp->treenode->path,path);
	temp->treenode->type = type;
	list_init(&(temp->treenode->children));

	list->head = temp;
	//(list->size)++;
	return (list->head->treenode);
}

struct ListNode* list_insertafter(struct List *list,char *aftername,char *name,char *path,char type) {
	struct ListNode *cur,*temp;
	cur = list->head;

	while (cur != NULL) {
		if (strcmp(cur->treenode->name,aftername) == 0) {
			temp = (struct ListNode*)malloc(sizeof(struct ListNode));
			temp->treenode = (struct TreeNode*)malloc(sizeof(struct TreeNode));
			temp->treenode->name = (char*)malloc((strlen(name)+1)*sizeof(char));
			strcpy(temp->treenode->name,name);
			temp->treenode->path = (char*)malloc((strlen(path)+1)*sizeof(char));
			strcpy(temp->treenode->path,path);
			temp->treenode->type = type;
			list_init(&(temp->treenode->children));

			temp->next = cur->next;
			cur->next = temp;
			return temp;
		}
		else {
			cur = cur->next;
		}
	}
	return NULL;
}

int list_remove(struct List *list,char* name) {
	struct ListNode *cur,*prev;

	cur = list->head;
	if (list->head == NULL) return -1; //empty list

	if (strcmp(cur->treenode->name,name) == 0 ||
		strcmp(cur->treenode->path,name) == 0 ) { //in case we delete the first node.
		list->head = list->head->next;
		treenode_remove_all(cur->treenode);
		free(cur);
		//(list->size)--;
		return 1;
	}

	while (cur != NULL) {
		if (strcmp(cur->treenode->name,name) == 0 || 
			strcmp(cur->treenode->path,name) == 0 ) {

			prev->next = cur->next;
			treenode_remove_all(cur->treenode);
			free(cur);
			//(list->size)--;
			return 1;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}
	return -1;
}

void list_sort(struct List *list) {
	struct ListNode *temp,*first,*min,*prev_first,*prev_min,*prev_temp,*temp2,*temp3;
	if (list->head == NULL) {return;}

	first = list->head;
	while(first->next) {
		min = first;
		prev_temp = first; 
		temp = first->next;
		while(temp) {
			if (strcmp(listnode_get_name(*temp),listnode_get_name(*min)) < 0) {
				prev_min = prev_temp;
				min = temp;
			}
			prev_temp = temp;
			temp = temp->next;
		}
		//printf("p=:%s m=:%s\n",listnode_get_name(*prev_min),listnode_get_name(*min));
		//swapping
		if (min != first) {
			if (first == list->head) { //first iteration
				if (min == first->next) { //in case min is the second node of the list
					temp2 = min->next;
					list->head = min;
					min->next = first;
					first->next = temp2;
				}
				else {
					temp2 = min->next;

					list->head = min;
					min->next = first->next;
					prev_min->next = first;
					first->next = temp2;
				}
			}
			else if (min == first->next) { //in case min is the second node of the list
				temp2 = min->next;

				prev_first->next = min;
				min->next = first;
				first->next = temp2;
			}
			else if (prev_min != first) {
				temp2 = min->next;

				prev_first->next = min;
				min->next = first->next;
				prev_min->next = first;
				first->next = temp2;
			}
		}
		//swapping
		first = min;
		//prepare for next iteration
		prev_first = first;
		first = first->next;
	}
	//list_print(list);
	///getchar();
	return;
}

struct ListNode* list_search(struct List *list,char *name) {
	struct ListNode *temp;
	temp = list->head;

	while (temp != NULL) {
		if (strcmp(listnode_get_name(*temp),name) == 0) {return temp;}
		temp = temp->next;
	}
	return NULL;
}

void list_print(struct List *list) {
	struct ListNode *temp;
	temp = list->head;
	while (temp != NULL) {
		printf("%s ",temp->treenode->name);
		temp = temp->next;
	}
	printf("\n");
	return;
}

struct ListNode* list_get_head(struct List *list) {
	return list->head;
}

void list_destroy(struct List **list) {
	struct ListNode *temp;

	while((*list)->head != NULL) {
		temp = (*list)->head;
		(*list)->head = (*list)->head->next;
		free(temp->treenode->name);
		free(temp->treenode->path);
		free(temp->treenode);
		free(temp);
	}
	return;
}


	//TreeNode functions
void treenode_init(struct TreeNode *treenode) {
	treenode->children.head = NULL;
	return;
}

struct TreeNode* treenode_insert(struct TreeNode *treenode,char *name,char *path,char t) {
	return list_insert(&(treenode->children),name,path,t);
}

struct TreeNode* treenode_t_insert(struct TreeNode *treenode,char *name,char* path,char t,char *search_path) {
	int i;
	char *temp_path;
	struct TreeNode *trnode_ptr;
	struct ListNode *listnode;

	listnode = treenode->children.head;
	if (listnode == NULL) {return NULL;}

	while (listnode != NULL) {
		if (strcmp(listnode->treenode->path,search_path) == 0) {
			return list_insert(&listnode->treenode->children,name,path,t);
		}
		if (( trnode_ptr = treenode_t_insert(listnode->treenode,name,path,t,search_path)) != NULL) {
			return trnode_ptr;
		}
		listnode = listnode->next;
	}
	return NULL;
}

struct TreeNode* treenode_treeinsert(struct TreeNode *treenode,char *name,char* path,char t,char *search_path) {
	if(strcmp(treenode->path,search_path) == 0) {
		return list_insert(&treenode->children,name,path,t);
	}
	return treenode_t_insert(treenode,name,path,t,search_path);
}

int treenode_remove(struct TreeNode *treenode,char *name) {
	return list_remove(&(treenode->children),name);
}

int treenode_t_remove(struct i_nodeList *ilist,struct TreeNode *treenode,char *search_path) {
	int i;
	char *temp_path;
	struct ListNode *listnode,*prev;

	listnode = treenode->children.head;
	if (listnode == NULL) {return -1;}

	
	if (strcmp(listnode->treenode->path,search_path) == 0) { //in case we delete the first node.
		treenode->children.head = treenode->children.head->next;
		treenode_remove_name_inode(ilist,listnode->treenode);
		treenode_remove_all(listnode->treenode);
		free(listnode->treenode->name);
		free(listnode->treenode->path);
		free(listnode->treenode);
		free(listnode);
		//(list->size)--;
		return 1;
	}

	while (listnode != NULL) {
		if (strcmp(listnode->treenode->path,search_path) == 0) {
			prev->next = listnode->next;
			treenode_remove_name_inode(ilist,listnode->treenode);
			treenode_remove_all(listnode->treenode);
			free(listnode->treenode->name);
			free(listnode->treenode->path);
			free(listnode->treenode);
			free(listnode);
			return 1;
		}
		if (treenode_t_remove(ilist,listnode->treenode,search_path) != -1) {
			return 1;
		}
		prev = listnode;
		listnode = listnode->next;
	}
	return -1;
}

int treenode_treeremove(struct i_nodeList *ilist,struct TreeNode *treenode,char *search_path) {
	int ret_val;

	if(strcmp(treenode->path,search_path) == 0) {
		treenode_remove_name_inode(ilist,treenode);
		ret_val = treenode_remove_all(treenode);
		free(treenode->name);
		free(treenode->path);
		free(treenode);
		return ret_val;
	}
	return treenode_t_remove(ilist,treenode,search_path);
}

int treenode_t_remove2(struct i_nodeList *ilist,struct TreeNode *treenode,char *search_path) {
	int i;
	char *temp_path;
	struct ListNode *listnode,*prev;

	listnode = treenode->children.head;
	if (listnode == NULL) {return -1;}

	if (strcmp(listnode->treenode->path,search_path) == 0) { //in case we delete the first node.
		treenode->children.head = treenode->children.head->next;
		i_node_remove(listnode->treenode->i_node,listnode->treenode->path);
		treenode_remove_all(listnode->treenode);
		free(listnode);
		//(list->size)--;
		return 1;
	}

	while (listnode != NULL) {
		if (strcmp(listnode->treenode->path,search_path) == 0) {
			prev->next = listnode->next;
			i_node_remove(listnode->treenode->i_node,listnode->treenode->path);
			treenode_remove_all(listnode->treenode);
			free(listnode);
			return 1;
		}
		if (treenode_t_remove2(ilist,listnode->treenode,search_path) != -1) {
			return 1;
		}
		prev = listnode;
		listnode = listnode->next;
	}
	return -1;
}

int treenode_treeremove2(struct i_nodeList *ilist,struct TreeNode *treenode,char *search_path) {
	if(strcmp(treenode->path,search_path) == 0) {
		i_node_remove(treenode->i_node,treenode->path);
		return treenode_remove_all(treenode);
	}
	return treenode_t_remove2(ilist,treenode,search_path);
}

int treenode_remove_all(struct TreeNode *treenode) {
	struct ListNode *temp;
	struct List *list_ptr;
	if (treenode == NULL) {return -1;}

	temp = treenode->children.head;
	while (temp != NULL) {
		treenode_remove_all(temp->treenode);
		temp = temp->next;
	}
	list_ptr = &treenode->children;
	list_destroy(&list_ptr);
	return 0;
}

struct TreeNode* treenode_search_(struct TreeNode *treenode,char *path) {
	char *temp_path;
	struct TreeNode *trnode_ptr;
	struct ListNode *listnode;

	listnode = treenode->children.head;
	if (listnode == NULL) {return NULL;}

	while (listnode != NULL) {
		if (strcmp(listnode->treenode->path,path) == 0) {
			return listnode->treenode;
		}
		if (( trnode_ptr = treenode_search_(listnode->treenode,path)) != NULL) {
			return trnode_ptr;
		}
		listnode = listnode->next;
	}
	return NULL;
}

struct TreeNode* treenode_search(struct TreeNode *treenode,char *path) {
	if(strcmp(treenode->path,path) == 0) {
		return treenode;
	}
	return treenode_search_(treenode,path);
}

void treenode_remove_name_inode(struct i_nodeList *ilist,struct TreeNode *treenode) {
	int ret;
	ret = i_node_remove(treenode->i_node,treenode->path);
	if (ret == 0) {
		i_node_list_remove(ilist,treenode->i_node->inode.i_no);
	}
	return;
}

void treenode_print(struct TreeNode *treenode,int space) {
	int i;
	struct TreeNode *treenode_ptr;
	struct ListNode *listnode;

	space += 4;
	listnode = treenode->children.head;
	if (listnode == NULL) {return;}

	while (listnode != NULL) {
		treenode_print(listnode->treenode,space);
		for (i=0; i< space; i++) {
			printf(" ");
		}
		printf("[%s]\n",listnode->treenode->name);
		listnode = listnode->next;
	}
}

void treenode_sort(struct TreeNode *treenode) {
	int i;
	struct TreeNode *treenode_ptr;
	struct ListNode *listnode;

	list_sort(&treenode->children);

	listnode = treenode->children.head;
	while (listnode != NULL) {
		treenode_sort(listnode->treenode);
		listnode = listnode->next;
	}
	return;
}

char treenode_get_type(struct TreeNode *treenode) {
	return treenode->type;
}

struct i_node_node* treenode_get_dest_inode(struct TreeNode* treenode) {
	return treenode->i_node->inode.dest_node;
}

struct i_node_node* treenode_get_inode(struct TreeNode* treenode) {
	return treenode->i_node;
}


	//Tree functions
void tree_init(struct Tree **tree) {
	*tree = (struct Tree*)malloc(sizeof(struct Tree));
	(*tree)->root = NULL;
	return;
}

void tree_print(struct Tree *tree) {
	printf("[%s]\n",tree->root->name);
	treenode_print(tree->root,0);
}

void tree_root_insertion(struct Tree **tree,char *name) {
	(*tree)->root = (struct TreeNode*)malloc(sizeof(struct TreeNode));
	(*tree)->root->children.head = NULL;
	(*tree)->root->i_node = NULL;
	(*tree)->root->name = (char*)malloc((strlen(name)+1)*sizeof(char));
	strcpy((*tree)->root->name,name);
	(*tree)->root->path = (char*)malloc((strlen(name)+1)*sizeof(char));
	strcpy((*tree)->root->path,name);
	return;
}

void tree_destroy(struct Tree **tree) {
	treenode_remove_all((*tree)->root);
	free((*tree)->root->name);
	free((*tree)->root->path);
	free((*tree)->root);
	free(*tree);
	return;
}