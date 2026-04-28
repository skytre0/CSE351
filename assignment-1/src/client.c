#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

// this is not needed, but to erase the red line under optarg.
extern char *optarg;


int main (int argc, char *argv[]) {
    
    // check argc == 9 -> strict input = doesn't allow -p8888 form (need space between, which can pass in switch)
    if (argc != 9) {
        // fprintf(stderr, "need more arguments to be like : ./client -h xxx.xx.x.x -p xxxx -o x -k xxxxx \n");
        exit(1);
    }

    // parse cmd line arguments
    int opt;
    uint16_t op_val = -1;
    char *ip_start_addr = NULL, *cipher_key = NULL, *port = NULL;           // ip_start_addr to remember shape

    while ((opt = getopt(argc, argv, "h:p:o:k:")) != -1) {
        switch (opt) {
            case 'h':
                ip_start_addr = optarg;
                break;
            case 'p':
                port = optarg;        // due to getaddrinfo -> use it as char, not int
                break;
            case 'o':
                op_val = atoi(optarg);        // as a plus, if need to use strtol, int = 4 bytes, long = 8 bytes.
                break;
            case 'k':
                cipher_key = optarg;
                break;
            default:
                // fprintf(stderr, "got unknown -?, need to be like : ./client -h xxx.xx.x.x -p xxxx -o x -k xxxxx \n");
                exit(1);
        }
    }
    
    // it can happen when ./client -h 172.18.0.2 -p 8888 -o 0 -o unist -> meaning all arguments need to change
    if (port == NULL || op_val == -1 || ip_start_addr == NULL || cipher_key == NULL) {
        // fprintf(stderr, "seems like more than one same arguments...? need to be : ./client -h xxx.xx.x.x -p xxxx -o x -k xxxxx \n");
        exit(1);
    }

    // debugging -> mid check for parsing
    // fprintf(stderr, "mid check : ip : %s, port : %s, op : %d, cipher key : %s\n", ip_start_addr, port, op_val, cipher_key);
    // fprintf(stderr, "mid check : cipher key len : %ld\n", strlen(cipher_key));


    if (!strlen(cipher_key) || strlen(cipher_key) > ((2 << 16) - 1)) {
        // fprintf(stderr, "key's length should be between 1 ~ 2^16 - 1\n");
        exit(1);
    }


    // need socket to link with server.
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets -> written in [Appendix B]
    // no AI_PASSIVE here -> used with bind() = for server.
    
    // check socket link
    if (getaddrinfo(ip_start_addr, port, &hints, &servinfo)) {
        // fprintf(stderr, "socket setting error\n");
        exit(1);
    }


    // debugging check (getaddrinfo) -> host order, network order, etc...
    // fprintf(stderr, "1. host order -> original port value: %s\n", port);

    // uint16_t port_gai;
    // struct sockaddr_in *addr = (struct sockaddr_in *)servinfo->ai_addr;     // 3.3 struct s -> ai_addr = point to sockaddr == sockaddr_in
    // port_gai = addr->sin_port;      // sin_port == network order!

    // fprintf(stderr, "2. auto network order -> from %s, to %u\n", port, port_gai);

    // uint16_t port_htons = htons(atoi(port));

    // fprintf(stderr, "3. correct network order -> from %d, used htons : %u\n", atoi(port), port_htons);
    // port_gai == port_htons ? fprintf(stderr, "-> same network order\n") : fprintf(stderr, "-> need to check\n");
    // debugging end


    // code to get socket (socket's fd = sockfd) & connect it -> might be able to use since not www, and ip address, but to make sure.
    // int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    // connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)        // client = connect, server = bind


    // servinfo : point to a linked list of struct addrinfo = need to search.
    // loop through all the results and connect to the first we can
    struct addrinfo *p;
    int sockfd = -1;
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            // perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            // perror("connect");
            close(sockfd);
            continue;
        }

        break; // meaning found socket & connected safely
    }

    if (p == NULL) {
        // looped off the end of the list with no connection
        // fprintf(stderr, "failed to connect\n");
        exit(1);
    }

    // free after done using linked list.
    freeaddrinfo(servinfo);


    // from here, problem 1 - (b), termiate when EOF is met -> since " < test.txt" becomes stdin
    // string = key + data -> need to consider actual key's length (key could be around 10MB at extreme)
    size_t data_limit = 10000000 - 8 - strlen(cipher_key);     // for op (2B), key_length (2B), data_length (4B)
    char *read_data = malloc(data_limit);

    // debugging to see socket & connect success
    // fprintf(stderr, "now entering loop\n");

    size_t bytes_read;
    while ((bytes_read = fread(read_data, sizeof(char), data_limit, stdin)) > 0) {
        // read data (max read size = buffer size (thinking about 10MB if possible))
        // need to check / send multiple times -> since return val = actual send, not what I have wrote down.

        // just in case, think fread within while more accurate?
        // size_t bytes_read = fread(read_data, sizeof(char), data_limit, stdin);
        // if (!bytes_read)    continue;

        if (bytes_read) {
            int to_send = 8 + strlen(cipher_key) + bytes_read;
            char *to_server = malloc(to_send);

            // op field need to be checked for discussion reply
            uint16_t msg_key_len = htons(strlen(cipher_key));
            uint32_t msg_data_len = htonl(bytes_read);           // key len defined above = only actual data -> not unsigned long -> 8 bytes = need 4 bytes

            // acts as offset
            int cpy_starting_point = 0;

            // according to discussion, op = 0 / 1 = 1 byte -> 16 bit field -> first 8 bit = padding (in case more than 8 bits), next 8 bit = op value
            // memcpy(to_server + cpy_starting_point, &op_val, sizeof(op_val));       // op field
            to_server[0] = 0;
            to_server[1] = (uint8_t) op_val;

            // check input
            // fprintf(stderr, "op : %u, kl : %u, dl : %ld\n", op_val, msg_key_len, bytes_read);

            // if (op_val) to_server[1] = 1;
            // else        to_server[1] = 0;
            cpy_starting_point += sizeof op_val;
            memcpy(to_server + cpy_starting_point, &msg_key_len, sizeof(msg_key_len));     // key_length field
            cpy_starting_point += sizeof msg_key_len;
            memcpy(to_server + cpy_starting_point, &msg_data_len, sizeof(msg_data_len));   // data_length field
            cpy_starting_point += sizeof msg_data_len;
            memcpy(to_server + cpy_starting_point, cipher_key, strlen(cipher_key));        // string (key) field
            cpy_starting_point += strlen(cipher_key);
            memcpy(to_server + cpy_starting_point, read_data, bytes_read);        // string (data) field

            // not all of them could be sent = need to send the remainder = use loop
            int bytes_sent = 0;
            while (to_send) {
                int just_sent = send(sockfd, to_server + bytes_sent, to_send, 0);

                if (just_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                    continue;
                if (just_sent == -1) {
                    // perror("send");
                    break;
                }

                bytes_sent += just_sent;
                to_send -= just_sent;
                // fprintf(stderr, "\nread %zu bytes, sent %d out of %zu\n", bytes_read, bytes_sent, bytes_read + strlen(cipher_key) + 8);
            }

            // sent all = need to receive answer = same message length = use to_server again.
            // receive only when message has been sent
            int to_get = 8 + strlen(cipher_key) + bytes_read;

            // same reason as sending -> use loop
            int bytes_got = 0;
            while (to_get) {
                int just_got = recv(sockfd, to_server + bytes_got, to_get, 0);
                if (just_got == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                    continue;
                if (just_got <= 0) {
                    // fprintf(stderr, "probably wrong format\n");
                    break;
                }
                // if (just_got == -1)    perror("recv");
                // fprintf(stderr, "\nrecv %d out of %zu\n", bytes_got, bytes_read);
                // if client -> invalid protocol -> might loop here = need to check if server closed the socket
                bytes_got += just_got;
                to_get -= just_got;
            }

            // parse data -> no need since already know data size + position = immediate reply possible
            if (bytes_got == 8 + strlen(cipher_key) + bytes_read) {
                fwrite(to_server + 8 + strlen(cipher_key), sizeof(char), bytes_read, stdout);
                fflush(stdout);
            }
            free(to_server);
        }
    }

    free(read_data);
    close(sockfd);

    return 0;
}