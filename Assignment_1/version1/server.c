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

struct clientsList cl;

int main() {
	struct sockaddr_in addr;
	int socket_fd;
	int client_fd;
	int socket_option;

	// data required to read the IP address of the connected client
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char client_ip[INET_ADDRSTRLEN];

	char msg[MAX_MSG_LEN + 1];
	char print_msg[MAX_PRINT_MSG_LEN + 1];

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

	client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	if (client_fd == -1) {
		close(socket_fd);
		perror("accept failure");
		exit(EXIT_FAILURE);
	}

	inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
	snprintf(print_msg, sizeof(print_msg), "-- User %s has joined the conversation\n", client_ip);
	printMsg(stdout, print_msg);

	while (1) {
		ssize_t read_bytes = recvMessage(client_fd, msg, MAX_MSG_LEN);

		if (read_bytes == 0) {
			snprintf(print_msg, sizeof(print_msg), "-- User %s has left the conversation\n", client_ip);
			printMsg(stdout, print_msg);
			close(client_fd);
			break;
		}
		else if (read_bytes == -1) {
			perror("recvMessage failure");
			close(client_fd);
			close(socket_fd);
			exit(EXIT_FAILURE);
		} else if (read_bytes == -2) {
			snprintf(print_msg, sizeof(print_msg), "error: the message of the client cannot fit in the 'msg' buffer\n");
			printMsg(stderr, print_msg);
			close(client_fd);
			close(socket_fd);
			exit(EXIT_FAILURE);
		}

		snprintf(print_msg, sizeof(print_msg), "\033[35mUser %s\033[0m> %s\n", client_ip, msg);
		printMsg(stdout, print_msg);
		strcpy(msg, "");
		
		// my addition, send a message to the client
		char reply[MAX_MSG_LEN+1];
		int EOF_found = 0;
		do{
			snprintf(print_msg, sizeof(print_msg), "> ");
			printMsg(stdout, print_msg);
			if (fgets(reply, sizeof(reply), stdin) == NULL){
				EOF_found = 1;
				break;
			}
			reply[strcspn(reply, "\n")] = '\0';
		}while(strlen(reply) ==0 && !EOF_found);
		
		if (!EOF_found){
			if (sendMessage(client_fd, reply) == -1){
				perror("sendMessage failure");
				close(client_fd);
				close(socket_fd);
				exit(EXIT_FAILURE);
			}
		}else{
			printf("\n Shutting down the server \n");
			close(client_fd);
			break;
		}
	}

	close(socket_fd);

	return 0;
}