#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include "err.h"

#define SMALL_BUFF 1000
#define BIG_BUFF 100000

struct list {
    char* hostname;
    uint16_t port;
    time_t lastMsg;
    struct sockaddr_in addres;
    struct list* next;
} list;

struct message {
    uint16_t type;
    char data[BIG_BUFF];
    int len;
    struct message* next;
} message;

int string2int(char* s) {
    int answer = 0;
    int h = 0;
    while (s[h] != ' ' && s[h] != '\0') {
        answer *= 10;
        answer += (int)(s[h]) - (int)('0');
        h++;
    }
    return answer;
}

int checkParams(int argc, char* argv[], char** host, char** resource, char** broadcast, int* portRadio, int* portClient, int* m, int* timeoutRadio, int* timeoutClient) {

    int presentHost = 0, presentResource = 0, presentBroadcast = 0, presentPortRadio = 0, presentPortClient = 0, presentM = 0, presentTimeoutRadio = 0, presentTimeoutClient = 0;

    if (argc != 7 && argc != 9 && argc != 11 && argc != 13 && argc != 15 && argc != 17) {
        return -1;
    }

    for (int i = 1; i < argc; i += 2) {
        if (strncmp(argv[i], "-h", 3) == 0) {
            *host = argv[i+1];
            presentHost = 1;
        } else if (strncmp(argv[i], "-r", 3) == 0) {
            *resource = argv[i+1];
            presentResource = 1;
        } else if (strncmp(argv[i], "-p", 3) == 0) {
            *portRadio = string2int(argv[i+1]);
            presentPortRadio = 1;
        } else if (strncmp(argv[i], "-m", 3) == 0) {
            if (strncmp(argv[i+1], "yes", 4) == 0) {
                *m = 1;
            } else {
                *m = 0;
            }
            presentM = 1;
        } else if (strncmp(argv[i], "-t", 3) == 0) {
            *timeoutRadio = string2int(argv[i+1]);
            presentTimeoutRadio = 1;
            if (*timeoutRadio == 0) {
                return -1;
            }
        } else if (strncmp(argv[i], "-P", 3) == 0) {
            *portClient = string2int(argv[i+1]);
            presentPortClient = 1;
        } else if (strncmp(argv[i], "-B", 3) == 0) {
            *broadcast = argv[i+1];
            presentBroadcast = 1;
        } else if (strncmp(argv[i], "-T", 3) == 0) {
            *timeoutClient = string2int(argv[i+1]);
            presentTimeoutClient = 1;
            if (*timeoutClient == 0) {
                return -1;
            }
        }
    }

    if (presentHost == 0 || presentResource == 0 || presentPortRadio == 0) {
        return -1;
    } else if (presentPortClient == 1) {
        //ma działać w trybie pośrednika (B)
        int ile = 8; // 4 parametry już na pewno są - ale mogą być jeszcze 4 opcjonalne
        if (presentTimeoutClient)
            ile += 2;
        if (presentTimeoutRadio)
            ile += 2;
        if (presentM)
            ile += 2;
        if (presentBroadcast)
            ile += 2;
        if (argc - 1 != ile)
            return -1;
        else
            return 2; // 2 czyli działaj jako pośrednik
    } else {
        //ma działać w trybie nadajnika (A)
        if (argc == 9 && presentM == 0 && presentTimeoutRadio == 0) {
            return -1;
        } else if (argc == 11 && (presentM == 0 || presentTimeoutRadio == 0)) {
            return -1;
        } else {
            return 1; // 1 czyli działaj jako nadajnik
        }
    }
}

struct sockaddr_in createRadioSockAndConnect(char* hostname, int port, int* sock, int timeoutRadio) {
    struct hostent *host;
    struct sockaddr_in serv_addr;

    /// Find host IP
    if ((host = gethostbyname(hostname)) == NULL) {
        syserr("server not found");
    }

    // Create IPv4 TCP socket
    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        syserr("opening socket fail");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = *(long *)(host->h_addr_list[0]);
    //char *ip = inet_ntoa(serv_addr.sin_addr);
    //printf("IP Address is : %s\n", ip);

    if (connect(*sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        close(*sock);
        syserr("connecting to socket fail");
    }

    struct timeval timeout;      
    timeout.tv_sec = timeoutRadio;
    timeout.tv_usec = 0;

    if (setsockopt (*sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        syserr("setsockopt failed\n");

    return serv_addr;
}

struct sockaddr_in createClientSock(int* sock, int port, char* multicastAddr) {
    struct sockaddr_in myAddressToClient;

    *sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
	if (*sock < 0)
		syserr("socket");

	myAddressToClient.sin_family = AF_INET; // IPv4
	myAddressToClient.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
	myAddressToClient.sin_port = htons((uint16_t) port); // default port for receiving is PORT_NUM

    if(strnlen(multicastAddr, SMALL_BUFF) != 0) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(*sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            syserr("Multicasting add membership failed. Multicast address may not work properly.\n");
        }
    }

	// bind the socket to a concrete address
	if (bind(*sock, (struct sockaddr *) &myAddressToClient,
			(socklen_t) sizeof(myAddressToClient)) < 0)
		syserr("bind");
    return myAddressToClient;
}

void sendRequest(int sock, char* resource, char* hostname, int wantMetadata) {
    char request[8000] = {0};
    char* metadataPlease = "";
    if (wantMetadata) {
        metadataPlease = "\r\nIcy-MetaData:1";
    }
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s%s\r\n\r\n", resource, hostname, metadataPlease);
    //printf("%s", request);

    if (write(sock, (const void*) &request , strlen(request)) != strlen(request)) {
        close(sock);
        syserr("writing to socket fail");
    }
    //return request;
}

void sendRadioData(char* buffer, int length, int metaint, int* restCode, int* howManyRest) {
    int i = 0;
    while(i < length) {
        //printf("\nwhile1: i=%d, restCode=%d", i, *restCode);
        if (*restCode >= 0) {
            int howManyRead;
            if (*restCode == 1) {
                howManyRead = *howManyRest;
            } else {
                howManyRead = metaint;
            }
            *restCode = 0;
            //wypisuję audio na stdout
            //printf("\nAUDIO %d\n", howManyRead+i);
            for(int a = i; a < howManyRead + i && a < length; a++) {
                printf("%c", buffer[a]);
            }
            i += howManyRead;
            //printf("\ncompare1: i=%d, length=%d", i, length);
            if (i > length) {
                // to audio nie zmieściło się w całości w tej wiadomości
                *restCode = 1; // czyli zostało jakieś audio
                *howManyRest = i - length; // tyle bajtów audio zostało jeszcze do wczytania
                return;
            } else if (i == length) {
                // czyli wczytałam całe audio, następny bajt do wczytania to bajt oznaczający długość metadanych
                *restCode = -2;
                *howManyRest = 0;
                return;
            }
        }
        //printf("\nwhile2: i=%d, restCode=%d", i, *restCode);
        if (*restCode <= 0) {
            int howManyRead;
            if (*restCode == -2 || *restCode == 0) {
                int metadataLen = buffer[i];
                metadataLen *= 16;

                //printf("\nmetadataLen=%d*%c*%d*\n", metadataLen, buffer[i], (int)(buffer[i]));
                i++;
                howManyRead = metadataLen;
            } else {
                howManyRead = *howManyRest;
            }
            *restCode = 0;
            //printf("\nMETADATA %d\n", howManyRead+i);
            for (int m = i; m < howManyRead + i && m < length; m++) {
                fprintf(stderr, "%c", buffer[m]);
            }
            i += howManyRead;
            //printf("\ncompare2: i=%d, length=%d", i, length);
            if (i > length) {
                // te metadane nie zmieściły się w całości w tej wiadomości
                *restCode = -1; // czyli zostały jakieś metadane
                *howManyRest = i - length; // tyle bajtów metadanych zostało jeszcze do wczytania
                return;
            } else if (i == length) {
                // wczytałam dokładnie tyle, że następny bajt to będzie pierwszy bajt segmentu audio
                *restCode = 2;
                *howManyRest = 0; // tyle bajtów metadanych zostało jeszcze do wczytania
                return;
            }
        }
    }
}

struct message* sendRadioData2(char* buffer, int length, int metaint, int* restCode, int* howManyRest) {
    int i = 0;
    struct message* msgs = malloc(sizeof(message));
    struct message* begin = msgs;
    int haveAlreadyMsg = 0;
    while(i < length) {
        //printf("\nwhile1: i=%d, restCode=%d", i, *restCode);
        if (*restCode >= 0) {
            int howManyRead;
            if (*restCode == 1) {
                howManyRead = *howManyRest;
            } else {
                howManyRead = metaint;
            }
            *restCode = 0;
            //wypisuję audio na stdout
            //printf("\nAUDIO %d\n", howManyRead+i);
            int msgCounter = 0;
            if (haveAlreadyMsg == 1) {
                struct message* newMsg = malloc(sizeof(message));
                newMsg->type = 4; //AUDIO
                newMsg->next = NULL;
                msgs->next = newMsg;
                msgs = msgs->next;
            }
            haveAlreadyMsg = 1;
            for(int a = i; a < howManyRead + i && a < length; a++) {
                msgs->data[msgCounter] = buffer[a];
                msgCounter++;
                //printf("%c", buffer[a]);
            }
            msgs->len = msgCounter;
            i += howManyRead;
            //printf("\ncompare1: i=%d, length=%d", i, length);
            if (i > length) {
                // to audio nie zmieściło się w całości w tej wiadomości
                *restCode = 1; // czyli zostało jakieś audio
                *howManyRest = i - length; // tyle bajtów audio zostało jeszcze do wczytania
                break;
            } else if (i == length) {
                // czyli wczytałam całe audio, następny bajt do wczytania to bajt oznaczający długość metadanych
                *restCode = -2;
                *howManyRest = 0;
                break;
            }
        }
        //printf("\nwhile2: i=%d, restCode=%d", i, *restCode);
        if (*restCode <= 0) {
            int howManyRead;
            if (*restCode == -2 || *restCode == 0) {
                int metadataLen = buffer[i];
                metadataLen *= 16;

                //printf("\nmetadataLen=%d*%c*%d*\n", metadataLen, buffer[i], (int)(buffer[i]));
                i++;
                howManyRead = metadataLen;
            } else {
                howManyRead = *howManyRest;
            }
            *restCode = 0;
            //printf("\nMETADATA %d\n", howManyRead+i);
            if (haveAlreadyMsg == 1) {
                struct message* newMsg = malloc(sizeof(message));
                newMsg->type = 5; //METADATA
                newMsg->next = NULL;
                msgs->next = newMsg;
                msgs = msgs->next;
            }
            haveAlreadyMsg = 1;
            int msgCounter = 0;
            for (int m = i; m < howManyRead + i && m < length; m++) {
                msgs->data[msgCounter] = buffer[m];
                msgCounter++;
                fprintf(stderr, "%c", buffer[m]);
            }
            msgs->len = msgCounter;
            i += howManyRead;
            //printf("\ncompare2: i=%d, length=%d", i, length);
            if (i > length) {
                // te metadane nie zmieściły się w całości w tej wiadomości
                *restCode = -1; // czyli zostały jakieś metadane
                *howManyRest = i - length; // tyle bajtów metadanych zostało jeszcze do wczytania
                break;
            } else if (i == length) {
                // wczytałam dokładnie tyle, że następny bajt to będzie pierwszy bajt segmentu audio
                *restCode = 2;
                *howManyRest = 0; // tyle bajtów metadanych zostało jeszcze do wczytania
                break;
            }
        }
    }
    return begin;
}

char* readHeader(int sock, int* metaint, int* restCode, int *howManyRest) {
    char buffer[BIG_BUFF] = {0};
    char line[SMALL_BUFF] = {0};
    char* name = malloc(SMALL_BUFF*sizeof(char));

    int len = read(sock, buffer, BIG_BUFF);
    if (len < 0) {
        close(sock);
        syserr("reading from socket fail");
    }

    // zamiast ICY 200 OK może odpowiedzieć HTTP/1.0 200 OK lub HTTP/1.1 200 OK
    if (strncmp(buffer, "ICY 200 OK\r\n", 12) != 0 &&
        strncmp(buffer, "HTTP/1.0 200 OK\r\n", 17) != 0 &&
        strncmp(buffer, "HTTP/1.1 200 OK\r\n", 17) != 0) {
        syserr("incorrect header");
    }

    //printf("%s\n", buffer);

    int i = 0, k = 0;
    //read till the second in line \r\n occurs, which means end of header
    while (i < BIG_BUFF-1 && buffer[i] != '\r' && buffer[i+1] != '\n') {
        
        //read whole line with \r\n at the end
        k = 0; // k iteruje po buforze "line"
        while (i < BIG_BUFF-1 && buffer[i] != '\r' && buffer[i+1] != '\n') {
            line[k] = buffer[i];
            i++;
            k++;
        }
        i += 2;
        // now i is on the beginnning of next line (if exist)
        if (strncmp(line, "icy-name:", 9) == 0) {
            //w tej lini jest podana nazwa radio
            for (int n = 0; n < k - 9; n++) {
                name[n] = line[n+9];
            }
            name[k-9] = '\0';
        } else if (strncmp(line, "icy-metaint:", 12) == 0) {
            *metaint = 0;
            //w tej lini jest podane co ile znaków będą metadane
            for (int n = 0; n < k - 12; n++) {
                *metaint *= 10;
                *metaint += (int)line[n+12] - (int)'0';
            }
        }
    }
    i += 2;
    if (i == len) {
        return name;
    } else {
        for (int b = 0; b < len - i; b++) {
            buffer[b] = buffer[i + b];
        }
        *restCode = 0;
        sendRadioData(buffer, len - i, *metaint, restCode, howManyRest);
    }
    return name;
}

void readAudioAndMetadata(int sock, int metaint, int* restCode, int* howManyRest) {
    char buffer[BIG_BUFF] = {0};

    for (;;) {
        int len = read(sock, buffer, BIG_BUFF);
        if (len < 0) {
            close(sock);
            syserr("reading from socket fail");
        }
        sendRadioData(buffer, len, metaint, restCode, howManyRest);
    }
}

void readAudio(int sock) {
    char buffer[BIG_BUFF] = {0};

    for (;;) {
        int len = read(sock, buffer, BIG_BUFF);
        if (len < 0) {
            close(sock);
            syserr("reading from socket fail");
        }
        for (int j = 0; j < len; j++) {
            printf("%c", buffer[j]);
        }
    }
}

char* createMsg(uint16_t type, uint16_t len, char* data) {
    type = htons(type);
    char* msg = malloc((4+len)*sizeof(char));
    uint16_t length = htons(len);
    
    msg[0] = type >> 8;
    msg[1] = type - (msg[0] << 8);
    msg[2] = length >> 8;
    msg[3] = length - (msg[2] << 8);
    for (int i = 0; i < len; i++) {
        msg[i+4] = data[i];
    }
    msg[len+4] = '\0';

    return msg;
}

void sendIAMmsg(int sockClient, struct sockaddr_in clientAddr, char* radioName) {
    size_t len = strnlen(radioName, SMALL_BUFF);
    char* iamMsg = createMsg(2, len, radioName);
    len += 4;
    socklen_t addressLen = (socklen_t) sizeof(clientAddr);
    int sflags = 0;
    ssize_t snd_len = sendto(sockClient, iamMsg, len, sflags,
            (struct sockaddr *) &clientAddr, addressLen);
    if (snd_len != (ssize_t) len)
        syserr("sending message to client");
    printf("sent iam msg\n");
}

struct list* newKeepAlive(char* hostname, int port, struct list* clients, struct sockaddr_in clientAddres) {
    struct list* begin = clients, *prev = NULL;
    int found = 0;
    while (clients != NULL) {
        if (clients->hostname == hostname && clients->port == port) {
            // znalazłam tego klienta
            clients->lastMsg = time(NULL);
            found = 1;
            break;
        }
        prev = clients; // jeśli ktokolwiek jest klientem, to prev nie jest NULLem
        clients = clients->next;
    }
    if (found == 0) {
        // dodaj nowego klienta
        struct list* newClient = malloc(sizeof(struct list));
        newClient->hostname = hostname;
        newClient->port = port;
        newClient->lastMsg = time(NULL);
        newClient->addres = clientAddres;
        newClient->next = NULL;
        if (prev == NULL) {
            //czyli nie było żadnego klienta
            begin = newClient;
        } else {
            prev->next = newClient;
        }
    }
    return begin;
}

struct list* checkClientsTimeout(struct list* clients, int timeout) {
    struct list* begin = clients, *prev = NULL, *help;
    while (clients != NULL) {
        if (clients->lastMsg - time(NULL) >= timeout) {
            help = clients;
            // klient przekroczył timeout, usuwam go
            if (prev == NULL) {
                //to był pierwszy klient na liście
                begin = clients->next;
            } else {
                prev->next = clients->next;
            }
            clients = clients->next;
            free(help);
        } else {
            prev = clients;
            clients = clients->next;
        }
    }
    return begin;
}

struct list* receiveUDPmsgs(int sockClient, struct sockaddr_in clientAddress, char* radioName, struct list* clients) {
    char buffer[SMALL_BUFF];
    int flags = 0;
    socklen_t addressLen = (socklen_t) sizeof(clientAddress);

    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = 500;

    if (setsockopt (sockClient, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        syserr("setsockopt failed\n");

    for (int i = 0; i < 100; i++) {
        //odbierz 100 wiadomości od klientów
        (void) memset(buffer, 0, SMALL_BUFF);
        ssize_t rcv_len = recvfrom(sockClient, buffer, SMALL_BUFF, flags,
            (struct sockaddr *) &clientAddress, &addressLen);
        if (rcv_len == -1) {
            break;
        } else if (rcv_len < 0) {
            syserr("receive message from socket");
        }

        uint16_t type = ntohs(((int)buffer[0] << 8) + (int)buffer[1]);
        uint16_t length = ntohs(((int)buffer[2] << 8) + (int)buffer[3]);

        if (type != 1 && type != 3)
            continue;

        if (length != 0)
            syserr("read incorrect mesage from client");

        char* hostname = inet_ntoa(clientAddress.sin_addr);
        uint16_t port = ntohs(clientAddress.sin_port);

        if (type == 1) {
            //DISCOVER msg, odpowiedz IAM
            printf("discover msg\n");
            sendIAMmsg(sockClient, clientAddress, radioName);
        } else {
            //KEEPALIVE msg: sprawdź czy już masz go na liście, zaktualizuj ostani czas
            printf("keepalive msg\n");
            clients = newKeepAlive(hostname, port, clients, clientAddress);
        }
    }
    return clients;
}

void sendRadioToClients(int sockClient, struct list* clients, char* bufferRadio, int len) {
    char* msg = createMsg(4, len, bufferRadio);
    len += 4;
    int flags = 0;
    struct list* begin = clients;

    while(clients != NULL) {
        socklen_t addressLen = (socklen_t) sizeof(clients->addres);
        int snd_len = sendto(sockClient, msg, len, flags,
            (struct sockaddr *) &clients->addres, addressLen);
        if (snd_len != len)
            syserr("sending data to client");
        
        clients = clients->next;
    }
    clients = begin;
}

void sendRadioToClients2(int sockClient, struct list* clients, struct message* msgs, int len) {
    struct message* beginMsgs = msgs;

    while (msgs != NULL) {
        char* msg = createMsg(msgs->type, msgs->len, msgs->data);
        int len = 4 + msgs->len;
        int flags = 0;
        struct list* begin = clients;

        while(clients != NULL) {
            socklen_t addressLen = (socklen_t) sizeof(clients->addres);
            int snd_len = sendto(sockClient, msg, len, flags,
                (struct sockaddr *) &clients->addres, addressLen);
            if (snd_len != len)
                syserr("sending data to client");
            
            clients = clients->next;
        }
        clients = begin;
    }
    msgs = beginMsgs;
}

void workInLoopAudio(int sockRadio, int sockClient, struct sockaddr_in clientAddress, char* radioName, int timeoutClient) {
    struct list* clients = NULL;

    for (;;) {
        //odbierz porcję danych od serwera radiowego
        char bufferRadio[BIG_BUFF] = {0};
        int len = read(sockRadio, bufferRadio, BIG_BUFF);
        if (len < 0) {
            close(sockRadio);
            syserr("reading from socket fail");
        }
        //odbierz dane od klientów, max. 100
        clients = receiveUDPmsgs(sockClient, clientAddress, radioName, clients);
        //sprawdź czy któryś z klientów nie przekroczył timeoutu
        clients = checkClientsTimeout(clients, timeoutClient);
        //wyślij nowe dane z radia do klientów
        sendRadioToClients(sockClient, clients, bufferRadio, len);
    }
}

void workInLoopAudioAndMetadata(int sockRadio, int sockClient, struct sockaddr_in clientAddress, char* radioName, int timeoutClient, int metaint, int* restCode, int* howManyRest) {
    struct list* clients = NULL;

    for (;;) {
        //odbierz porcję danych od serwera radiowego
        char bufferRadio[BIG_BUFF] = {0};
        int len = read(sockRadio, bufferRadio, BIG_BUFF);
        if (len < 0) {
            close(sockRadio);
            syserr("reading from socket fail");
        }
        struct message* msgs = sendRadioData2(bufferRadio, len, metaint, restCode, howManyRest);
        //odbierz dane od klientów, max. 100
        clients = receiveUDPmsgs(sockClient, clientAddress, radioName, clients);
        //sprawdź czy któryś z klientów nie przekroczył timeoutu
        clients = checkClientsTimeout(clients, timeoutClient);
        //wyślij nowe dane z radia do klientów
        sendRadioToClients2(sockClient, clients, msgs, len);
    }
}

int main(int argc, char *argv[]) {
    char* hostname, *resource, *multicast = "";
    int portRadio, portClient, timeoutRadio = 5, timeoutClient = 5, m = 1;
    int sockRadio = 0, sockClient = 0;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    struct sockaddr_in myAddrToClient;
    
    int mode = checkParams(argc, argv, &hostname, &resource, &multicast, &portRadio, &portClient, &m, &timeoutRadio, &timeoutClient);

    if (mode == -1) {
        printf("Uzycie: %s -h [nazwa serwera udost. strumien audio] -r [nazwa zasobu na serwerze] -p [port na ktorym serwer udostepnia strumien audio] -m [yes|no, yes jesli chcemy otrzymywac metadane, no wpp, opcjonalny] -t [timeout dla serwera radiowego w sekundach, opcjonalny] -P [port nasłuchiwania UDP dla klientów, obowiązkowy jeśli ma działać jako pośrednik] -B [adres rozsyłania grupowego na którym ma nasłuchiwać, opcjonalny] -T [timeout dla klienta, w sekundach, opcjonalny]\n", argv[0]);
        return 1;
    }

    serverAddr = createRadioSockAndConnect(hostname, portRadio, &sockRadio, timeoutRadio);
    sendRequest(sockRadio, resource, hostname, m);

    int metaint = -1;
    int restCode = 0;
    int howManyRest = 0;
    char* name = readHeader(sockRadio, &metaint, &restCode, &howManyRest);

    if (mode == 2) {
        myAddrToClient = createClientSock(&sockClient, portClient, multicast);
    }

    if (m == 0) {
        if (metaint == 0) // jeśli chciałam metadane a ich nie wysyła to ok, ale jeśli nie chciałam a wysyła to błąd
            return 1;
        if (mode == 1) {
            readAudio(sockRadio);
        } else {
            workInLoopAudio(sockRadio, sockClient, clientAddr, name, timeoutClient);
        }
    } else {
        if (mode == 1) {
            readAudioAndMetadata(sockRadio, metaint, &restCode, &howManyRest);
        } else {
            workInLoopAudioAndMetadata(sockRadio, sockClient, clientAddr, name, timeoutClient, metaint, &restCode, &howManyRest);
        }
    }

    if (mode == 2) {
        close(sockClient);
    }
    close(sockRadio);

    return 0;
}