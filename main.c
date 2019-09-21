#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include "iheader.h"

static volatile sig_atomic_t keep_running = 1;

static void sig_handler(int num)
{
    keep_running = 0;
}

int main(int argc, char *argv[]) {
	int fd,length, read_ptr, read_offset,stop = 10,cookie = -1;
	int flag_moved_to = 0,flag_moved_from = 0;
	struct stat statbuf;
	struct Tree *tree,*bu_tree;
	struct TreeNode *treenode,*treenode_ptr;
	struct i_nodeList *inode_list,*bu_inode_list;
	struct i_node_node *i_node_node_ptr,*i_node_node_ptr2;
	struct i_node_node *i_node_n;
	pid_t pid;
	struct wd_list *wd_list;
	char backup_name[1024],dir_name[1024],buffer[EVENT_BUF_LEN],parent_path[1024];
	char *ret,*full_path,*bu_full_path,*moved_from,type,*moved_out;

	if (argc < 3 ) { //a source-name and back-name should be given
		printf("usage: %s <source> <backup>\n",argv[0]);exit(-1);
	}
	
	signal(SIGINT, sig_handler);

	if(access(argv[1],F_OK) == -1 ) {//check if the given directory exists
		printf("main error:%s doesn't exist\n",argv[1]);return -1;
	}
	strcpy(dir_name,argv[1]);

	if(access(argv[2],F_OK) == -1 ) {//check if the given directory-name exists
		//if it doesn't exit,create a new one
		if (mkdir(argv[2],0700) == -1) {printf("main error: mkdir\n");return -1;}
	}
	strcpy(backup_name,argv[2]);


	full_path = (char*)malloc((1024)*sizeof(char));
	bu_full_path = (char*)malloc((1024)*sizeof(char));
	moved_from = (char*)malloc((1024)*sizeof(char));
	moved_out = (char*)malloc((1024)*sizeof(char));
	//initialize the tree structures
	tree_init(&tree);
	tree_init(&bu_tree); //back up tree
	//initialize the i-node lists
	i_node_list_init(&inode_list);
	i_node_list_init(&bu_inode_list); //back up i-node list
	//initialize the list with the WDs
	wd_list_init(&wd_list);
	//insert the root directory to the tree structure's root
	tree_root_insertion(&tree,dir_name);
	tree_root_insertion(&bu_tree,backup_name);
	//create tree structures
	dir_tree_create(inode_list,tree->root,dir_name,0);
	dir_tree_create(bu_inode_list,bu_tree->root,backup_name,0);
	//sort the tree structures
	treenode_sort(tree->root);	
	treenode_sort(bu_tree->root);
	
	//sychronize the tree structures
	sychronization(tree->root,bu_tree->root,inode_list,bu_inode_list);
	
	if ((fd = inotify_init()) < 0) {printf("main error: inotify_init");return -1;}
	//add all the directories for monitoring
	add_dir_watch(fd,dir_name,wd_list);
	
	read_offset = 0; //remaining number of bytes from previous read
	while (keep_running) {
		//printf("continue?[y/n]\n");
		//if (getchar() == 'n') {break;}
		/* read next series of events */
		length = read(fd, buffer + read_offset, sizeof(buffer) - read_offset);
		if (length < 0)
			fail("read");
		length += read_offset; // if there was an offset, add it to the number of bytes to process
		read_ptr = 0;
		
		// process each event
		// make sure at least the fixed part of the event in included in the buffer
		while (read_ptr + EVENT_SIZE <= length) { 
			//point event to beginning of fixed part of next inotify_event structure
			struct inotify_event *event = (struct inotify_event *) &buffer[ read_ptr ];
			
			// if however the dynamic part exceeds the buffer, 
			// that means that we cannot fully read all event data and we need to 
			// deffer processing until next read completes
			if( read_ptr + EVENT_SIZE + event->len > length ) 
				break;
			//event is fully received, process

			//########################################
			if (strstr(event->name,".swp") != NULL) { //ignore .swp files
				read_ptr += EVENT_SIZE + event->len;continue;
			}
			printf("WD:%i %s %s %s COOKIE=%u\n",event->wd, event_name(event), 
				target_type(event), target_name(event), event->cookie);

			//this "if" statement is true, if the previous event was "in_moved from"
			//and the current event is not "in_moved_to"
			if (flag_moved_from == 1 && !(event->mask & IN_MOVED_TO)) {
				in_moved_from(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,moved_out,event,
					backup_name,full_path,bu_full_path);	
				flag_moved_from = 0;
				printf("in_moved from\n");
			}

			if (event->mask & IN_CREATE){
				if (event->mask & IN_ISDIR) {
					in_create_dir(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
						backup_name,full_path,bu_full_path);
				}
				else {
					in_create_file(tree,bu_tree,inode_list,bu_inode_list,wd_list,event,
						backup_name,full_path,bu_full_path);
				}
				printf("create\n");
			}
			else if (event->mask & IN_ATTRIB){
				if (!(event->mask & IN_ISDIR)) {
					in_attrib(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
						backup_name,full_path,bu_full_path);
				}
				printf("attrib\n");
			}
			else if (event->mask & IN_MODIFY){
				if (!(event->mask & IN_ISDIR)) {
					in_modify(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
						backup_name,full_path,bu_full_path);	
				}
				printf("modify\n");
			}
			else if (event->mask & IN_CLOSE_WRITE){
				in_close_write(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
					backup_name,full_path,bu_full_path);	

				printf("close write\n");
			}
			else if (event->mask & IN_DELETE){
				if (!(event->mask & IN_ISDIR)) {
					in_delete(tree,bu_tree,inode_list,bu_inode_list,wd_list,event,
						backup_name,full_path,bu_full_path);
				}
				printf("delete\n");
			}
			else if (event->mask & IN_DELETE_SELF){
				in_delete_self(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
					backup_name,full_path,bu_full_path);
				printf("watch target deleted\n");

			}
			else if(event->mask & IN_MOVED_FROM){
				cookie = event->cookie; //we keep the cookie 
				flag_moved_from = 1; //and name field for the next event
				wd_list_search(wd_list,event->wd,moved_from);
				strcpy(moved_out,moved_from);strcat(moved_out,"/");strcat(moved_out,event->name);
				printf("moved out\n");
			}
			else if (event->mask & IN_MOVED_TO){
				if (cookie == event->cookie) {
					in_moved_to(tree,bu_tree,inode_list,bu_inode_list,wd_list,fd,event,
						backup_name,full_path,bu_full_path,moved_from);
				}
				else {
					in_create_file(tree,bu_tree,inode_list,bu_inode_list,wd_list,event,
						backup_name,full_path,bu_full_path);
					//moreover,copy the data//
				}
				cookie = -1;
				flag_moved_from = 0;
				printf("moved into\n");
			}
			printf("****************Tree*****************************\n");
			tree_print(tree);
			printf("****************Back up Tree*********************\n");
			tree_print(bu_tree);printf("\n\n");
			printf("****************original inode list**************\n");
			i_node_list_print(inode_list);printf("\n\n");
			printf("****************backup inode list****************\n");
			i_node_list_print(bu_inode_list);printf("\n\n");
			//printf("dest inode list\n");
			//i_node_list_dest_print(inode_list);printf("\n\n");
			//########################################

			//advance read_ptr to the beginning of the next event
			read_ptr += EVENT_SIZE + event->len;
		}
		//check to see if a partial event remains at the end
		if( read_ptr < length ) {
			//copy the remaining bytes from the end of the buffer to the beginning of it
			memcpy(buffer, buffer + read_ptr, length - read_ptr);
			//and signal the next read to begin immediatelly after them			
			read_offset = length - read_ptr;
		} 
		else
			read_offset = 0;
	}

	printf("Memory Deallocation\n");
		//destruction of the allocated memory
	destroy_dir_watch(fd,wd_list);
	wd_list_destroy(&wd_list);
	tree_destroy(&tree);
	tree_destroy(&bu_tree);
	i_node_list_destroy(&inode_list);
	i_node_list_destroy(&bu_inode_list);
	free(full_path);
	free(bu_full_path);
	free(moved_from);
	free(moved_out);
	close(fd);
	return 0;
}	