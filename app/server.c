#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024

char *resp_ok = "HTTP/1.1 200 OK\r\n\r\n";
char *resp_not_found = "HTTP/1.1 404 Not Found\r\n\r\n";

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd;
	unsigned int client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		goto error;
	}

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// // ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
		0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		goto error;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) !=
		0) {
		printf("Bind failed: %s \n", strerror(errno));
		goto error;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		goto error;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int client_fd =
		accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client_fd < 0) {
		printf("accept failed\n");
		goto error;
	}
	printf("Client connected\n");

	char req_buf[BUF_SIZE] = {0};
	if (read(client_fd, req_buf, sizeof(req_buf)) < 0) {
		printf("read request failed\n");
		goto error;
	}

	char *path = strtok(req_buf, " ");
	path = strtok(NULL, " ");

	char *resp = !strcmp(path, "/") ? resp_ok : resp_not_found;

	if (send(client_fd, resp, strlen(resp), 0) < 0) {
		printf("send response failed\n");
		goto error;
	}

	close(server_fd);

	return 0;
error:
	close(server_fd);
	return 1;
}
