#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024

const char *resp_ok = "HTTP/1.1 200 OK\r\n\r\n";
const char *resp_not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
int server_fd;
char *directory;

void *handle_reqs(void *fd) {
	int client_fd = *((int *)fd);
	char req_buf[BUF_SIZE] = {0};
	if (read(client_fd, req_buf, sizeof(req_buf)) < 0) {
		printf("read request failed\n");
		goto error;
	}

	char *tmp = strdup(req_buf);
	char *path = strtok(tmp, " ");
	char method[10] = {0};
	strcpy(method, path);
	path = strtok(NULL, " ");

	const char *resp = NULL;
	if (strstr(path, "/files/")) {
		char *filename = strtok(path, "/");
		filename = strtok(NULL, "/");
		char file_path[BUF_SIZE] = {0};
		sprintf(file_path, "%s%s", directory, filename);
		if (strcmp(method, "GET") == 0) {
			int fide = open(file_path, O_RDONLY);
			if (fide > 0) {
				char resp_file[BUF_SIZE] = {0};
				char file_contents[BUF_SIZE] = {0};
				ssize_t bytes_read =
					read(fide, file_contents, sizeof(file_contents));
				sprintf(resp_file,
						"HTTP/1.1 200 OK\r\nContent-Type: "
						"application/octet-stream\r\nContent-Length: "
						"%lu\r\n\r\n%s\r\n\r\n",
						bytes_read, file_contents);
				resp = resp_file;
			} else {
				resp = resp_not_found;
			}
		} else {
			int fide = open(file_path, O_CREAT | O_WRONLY);
			char file_contents[BUF_SIZE] = {0};
			char *tmp = strtok(req_buf, "\r\n");
			while (1) {
				tmp = strtok(NULL, "\r\n");
				if (tmp == NULL)
					break;
				strcpy(file_contents, tmp);
			}
			ssize_t bytes_written =
				write(fide, file_contents, strlen(file_contents));
			char resp_file[BUF_SIZE] = "HTTP/1.1 201 Created\r\n\r\n";
			resp = resp_file;
		}
	} else if (strstr(path, "/user-agent")) {
		char *user_agent = strstr(req_buf, "User-Agent");
		printf("%s\n", user_agent);
		user_agent += 12;
		char resp_user_agent[BUF_SIZE] = {0};
		sprintf(resp_user_agent,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/plain\r\nContent-Length: %lu\r\n\r\n%s",
				strlen(user_agent) - 4, user_agent);
		resp = resp_user_agent;
		printf("%s\n", resp);
	} else if (strstr(path, "/echo/")) {
		char *str = strtok(path, "/");
		str = strtok(NULL, "/");
		printf("%s\n", str);

		char resp_echo[BUF_SIZE] = {0};
		char *encoding = strstr(req_buf, "Accept-Encoding:");
		if (encoding == NULL || strstr(encoding, "gzip") == NULL) {
			sprintf(resp_echo,
					"HTTP/1.1 200 OK\r\nContent-Type: "
					"text/plain\r\nContent-Length: "
					"%lu\r\n\r\n%s",
					strlen(str), str);
		} else {
			encoding += 17;
			sprintf(resp_echo,
					"HTTP/1.1 200 OK\r\nContent-Type: "
					"text/plain\r\nContent-Encoding: %s\r\nContent-Length: "
					"%lu\r\n\r\n%s",
					encoding, strlen(str), str);
		}
		resp = resp_echo;
	} else if (strcmp(path, "/") == 0) {
		resp = resp_ok;
	} else {
		resp = resp_not_found;
	}

	if (send(client_fd, resp, strlen(resp), 0) < 0) {
		printf("send response failed\n");
		goto error;
	}
	return NULL;

error:
	close(server_fd);
	return NULL;
}

int main(int argc, char **argv) {

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--directory") == 0) {
			directory = argv[i + 1];
		}
	}

	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

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

	while (1) {
		unsigned int client_addr_len;
		struct sockaddr_in client_addr;

		client_addr_len = sizeof(client_addr);

		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
							   &client_addr_len);
		if (client_fd < 0) {
			printf("accept failed\n");
			goto error;
		}
		pthread_t id;
		pthread_create(&id, NULL, handle_reqs, &client_fd);
		printf("Client connected - %d\n", client_fd);
	}

	close(server_fd);

	return 0;
error:
	close(server_fd);
	return 1;
}
