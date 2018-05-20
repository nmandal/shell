#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define DELIM " \t\r\n\a"

char error_message[30] = "An error has occurred\n";
char *PATH[20];

void printError(void);
int exec(char *args[], char *rArgs[]);
int parallel(char *ret, char *line);
int redir(char *ret, char *line);
int readCom(char *args[],FILE *fp);
int sepLine(char *line, char *args[], char *lim);
int exec_cd(char *args[], int numArgs);
int exec_path(char *args[], int numArgs);

void
printError(void)
{
  write(STDERR_FILENO, error_message, strlen(error_message));
}

int
exec(char *args[], char *rArgs[])
{
  int pid;
  int status;
  char *comPath = malloc(100 * sizeof(char *));

  int i = 0;
  int notFound = 1;
  if (PATH[0] == NULL){
    printError();
    return 1;
  }
  if (args == NULL)
    return 1;
  if (args[0] == NULL)
    return 1;

  while (1) {
    if(PATH[i] == NULL)
      break;
      
    char *tempStr = malloc(100 * sizeof(char));
    if (!strcpy(tempStr, PATH[i])){
      printError();
      return 1;
    }
    int strLen = strlen(tempStr);
    tempStr[strLen] = '/';
    tempStr[strLen + 1] = '\0';
    strcat(tempStr, args[0]);
    if (access(tempStr, X_OK) == 0){
      strcpy(comPath, tempStr);
      notFound = 0;
      free(tempStr);
      break;
    }
    free(tempStr);
    i++;
  }
  if (notFound) {
    printError();
    return 1;
  }
  pid = fork();
  switch(pid){
    case -1:
      printError();
      return 1;
    case 0:
      if (rArgs){
        int fd_out = open(rArgs[0],O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
        if (fd_out > 0){
          dup2(fd_out, STDOUT_FILENO);
          fflush(stdout);
        }
      }
      execv(comPath, args);
    default:
      waitpid(pid, &status, 0);
  }
  return 0;
}

int
parallel(char *ret, char *line)
{
    char **coms = malloc(100 * sizeof(char *));
    int numComs = sepLine(line, coms, "&");
    char **args = malloc(50 * sizeof(char *));
    char *rDir = malloc(100 * sizeof(char));
    for (int i = 0; i < numComs; i++) {
        if ((rDir = strchr(coms[i], '>'))) {
            redir(rDir, coms[i]);
            continue;
        }
        sepLine(coms[i], args, DELIM);
        exec(args, NULL);
    }
    free(args);
    free(coms);
    return 0;
}

int
redir(char *ret, char *line)
{
    char *pArgs[100];
    char *rArgs[100];
    ret[0] = '\0';
    ret = ret + 1;
    int argsNum = sepLine(line, pArgs, DELIM);
    if (argsNum == 0){
        printError();
        return 1;
    }
    int rArgc = sepLine(ret, rArgs, DELIM);
    if (rArgc != 1){
        printError();
        return 1;
    }
    exec(pArgs, rArgs);
    return 0;
}

int
readCom(char *args[],FILE *fp)
{
    char *line = malloc(100 * sizeof(char));
    size_t len = 0;
    ssize_t nread;
    char *rDir = NULL;
    char *rPar = NULL;
    fflush(stdin);
    
    if ((nread = getline(&line, &len, fp)) == -1)
        return 1;
    
    if ((strcmp(line, "\n") == 0) || (strcmp(line, "") == 0))
        return -1;
    
    if (line[strlen(line) - 1] == '\n')
        line[strlen(line) - 1] = '\0';
    
    if (line[0] == EOF)
        return 1;
    
    if ((rPar = strchr(line, '&'))) {
        parallel(rPar, line);
        return -1;
    }
    
    if ((rDir = strchr(line, '>'))) {
        redir(rDir, line);
        return -1;
    }
    
    int argsIndex = sepLine(line, args, DELIM);
    if (argsIndex == 0)
        return 0;
    
    if (strcmp(args[0], "exit") == 0) {
        if (args[1])
            printError();
        exit(0);
    }
    
    if (strcmp(args[0], "cd") == 0) {
        exec_cd(args, argsIndex);
        return -1;
    }
    
    if (strcmp(args[0], "path") == 0) {
        exec_path(args, argsIndex);
        return -1;
    }
    return 0;
}

int
sepLine(char *line, char *args[], char *lim)
{
    char *s;
    int i = 0;
    if (!args)
        args = malloc(100 * sizeof(char));
    args[i] = strtok_r(line, lim, &s);
    i++;
    while (1) {
        if (!(args + i)) {
            char *tmp = (char *)(args + i);
            tmp = malloc(100*sizeof(char));
            tmp++;
        }
        args[i] = strtok_r(NULL, lim, &s);
        if (args[i] == NULL)
            break;
        i++;
    }
    if (args[0] == NULL)
        return 0;
    return i;
}

int
exec_cd(char *args[], int numArgs)
{
    if (numArgs != 2) {
        printError();
        return 1;
    }
    if (chdir(args[1]) == -1){
        printError();
        return 1;
    }
    return 0;
}

int
exec_path(char *args[], int numArgs)
{
    int i = 0;
    while (1) {
        if ((PATH[i] = args[i+1]) == NULL)
            break;
        i++;
    }
    return 0;
}

int
main(int argc, char* argv[])
{
  int batchMode = 0;
  PATH[0] = "/bin";

  FILE *fp;
  char **args;
  if (argc == 2) {
    if (!(fp = fopen(argv[1], "r")) ) {
      printError();
      exit(1);
    }
    batchMode = 1;
  } else if (argc < 1 || argc > 2) {
    printError();
    exit(1);
  }
  int isFinish = 0;
  while (1) {
    if (batchMode == 1){
      while (1) {
        args = malloc(100 * sizeof(char));
        int status = readCom(args, fp);
        fflush(fp);
        if (status == -1)
          continue;
        if (status == 1) {
          isFinish = 1;
          break;
        }
        int errNum = exec(args, NULL);
        free(args);
        if (errNum == 1)
          continue;
      }
      break;
    } else {
      fprintf(stdout, "wish> ");
      fflush(stdout);
      args = malloc(100 * sizeof(char));
      int status = readCom(args, stdin);
      fflush(stdin);
      if (status == -1)
        continue;   // built-in -> continue
      if (status == 1)
        break;
      if (exec(args, NULL) == 1)
        continue;   // error -> continue
      free(args);
    }
    if (isFinish)
      break;
  }
  return 0;
}
