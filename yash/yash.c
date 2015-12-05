#include <stdio.h>
#include <malloc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

typedef struct process
{
  struct process *next;
  pid_t pid;
} process;

typedef struct job
{
  struct job *next;
  char *name;
  process *first_process;
  pid_t pgid;
} job;
job *first_job = NULL;

void toForeground (job *j)
{
  int status;
  int savestdout = dup(STDOUT_FILENO);
  pid_t stdoutpgid = tcgetpgrp(savestdout);
  job * i;
  tcsetpgrp (STDOUT_FILENO, j->pgid);
  kill(- j->pgid, SIGCONT);
  printf("%s\n",j->name);
  waitpid(- j->pgid, &status, 0);
  tcsetpgrp(STDOUT_FILENO, stdoutpgid);
  if (first_job->pgid == j->pgid) {
    free(first_job);
    first_job = NULL;
    return;
  } else {
    i = first_job;
    while (i->next->pgid != j->pgid) {
      i = j->next;
    }
    free(i->next);
    i->next = NULL;
  }
}

void sig_handler(int signo)
{
  if (signo == SIGINT) {
    kill(- first_job->pgid, SIGINT);
  }
  if (signo == SIGTSTP) {
    kill(- first_job->pgid, SIGTSTP);
  }
}

int main(int argc, char **argv) {
  pid_t pid;
  int status;
  int bytes_read;
  char * tokens1[2001];
  char * tokens2[2001];
  int i = 0;
  char * tok;
  size_t NCHARS = 3000;
  char* input = (char *) malloc(NCHARS +1);
  char * special = "";
  int fd[3];
  bool amper;
  int savestdin;

  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);

  signal(SIGINT, sig_handler);
  signal(SIGTSTP, sig_handler);

  for (i=0; i<2001; i++) {
    tokens1[i] = (char *) malloc(15);
    tokens2[i] = (char *) malloc(15);
  }
  while (!feof(stdin)) {
    amper = false;
    special = "";
    for (i=0; i<2001; i++) {
      tokens1[i] = "";
      tokens2[i] = "";
    }
    i=0;
    printf("$ ");

    bytes_read = getline (&input, &NCHARS, stdin);
    if (bytes_read == -1) {
      puts ("INPUT ERROR");
      continue;
    }
    if (strcmp("\n", input) == 0) {continue;} // handles case for no command entry

    tok = strtok (input, " \t\n");
    while (tok != NULL) {
      if (strcmp("&", tok) == 0) {
        amper = true;
        tok = strtok(NULL, " \t\n");
        tokens2[i] = NULL;
        continue;
      }

      if (strcmp("fg", tok) == 0) {
        savestdin = dup(0);
        tokens1[0] = NULL;
        job* j = first_job;
        while (j->next != NULL) {
          j = j->next;
        }
        toForeground (j);

      }
      if (strcmp("|", tok) == 0) { // pipe found, get next portion of commands
        special = "|";
        tokens1[i] = NULL; // null terminate list of tokens1
        i = 0;
        tok = strtok (NULL, " \t\n");
        while (tok != NULL) {
          if (strcmp("&", tok) == 0) {
            amper = true;
            break;
          }
          tokens2[i] = tok;
          i++;
          tok = strtok (NULL, " \t\n");
        }
        tokens2[i] = NULL; // null terminate list of tokens2
        break;
      }
      if (strcmp("<", tok) == 0) { // stdin found, get next portion of commands
        special = "<";
        tokens1[i] = NULL; // null terminate list of tokens1
        i = 0;
        tok = strtok (NULL, " \t\n");
        while (tok != NULL) {
          tokens2[i] = tok;
          i++;
          tok = strtok (NULL, " \t\n");
        }
        tokens2[i] = NULL; // null terminate list of tokens2
        break;
      }
      if (strcmp(">", tok) == 0) { // stdout found, get next portion of commands
        special = ">";
        tokens1[i] = NULL; // null terminate list of tokens1
        i = 0;
        tok = strtok (NULL, " \t\n");
        while (tok != NULL) {
          tokens2[i] = tok;
          i++;
          tok = strtok (NULL, " \t\n");
        }
        tokens2[i] = NULL; // null terminate list of tokens2
        break;
      }
      if (strcmp("2>", tok) == 0) { // stdout found, get next portion of commands
        special = "2>";
        i = 0;
        tok = strtok (NULL, " \t\n");
        while (tok != NULL) {
          tokens2[i] = tok;
          i++;
          tok = strtok (NULL, " \t\n");
        }
        tokens2[i] = NULL; // null terminate list of tokens2
        break;
      }

      tokens1[i] = tok;
      i++;
      tok = strtok (NULL, " \t\n");
    }
    if (strcmp("", special) == 0) {
      tokens1[i] = NULL; // null terminate list of tokens1 if no special chars found
    }
    if (strcmp("fg", tokens1[0]) == 0) {
      dup2(savestdin, 0);
      continue;
    }
    /*Execute next portion according to special character received */
    if (strcmp("q", tokens1[0]) == 0) {return 0;}

    if (strcmp("<", special) == 0) {
      int savestdin = dup(0);
      fd[0] = open(tokens2[0], O_RDONLY);
      if (fd[0] == -1) {
        printf("ERROR: File does not exist\n");
        continue;
      }
      dup2(fd[0], 0);
      pid = fork();
      close(fd[0]);
      if (pid < 0) {     
        printf("ERROR: Fork failed\n");
        continue;
      }
      else if (pid == 0) {          
        if (execvp(tokens1[0], tokens1) < 0) {     
          printf("ERROR: exec failed\n");
          _exit(1);
        }
      }
      else {                         
        if (!amper) {
          while (wait(&status) != pid);     
        }
        dup2(savestdin, 0);
        continue;
      }
    }
    if (strcmp(">", special) == 0) {
      int savestdout = dup(1);
      fd[1] = open(tokens2[0], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
      dup2(fd[1], 1);
      pid = fork();
      close(fd[1]);
      if (pid < 0) {     
        printf("ERROR: Fork failed\n");
        continue;
      }
      else if (pid == 0) {     
        if (execvp(tokens1[0], tokens1) < 0) {   
          printf("ERROR: exec failed\n");
          _exit(1);
        }
      }
      else {                          
        if (!amper) {
          while (wait(&status) != pid);      
        }
        dup2(savestdout, 1);
        continue;
      }
    }
    if (strcmp("2>", special) == 0) {
      fd[2] = open(tokens2[0], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
      dup2(fd[2], 2);
      close(fd[2]);
      continue;
    }
    if (strcmp("|", special) == 0) {
      pipe(fd);
      if ((pid = fork()) < 0) {     
        printf("ERROR: Fork failed\n");
        continue;
      }
      else if (pid == 0) {          
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        if (execvp(tokens1[0], tokens1) < 0) {     
          printf("ERROR: exec failed\n");
          _exit(1);
        }
      }
      else {
        pid=fork();
        if(pid==0) {
          dup2(fd[0], STDIN_FILENO);
          close(fd[0]);
          if (execvp(tokens2[0], tokens2) < 0) {   
            printf("ERROR: exec failed\n");
            _exit(1);
          }
        }
        else {
        }
      }
      if (!amper) {
        wait(&status);       
      }
    }
    /* Handles single command entries */
    else {
      int savestdout = dup(STDOUT_FILENO);
      pid_t stdoutpgid = tcgetpgrp(savestdout);
      pid = fork();
      process * p = (process*) malloc (sizeof(process));
      p->pid = pid;
      p->next = NULL;
      job * j = (job*) malloc (sizeof(job));
      job * jit;
      j->name = (char *) malloc (sizeof(100));
      strcpy(j->name, tokens1[0]);
      j->next = NULL;
      j->pgid = pid;
      setpgid(j->pgid, pid);
      j->next = NULL;
      if (first_job == NULL) {
        first_job = j;
      } else {
        job * i = first_job;
        while (i != NULL) {
          i = i->next;
        }
      }
      if (pid < 0) {
        printf("ERROR: Fork failed\n");
        continue;
      }
      else if (pid == 0) {
        if (!amper) {
          tcsetpgrp (STDOUT_FILENO, j->pgid);
        }
        if (execvp(tokens1[0], tokens1) < 0) {
          printf("ERROR: exec failed\n");
          _exit(1);
        }
      }
      else {
        if (!amper) {
          while (wait(&status) != pid);
          tcsetpgrp(STDOUT_FILENO, stdoutpgid); // just in case
          if (first_job->pgid == j->pgid) {
            free(first_job);
            first_job = NULL;
          } else {
            jit = first_job;
            while (jit->next->pgid != j->pgid) {
              jit = j->next;
            }
            free(jit->next);
            jit->next = NULL;
          }
        }
        continue;
      }
    }
  }
  return 0;
}
