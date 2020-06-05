#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "err.h"

#define BUFFER_SIZE   1000
#define PORT_NUM     10001

int main(int argc, char *argv[]) {
	int sock;
	int flags, sflags;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	char buffer[BUFFER_SIZE];
	socklen_t snda_len, rcva_len;
	ssize_t len, snd_len;

	sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
	if (sock < 0)
		syserr("socket");
	// after socket() call; we should close(sock) on any execution path;
	// since all execution paths exit immediately, sock would be closed when program terminates

	server_address.sin_family = AF_INET; // IPv4
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
	server_address.sin_port = htons(PORT_NUM); // default port for receiving is PORT_NUM

	// bind the socket to a concrete address
	if (bind(sock, (struct sockaddr *) &server_address,
			(socklen_t) sizeof(server_address)) < 0)
		syserr("bind");

	snda_len = (socklen_t) sizeof(client_address);
	int got1prev = 0;
	for (;;) {
		do {
			if (got1prev != 1) {
			rcva_len = (socklen_t) sizeof(client_address);
			flags = 0; // we do not request anything special
			len = recvfrom(sock, buffer, sizeof(buffer), flags,
					(struct sockaddr *) &client_address, &rcva_len);
			}
			got1prev = 0;
			if (len < 0)
				syserr("error on datagram from client socket");
			else {
				uint16_t type = ntohs(((int)buffer[0] << 8) + (int)buffer[1]);
        		uint16_t length = ntohs(((int)buffer[2] << 8) + (int)buffer[3]);
				printf("read from socket: %ld bytes: %d %d\n", len, type, length);
				sflags = 0;
				if (type == 1) {
					printf("send three IAM messages\n");
					type = htons((uint16_t)2);
					length = htons((uint16_t)15);
					char iamMsg[50];
					iamMsg[0] = type >> 8;
					iamMsg[1] = type - (iamMsg[0] << 8);
					iamMsg[2] = length >> 8;
					iamMsg[3] = length - (iamMsg[2] << 8);
					char* ip = "Radio Nadzieja";
					for (int i = 0; i < strlen(ip); i++) {
						iamMsg[i+4] = ip[i];
					}
					iamMsg[4+strlen(ip)] = '\0';
					size_t iamLen = 5 + strlen(ip);
					snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
							(struct sockaddr *) &client_address, snda_len);
					if (snd_len != iamLen)
						syserr("error on sending datagram to client socket");
					
					ip = "Radio Warszawa";
					for (int i = 0; i < strlen(ip); i++) {
						iamMsg[i+4] = ip[i];
					}
					snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
							(struct sockaddr *) &client_address, snda_len);
					if (snd_len != iamLen)
						syserr("error on sending datagram to client socket");
					
					ip = "Radioaktywnych";
					for (int i = 0; i < strlen(ip); i++) {
						iamMsg[i+4] = ip[i];
					}
					snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
							(struct sockaddr *) &client_address, snda_len);
					if (snd_len != iamLen)
						syserr("error on sending datagram to client socket");
					
				}
				else if (type == 3) {
					for (;;) {
						printf("send two AUDIO msgs and one METADATA msg\n");

						type = htons((uint16_t)4); //AUDIO
						length = htons((uint16_t)29);
						char iamMsg[50];
						iamMsg[0] = type >> 8;
						iamMsg[1] = type - (iamMsg[0] << 8);
						iamMsg[2] = length >> 8;
						iamMsg[3] = length - (iamMsg[2] << 8);
						char* data = "nadaje jakis program radiowy";
						for (int i = 0; i < strlen(data); i++) {
							iamMsg[i+4] = data[i];
						}
						iamMsg[4+strlen(data)] = '\0';
						size_t iamLen = 5 + strlen(data);
						snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
								(struct sockaddr *) &client_address, snda_len);
						if (snd_len != iamLen)
							syserr("error on sending datagram to client socket");


						type = htons((uint16_t)4); //AUDIO
						length = htons((uint16_t)34);
						iamMsg[0] = type >> 8;
						iamMsg[1] = type - (iamMsg[0] << 8);
						iamMsg[2] = length >> 8;
						iamMsg[3] = length - (iamMsg[2] << 8);
						data = "nadaje jakis inny program radiowy";
						for (int i = 0; i < strlen(data); i++) {
							iamMsg[i+4] = data[i];
						}
						iamMsg[4+strlen(data)] = '\0';
						iamLen = 5 + strlen(data);
						snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
								(struct sockaddr *) &client_address, snda_len);
						if (snd_len != iamLen)
							syserr("error on sending datagram to client socket");


						type = htons((uint16_t)5); //METADATA
						length = htons((uint16_t)16);
						iamMsg[0] = type >> 8;
						iamMsg[1] = type - (iamMsg[0] << 8);
						iamMsg[2] = length >> 8;
						iamMsg[3] = length - (iamMsg[2] << 8);
						data = "jakies metadane";
						for (int i = 0; i < strlen(data); i++) {
							iamMsg[i+4] = data[i];
						}
						iamMsg[4+strlen(data)] = '\0';
						iamLen = 5 + strlen(data);
						snd_len = sendto(sock, iamMsg, (size_t) iamLen, sflags,
								(struct sockaddr *) &client_address, snda_len);
						if (snd_len != iamLen)
							syserr("error on sending datagram to client socket");
						
						printf("want to get KEEPALIVE msg\n");
						struct timeval timeout;      
    					timeout.tv_sec = 5;
						timeout.tv_usec = 0;

						if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
									sizeof(timeout)) < 0)
							syserr("setsockopt failed\n");
						rcva_len = (socklen_t) sizeof(client_address);
						flags = 0; // we do not request anything special
						len = recvfrom(sock, buffer, sizeof(buffer), flags,
								(struct sockaddr *) &client_address, &rcva_len);
						if (len < 0) {
							len = 1;
							timeout.tv_sec = 0;
							timeout.tv_usec = 0;

							if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
									sizeof(timeout)) < 0)
								syserr("setsockopt failed\n");
							break;
						}
						uint16_t type = ntohs(((int)buffer[0] << 8) + (int)buffer[1]);
						uint16_t length = ntohs(((int)buffer[2] << 8) + (int)buffer[3]);
						printf("read from socket: %ld bytes: %d %d\n", len, type, length);
						if (type != 3) {
							if (type == 1)
								got1prev = 1;
    							timeout.tv_sec = 0;
								timeout.tv_usec = 0;

								if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
										sizeof(timeout)) < 0)
									syserr("setsockopt failed\n");
							break;
						}
					}
				}
			}
		} while (len > 0);
		printf("finished exchange\n");
	}

	return 0;
}
