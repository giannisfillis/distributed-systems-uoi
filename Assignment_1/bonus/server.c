#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "readwrite.h"
#include "msg.h"
#include "clientsList.h"
#include "printMsg.h"
#include <pthread.h>

struct clientsList cl;
struct client_info {
	int fd;
	char name[MAX_NAME_LEN];
};

struct history_node{
	char sender[MAX_NAME_LEN];
	char msg[MAX_MSG_LEN];
	struct history_node *next;
};

// ptrs for the start and end of the history linked list 
struct history_node *history_head = NULL;
struct history_node *history_tail = NULL;

// lock for the history 
pthread_mutex_t history_lock = PTHREAD_MUTEX_INITIALIZER;

void add_to_history(const char *sender, const char *msg){
	// creating new node
	struct history_node *new_node = malloc(sizeof(struct history_node));
	if (!new_node){
		perror("Error with malloc");
		return;
	}
	strncpy(new_node->sender, sender, MAX_NAME_LEN-1);
	new_node->sender[MAX_NAME_LEN-1] = '\0'; // adding to signal end of string 
	strncpy(new_node->msg, msg, MAX_MSG_LEN-1);
	new_node->msg[MAX_MSG_LEN-1] = '\0'; // adding to signal end of string 
	
	new_node->next = NULL;
	
	// lock and add to the end of the list 
	pthread_mutex_lock(&history_lock);
	if (history_tail == NULL){ // first message 
		history_head = new_node;
		history_tail = new_node;
	}else{
		history_tail->next = new_node;
		history_tail = new_node;
	}
	pthread_mutex_unlock(&history_lock);
}

void send_history_to_client(int client_fd){
	pthread_mutex_lock(&history_lock);
	struct history_node *current = history_head;
	
	while(current != NULL){
		sendNewMessage(client_fd, current->msg, current->sender);
		current = current->next;
	}
	pthread_mutex_unlock(&history_lock);
}

void free_history(){
	pthread_mutex_lock(&history_lock);
	
	struct history_node *current = history_head;
	struct history_node *next_node;
	
	while(current != NULL){
		
		// get the pointer of the next node before free 
		next_node = current->next;
		
		free(current);
		current= next_node;
	}
	
	// set to null these pointers 
	history_head = NULL;
	history_tail = NULL;
	
	pthread_mutex_unlock(&history_lock);
}

void *handle_client(void *arg){
	struct client_info *cinfo = (struct client_info *)arg;
	int client_fd = cinfo->fd;
	
	char name[MAX_NAME_LEN];
	strcpy(name, cinfo->name); //copy name 
	
	char msg[MAX_MSG_LEN+1];
	char print_msg[MAX_PRINT_MSG_LEN+1];
	
	
	snprintf(print_msg, sizeof(print_msg), " -- %s has joined the chat\n ", name);
	
	printMsg(stdout, print_msg);
	
	//bonus
	send_history_to_client(client_fd);
	
	while(1){
		ssize_t read_bytes = recvMessage(client_fd, msg, MAX_MSG_LEN);
		if (read_bytes == 0 || read_bytes == -1){
			// error 
			snprintf(print_msg, sizeof(print_msg), " -- %s has left the chat --\n",name);
			printMsg(stdout, print_msg);
			
			removeClient(&cl, client_fd); //remove from list
			close(client_fd); // close socket 
			free(cinfo);
			pthread_exit(NULL); // terminate thread 
			
		} else if (read_bytes == -2){
			snprintf(print_msg, sizeof(print_msg), "error message too large from %s \n", name);
			printMsg(stderr, print_msg);
			continue;
		}
		
		// check if the name has changed
		if (strncmp(msg, "\\name ",6) == 0){
			char *new_name = msg + 6; 
			if (strlen(new_name) > MAX_NAME_LEN) {
                snprintf(print_msg, sizeof(print_msg), "-- User %s tried to set invalid name\n", name);
                printMsg(stdout, print_msg);
            } else {
				snprintf(print_msg, sizeof(print_msg), "-- %s Changed name to %s\n", name, new_name);
				strncpy(name, new_name, MAX_NAME_LEN -1);
				name[MAX_NAME_LEN-1] = '\0';
                printMsg(stdout, print_msg);
            }
		}
		else{
			// print to server
			//snprintf(print_msg, sizeof(print_msg), "\033[35m%s\033[0m> %s\n", name, msg);
			//printMsg(stdout, print_msg);
			
			broadcastClientsList(&cl, msg, name, client_fd);
			
			// bonus 
			add_to_history(name, msg); 
		}					
	}
	return NULL;	
}

int main() {
	struct sockaddr_in addr;
	int socket_fd;
	int socket_option;

	// data required to read the IP address of the connected client
	struct sockaddr_in client_addr;
	
	// initialize list of clients 
	initClientsList(&cl);

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		perror("socket failure");
		exit(EXIT_FAILURE);
	}

	// necessary option for quick reusage of the port
	socket_option = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) == -1) {
		close(socket_fd);
		perror("setsockopt failure");
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(socket_fd);
		perror("bind failure");
		exit(EXIT_FAILURE);
	}

	if (listen(socket_fd, SOMAXCONN) == -1) {
		close(socket_fd);
		perror("listen failure");
		exit(EXIT_FAILURE);
	}

	printf("Listening on port %d\n", PORT);


	int client_counter = 1;
	while (1) {
		socklen_t client_addr_len = sizeof(client_addr);
		int client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		if (client_fd == -1){
			perror("accept failure");
			continue; // go to the next accept
		}
		
		// add client to the list 
		if (addClient(&cl, client_fd) == -1){
			printf("Error adding new user the list\n");
			close(client_fd);
			continue;
		}
		
		// allocating memory for user struct 
		struct client_info *cinfo = malloc(sizeof(struct client_info));
		if (!cinfo){
			perror("malloc failure");
			removeClient(&cl, client_fd);
			close(client_fd);
			continue;
		}
		
		cinfo->fd = client_fd;
		snprintf(cinfo->name, MAX_NAME_LEN, "User%d",client_counter++);
		pthread_t tid;
		if(pthread_create(&tid, NULL, handle_client,cinfo) !=0){
			perror("pthread create failure");
			removeClient(&cl, client_fd);
			close(client_fd);
			free(cinfo);
		}else{
			pthread_detach(tid);
		}
	}	

	//close(socket_fd);
	destroyClientsList(&cl);
	free_history();
	return 0;
}