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

#include <sys/epoll.h>
#include <time.h>

extern char *optarg;

struct cacher;

typedef struct cacher {        // will create cacher head; (SLL)
    char *url;
    char *response;
    int datasize;
    time_t expiration;
    struct cacher *next_cache;     // if null = end of cache
    struct cacher *before_cache;     // if null = end of cache
} cacher;

typedef struct {
    int fd;
    int opfd;
    char *buf;     // enough space
    int bufsize;
    int status;     // 0 = read all of client's request, 1 = parse all argument, 2 = validate condition & write to server (can get serv's fd here), 3 = get answer from server, 4 = write to client
    int msg_read;
    char *method, *request, *protocol;
    char *hostname;
} data_center;


int main (int argc, char *argv[]) {

    if (argc != 2) {
        // fprintf(stderr, "need to be like ./server -p xxxx\n");
        exit(1);
    } 
    
    // parse cmd line arguments
    char *port = argv[1];



    // proxy start -> need to "listen" = server = need socket + bind + listen -> wait for client to arrive.
    // client arrive = read data + check http request format satisfied or not (invalid format = print according error code)
    // kind of error -> has detailed explanation in step 1-1.

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
    // no need for perror now.
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) == -1) continue;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1;
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        // fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, SOMAXCONN) == -1) {      // maximum waiting queue allowed in listen
        // perror("listen");
        exit(1);
    }

    // debug line
    // fprintf(stderr, "proxy server ready to run\n");
    // fprintf(stderr, "now setting epoll for multiple clients\n");

    // epoll setting
    struct epoll_event ev, events[51];      // one for proxy server (one for listening socket, rest for client)
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

    // debug line
    // fprintf(stderr, "now entering loop on port %s\n", port);

    data_center proxys[1024];
    cacher *head = (cacher *)calloc(1, sizeof (cacher));
    head->next_cache = NULL;

    int cur_clients = 0;    // number of clients
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

                // if need to limit number of clients -> place it here.


                // cur_clients++;
                fcntl(newfd, F_SETFL, O_NONBLOCK);

                // new setting on ev -> give that setting to newfd
                ev.data.fd = newfd;
		        // level trigger, not edge trigger (EOLLIN | EPOLLET)
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev);
                proxys[newfd].fd = newfd;
                proxys[newfd].status = 0;
                proxys[newfd].msg_read = 0;
                proxys[newfd].buf = (char *)calloc(4096, sizeof (char));
                proxys[newfd].bufsize = 4096;
                // proxys[newfd].buf = malloc(4096);
                // memset(proxys[newfd].buf, 0, sizeof (proxys[newfd].buf));

            }
            else {      // existing client
                int curfd = events[i].data.fd;
                switch (proxys[curfd].status)
                {
                // need to check inital line : GET
                case 0: ;       

                    // just in case
                    if (proxys[curfd].msg_read + 1 == proxys[curfd].bufsize) {
                        char *tmp_alloc = realloc(proxys[curfd].buf, proxys[curfd].bufsize * 2);
                        if (tmp_alloc == NULL) {
                            free(proxys[curfd].buf);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                            close(curfd);   
                            continue;
                        }
                        proxys[curfd].buf = tmp_alloc;
                        memset(proxys[curfd].buf + proxys[curfd].msg_read, 0, proxys[curfd].msg_read + 1);
                        proxys[curfd].bufsize *= 2;
                    }

                    // more safe?
                    int read_bytes = read(curfd, proxys[curfd].buf + proxys[curfd].msg_read, proxys[curfd].bufsize - proxys[curfd].msg_read - 1);
                    if (read_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                        continue;

                    if (read_bytes <= 0) {
                        free(proxys[curfd].buf);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                        close(curfd);   
                        continue;
                    }

                    // fprintf(stderr, "read : %d, size : %ld, tmp buf : %s\n", read_bytes, strlen(tmp_buf), tmp_buf);
                    // fprintf(stderr, "read : %d, size : %ld, proxys[curfd].buf : right next line & so on\n%s", read_bytes, strlen(proxys[curfd].buf), proxys[curfd].buf);

                    // memcpy(proxys[curfd].buf + proxys[curfd].msg_read, tmp_buf, read_bytes);
                    proxys[curfd].msg_read += read_bytes;
                    proxys[curfd].buf[proxys[curfd].msg_read] = '\0';

                    // need to read more
                    if (strstr(proxys[curfd].buf, "\n\n") == NULL && strstr(proxys[curfd].buf, "\r\n\r\n") == NULL) break;

                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                    proxys[curfd].status++;

                
                    // no break needed -> case 1 = search for "Host:" in the header.
                    // break;

                // need to inital line + header
                case 1: ;
                    // inital line + header(s) all in proxys[curfd].buf = can move to next
                    char *for_tok = (char *)calloc(proxys[curfd].bufsize, sizeof (char)), *all_ptr;
                    memcpy(for_tok, proxys[curfd].buf, strlen(proxys[curfd].buf));

                    char *init_line = strtok_r(for_tok, "\r\n", &all_ptr);

                    // need same logic
                    char *method, *request, *protocol;
                    char *cut_char = " \t\r\n", *init_ptr;

                    int NULL_found = 0;

                    // check each part of input
                    method = strtok_r(init_line, cut_char, &init_ptr);    // GET
                    if (method == NULL) NULL_found += 1;

                    if (!NULL_found)
                        request = strtok_r(NULL, cut_char, &init_ptr);           // link
                    if (request == NULL) NULL_found += 2;

                    if (!NULL_found)
                        protocol = strtok_r(NULL, cut_char, &init_ptr);          // HTTP/1.0
                    if (protocol == NULL) NULL_found += 4;

                    // possiblity of more than 3 arguments
                    if (!NULL_found && (strtok_r(NULL, cut_char, &init_ptr) == NULL)) {
                        proxys[curfd].method = malloc(strlen(method) + 1);
                        strcpy(proxys[curfd].method, method);
                        // remove http:// in request
                        if (strncmp(request, "http://", 7) == 0)
                            request += 7;
                        proxys[curfd].request = malloc(strlen(request) + 1);
                        strcpy(proxys[curfd].request, request);
                        proxys[curfd].protocol = malloc(strlen(protocol) + 1);
                        strcpy(proxys[curfd].protocol, protocol);
                    }         

                    // wrong inital line
                    if (NULL_found) {
                        // fprintf(stderr, "invalid inital line / header\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].buf); free(for_tok);
                        if (NULL_found & 1) free(proxys[curfd].method);
                        if (NULL_found & 2) free(proxys[curfd].request);
                        if (NULL_found & 4) free(proxys[curfd].protocol);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "valid header\n");


                    // now need to parse "Host:" header
                    char *host, *hostname = NULL, *head_ptr;
                    char *tmp_buf = (char *)calloc(proxys[curfd].bufsize, sizeof (char));
                    // memset(tmp_buf, 0, sizeof (tmp_buf));
                    int remain_cnt = 0;

                    char *oneline = strtok_r(NULL, "\r\n", &all_ptr);
                    while (oneline != NULL) {
                        // if needed, need to use memcpy to get the rest of the headers
                        if (strlen(oneline) > 4) {
                            // found "Host" header
                            if (strncmp(oneline, "Host:", 5) == 0) {
                                host = strtok_r(oneline, " \t\r\n", &head_ptr);
                                hostname = strtok_r(NULL, " \t\r\n", &head_ptr);
                                if (hostname == NULL) NULL_found += 8;
                                break;
                            }
                        }
                        else {
                            // reuse tmp_buf here.
                            memcpy(tmp_buf + remain_cnt, oneline, strlen(oneline));
                            remain_cnt += strlen(oneline);
                            tmp_buf[remain_cnt] = '\n';
                            remain_cnt++;
                            tmp_buf[remain_cnt] = '\0';
                        }
                        oneline = strtok_r(NULL, "\r\n", &all_ptr);
                    }
                    
                    // no Host header field (or not written correctly)
                    if (hostname == NULL) {
                        free(proxys[curfd].buf); free(for_tok); free(tmp_buf);
                        // fprintf(stderr, "invalid inital line / header\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "valid header\n");

                    // remove http:// again.
                    if (strncmp(hostname, "http://", 7) == 0)
                        hostname += 7;
                    proxys[curfd].hostname = malloc(strlen(hostname) + 1);
                    strcpy(proxys[curfd].hostname, hostname);  


                    // check inital line url match header url = need to parse
                    // inital line : GET host:port/path HTTP/1.0
                    // see if it contains port (strchr with ":"), then path(strchr with "/")

                    // ease of parsing (same start, if ':' comes first, things change if port NULL or not)
                    char *init_host, *init_port = NULL, *init_path = NULL;
                    init_host = proxys[curfd].request;
                    // has path (might not be default(/))
                    if ((init_path = strchr(proxys[curfd].request, '/')) != NULL) {
                        *init_path = '\0';
                        init_path++;
                    }
                    // has port (might not be default(80))
                    if ((init_port = strchr(proxys[curfd].request, ':')) != NULL) {
                        *init_port = '\0';
                        init_port++;
                    }

                    char *head_host, *head_port = NULL, *head_path = NULL;
                    head_host = proxys[curfd].hostname;
                    // has path (might not be default(/))
                    if ((head_path = strchr(proxys[curfd].hostname, '/')) != NULL) {
                        *head_path = '\0';
                        head_path++;
                    }
                    // has port (might not be default(80))
                    if ((head_port = strchr(proxys[curfd].hostname, ':')) != NULL) {
                        *head_port = '\0';
                        head_port++;
                    }


                    free(for_tok);
                    proxys[curfd].status++;
                    
                    // need to read more header -> no need now.
                    // else break;

                case 2: ;
                    // need to validate all condition
                    // debug line
                    // fprintf(stderr, "in case 2\n");


                    // method other than GET came in
                    if (strlen(proxys[curfd].method) != 3 || strncmp(proxys[curfd].method, "GET", 3) != 0) {
                        free(proxys[curfd].buf); free(tmp_buf);
                        // fprintf(stderr, "NOT GET\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "GET correct\n");
                    
                    // HTTP version not 1.0
                    if (strlen(proxys[curfd].protocol) != 8 || strncmp(proxys[curfd].protocol, "HTTP/1.0", 8) != 0) {
                        free(proxys[curfd].buf); free(tmp_buf);
                        // fprintf(stderr, "not HTTP/1.0\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "version 1.0\n");                    

                    // inital line host != header field host
                    if (strncmp(init_host, head_host, strlen(init_host)) != 0) {
                        free(proxys[curfd].buf); free(tmp_buf);
                        // fprintf(stderr, "different host\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol); 
                        free(proxys[curfd].hostname);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "same host name\n");    

                    // try to connect with actual server -> almost the same as start of client code.
                    struct addrinfo serv_hints;
                    struct addrinfo *serv_servinfo;  // will point to the results

                    memset(&serv_hints, 0, sizeof serv_hints); // make sure the struct is empty
                    serv_hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
                    serv_hints.ai_socktype = SOCK_STREAM; // TCP stream sockets -> written in [Appendix B]
                    
                    // get port (just in case)
                    if (init_port == NULL && head_port == NULL)  init_port = "80";
                    else if (init_port == NULL && head_port != NULL)  init_port = head_port;

                    // check socket link
                    int check_rv;
                    if ((check_rv = getaddrinfo(init_host, init_port, &serv_hints, &serv_servinfo)) != 0) {
                        free(proxys[curfd].buf); free(tmp_buf);
                        // fprintf(stderr, "invalid host\n");
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol);
                        free(proxys[curfd].hostname);
                        close(curfd);
                        continue;
                    }
                    // fprintf(stderr, "valid host\n");  


                    // debug line
                    // fprintf(stderr, "method : %s, request : %s, protocol : %s\n", proxys[curfd].method, proxys[curfd].request, proxys[curfd].protocol);
                    // fprintf(stderr, "hostname : %s\n", proxys[curfd].hostname);
                    // debug line
                    // fprintf(stderr, "init host : %s, init port : %s, init path : %s\n", init_host, init_port, init_path);
                    // fprintf(stderr, "head host : %s, head port : %s, head path : %s\n", head_host, head_port, head_path);


                    // need to implement cache here -> need to know URL of server
                    // also if it exist in the cache, no need to make a request = just write down data in cache.
                    int in_cache = 0;
                    cacher *c;
                    for (c = head->next_cache; c != NULL; c = c->next_cache) {
                        // cache has same URL (host)
                        if (strncmp(c->url, init_host, strlen(init_host)) == 0) {
                            // need to check time (expire)
                            if (c->expiration > time(NULL)) {
                                // send data in the cache to the client
                                in_cache++;
                                break;
                            }
                            else {
                                // need to write & check response's cache control & send msg to client
                                // will just implement in a way to remove past cache, read all response, check cache-control, do as written
                                c->before_cache->next_cache = c->next_cache;
                                if (c->next_cache != NULL) c->next_cache->before_cache = c->before_cache;
                                free(c->url); free(c->response);
                                free(c);
                                break;
                            }
                        }
                    }


                    // no need to write to the server, get from cache -> client
                    if (in_cache) {
                        // debug line
                        // fprintf(stderr, "cache hit here\n");

                        int cache_sent = 0, to_send = c->datasize;
                        while (to_send) {
                            int just_sent = write(curfd, c->response + cache_sent, to_send);
                            if (just_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                                continue;
                            cache_sent += just_sent;
                            to_send -= just_sent;
                        }
                        // no need (since cache hit)
                        freeaddrinfo(serv_servinfo);

                        // clean up
                        free(proxys[curfd].buf); free(tmp_buf);
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol); 
                        free(proxys[curfd].hostname); 
                        close(curfd);
                        continue;
                    }

                    // debug line
                    // fprintf(stderr, "cache miss here\n");


                    // servinfo : point to a linked list of struct addrinfo = need to search.
                    // loop through all the results and connect to the first we can
                    struct addrinfo *serv_p;
                    int serv_sockfd = -1;
                    for(serv_p = serv_servinfo; serv_p != NULL; serv_p = serv_p->ai_next) {
                        if ((serv_sockfd = socket(serv_p->ai_family, serv_p->ai_socktype, serv_p->ai_protocol)) == -1) {
                            // perror("socket");
                            continue;
                        }

                        if (connect(serv_sockfd, serv_p->ai_addr, serv_p->ai_addrlen) == -1) {
                            // perror("connect");
                            close(serv_sockfd);
                            continue;
                        }

                        break; // meaning found socket & connected safely
                    }

                    if (serv_p == NULL) {
                        free(proxys[curfd].buf); free(tmp_buf);
                        write(curfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
                        free(proxys[curfd].method); free(proxys[curfd].request); free(proxys[curfd].protocol); 
                        free(proxys[curfd].hostname);
                        close(curfd);
                        continue;
                    }
                    // free after done using linked list.
                    freeaddrinfo(serv_servinfo);

                    // no need to read from client anymore
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);        

                    // need to write GET msg to the "actual" server
                    fcntl(serv_sockfd, F_SETFL, O_NONBLOCK);

                    // new setting on ev
                    ev.data.fd = serv_sockfd;
                    // level trigger, not edge trigger (EOLLIN | EPOLLET)
                    ev.events = EPOLLIN;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sockfd, &ev);
                    proxys[serv_sockfd].fd = serv_sockfd;
                    proxys[serv_sockfd].status = 3;     // need to get ans from serv
                    proxys[serv_sockfd].msg_read = 0;
                    proxys[serv_sockfd].opfd = curfd;
                    proxys[serv_sockfd].buf = (char *)calloc(proxys[curfd].bufsize, sizeof (char));
                    proxys[serv_sockfd].bufsize = proxys[curfd].bufsize;
                    // memset(proxys[serv_sockfd].buf, 0, sizeof (proxys[serv_sockfd].buf));    // used when fixed size

                    // need to know as well -> to send response back to client
                    proxys[curfd].opfd = serv_sockfd;
                    proxys[curfd].status = 4;   // status to send ans to client


                    // write to actual server, data all in proxys[serv_sockfd].buf
                    // need to use parsed msg, not original -> rest of header is kept in tmp_buf = only need to make inital line + Host header
                    // write msg to server in proxys[serv_sockfd].buf
                    char *write_buf = proxys[serv_sockfd].buf;
                    int offset = 0;

                    // "GET" + "Host:" line
                    memcpy(write_buf, "GET /", 5);
                    offset += 5;
                    if (init_path != NULL) {
                        memcpy(write_buf + offset, init_path, strlen(init_path));
                        offset += strlen(init_path);
                    }

                    memcpy(write_buf + offset, " HTTP/1.0\r\nHost: ", strlen(" HTTP/1.0\r\nHost: "));
                    offset += strlen(" HTTP/1.0\r\nHost: ");
                    if (head_host != NULL) {
                        memcpy(write_buf + offset, head_host, strlen(head_host));
                        offset += strlen(head_host);
                    }
                    write_buf[offset++] = '\r';
                    write_buf[offset++] = '\n';

                    // rest of the header
                    memcpy(write_buf + offset, tmp_buf, strlen(tmp_buf));
                    offset += strlen(tmp_buf);

                    // final '\n' (+ '\0')
                    write_buf[offset++] = '\r';
                    write_buf[offset++] = '\n';
                    write_buf[offset++] = '\0';

                    // no use now (since done with memcpy).
                    free(tmp_buf);

                    // debug line
                    // fprintf(stderr, "my proxy's request : right next line & so on\n%s", write_buf);
                    // write(2, write_buf, strlen(write_buf));

                    int to_write = strlen(write_buf), bytes_written = 0;
                    while (to_write) {
                        int just_wrote = write(serv_sockfd, write_buf + bytes_written, to_write);
                        if (just_wrote == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                            continue;
                        bytes_written += just_wrote;
                        to_write -= just_wrote;
                    }


                    // opfd will wake when response arrive & it knows fd for client
                    break;

                case 3: ;
                    // will read response from server : curfd = serv_sockfd
                    // no loop needed -> level trigger = will signal again by epoll_wait
                    int bytes_read;
                    int opfd = proxys[curfd].opfd;

                    // need to implement in a way to save all response in the buf.
                    // when bytes_read < 1, check cache control, save in cache or not

                    // response might be big
                    if (proxys[curfd].msg_read + 1 == proxys[curfd].bufsize) {
                        char *tmp_alloc = realloc(proxys[curfd].buf, proxys[curfd].bufsize * 2);
                        if (tmp_alloc == NULL) {
                            free(proxys[curfd].buf); free(proxys[opfd].buf);
                            free(proxys[opfd].method); free(proxys[opfd].request); free(proxys[opfd].protocol); 
                            free(proxys[opfd].hostname); 
                            epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                            close(curfd); close(opfd);
                            continue;
                        }
                        proxys[curfd].buf = tmp_alloc;
                        memset(proxys[curfd].buf + proxys[curfd].msg_read, 0, proxys[curfd].msg_read + 1);
                        proxys[curfd].bufsize *= 2;
                    }

                    bytes_read = read(curfd, proxys[curfd].buf + proxys[curfd].msg_read, proxys[curfd].bufsize - proxys[curfd].msg_read - 1);

                    // close fd with server, client, free memory
                    if (bytes_read < 1) {

                        // need to add new codes for caching.
                        // search for "Cache-Control: " -> private / public / just max-age, "max-age=" -> for expiration
                        char *cc = strstr(proxys[curfd].buf, "Cache-Control: ");
                        if (cc != NULL) {
                            char *sn = strchr(cc, '\n');
                            int to_cpy = sn - cc;
                            char *cache_header = malloc(to_cpy + 1);    // 1 for \0
                            memcpy(cache_header, cc, to_cpy);
                            cache_header[to_cpy] = '\0';

                            char *pp = strstr(cache_header, "private");
                            // if pp != NULL, can't cache = just clean & exit
                            if (pp == NULL) {
                                // pp = strstr(cache_header, "public");
                                char *ap = strstr(cache_header, "max-age=");
                                if (ap != NULL) {
                                    // caching
                                    cacher *new_cache = (cacher *)calloc(1, sizeof (cacher));

                                    new_cache->url = malloc(strlen(proxys[opfd].hostname) + 1);
                                    memcpy(new_cache->url, proxys[opfd].hostname, strlen(proxys[opfd].hostname) + 1);
                                    new_cache->response = (char *)calloc(proxys[curfd].bufsize, sizeof (char));
                                    memcpy(new_cache->response, proxys[curfd].buf, proxys[curfd].msg_read + 1);

                                    new_cache->datasize = proxys[curfd].msg_read;
                                    new_cache->expiration = time(NULL) + atoi(ap + 8);

                                    if (head->next_cache != NULL) head->next_cache->before_cache = new_cache;
                                    new_cache->next_cache = head->next_cache;
                                    head->next_cache = new_cache;
                                    new_cache->before_cache = head;

                                    // debug line
                                    // write(2, new_cache->response, new_cache->datasize);

                                }
                                // do same thing as private if no max-age (could be public with no max-age)
                            }
                            free(cache_header);
                        }


                        // clean & exit
                        free(proxys[curfd].buf); free(proxys[opfd].buf);
                        free(proxys[opfd].method); free(proxys[opfd].request); free(proxys[opfd].protocol); 
                        free(proxys[opfd].hostname); 
                        epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);    // fd linked with client already closed
                        close(curfd); close(opfd);
                    }
                    else {
                        to_write = bytes_read; bytes_written = 0;
                        while (to_write) {
                            int just_wrote = write(opfd, proxys[curfd].buf + proxys[curfd].msg_read + bytes_written, to_write);
                            if (just_wrote == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                                continue;
                            bytes_written += just_wrote;
                            to_write -= just_wrote;
                        }
                        proxys[curfd].msg_read += bytes_read;
                    }
                    
                    break;
                
                // case 4: ;
                    // will reply to client -> might be not if use while loop
                    // break;


                default:
                    break;
                }

            }
        }
    }

    // logic to free all cache
    cacher *to_free = head->next_cache, *tmp_to_free;
    while (to_free != NULL) {
        tmp_to_free = to_free->next_cache;
        free(to_free->url); free(to_free->response);
        free(to_free);
        to_free = tmp_to_free;
    }
    free(head);
    close(epfd);
    close(sockfd);

    return 0;
}