#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>        //needed for open
#include <unistd.h>       //needed for write
#include <errno.h>        //needed to get specific errors
#include <time.h>
#include <poll.h>

#define BUFSIZE 1024

int parse(char doc_root[BUFSIZE], int connfd);
int send_file(char document_root[BUFSIZE], char filename[BUFSIZE], int connfd, int protocol, int malformed_flag);

int conncnt = 0;

typedef struct args {
  int connfd;
  char doc_root[BUFSIZE];
} args;


// serve_client takes in the args struct which has the doc_root and the connection file descriptor
// detatches the new thread and calls parse which begins the next stage of the program 
void *serve_client(void *thread_args) {
    char doc_root[BUFSIZE]; //create message buffer and document_root path string 
    int connfd = ((args*)thread_args)->connfd; // our client's socket fd
    strcpy(doc_root, ((args*)thread_args)->doc_root); 

    if (pthread_detach(pthread_self()) != 0){
        printf("error detaching\n");
        return (void *) 1;
    }

    // free heap space since we have connfd and doc_root
    free(thread_args);
    
    //Parse the request
    parse(doc_root, connfd);

    shutdown(connfd, 0);
    close(connfd);
    // we lose a connection
    conncnt--;

    return NULL;
}

int parse(char doc_root[BUFSIZE], int connfd) {
  char request[BUFSIZE], protocolArg[BUFSIZE], filename[BUFSIZE], host[BUFSIZE], buf[BUFSIZE]; ;
  int num_read; 
  

  // starts counting timeout when client specifies HTTP1.1 in request 
  do {

    // eliminate potential previous usage from thread overlaps and recalling this function 
    bzero(buf, BUFSIZE);
    bzero(request, BUFSIZE);
    bzero(protocolArg, BUFSIZE);
    bzero(filename, BUFSIZE);
    bzero(host, BUFSIZE);

    int total_read = 0;

    // it needs to be an array and we're only dealing with one pointer
    struct pollfd pollfd[1];

    pollfd[0].fd = connfd;
    // POLLIN is a read event, we will be reading when we receive
    pollfd[0].events = POLLIN;

    // only will happen on the second iteration
    if (!strcmp(protocolArg, "HTTP/1.1")) {

      // timeout is 1 minute divided by number of connections
      // timeout is in milliseconds
      int timeout = 60000/conncnt;
      int pll = poll(pollfd, 1, timeout);
      if (pll == 0) {
        send(connfd, "408 Request Timeout\n", 21, 0);
        return 1;
      }
    }

    // while loop checks for a double-carriage return for mac or windows while reading the client queries 
    // loop checks that none of the potential symbols for double-returns have been found
    while (strstr(buf, "\r\n\r\n") == NULL && strstr(buf, "\r\r") == NULL && strstr(buf, "\n\n") == NULL){
        //Reads in every line given by client query 
        num_read = recv(connfd, buf + total_read, BUFSIZE, 0);
        if (num_read < 0) {
          printf("ERROR reading from socket\n");
          return 1;
        } 
        else {
          if (num_read > 0){
            printf("server received %d bytes: %s\n", num_read, buf);
            total_read += num_read;
          }
        }
    }

    // argument counter since HTTP1.1 has four arguments and HTTP1.0 has only three 
    int ctr = 0;
    int protocolFlag = -1; 
    int malformed_flag = 0; // need to save for header formatting later

    // split string into parts by a space delimiter
    char* args = strtok(buf, " \r\n");
    while (args != NULL) {
        switch (ctr) {
            case 0:
                strcpy(request, args);
                ctr++;
                break;
            case 1:
                strcpy(filename, args);
                ctr++;
                break;
            case 2:
                strcpy(protocolArg, args);
                ctr++;
                break;
            case 3:
                strcpy(host, args);
            default:
                ctr++;                
        }
        // strtok() needs NULL on repeated call, then it will return the next substring
        // once it's done is actually returns NULL
        args = strtok(NULL, " \r\n");
    }  

    // set protocol flag based on protocolArg
    // if both 1.1 and 1.0 are in the request, it'll be caught by strcmp later on
    if(strstr(protocolArg, "1.1") != NULL){protocolFlag = 1;}
    else if(strstr(protocolArg, "1.0") != NULL){protocolFlag = 0;}

    // argument checking for HTTP1.1 then HTTP1.0
    if (!(strcmp(protocolArg, "HTTP/1.1"))) {
      // need to check for host argument sent by firefox 
      if (ctr < 4 || strcmp(request, "GET") || strcmp(host, "Host:")) {
          malformed_flag = 1;
      }
    } else if ((ctr < 3) || strcmp(request, "GET") || (strcmp(protocolArg, "HTTP/1.0"))) {
          // check 1.0 request
          malformed_flag = 1;
    }

    // request is ready: we should be good to send specified file or header 
    send_file(doc_root, filename, connfd, protocolFlag, malformed_flag);

  } while (!(strcmp(protocolArg, "HTTP/1.1")));

    return 0;
}

// function takes in an argument and returns day of week for easier string construction in header  
char* day_of_week(int t) {
  switch (t) {
    case 0:
      return "Sunday";
    case 1:
      return "Monday";
    case 2:
      return "Tuesday";
    case 3:
      return "Wednesday";
    case 4:
      return "Thursday";
    case 5:
      return "Friday";
    case 6:
      return "Saturday";
  }

  // silences makefile
  return NULL;
}

char* get_month(int t) {
  switch (t) {
    case 0:
      return "Jan";
    case 1:
      return "Feb";
    case 2:
      return "Mar";
    case 3:
      return "Apr";
    case 4:
      return "May";
    case 5:
      return "Jun";
    case 6:
      return "Jul";
    case 7:
      return "Aug";
    case 8:
      return "Sep";
    case 9:
      return "Oct";
    case 10:
      return "Nov";
    case 11:
      return "Dec";
  }

  // silences makefile
  return NULL;
}

// takes in a path to file, filename, the HTTP protocol flag, and the socket fd 
// sends file to client if succesful checking 
int send_file(char document_root[BUFSIZE], char filename[BUFSIZE], int connfd, int protocol, int malformed_flag) {
  int fd, nread, num_sent;
  char* ftype;
  char target[BUFSIZE], buf[BUFSIZE], to_add[BUFSIZE], content_type[BUFSIZE];
  time_t* t;
  struct tm tm;

  char protocolHeader[BUFSIZE] = "HTTP/1.1 ";
  // reassigns last character to 0 if HTTP1.0
  if (!protocol) {
    protocolHeader[7] = 48; 
  }

  if (!malformed_flag) {
      // check the filename here
      // if it's / we want index
      if (!strcmp(filename, "/")) {
        strcpy(filename, "/index.html");
      } 

      // concatenate root and filename to produce target 
      strcpy(target, document_root);
      strcat(target, filename);  

      fd = open(target, O_RDONLY);
      printf("File passed through: %s\n", target);

      // initialize error code for send()
      char* error = "404 not found\n";

      // get error response from errno.h
      // concatenates protocol header with specified error code

      if ((strstr(filename, "..") != NULL) || strstr(filename, "./") != NULL) {
        // trying to access above root or use relative file paths (which is bad in HTTP)
        error = "403 Permission Denied\n";
        strcat(protocolHeader, error);
        strcpy(buf, protocolHeader);
        bzero(filename, BUFSIZE);
        bzero(target, BUFSIZE);

        strcpy(filename, "/403.html");

        strcpy(target, document_root);
        strcat(target, filename); 
        
        fd = open(target, O_RDONLY);

      } else if (fd == -1) {
          if (errno == 13) {
            // 403 permission denied
            error = "403 Permission Denied\n";
            strcat(protocolHeader, error);
            strcpy(buf, protocolHeader);

            bzero(filename, BUFSIZE);
            bzero(target, BUFSIZE);

            strcpy(filename, "/403.html");

            strcpy(target, document_root);
            strcat(target, filename); 
            fd = open(target, O_RDONLY);

          } else {
            // 404 error or other error of unknown origin
            // special web page for this one to demonstrate combining HTTP and HTML
            strcat(protocolHeader, error);
            strcpy(buf, protocolHeader);

            bzero(filename, BUFSIZE);
            bzero(target, BUFSIZE);

            strcpy(filename, "/404.html");

            strcpy(target, document_root);
            strcat(target, filename); 
            fd = open(target, O_RDONLY);

          }
      } else {
        // 200 OK
        error = "200 OK \n";
        strcat(protocolHeader, error);
        strcpy(buf, protocolHeader);
      }

      // need to get date, content-type, and content-length

      // gets file type
      ftype = strtok(filename, ".");
      ftype = strtok(NULL, ".");

      if (ftype == NULL) {
        // catches bad file name issues
        strcpy(content_type, "unknown"); 
      } else if (!strcmp(ftype, "html")) {
        strcpy(content_type, "text/html");
      } else if (!strcmp(ftype, "txt")) {
        strcpy(content_type, "text/txt");
      } else if (!strcmp(ftype, "gif")) {
        strcpy(content_type, "image/gif");
      } else if (!strcmp(ftype, "jpg")) {
        strcpy(content_type, "image/jpg");
      } else {
        strcpy(content_type, "unknown"); 
      }

  } else {
    strcat(protocolHeader, "400 Malformed Request\n");
    strcpy(buf, protocolHeader);
    
    bzero(filename, BUFSIZE);
    bzero(target, BUFSIZE);

    strcpy(filename, "/400.html");
    strcpy(target, document_root);
    strcat(target, filename); 
    fd = open(target, O_RDONLY);

    strcpy(content_type, "text/html");
  }

  //Makes sure date-time don't leak between threads or get used while calculating since our translation functions are rigid 
  t = malloc(sizeof(time_t));
  time(t);
  tm = *gmtime(t);
  
  snprintf(to_add, BUFSIZE, "%s, %d %s %d %d:%d:%d UTC\n", day_of_week(tm.tm_wday), tm.tm_mday, 
          get_month(tm.tm_mon), tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
  strcat(buf, to_add);
  free(t);

  snprintf(to_add, BUFSIZE, "Content-Type: %s\n", content_type);  
  strcat(buf, to_add);
      
  num_sent = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  snprintf(to_add, BUFSIZE, "Content-Length: %d\n\n", num_sent);
  strcat(buf, to_add);

  // send header before sending information
  send(connfd, buf, strlen(buf), 0);

  // send file, if no error

  while ((nread = read(fd, buf, sizeof(buf)))) {
    // write to connfd
    send(connfd, buf, nread, 0);
  }
  
  return 0;
}

// check flags is a simple command-line argument to check that the usage is correct for ./server
int check_flags(char **argv) {
  if (!strcmp(argv[1], "-document_root") &&
      !strcmp(argv[3], "-port") &&
      atoi(argv[4])) {
        return 1;
  }
  return -1;
}

// usage ./server -document_root "filepath" -port num
int main(int argc, char **argv) {
    int listenfd; // listening socket
    int *connfd = NULL; // connection socket
    int portno; // port to listen on
    socklen_t clientlen; // byte size of client's address
    pthread_t tid; // thread id
    int optval;
    struct sockaddr_in myaddr; // my ip address info
    struct sockaddr clientaddr; // client's info
    char doc_root[BUFSIZE];
    args* tuple;

    // the executable counts as an argument in the commandline
    if (argc != 5 || !check_flags(argv)){
      fprintf(stderr, "usage: %s -document_root <filepath> -port <port>\n", argv[0]);
      return 1;
    }

    strcpy(doc_root, argv[2]);

    // convert portno to string
    portno = atoi(argv[4]);

    // set up local address structs
    myaddr.sin_port = htons(portno);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // create server socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        printf("ERROR opening socket\n");
        return 1;
    }

    // setsockopt: eilminates "ERROR on binding: Address already in use" error
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

    // bind the listening socket (listenfd) with a specific address/port
    if (bind(listenfd, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0) {
        printf("ERROR on binding\n");
        return 1;
    }

    if (listen(listenfd, 10) < 0) {
        printf("ERROR on listen\n");
        return 1;
    }
    
    // infinite listening loop
    while (1){

        // accept: wait for a connection request
        clientlen = sizeof(clientaddr);

        // bundling doc_root because of sending thread args
        tuple = malloc(sizeof(args));
        strcpy(tuple->doc_root, doc_root);
        tuple->connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        if (connfd < 0) {
            printf("ERROR on accept\n");
            return 1;
        }

        printf("Client connected!\n");
        // new connection - increment counter for timeout function
        conncnt++;

        // try to create thread and run function 'serve_client'
        if (pthread_create(&tid, NULL, serve_client, (void *)tuple) != 0) {
            printf("error creating threads\n");
            return 1;
        }
    }
    return 0;
}