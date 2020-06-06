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
#define BIG_BUFF 1000000

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

int checkParams(int argc, char* argv[], char** host, char** resource, int* port, int* m, int* timeout) {

    int presentHost = 0, presentResource = 0, presentPort = 0, presentM = 0, presentTimeout = 0;

    if (argc != 7 && argc != 9 && argc != 11) {
        return 1;
    }

    for (int i = 1; i < argc; i += 2) {
        if (strncmp(argv[i], "-h", 3) == 0) {
            *host = argv[i+1];
            presentHost = 1;
        } else if (strncmp(argv[i], "-r", 3) == 0) {
            *resource = argv[i+1];
            presentResource = 1;
        } else if (strncmp(argv[i], "-p", 3) == 0) {
            *port = string2int(argv[i+1]);
            presentPort = 1;
        } else if (strncmp(argv[i], "-m", 3) == 0) {
            if (strncmp(argv[i+1], "yes", 4) == 0) {
                *m = 1;
            } else {
                *m = 0;
            }
            
            presentM = 1;
        } else if (strncmp(argv[i], "-t", 3) == 0) {
            *timeout = string2int(argv[i+1]);
            presentTimeout = 1;
            if (*timeout == 0) {
                return 1;
            }
        }
    }

    if (presentHost == 0 || presentResource == 0 || presentPort == 0) {
        return 1;
    } else if (argc == 9 && presentM == 0 && presentTimeout == 0) {
        return 1;
    } else if (argc == 11 && (presentM == 0 || presentTimeout == 0)) {
        return 1;
    } else {
        return 0;
    }
}

struct sockaddr_in createSockAndConnect(char* hostname, int port, int* sock) {
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
    return serv_addr;
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

int main(int argc, char *argv[]) {
    char* hostname, *resource;
    int port, timeout = 5, m = 1;
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if (checkParams(argc, argv, &hostname, &resource, &port, &m, &timeout) == 1) {
        printf("Uzycie: %s -h [nazwa serwera udost. strumien audio] -r [nazwa zasobu na serwerze] -p [port na ktorym serwer udostepnia strumien audio] -m [yes|no, yes jesli chcemy otrzymywac metadane, no wpp, opcjonalny] -t [timeout w sekundach, opcjonalny]\n", argv[0]);
        return 1;
    }

    serv_addr = createSockAndConnect(hostname, port, &sock);

    sendRequest(sock, resource, hostname, m);

    int metaint = -1;
    int restCode = 0;
    int howManyRest = 0;
    char* name = readHeader(sock, &metaint, &restCode, &howManyRest);

    if (m == 0) {
        if (metaint == 0) // jeśli chciałam metadane a ich nie wysyła to ok, ale jeśli nie chciałam a wysyła to błąd
            return 1;
        readAudio(sock);
    } else {
        readAudioAndMetadata(sock, metaint, &restCode, &howManyRest);
    }

    return 0;
}