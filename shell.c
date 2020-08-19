/* This is the only file you should update and submit. */

/* Fill in your Name and GNumber in the following two comment fields
 * Name: Zain Fazal
 * GNumber: G01129484
 */

#include "logging.h"
#include "shell.h"
#include <string.h>

/* Feel free to define additional functions and update existing lines */

//One node of the child process
typedef struct _child_process
{
  char cmd[100]; //stores the command that the user entered
  int job_id; //the job ID of the command, it's the smallest avaiable whole number
  char status[10]; //is it stopped, terminated, running?
  pid_t pid; //The UNIX provided process ID
  struct _child_process *next; //points to the next child process. This is a linked list, after all
} child_process;

//The entire list of background proccesses
typedef struct _list_bg_processes
{
  child_process *head; //head child process node
  child_process *tail; //tail child process node
  int num_bg_processes; //number of background proceses
} list_bg_processes;

list_bg_processes *list; //List of background processes
child_process *fg_child_process; //current foreground processes. Can only be one at a time, unlike bg processes
pid_t pid; //Current pid
char command[100]; //Temporarily stores command that user wants to do, such as ls or wc or cat

void error_exit();

int is_prog_cmd(char *command, char *arg, const char *delim, int child_status);

void insert_bg_process(child_process *new_bg_process, list_bg_processes *list);

child_process * delete_bg_process(pid_t rm_pid, list_bg_processes *list);

void printList(list_bg_processes *list);

//Writes out what the error is
void error_exit()
{
  char buffer[255] = {0};
  sprintf(buffer, "An error occured.\n"); //sends formatted output to a the string buffer
  write(STDOUT_FILENO, buffer, strlen(buffer)); //Integer file descriptor for stdout, where STDOUT_FILENO == fileno(stdout)
  perror("Error is"); //prints the argument as a prefix, then after colon and space, writes the error message corresponding
  //to the integer stored in errno, which is immediately set after unsuccessful library or system call
}

//Quickly check when user enters command if it is a shell-specific command, such as help or quit
int is_prog_cmd(char *command, char *arg, const char *delim, int child_status)
{
  if(strcmp(arg, "help") == 0)
  {
      log_help();
      return 1;
  }

  else if(strcmp(arg, "quit") == 0)
  {
    log_quit();
    //Forces every child to quit, 0 means no option specified
    while(waitpid(-1, &child_status, 0) >= 0);
    //TODO: Then free memory
    exit(0);
  }

  else if(strcmp(arg, "jobs") == 0)
  {
    log_job_number(list->num_bg_processes); //How many processes are there? Use this number as param for log function
    printList(list);
    return 1;
  }
   
  //If there is only fg specified, then we grab the bg process with the highest job ID and move it to fg.
  //This will be the one which has the list's tail pointing to it
  //We can then delete it from the bg list
  else if(strcmp(arg, "fg") == 0)
  {
    if(list->num_bg_processes != 0)
    {
      arg = strtok(NULL, delim);
      if(arg == NULL) //We only have fg
      {
        //Stop the bg job with highest job_id
        strcpy(list->tail->status, "Stopped");
        fg_child_process = delete_bg_process(list->tail->pid, list);
      }

      else
      {  
        int rm_job_id = atoi(arg);
        child_process *temp = list->head;
        while( (temp->job_id != rm_job_id) && temp != NULL)
          temp = temp->next;

        if(temp->job_id != rm_job_id)//Not found
          log_fg_notfound_error(rm_job_id);

        //Stop the bg process with the specified job_id
        strcpy(temp->status, "Stopped");
        fg_child_process = delete_bg_process(temp->pid, list);
      }
    
      kill(fg_child_process->pid, 19); //SIGSTOP

      //In case we can't continue the process
      int check = kill(fg_child_process->pid, 18); //SIGCONT
      if(check < 0)
        log_job_fg_fail(fg_child_process->pid, fg_child_process->cmd);
      else
        strcpy(fg_child_process->status, "Running");
    }

    else
      log_no_bg_error();

    return 1;
  }

  return 0;
}

void handle_sigint(int sig)
{
  if(sig == SIGINT)
  {
    kill(pid, 2);
    log_job_fg_term(pid, fg_child_process->cmd);
    return;
  }
}

void handle_sigtstp(int sig)
{
  kill(pid, 20);

  child_process *new_bg_process = (child_process *)malloc(sizeof(child_process));
  if(new_bg_process == NULL)
    atexit(error_exit);
 
  strcpy(fg_child_process->status, "Stopped");
  memcpy(new_bg_process, fg_child_process, sizeof(child_process));
  log_job_fg_stopped(pid, fg_child_process->cmd);
  insert_bg_process(new_bg_process, list);
  return;
}

/* main */
/* The entry of your shell program */
int main() 
{ 
  //TODO: Add for Ctrl+Z, SIGTSTP, Ctrl+C, SIGINT, and child stops or terminating, SIGCHLD
  struct sigaction sigint;
  sigint.sa_handler = handle_sigint;
  sigint.sa_flags = 0;
  //sigaction(SIGCHLD, &sigint, NULL);
  sigaction(SIGINT, &sigint, NULL);
  
  struct sigaction sigtstp;
  sigtstp.sa_handler = handle_sigtstp;
  sigtstp.sa_flags = 0;
  sigaction(SIGTSTP, &sigtstp, NULL);

  int child_status;
  char cmdline[MAXLINE]; /* Command line */

  list = (list_bg_processes *)malloc(sizeof(list_bg_processes));
  if(list == NULL)
    atexit(error_exit);
  list->head = NULL;
  list->tail = NULL;
  list->num_bg_processes = 0;

  fg_child_process = (child_process *)malloc(sizeof(child_process));
  if(fg_child_process == NULL)
    atexit(error_exit);
  
  fg_child_process -> job_id = 0;
  fg_child_process -> next = NULL;
  //if we need to move to background, then we need to do memcpy to a new bg_child_process node

  while (1)
  {
    fg_child_process -> job_id = 0;
    fg_child_process -> next = NULL;
    strcpy(fg_child_process->cmd, "");
    strcpy(fg_child_process->status, "Stopped");
	  /* Print prompt */
  	log_prompt();

	  /* Read a line */
	  // note: fgets will keep the ending '\'
	  if (fgets(cmdline, MAXLINE, stdin)==NULL)
	  {
	    if (errno == EINTR)
			continue;
	    exit(-1); 
	  }

	  if (feof(stdin))
    {
	    exit(0);
  	}

    int is_background = 0;

    int std_out = dup(STDOUT_FILENO);
    int std_in = dup(STDIN_FILENO);
 
    int fd_out;
    int fd_in;
    int redirect_out = 0;
    int redirect_in = 0;
    int append = 0;
    char *file_out = NULL;
    char *file_in = NULL;
    char command_copy[100];

	  /* Parse command line and evaluate */
    char *arguments[MAXLINE];    
    const char delim[2] = " ";
    char *token = " ";
    
    cmdline[strlen(cmdline)-1] = '\0';
    strcpy(command_copy, cmdline);
		token = strtok(cmdline, delim);
    arguments[0] = token;

    if(is_prog_cmd(command_copy, token, delim, child_status) == 1)
      continue;

    
    else
    {
      int i = 1;
	    while(token != NULL)
      {
        token = strtok(NULL, delim);

        if((token != NULL) && strcmp(token,"&") == 0)
          is_background = 1;

        else if( (token != NULL) && strcmp(token, ">") == 0)
          redirect_out = 1;

        else if( (token != NULL) && strcmp(token, ">>") == 0)
          append = 1;

        else if( (token != NULL) && strcmp(token, "<") == 0)
          redirect_in = 1;

        else if( (file_out == NULL) && ( (redirect_out == 1) || (append == 1) ) )
          file_out = token;

        else if( (file_in == NULL) && (redirect_in == 1))
          file_in = token;
          
        else
        {
          arguments[i] = token;
          i++;
        }
      }
    }

   
    //code for blocking signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, NULL);//SIGCHLD is now blocked
    pid = fork();

    if(pid == -1)
    {
      perror("Error, ");
      continue;
    }

    else if(pid == 0) //child? Child
    {
      setpgid(0,0);
      if(redirect_out == 1)
      {
        fd_out = open(file_out, O_RDWR | O_CREAT | O_TRUNC, 0700);
        if(fd_out == -1)
          log_file_open_error(file_out);
        else
          dup2(fd_out, STDOUT_FILENO);
      }

      if(append == 1)
      {
        fd_out = open(file_out, O_RDWR | O_CREAT | O_APPEND, 0700);
        if(fd_out == -1)
          log_file_open_error(file_out);
        else
          dup2(fd_out, STDOUT_FILENO);
      }

      if(redirect_in == 1)
      {
        fd_in = open(file_in, O_RDWR, 0700);
        if(fd_in == -1)
          log_file_open_error(file_in);
        else
          dup2(fd_in, STDIN_FILENO);
      }

      //unblock signal
      sigprocmask(SIG_UNBLOCK, &set, NULL);
      execv(arguments[0], arguments);
    }

    else
    { 
      setpgid(pid, pid); //In case we have a race condition, add child to different pgid
      if(file_out != NULL)
      {
        dup2(std_out, STDOUT_FILENO);
        close(std_out);
        //close(fd_out);
       //printf("Child terminated normally with exit status %d\n", WEXITSTATUS(child_status));
      }

      if(file_in != NULL)
      {
        dup2(std_in, STDOUT_FILENO);
        close(std_in);
        //close(fd_in);
      }

      if(!is_background)
      {
        //printf("Not a background process\n");
        //fg_child_process -> pid = 0;
        strcpy(fg_child_process->cmd, cmdline);
        strcpy(fg_child_process->status, "Running");
        waitpid(pid, &child_status, 0);
        strcpy(fg_child_process->status, "Stopped");
      }
     
      else
      {
        //code for adding to the list of background jobs using job_id and insert
        child_process *new_bg = (child_process *)malloc(sizeof(child_process));
        new_bg-> next = NULL;
        new_bg-> pid = pid;
        
        strcpy(new_bg-> cmd, command_copy);
        strcpy(new_bg-> status, "Running"); 
        insert_bg_process(new_bg,list);
      }
      //Unblock in parent
      sigprocmask(SIG_UNBLOCK, &set, NULL);
    }
  }
}

void insert_bg_process(child_process *new_bg_process, list_bg_processes *list)
{
  //Inserting at empty list
  if(list->head == NULL)
  {
    list->head = new_bg_process;
    list->tail = new_bg_process;
  }
    
  //Otherwise, we are inserting at the end of the list, based on job_id
  else
  {
    int temp_job_id = list->tail->job_id;
    list->tail->next = new_bg_process;
    list->tail = new_bg_process;
    list->tail->job_id = temp_job_id + 1;
  }
  
  list->num_bg_processes++;
}
/* end main */
  
child_process * delete_bg_process(pid_t rm_pid, list_bg_processes *list)
{
  child_process *curr = list->head;
  child_process *prev = list->head;
  if(list->head == NULL) //empty list
    curr = NULL;

  else if(list->head->pid == rm_pid) //First item is a match!
  {
    //When to free the node?
    list->head = list->head->next;
    if(list->num_bg_processes == 1) //Only one item left in the list
      list->tail = list->head; //Point to NULL
    curr->next = NULL;
  }

  else
  {
    while( (curr != NULL) && (curr->pid != rm_pid) )
    {
      prev = curr;
      curr = curr->next;
    }
    if(curr != NULL)
    {
      prev->next = curr->next;
      curr->next = NULL;
      if(list->tail == curr) //In case the one removed was the last one on the list...
        list->tail = prev;
    }
  }
  int temp = list->num_bg_processes;
  temp--;
  list->num_bg_processes = temp; 
  return curr;
}

void printList(list_bg_processes *list)
{
  child_process *temp = list->head;
  while(temp != NULL)
  {
    log_job_details(temp->job_id, temp->pid, temp->status, temp->cmd);
    temp = temp->next;
  }
}
