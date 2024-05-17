#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int restart_openocd() {
	return system("systemctl restart openocd");
}

int restart_serial() {
	return system("systemctl restart debug");
}

int set_target(const char *target_name) {
	char command[256];
	snprintf(command, sizeof(command), "sed -i 's/TARGET=.*/TARGET=%s/' /opt/mcuprogrammer/config.env", target_name);

	return system(command);
}

int set_baudrate(const char *baudrate) {
	char command[256];
	snprintf(command, sizeof(command), "sed -i 's/BAUDRATE=.*/BAUDRATE=%s/' /opt/mcuprogrammer/config.env", baudrate);

	return system(command);
}

void error(const char *msg) {
	perror(msg);
	exit(1);
}

void handle_client(int newsockfd) {
	char buffer[BUFFER_SIZE];
	int n;

	const char *help_msg =
		"Available commands:\n"
		"  help                  - Show this help message\n"
		"  target <target name>  - Set target name for OpenOCD\n"
		"  baudrate <baudrate>   - Set baudrate for serial port\n"
		"  exit                  - Disconnect from the server\n";

	n = write(newsockfd, "Connected to MCU Programmer CLI. Type 'help' for a list of commands.\n", 69);

	if (n < 0) {
		error("ERROR writing to socket");
	}

	while (1) {
		bzero(buffer, BUFFER_SIZE);
		n = read(newsockfd, buffer, BUFFER_SIZE - 1);

		if (n <= 0) {
			break;
		}

		buffer[strcspn(buffer, "\r\n")] = 0; // Strip newline characters

		printf("Received command: %s\n", buffer);

		if (strcmp(buffer, "help") == 0) {
			n = write(newsockfd, help_msg, strlen(help_msg));
		} else if (strncmp(buffer, "target ", 7) == 0) {
			char *target_name = buffer + 7;
			int ret = 0;

			ret = set_target(target_name);

			if (ret != 0) {
				n = write(newsockfd, "Failed to set target.\n", 22);
			}

			ret = restart_openocd();

			if (ret != 0) {
				n = write(newsockfd, "Failed to restart OpenOCD.\n", 28);
			}
		} else if (strncmp(buffer, "baudrate ", 9) == 0) {
			char *baudrate = buffer + 9;
			int ret = 0;

			ret = set_baudrate(baudrate);

			if (ret != 0) {
				n = write(newsockfd, "Failed to set baudrate.\n", 24);
			}

			ret = restart_serial();

			if (ret != 0) {
				n = write(newsockfd, "Failed to restart serial.\n", 26);
			}
		} else if (strcmp(buffer, "exit") == 0) {
			n = write(newsockfd, "Goodbye!\n", 9);

			break;
		} else {
			const char *unknown_cmd = "Unknown command. Type 'help' for a list of commands.\n";
			n = write(newsockfd, unknown_cmd, strlen(unknown_cmd));
		}

		if (n < 0) {
			error("ERROR writing to socket");
		}
	}

	close(newsockfd);
}

int main() {
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("ERROR opening socket");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		error("ERROR on binding");
	}

	listen(sockfd, 5);
	clilen = sizeof(cli_addr);

	printf("Server running on port %d\n", PORT);

	while (1) {
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd < 0) {
			error("ERROR on accept");
		}

		handle_client(newsockfd);
	}

	close(sockfd);

	return 0;
}
