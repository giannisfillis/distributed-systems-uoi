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

int main()
{
	struct sockaddr_in addr;
	int socket_fd;
	char msg[MAX_MSG_LEN + 1];
	char print_msg[MAX_PRINT_MSG_LEN + 1];
	int EOF_found;

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

	EOF_found = 0;

	do {
		do {
			snprintf(print_msg, sizeof(print_msg), "> ");
			printMsg(stdout, print_msg);

			if (fgets(msg, sizeof(msg), stdin) == NULL) {
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

			char reply[MAX_MSG_LEN+1];
			ssize_t read_bytes = recvMessage(socket_fd, reply, MAX_MSG_LEN);
			if (read_bytes > 0){
				snprintf(print_msg, sizeof(print_msg), "\033[35mServer\033[0m> %s\n", reply);
				printMsg(stdout, print_msg);
			}
			else if (read_bytes == 0){
				snprintf(print_msg, sizeof(print_msg), "-- The server has left the conversation\n");
				printMsg(stdout, print_msg);
				EOF_found = 1;
			}
			else{
				snprintf(print_msg, sizeof(print_msg), "-- An error occured\n");
				printMsg(stdout, print_msg);
				EOF_found = 1;
			}
		}
	} while (!EOF_found);

	close(socket_fd);

	return 0;
}
