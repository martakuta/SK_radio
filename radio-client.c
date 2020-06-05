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

#define BUFFER_SIZE   1000
#define QUEUE_LENGTH     5
#define PORT_NUM     10001

typedef struct list {
    uint16_t type;
    uint16_t length;
    char data[BUFFER_SIZE/2];
    struct list* next;
    char* hostname;
    uint16_t port;
} list;

static char* menuBegin = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n   MENU\n\n 1. Szukaj pośrednika\n";
static char* menuEnd = "\n Podaj numer opcji, którą chcesz wybrać: ";
static char* notCorrect = "\n Podaj poprawną wartość: ";

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

int checkParams(int argc, char *argv[], char** hostRP, int* portRP, int* portTelnet, int* timeout) {

    int presentHostRP = 0, presentPortRP = 0, presentPortTelnet = 0, presentTimeout = 0;

    if (argc != 7 && argc != 9) {
        return 1;
    }

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-H") == 0) {
            *hostRP = argv[i+1];
            presentHostRP = 1;
        } else if (strcmp(argv[i], "-P") == 0) {
            *portRP = string2int(argv[i+1]);
            presentPortRP = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            *portTelnet = string2int(argv[i+1]);
            presentPortTelnet = 1;
        } else if (strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "-t") == 0) {
            *timeout = string2int(argv[i+1]);
            presentTimeout = 1;
            if (*timeout == 0) {
                return 1;
            }
        }
    }

    if (argc == 7 && (presentHostRP == 0 || presentPortRP == 0 || presentPortTelnet == 0)) {
        return 1;
    } else if (argc == 9 && (presentHostRP == 0 || presentPortRP == 0 || presentPortTelnet == 0 || presentTimeout == 0)) {
        return 1;
    } else {
        return 0;
    }
}

struct sockaddr_in createRPSock(int* sockRP, char* hostRP, int portRP) {
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    struct sockaddr_in myAddressRP;
    
    // 'converting' host/port in string to struct addrinfo
    (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;
    if (getaddrinfo(hostRP, NULL, &addr_hints, &addr_result) != 0) {
        syserr("getaddrinfo");
    }

    myAddressRP.sin_family = AF_INET; // IPv4
    myAddressRP.sin_addr.s_addr =
        ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
    myAddressRP.sin_port = htons((uint16_t) portRP); // port from the command line

    freeaddrinfo(addr_result);

    *sockRP = socket(PF_INET, SOCK_DGRAM, 0);
    if (*sockRP < 0)
        syserr("socket");

    return myAddressRP;
}

struct sockaddr_in createTelnetSock(int* sockTelnet, int portTelnet) {
    struct sockaddr_in myAddressTelnet;

    *sockTelnet = socket(PF_INET, SOCK_STREAM, 0);
    if (*sockTelnet < 0)
        syserr("socket");

    myAddressTelnet.sin_family = AF_INET; // IPv4
    myAddressTelnet.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    myAddressTelnet.sin_port = htons(portTelnet); // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(*sockTelnet, (struct sockaddr *) &myAddressTelnet, sizeof(myAddressTelnet)) < 0)
        syserr("bind");

    // switch to listening (passive open)
    if (listen(*sockTelnet, QUEUE_LENGTH) < 0)
        syserr("listen");

    return myAddressTelnet;
}

char* createMessage(uint16_t type, uint16_t length) {
    type = htons(type);
    length = htons(length);
    char* msg = malloc(5*sizeof(char));
    
    msg[0] = type >> 8;
    msg[1] = type - (msg[0] << 8);
    msg[2] = length >> 8;
    msg[3] = length - (msg[2] << 8);
    msg[4] = '\0';

    return msg;
}

struct sockaddr_in connectAndMenuStart(struct sockaddr_in telnetClientAddress, int sockTelnet, int* msgTelnetSock) {
    char buffer[BUFFER_SIZE];

    // get client connection from the socket
    socklen_t addressLen = sizeof(telnetClientAddress);
    *msgTelnetSock = accept(sockTelnet, (struct sockaddr *) &telnetClientAddress, &addressLen);
    if (*msgTelnetSock < 0)
        syserr("accept");

    // send start menu 
    (void) memset(buffer, 0, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "%s 2. Koniec\n%s", menuBegin, menuEnd);
    size_t len = strnlen(buffer, BUFFER_SIZE);
    size_t snd_len = write(*msgTelnetSock, buffer, len);
    if (snd_len != len)
        syserr("writing to client socket");

    return telnetClientAddress;
}

int sendMsg(int sockRP, char* msg, struct sockaddr_in myAddressRP) {
    size_t len = 5;
    socklen_t addressLen = (socklen_t) sizeof(myAddressRP);
    int sflags = 0;
    ssize_t snd_len = sendto(sockRP, msg, len, sflags,
            (struct sockaddr *) &myAddressRP, addressLen);
    if (snd_len != (ssize_t) len)
        return 1;
    else
        return 0;
}

struct list* receiveIAMmsgs(struct sockaddr_in RPServerAddress, int sockRP) {
    
    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = 500;

    if (setsockopt (sockRP, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        syserr("setsockopt failed\n");

    struct list* begin = NULL, *end = NULL;
    char buffer[BUFFER_SIZE/2];
    size_t len = (size_t) BUFFER_SIZE/2;
    socklen_t addressLen = (socklen_t) sizeof(RPServerAddress);
    int flags = 0;

    while (1) {
        (void) memset(buffer, 0, len);
        ssize_t rcv_len = recvfrom(sockRP, buffer, len, flags,
            (struct sockaddr *) &RPServerAddress, &addressLen);
        if (rcv_len == -1) {
            break;
        } else if (rcv_len < 0) {
            syserr("receive message from socket");
        }

        uint16_t type = ntohs(((int)buffer[0] << 8) + (int)buffer[1]);
        if (type != 2) // interesują mnie teraz wyłącznie wiadomości typu IAM
            continue;

        struct list* list = malloc(sizeof(struct list));
        list->next = NULL;
        list->hostname = inet_ntoa(RPServerAddress.sin_addr);
        list->port = ntohs(RPServerAddress.sin_port);
        list->type = type;
        list->length = ntohs(((int)buffer[2] << 8) + (int)buffer[3]);
        for (size_t i = 0; i <= list->length; i++) {
            list->data[i] = buffer[i+4];
        }

        if (begin == NULL) {
            begin = list;
        }
        if (end == NULL) {
            end = list;
        } else {
            end->next = list;
            end = list;
        }
    }
    
    return begin;
}

char* receiveAUDIOandMETADATAmsgs(time_t* lastActivityTime, struct sockaddr_in RPServerAddress, int radioSock, int msgTelnetSock) {
    
    struct timeval timeout;      
    timeout.tv_sec = 3;
    timeout.tv_usec = 500;

    if (setsockopt (radioSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        syserr("setsockopt failed\n");

    char buffer[BUFFER_SIZE];
    size_t len = (size_t) BUFFER_SIZE;
    socklen_t addressLen = (socklen_t) sizeof(RPServerAddress);
    int flags = 0;

    char* metadata = malloc(BUFFER_SIZE*sizeof(char));

    while (1) {
        (void) memset(buffer, 0, len);
        ssize_t rcv_len = recvfrom(radioSock, buffer, len, flags,
            (struct sockaddr *) &RPServerAddress, &addressLen);
        if (rcv_len == -1) {
            return metadata;
        } else if (rcv_len < 0) {
            syserr("receive message from socket");
        }
        *lastActivityTime = time(NULL);

        uint16_t type = ntohs(((int)buffer[0] << 8) + (int)buffer[1]);
        uint16_t length = ntohs(((int)buffer[2] << 8) + (int)buffer[3]);
        for (size_t i = 0; i <= length; i++) {
            buffer[i] = buffer[i+4];
        }
        if (type != 4 && type != 5)
            continue;

        if (type == 4) {
            // wypisz AUDIO na standardowe wejście
            printf("%s", buffer);
        }
        else if (type == 5) {
            // zapisz najnowsze metadane
            (void) memset(metadata, 0, BUFFER_SIZE);
            snprintf(metadata, BUFFER_SIZE, "%s", buffer);
        }
    }
    
    return metadata;
}

void createAndSendMenu(struct list* msgs, int selectedItem, int* howMany, int msgTelnetSock, char* metadata) {
    char* buffer = malloc(BUFFER_SIZE*sizeof(char));
    (void) memset(buffer, 0, BUFFER_SIZE);
    strncat(buffer, menuBegin, BUFFER_SIZE);
    int num = (int)('2');
    char* endline = "\n";
    while (msgs != NULL) {
        if (msgs->type == 2) { //it is IAM statement
            char number[5];
            snprintf(number, 5, " %c. ", (char)num);
            strncat(buffer, number, BUFFER_SIZE);
            strncat(buffer, msgs->data, BUFFER_SIZE);
            if (selectedItem == num - (int)('0')) {
                strncat(buffer, " *", BUFFER_SIZE);
            }
            strncat(buffer, endline, BUFFER_SIZE);
            num++;
        }
        msgs = msgs->next;
    }
    char number[5];
    snprintf(number, 5, " %c. ", (char)num);
    strncat(buffer, number, BUFFER_SIZE);
    strncat(buffer, "Koniec\n", BUFFER_SIZE);
    if (strnlen(metadata, BUFFER_SIZE) > 0) {
        strncat(buffer, " ", BUFFER_SIZE);
        strncat(buffer, metadata, BUFFER_SIZE);
        strncat(buffer, endline, BUFFER_SIZE);
    }
    strncat(buffer, menuEnd, BUFFER_SIZE);

    // wyślij menu z wszystkimi dostępnymi radio
    size_t len = strnlen(buffer, BUFFER_SIZE);
    ssize_t snd_len = write(msgTelnetSock, buffer, len);
    if (snd_len != len)
        syserr("writing to client socket");

    *howMany = num - (int)('0') - 2;
}

struct list* lookForRadioProxy(int* howMany, int sockRP, int msgTelnetSock, char* discoverMsg, struct sockaddr_in myAddressRP, struct sockaddr_in RPServerAddress) {
    
    // wyślij DISCOVER do wszystkich i zbierz od nich odpowiedzi
    if (sendMsg(sockRP, discoverMsg, myAddressRP) == 1)
        syserr("sending DISCOVER");

    struct list* msgs = receiveIAMmsgs(RPServerAddress, sockRP);
    struct list* begin = msgs;

    createAndSendMenu(msgs, 0, howMany, msgTelnetSock, "");
    
    return begin;
}

int getSelectedOption(int msgTelnetSock, int check) {

    if (check == 1) {
        struct timeval timeout;      
        timeout.tv_sec = 0;
        timeout.tv_usec = 500;

        if (setsockopt (msgTelnetSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");
    } else {
        // if the timeout is set to zero (the default) then the operation will never timeout
        struct timeval timeout;      
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        if (setsockopt (msgTelnetSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");
    }
    
    char buffer[BUFFER_SIZE];
    (void) memset(buffer, 0, BUFFER_SIZE);
    int len = read(msgTelnetSock, buffer, BUFFER_SIZE);
    if (len < 3) {
        return 0;
    }
    int num = 0;
    for (size_t i = 0; i < len-2; i++) {
        if ((int)buffer[i] < (int)'0' || (int)buffer[i] > (int)'9')
            return -1;
        num *= 10;
        num += (int)buffer[i] - (int)'0';
    }
    if ((int)buffer[len-2] != 13 || (int)buffer[len-1] != 10)
        return -1;
    return num;
}

void reactOnSelectedOption(int option, int howManyFoundRadios, struct list* foundRadios, int sockRP, int msgTelnetSock, struct sockaddr_in myAddressRP, struct sockaddr_in RPServerAddress, int timeout) {
    char* metadata = "";
    
    if (option == 1) {
        // szukaj dostępnych radio
        int howMany = 0;
        char* discoverMsg = createMessage(1, 0);
        foundRadios = lookForRadioProxy(&howMany, sockRP, msgTelnetSock, discoverMsg, myAddressRP, RPServerAddress);
        option = getSelectedOption(msgTelnetSock, 0);
        reactOnSelectedOption(option, howMany, foundRadios, sockRP, msgTelnetSock, myAddressRP, RPServerAddress, timeout);
        return;
    }
    else if (option == 2 + howManyFoundRadios) {
        // zakończ połączenie z telnetem
        if (close(msgTelnetSock) < 0)
            syserr("close");
        return;
    } 
    else if (option > 2 + howManyFoundRadios || option < 0) {
        // poproś o wybranie prawidłowej opcji
        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "%s", notCorrect);
        size_t len = strnlen(buffer, BUFFER_SIZE);
        size_t snd_len = write(msgTelnetSock, buffer, len);
        if (snd_len != len)
            syserr("writing to client socket");
        option = getSelectedOption(msgTelnetSock, 0);
        reactOnSelectedOption(option, howManyFoundRadios, foundRadios, sockRP, msgTelnetSock, myAddressRP, RPServerAddress, timeout);
        return;
    } 
    else {
        // połącz się z wybranym radio
        int i = option - 2;
        struct list* radio = foundRadios;
        while (i > 0) {
            radio = radio->next;
            i--;
        }
        int radioSock;
        struct sockaddr_in myAddressRP = createRPSock(&radioSock, radio->hostname, radio->port);
        // wyślij discover
        char* discoverMsg = createMessage(1,0);
        sendMsg(radioSock, discoverMsg, myAddressRP);
        int howMany = 0;
        createAndSendMenu(foundRadios, option, &howMany, msgTelnetSock, "");
        char* keepAliveMsg = createMessage(3,0);
        time_t lastActivityTime = time(NULL), currentTime = time(NULL);
            
        for (;;) {
            // wyślij keepAlive (jeśli minęło 3,5s)
            sendMsg(radioSock, keepAliveMsg, myAddressRP);
            // pobierz porcję danych audio / metadanych i prześlij ją użytkownikowi
            char* newMetadata = receiveAUDIOandMETADATAmsgs(&lastActivityTime, RPServerAddress, radioSock, msgTelnetSock);
            if(strlen(newMetadata) > 0)
                metadata = newMetadata;
            currentTime = time(NULL);
            if (currentTime - lastActivityTime > timeout) {
                option = 1;
                break;
            }
            //zaktualizuj menu, dodaj do niego najnowsze metadane
            howMany = 0;
            createAndSendMenu(foundRadios, option, &howMany, msgTelnetSock, metadata);
            // sprawdź czy nie ma nowej wiadomości od telnetu, jeśli tak wyskakuj z pętli
            int newOption = getSelectedOption(msgTelnetSock, 1);
            if (newOption != 0) {
                option = newOption;
                break;
            }
        }
        reactOnSelectedOption(option, howManyFoundRadios, foundRadios, sockRP, msgTelnetSock, myAddressRP, RPServerAddress, timeout);
        return;
    }
}

int main(int argc, char *argv[]) {
    char* hostRP;
    int portRP, portTelnet, timeout = 5;
    
    if (checkParams(argc, argv, &hostRP, &portRP, &portTelnet, &timeout) == 1) {
        printf("Uzycie: %s -H [adres na ktorym nasluchuje radio-proxy] -P [port UDP na ktorym nasluchuje radio-proxy] -p [port TCP na ktory mozna podlaczyc sie przez telnet -T [timeout w sekundach, opcjonalny]\n", argv[0]);
        return 1;
    }

    int sockRP, sockTelnet, msgTelnetSock;
    
    struct sockaddr_in RPServerAddress;
    struct sockaddr_in telnetClientAddress;

    struct sockaddr_in myAddressRP = createRPSock(&sockRP, hostRP, portRP);
    createTelnetSock(&sockTelnet, portTelnet);
    
    for (;;) {
        telnetClientAddress = connectAndMenuStart(telnetClientAddress, sockTelnet, &msgTelnetSock);
        
        int option = getSelectedOption(msgTelnetSock, 0);
        reactOnSelectedOption(option, 0, NULL, sockRP, msgTelnetSock, myAddressRP, RPServerAddress, timeout);
    }


    /*if ( system("./menu-skrypt.sh") != 0) {
        return 1;
    }*/

    if (close(sockRP) == -1) {
        syserr("close RP sock");
    }
    if (close(sockTelnet) == -1) {
        syserr("close telnet sock");
    }

    return 0;
}