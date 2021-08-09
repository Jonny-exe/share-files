#include "hashtable.c"
#include "helpers.c"
#include "socket.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#define MAX_FILENAME 80
#define TEMP_FILE "temp.tar"
#define OTHERS_FILES "tempdir/"
#define FILE_LIST "local_file_list"
#define HASH_TABLE "local_hash_table"
#define LOCALHOST "127.0.0.1"
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

int createFakeTree(char files[MAX_FILENAME][100], int size);
int handleFile(char *filename, server_t *server);
int listenFolder(char *dirname);
int main(int argc, char *argv[]) {
  int err;
  char dirname[100] = "./test";
  if (fork() == 0) {
    // Handle all the initial files and the TCP server
    int PORT = atoi(argv[2]);
    server_t server = {0};

    err = (server.listen_fd = socket(AF_INET, SOCK_STREAM, 0));
    if (err == -1) {
      perror("socket");
      printf("client: Failed to create socket endpoint\n");
      return err;
    }

    printf("Argv: %s\n", argv[1]);

    if (strcmp(argv[1], "send") == 0) {
      char files[100][MAX_FILENAME];
      int index, i;

      readFolderFiles(dirname, files, &index);
      if (index != 0) {
        err = server_connect(&server, LOCALHOST, PORT);
        if (err) {
          perror("connect");
          printf("client: Failed to connect to socket\n");
          return err;
        } else {
          printf("client: Connected to socket\n");
        }
      }

      for (i = 0; i < index; i++) {
        printf("File: %s\n", files[i]);
        handleFile(files[i], &server);
      }
      connection_close(server.listen_fd);
      return 0;
    }

    err = server_bind(&server, PORT);
    if (err == -1) {
      perror("bind");
      return err;
    } else {
      printf("server: Bind-ed\n");
    }

    err = server_listen(&server);
    if (err == -1) {
      printf("server: Failed to listen on address 0.0.0.0:%d\n", PORT);
      return err;
    } else {
      printf("server: Server listening\n");
    }

    while (1) {
      int conn_fd;
      puts("Waiting to accept");
      err = server_accept(&server, &conn_fd);
      if (err == -1) {
        perror("accept");
        printf("server: Failed accepting connection\n");
        return err;
      }

      int zero = 0, type = 0, i;
      // Type: the type of connection.
      //
      // 0 = request for some file.
      // 1 = request to store files on this pc

      err = read(conn_fd, &type, sizeof(int));
      if (err == -1) {
        perror("read");
        printf("client: Failed reading message\n");
        return err;
      }
      if (type == 1) {
        int times;
        err = read(conn_fd, &times, sizeof(times));
        if (err == -1) {
          perror("read");
          printf("client: Failed reading message\n");
          return err;
        }

        err = write(conn_fd, &zero, sizeof(int));
        if (err == -1) {
          perror("write");
          printf("client: Failed writting message\n");
          return err;
        }

        err = mkdir("tempdir/", 0777);
        if (err == -1) {
          printf("Error creating temp dir\n");
          perror("mkdir");
        }

        for (i = 0; i < times; i++) {
          char hash[80];
          int bytes[64];
          err = read(conn_fd, hash, 80);
          if (err == -1) {
            perror("read");
            printf("server: Failed reading message\n");
            return err;
          }

          err = write(conn_fd, &zero, sizeof(int));
          if (err == -1) {
            perror("write");
            printf("server: Failed writting message\n");
            return err;
          }

          err = read(conn_fd, bytes, 256);
          if (err == -1) {
            perror("read");
            printf("server: Failed reading message\n");
            return err;
          }

          err = write(conn_fd, &zero, sizeof(int));
          if (err == -1) {
            perror("write");
            printf("server: Failed writting message\n");
            return err;
          }

          int idx;
          char filename[60] = "tempdir/";
          strcat(filename, hash);
          FILE *file = fopen(filename, "wb");
          for (idx = 0; idx < 64; idx++)
            fputc(bytes[idx], file);
          fclose(file);
        }

        printf("Close\n");
        err = connection_close(conn_fd);
        if (err == -1) {
          perror("close");
          printf("server: Failed closing connection\n");
          return err;
        }
      } else {
        char hash[40];
        err = read(conn_fd, &hash, sizeof(hash));
        if (err == -1) {
          perror("read");
          printf("client: Failed reading message\n");
          return err;
        }

        char filename[60] = OTHERS_FILES;
        strcat(filename, hash);
        if (access(filename, F_OK) != 0) {
          i = 1; // Don't have the file
          err = write(conn_fd, &i, sizeof(int));
          if (err == -1) {
            perror("write");
            printf("client: Failed writting message\n");
            return err;
          }
          break;
        }
        i = 0; // Everything OK, I have the file
        err = write(conn_fd, &i, sizeof(int));
        if (err == -1) {
          perror("write");
          printf("client: Failed writting message\n");
          return err;
        }

        err = read(conn_fd, &i, sizeof(int));
        if (err == -1 || i != 0) {
          perror("read");
          printf("Error reading code\n");
        }

        int bytes[1][64];
        int times, size;
        FILE *file = fopen(filename, "rb");
        size = getFileSize(file);
        times = ceil((double)size / (double)64);
        splitFile2Bytes(file, size, bytes, times);
        err = write(conn_fd, bytes[0], 256);
        if (err == -1)
          printf("Error sending bytes\n");
        connection_close(conn_fd);
      }
    }
  } else {
    // Handle the file listening
    if (strcmp(argv[1], "send") != 0) {
      strcpy(dirname, "test2");
    }
    listenFolder(dirname);
    exit(0);
  }
  return 0;
}

int createFakeTree(char files[MAX_FILENAME][100], int size) {
  int i;
  for (i = 0; i < size; i++) {
    (void)createFile(files[i]);
  }
  return 0;
}

int handleFile(char *filename, server_t *server) {
  int err, i;
  char command[100];
  err = rename(filename, "temp");
  if (err == -1)
    printf("Error moving file\n");

  err = compressFile("temp");
  if (err == -1)
    printf("Error compressing file\n");
  strcat(filename, ".gz");
  sprintf(command, "mv temp.gz '%s'", filename);
  err = system(command);

  /*
  FILE *file = fopen(filename, "rb");

  int size = getFileSize(file);
  int times = ceil((double)size / (double)64);
  int zip_bytes[times][256 / sizeof(int)];
  splitFile2Bytes(file, size, zip_bytes, times);

  fclose(file);
  unsigned char hashes[times][80];
  for (i = 0; i < times; i++) {
    hashFile(zip_bytes[i], 64, hashes[i]);
  }
  */

  err = cryptFile("a random key", filename, "encrypt");
  if (err == -1)
    printf("Error encrypting file\n");
  strcat(filename, ".cpt");
  char file2Remove[100];
  strcpy(file2Remove, filename);

  FILE *file = fopen(file2Remove, "rb");
  int size = getFileSize(file);
  int times = ceil((double)size / (double)64);
  int bytes[times][256 / sizeof(int)];
  splitFile2Bytes(file, size, bytes, times);

  unsigned char hashes[times][80];
  for (i = 0; i < times; i++) {
    hashFile(bytes[i], 64, hashes[i]);
  }

  createFile(filename);
  strcat(filename, ".f");
  file = fopen(filename, "w");
  for (i = 0; i < times; i++)
    fprintf(file, "%s\n", hashes[i]);

  fclose(file);

  int code, type = 1;
  err = write(server->listen_fd, &type, sizeof(int));
  if (err == -1) {
    perror("write");
    printf("client: Failed writting message\n");
  }

  err = write(server->listen_fd, &times, sizeof(times));
  if (err == -1) {
    perror("write");
    printf("client: Failed writting message\n");
  }

  err = read(server->listen_fd, &code, sizeof(code));
  if (err == -1 && code == 0) {
    perror("read");
    printf("client: Failed reading message\n");
    return err;
  }
  for (i = 0; i < times; i++) {
    // Steps:
    // 1. Send times
    // 2. Send hash
    // 3. Send bytes
    // 4. Repeat 2 and 3 for times

    err = write(server->listen_fd, hashes[i], sizeof(hashes[i]));
    if (err == -1) {
      perror("write");
      printf("client: Failed writting message\n");
    }

    err = read(server->listen_fd, &code, sizeof(code));
    if (err == -1) {
      perror("read");
      printf("client: Failed reading message\n");
      return err;
    }

    err = write(server->listen_fd, bytes[i], sizeof(bytes[i]));
    if (err == -1) {
      perror("write");
      printf("client: Failed writting message\n");
      return err;
    }

    err = read(server->listen_fd, &code, sizeof(code));
    if (err == -1 && code == 0) {
      perror("read");
      printf("client: Failed reading message\n");
      return err;
    }
  }
  remove(file2Remove);
  return 0;
}

int receiveFile(char *originalFilename) {
  // FIXME: this function doesn't work
  int times, lines, err, PORT, i;
  row_t hashtable[200];
  read_table(HASH_TABLE, hashtable, &lines);

  char hashes[10][50];
  int hashIdx;
  int zero = 0;
  int type = 0;
  hashesFromFile(originalFilename, hashes, &hashIdx);
  int bytes[hashIdx][64];

  for (i = 0; i < hashIdx; i++) {
    server_t server = {0};
    PORT = 8080;
    err = (server.listen_fd = socket(AF_INET, SOCK_STREAM, 0));
    if (err == -1) {
      perror("socket");
      printf("client: Failed to create socket endpoint\n");
      return err;
    }

    err = server_connect(&server, LOCALHOST, PORT);
    if (err == -1) {
      perror("connect");
    }
    // TODO: make if one connection doens't work to try another one

    err = write(server.listen_fd, &type, sizeof(int));
    if (err == -1) {
      perror("write");
      printf("client: Failed writting message\n");
      return err;
    }

    err = write(server.listen_fd, hashes[i], 40);
    if (err == -1) {
      perror("write");
      printf("client: Failed writting message\n");
      return err;
    }

    err = read(server.listen_fd, &zero, sizeof(int));
    if (err == -1) {
      perror("read");
      printf("Error reading code or incorrect code\n");
      return err;
      // TODO: here in theory just try another ip
    }

    err = write(server.listen_fd, &zero, sizeof(int));
    if (err == -1) {
      perror("write");
      printf("client: Failed writting message\n");
      return err;
    }

    err = read(server.listen_fd, bytes[i], 256);
    if (err == -1) {
      perror("read");
      printf("Error reading bytes\n");
    }

    connection_close(server.listen_fd);
  }

  char filename[strlen(originalFilename) - 1];
  int j;
  memcpy(filename, originalFilename, strlen(originalFilename) - 2);
  filename[strlen(originalFilename) - 2] = '\0';
  FILE *file = fopen(filename, "wb");
  for (i = 0; i < hashIdx; i++) {
    for (j = 0; j < 64; j++)
      fputc(bytes[i][j], file);
  }
  fclose(file);

  err = cryptFile("a random key", filename, "decrypt");
  if (err == -1)
    printf("Error decrypting file\n");

  char zipfilename[strlen(filename) - 3];
  memcpy(zipfilename, filename, strlen(filename) - 4);
  zipfilename[strlen(filename) - 4] = '\0';
  err = uncompressFile(zipfilename);
  if (err == -1) {
    printf("Error uncompressing file\n");
  }
  return 0;
}

int listenFolder(char *dirname) {
  int length, i;
  int fd;
  int wd;

  while (1) {
    char buffer[BUF_LEN];
    i = 0;
    fd = inotify_init();

    if (fd < 0) {
      perror("inotify_init");
    }
    wd = inotify_add_watch(
        fd, dirname, IN_MODIFY | IN_CREATE | IN_DELETE | IN_ACCESS | IN_OPEN);
    length = read(fd, buffer, BUF_LEN);

    if (length < 0) {
      perror("read");
    }

    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len) {
        if (event->mask & IN_CREATE) {
          printf("The file %s was created.\n", event->name);
        } else if (event->mask & IN_DELETE) {
          printf("The file %s was deleted.\n", event->name);
        } else if (event->mask & IN_MODIFY) {
          printf("The file %s was modified.\n", event->name);
        } else if (event->mask & IN_ACCESS) {
          printf("The file %s was accessed.\n", event->name);
          if (event->name[strlen(event->name) - 1] == 'f' &&
              event->name[strlen(event->name) - 2] == '.') {
            char filename[80] = "test/";
            strcat(filename, event->name);
            receiveFile(filename);
          }
        } else if (event->mask & IN_OPEN) {
          printf("The file %s was opened.\n", event->name);
          if (event->name[strlen(event->name) - 1] == 'f' &&
              event->name[strlen(event->name) - 2] == '.') {
            char filename[80] = "test/";
            strcat(filename, event->name);
            receiveFile(filename);
          }
        }
      }
      i += EVENT_SIZE + event->len;
    }

    (void)inotify_rm_watch(fd, wd);
    (void)close(fd);
  }

  return 0;
}
