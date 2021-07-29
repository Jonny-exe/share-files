#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

// This server partly from: https://ops.tips/blog/a-tcp-server-in-c/#the-overview

typedef struct server {
  int listen_fd;
} server_t;

int server_accept(server_t *server);
int server_listen(server_t *server);
int server_bind(server_t *server, int PORT);
int server_connect(server_t *server, int PORT);
int server_write(server_t *server);
int server_read(server_t *server, char *buff);

int main() {
  int err = 0;
  int PORT = 8080;
  int type = 0;
  server_t server = {0};

  err = (server.listen_fd = socket(AF_INET, SOCK_STREAM, 0));
  if (err == -1) {
    perror("socket");
    printf("Failed to create socket endpoint\n");
    return err;
  }

  if (type == 1) {
    err = server_bind(&server, PORT);
    if (err == -1) {
      perror("bind");
      printf("Failed to bind socket to address\n");
      return err;
    } else {
      puts("Bind-ed\n");
    }

    err = server_listen(&server);
    if (err) {
      printf("Failed to listen on address 0.0.0.0:%d\n", PORT);
      return err;
    } else {
      puts("Server listening\n");
    }

    while (1) { // Check if this is working with lsof -i:PORT
      err = server_accept(&server);
      if (err) {
        printf("Failed accepting connection\n");
        return err;
      }
    }
  } else {
    err = server_connect(&server, PORT);
    if (err == -1) {
      perror("connect");
      printf("Failed to connect to socket\n");
      return err;
    } else {
      printf("Connected to socket\n");
    }
    //TODO: here goes server_write
  }
    
  return 0;
}

int server_accept(server_t *server) {
  int err = 0;
  int conn_fd;
  socklen_t client_len;
  struct sockaddr_in *client_addr;

  client_len = sizeof(client_addr);

  err = (conn_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr,
                          &client_len));
  if (err == -1) {
    perror("accept");
    printf("failed accepting connection\n");
    return err;
  }

  printf("Client connected!\n");

  err = close(conn_fd);

  if (err == -1) {
    perror("close");
    printf("failed to close connection\n");
    return err;
  }

  return err;
}

int server_listen(server_t *server) {
  int BACKLOG = 4;
  int err;
  err = listen(server->listen_fd, BACKLOG);
  if (err == -1) {
    perror("listen");
    printf("Failed to put socket in passive mode\n");
    return err;
  }
  return err;
}

int server_bind(server_t *server, int PORT) {
  int err;
  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  err = bind(server->listen_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr));
  return err;
}

int server_connect(server_t *server, int PORT) {
  int err;
  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);
  err = connect(server->listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  return err;
}

int server_write(server_t *server) {
  int err;
  char buff[80] = "Hello";
  bzero(buff, sizeof(buff));
  err = write(server->listen_fd, buff, sizeof(buff));
  return err;
}

int server_read(server_t *server, char *buff) {
  int err;
  err = read(server->listen_fd, buff, sizeof(buff));
  return err;
}
