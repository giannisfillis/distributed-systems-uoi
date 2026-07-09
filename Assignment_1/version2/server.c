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

struct thread_args{
	int client_fd;
	char client_ip[INET_ADDRSTRLEN];
};

int client_disconnected = 0;
pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_quit = 0;

void * receive_msg_thread_func(void *arg){
	struct thread_args *args = (struct thread_args *) arg;
	int fd = args->client_fd;
	char ip[INET_ADDRSTRLEN];
	strcpy(ip, args->client_ip);
	free(args);
	
	char msg[MAX_MSG_LEN+1];
	char print_msg[MAX_PRINT_MSG_LEN+1];
	
	while(1){
		ssize_t read_bytes = recvMessage(fd, msg, MAX_MSG_LEN);
		
		if (read_bytes <= 0) {
			int temp_server_quit = 0;
			pthread_mutex_lock(&disconnect_mutex);
			temp_server_quit = server_quit;
			pthread_mutex_unlock(&disconnect_mutex);
			if (temp_server_quit == 1){
				snprintf(print_msg, sizeof(print_msg), " Shutting Down Server...\n");
				printMsg(stdout, print_msg);
				shutdown(fd, SHUT_RDWR);
				break;
			}
			else{
				// lock, read, unlock
				pthread_mutex_lock(&disconnect_mutex);
				client_disconnected = 1;
				pthread_mutex_unlock(&disconnect_mutex);
				shutdown(fd, SHUT_RDWR);
				snprintf(print_msg, sizeof(print_msg), "-- User %s has left the conversation, press enter to continue\n", ip);
				printMsg(stdout, print_msg);
				//shutdown(fd, SHUT_RDWR);
				break;
			}
		}
		else if (read_bytes <= -1){
			pthread_mutex_lock(&disconnect_mutex);
			client_disconnected = 1;
			pthread_mutex_unlock(&disconnect_mutex);
			shutdown(fd, SHUT_RDWR);
			snprintf(print_msg, sizeof(print_msg), "recvMessage failure");
			printMsg(stdout, print_msg);
			//shutdown(fd, SHUT_RDWR);
			break;
		}
		
		snprintf(print_msg, sizeof(print_msg), "\033[35mUser %s\033[0m> %s\n>", ip, msg);
		printMsg(stdout, print_msg);
		strcpy(msg, "");
	}
	return NULL;
}

int main() {
	struct sockaddr_in addr;
	int socket_fd;
	int client_fd;
	int socket_option;

	
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char client_ip[INET_ADDRSTRLEN];

	char msg[MAX_MSG_LEN + 1];
	char print_msg[MAX_PRINT_MSG_LEN + 1];
	
	pthread_t read_thread;
	//int server_disconnected = 0;
	//pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		perror("socket failure");
		exit(EXIT_FAILURE);
	}

	
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

	client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	if (client_fd == -1) {
		close(socket_fd);
		perror("accept failure");
		exit(EXIT_FAILURE);
	}

	inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
	snprintf(print_msg, sizeof(print_msg), "-- User %s has joined the conversation\n", client_ip);
	printMsg(stdout, print_msg);
	
	// creating the struct and the thread
	struct thread_args *t_args = malloc(sizeof(struct thread_args));
	t_args->client_fd = client_fd;
	strcpy(t_args->client_ip, client_ip);
	int EOF_found = 0;
	
	if(pthread_create(&read_thread, NULL, receive_msg_thread_func, t_args) != 0){
		perror("pthread_create failure");
		close(client_fd);
		close(socket_fd);
		exit(EXIT_FAILURE);
	}
	
	
	
	
	do{
		snprintf(print_msg, sizeof(print_msg), ">");
		printMsg(stdout, print_msg);
		
		if (fgets(msg, sizeof(msg), stdin) == NULL){
			pthread_mutex_lock(&disconnect_mutex);
			server_quit = 1;
			pthread_mutex_unlock(&disconnect_mutex);
			EOF_found = 1;
			break;
		}
		
		int temp_disconnect_flag = 0;
		pthread_mutex_lock(&disconnect_mutex);
		temp_disconnect_flag = client_disconnected;
		pthread_mutex_unlock(&disconnect_mutex);
			
		if (temp_disconnect_flag){
			EOF_found = 1;
			break;
		}
		msg[strcspn(msg, "\n")] = '\0';
		
		if(strlen(msg) >0){
			if (sendMessage(client_fd,msg) == -1){
				break;
			}
		}

		
	}while(!EOF_found);
	
	shutdown(client_fd, SHUT_RDWR);
	pthread_join(read_thread, NULL);
	close(client_fd);
	close(socket_fd);
	
	return 0;
	
}