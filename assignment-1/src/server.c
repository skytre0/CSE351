#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include <sys/epoll.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

// this is not needed, but to erase the red line under optarg.
extern char *optarg;

typedef struct {
    // need initialize
    int cur_op;     // reading string = 0, reading header = 1 -> efficiency on if statement
    int header_read;
    unsigned long string_read;
    
    uint8_t op_val;
    uint16_t key_len;
    uint32_t data_len;
    uint32_t string_len;

    char msg_header[8];
    char *msg_string;
} ctatus;

int main (int argc, char *argv[]) {
    if (argc != 3) {
        // fprintf(stderr, "need to be like ./server -p xxxx\n");
        exit(1);
    } 
    
    // parse cmd line arguments
    int opt;
    char *port = NULL;  // need to check port num? -> between 1024 < port < 65535

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = optarg;        // due to getaddrinfo -> use it as char, not int
                break;
            default:
                // fprintf(stderr, "got unknown -?, need to be like : ./server -p xxxx \n");
                exit(1);
        }
    }
    
    // only one argument passed = no need to check null here (checked above)

    // make server...? -> need socket() -> need to listen = need bind() = use AI_PASSIVE -> cause subsequent all to bind() = nodename = null

    int sockfd, rv, yes = 1;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    // just in case : blocking default -> change to nonblocking : use fcntl() = use SOCK_NONBLOCK when using socket()
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) == -1) {
            // perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {        // not mandatory, think it is useful when debugging frequently
            // perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            // perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        // fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, 100) == -1) {
        // perror("listen");
        exit(1);
    }

    // epoll setting
    struct epoll_event ev, events[51];      // 50 clients + 1 server
    int epfd;

    epfd = epoll_create1(0);
    if (epfd == -1) {
        // perror("create1");
        exit(1);
    }
    // decide events -> edge trigger / level trigger
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        // perror("epoll_ctl: sockfd");
        exit(1);
    }

    // debug to see server started
    // fprintf(stderr, "server started\n");

    ctatus client_info[100];

    int cur_clients = 0;
    // while loop here + accept within -> need nonblocking
    while (1) {
        int events_occured = epoll_wait(epfd, events, 51, -1);
        int i;
        for (i = 0; i < events_occured; i++) {
            if (events[i].data.fd == sockfd) {      // new client
                struct sockaddr_storage new_client;
                socklen_t addr_size = sizeof new_client;
                int newfd = accept(sockfd, (struct sockaddr *)&new_client, &addr_size);

                if (newfd == -1) {
                    // perror("accept");
                    continue;
                }

                // check cur_clients < 51 (limit = 50)
                if (cur_clients == 50) {
                    // fprintf(stderr, "no more clients allowed\n");
                    close(newfd);
                    continue;
                }

                cur_clients++;
                // just in case -> beej's blog blocking
                fcntl(newfd, F_SETFL, O_NONBLOCK);
                ev.data.fd = newfd;
                // if EOLLIN | EPOLLET = edge trigger -> better operation & harder to implement -> use level trigger (only EPOLLIN) -> easy & inefficient
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev);
                client_info[newfd].cur_op = 1;      // need to read header = true
                client_info[newfd].header_read = 0;
                client_info[newfd].string_read = 0;
                // fprintf(stderr, "new client (fd = %d) arrived, current clients: %d\n", newfd, cur_clients);
            }
            else {      // existing client
                ctatus *client = &client_info[events[i].data.fd];
                // how it works : 
                // if (cur_op) -> true = need to read header -> can use fixed buffer(size 8), same memcpy (start + header_read, ..., 8 - header_read)
                // to do it, recv 8 - header_read for implement efficiency
                int bytes_read = 0;
                if (client->cur_op) {
                    bytes_read = recv(events[i].data.fd, client->msg_header + client->header_read, 8 - client->header_read, 0);

                    if (bytes_read < 0) {
                        if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                            continue;
                        cur_clients--;
                        // perror("recv1");
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                        close(events[i].data.fd);
                        continue;
                    }
                    // bytes_read == 0 = client left
                    if (!bytes_read) {
                        cur_clients--;
                        // fprintf(stderr, "client left, current clients : %d\n", cur_clients);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                        close(events[i].data.fd);
                        continue;
                    }

                    client->header_read += bytes_read;
                    if (client->header_read == 8) {
                        uint16_t tmp_kl;
                        uint32_t tmp_dl;
                        memcpy(&tmp_kl, client->msg_header + 2, 2);
                        memcpy(&tmp_dl, client->msg_header + 4, 4);
                        client->op_val = (uint8_t)client->msg_header[1];
                        client->key_len = ntohs(tmp_kl);
                        client->data_len = ntohl(tmp_dl);

                        // check input
                        // fprintf(stderr, "got all of the header, op : %u, kl : %u, dl : %d\n", client->op_val, client->key_len, client->data_len);
                        
                        if (client->op_val > 1 || client->op_val < 0) {
                            epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                            close(events[i].data.fd);
                            continue;
                        }

                        // need to check key_len + data_len <= 10MB
                        // type casting error if I use 10e7 instead of 10000000
                        if (8 + client->key_len + client->data_len > 10000000 || client->op_val > 1 || client->op_val < 0) {
                            cur_clients--;
                            // fprintf(stderr, "remove wrong client, current clients : %d\n", cur_clients);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                            close(events[i].data.fd);
                            continue;
                        }

                        // now get ready to read string
                        client->string_len = client->key_len + client->data_len;
                        client->msg_string = malloc(client->string_len);
                        client->cur_op = 0;
                    }
                }

                // else -> need to read string -> use malloc, set to string_len, use memcpy (start + string_read, ..., string_len - string_read)
                // recv string_len - string_read
                // when all correctly recv & sent -> will free malloc right away = cur_op = 1 + header_read = 0 + string_read = 0.
                else {
                    bytes_read = recv(events[i].data.fd, client->msg_string + client->string_read, client->string_len - client->string_read, 0);

                    if (bytes_read < 0) {
                        if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                            continue;
                        cur_clients--;
                        // perror("recv2");
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                        close(events[i].data.fd);
                        continue;
                    }

                    // check header & string mismatch.
                    if (!bytes_read) {
                        cur_clients--;
                        // fprintf(stderr, "client left, current clients : %d\n", cur_clients);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                        close(events[i].data.fd);
                        continue;
                    }


                    client->string_read += bytes_read;
                    if (client->string_read == client->string_len) {
                        char *key_part = client->msg_string;
                        char *data_part = client->msg_string + client->key_len;

                        int ok = 0;
                        for (int j = 0; j < client->key_len; j++) {
                            if (!isalpha(key_part[j])) {
                                // fprintf(stderr, "key can only have alphabet\n");
                                free(client->msg_string);
                                epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                                close(events[i].data.fd);
                                ok++;
                                break;
                            }
                        }
                        if (ok) continue;

                        // do cipher based on op value
                        int key_to_cipher = 0;
                        int data_to_cipher = 0;
                        while (data_to_cipher < client->data_len) {
                            if (isalpha(data_part[data_to_cipher])) {
                                data_part[data_to_cipher] = tolower(data_part[data_to_cipher]);
                                // op field = 1 = decryption
                                if (client->op_val) {
                                    data_part[data_to_cipher] = 'a' + ((data_part[data_to_cipher] - tolower(key_part[key_to_cipher]) + 26) % 26);
                                }
                                // op field = 0 = encryption
                                else {
                                    data_part[data_to_cipher] = 'a' + ((data_part[data_to_cipher] + tolower(key_part[key_to_cipher]) - (2 * 'a')) % 26);
                                }
                                key_to_cipher = (key_to_cipher + 1) % client->key_len;
                            }
                            data_to_cipher++;
                        }

                        // send the answer back to client
                        char *to_client = malloc(8 + client->key_len + client->data_len);
                        uint16_t msg_key_len = htons(client->key_len);
                        uint32_t msg_data_len = htonl(client->data_len);

                        to_client[0] = 0;
                        if (client->op_val) to_client[1] = 1;
                        else        to_client[1] = 0;
                        int offset = 2;     // due to op field
                        
                        memcpy(to_client + offset, &msg_key_len, sizeof(msg_key_len));     // key_length field
                        offset += sizeof msg_key_len;
                        memcpy(to_client + offset, &msg_data_len, sizeof(msg_data_len));   // data_length field
                        offset += sizeof msg_data_len;
                        memcpy(to_client + offset, key_part, client->key_len);        // string (key) field
                        offset += client->key_len;
                        memcpy(to_client + offset, data_part, client->data_len);        // string (data) field
                        
                        // same logic with client
                        int bytes_sent = 0;
                        int to_send = 8 + client->key_len + client->data_len;
                        while (to_send) {
                            int just_sent = send(events[i].data.fd, to_client + bytes_sent, to_send, 0);

                            if (just_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                                continue;
                            // if (just_sent == -1)    perror("send");
                            
                            bytes_sent += just_sent;
                            to_send -= just_sent;

                            // check if -1
                            // fprintf(stderr, "%d / %d \n", bytes_sent, 8 + client->key_len + client->data_len);
                        }                        

                        // fprintf(stderr, "finished sending msg\n");

                        // finish = get back to original state
                        free(to_client);
                        free(client->msg_string);
                        client->msg_string = NULL;
                        client->cur_op = 1;
                        client->header_read = 0;
                        client->string_read = 0;
                    }
                }
            }
        }
    }

    close(epfd);
    close(sockfd);


    return 0;
}