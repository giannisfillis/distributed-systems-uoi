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

int server_disconnected = 0;
pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;

void * receive_msg_thread_func(void *arg){
	int fd = *(int *) arg;
	char msg[MAX_MSG_LEN+1];
	char sender[MAX_NAME_LEN+1];
	char print_msg[MAX_PRINT_MSG_LEN+1];
	
	while(1){
		ssize_t read_bytes = recvNewMessage(fd, msg, MAX_MSG_LEN, sender, MAX_NAME_LEN);
		
		if (read_bytes == 0){
			snprintf(print_msg, sizeof(print_msg), "\r Server Shutdown, disconnecting. Press Enter to continue...\n");
			printMsg(stdout, print_msg);
			shutdown(fd, SHUT_RDWR); //  shutting down and unblocking the other blocked thread
			//exit(EXIT_FAILURE);
			pthread_mutex_lock(&disconnect_mutex);
			server_disconnected = 1;
			pthread_mutex_unlock(&disconnect_mutex);
			break;
		}
		
		else if (read_bytes <= -1){
			snprintf(print_msg, sizeof(print_msg), "\r Error with server communication\n");
			printMsg(stdout, print_msg);
			shutdown(fd, SHUT_RDWR); //  shutting down and unblocking the other blocked thread
			//exit(EXIT_FAILURE);
			pthread_mutex_lock(&disconnect_mutex);
			server_disconnected = 1;
			pthread_mutex_unlock(&disconnect_mutex);
			break;
		}
		
		snprintf(print_msg, sizeof(print_msg), "\r\033[35m%s\033[0m> %s\n>", sender, msg);
		printMsg(stdout, print_msg);
		
	}
	
	return NULL;
	
}

int main()
{
	struct sockaddr_in addr;
	int socket_fd;
	char msg[MAX_MSG_LEN + 1];
	char print_msg[MAX_PRINT_MSG_LEN + 1];
	int EOF_found = 0;
	

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		perror("socket failure");
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, SERVER_IP_ADDRESS, &addr.sin_addr) <= 0) {
	    close(socket_fd);
	    perror("inet_pton failure");
	    exit(EXIT_FAILURE);
	}

	if (connect(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(socket_fd);
	    perror("connect failure");
	    exit(EXIT_FAILURE);
	}

	pthread_t recv_tid;
	if (pthread_create(&recv_tid, NULL, receive_msg_thread_func, &socket_fd) != 0){
		perror("pthread_create failure");
		close(socket_fd);
		exit(EXIT_FAILURE);
	}

	do {
		do {
			snprintf(print_msg, sizeof(print_msg), "> ");
			printMsg(stdout, print_msg);

			if (fgets(msg, sizeof(msg), stdin) == NULL) {
				EOF_found = 1;
				break;
			}
			
			int temp_disconnect_flag = 0;
			pthread_mutex_lock(&disconnect_mutex);
			temp_disconnect_flag = server_disconnected;
			pthread_mutex_unlock(&disconnect_mutex);
			
			if (temp_disconnect_flag){
				EOF_found = 1;
				break;
			}

			msg[strcspn(msg, "\n")] = '\0';
		} while (strlen(msg) == 0 && !EOF_found);

		if (!EOF_found) {
			if (sendMessage(socket_fd, msg) == -1) {
				close(socket_fd);
				perror("sendMessage failure");
				exit(EXIT_FAILURE);
			}
			strcpy(msg, "");
		}
	} while (!EOF_found);

	shutdown(socket_fd, SHUT_RDWR);
	pthread_join(recv_tid, NULL); // wait for the receive message thread to finish
	close(socket_fd);

	return 0;
}
