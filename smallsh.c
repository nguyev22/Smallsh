#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdbool.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
char *token[MAX_WORDS];
size_t token_c = 0;
size_t wordsplit(char const *line);
char * expand(char const *word);
char *input_file = NULL;
char *output_file = NULL;
char *append_file = NULL;
char *param_str = NULL;
char bg_pid[MAX_WORDS] = "";
int word_len = 0;
//int bground = 0;
int fground = 0;
int fg_pid = 0;
int fg_status = 0;
int read_file;
int write_file;
int appending;
int child_status = 0;
int last_bg_pid = 0;
int counter = 0;
bool bground = false;
//pid_t child_pid;
//int status;

//char fg_status[21];
//sprintf(fg_status, "%d", fg_pid);

void background_process() {
    pid_t child_pid;
    int status = 0;
    pid_t pid_group = getpgrp();
    
    while ((child_pid = waitpid(-pid_group, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0){
          if (WIFEXITED(status)){
            // If exited: “Child process %jd done. Exit status %d.\n”, <pid>, <exit status>
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_pid, WEXITSTATUS(status));
            //fflush(stderr);
            exit(0);
          }
         
          else if (WIFSIGNALED(status)) {
            // If signaled: “Child process %jd done. Signaled %d.\n”, <pid>, <signal number>
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_pid, WTERMSIG(status));
            //fflush(stderr);
            exit(0);
            
          }
          else if (WIFSTOPPED(status)){
            // If a child process is stopped, smallsh shall send it the SIGCONT signal and print
            // the following message to stderr: “Child process %jd stopped. Continuing.\n”, <pid> (See KILL(2))
            kill(child_pid, SIGCONT); 
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_pid);
            exit(0);
            //kill(child_pid, SIGCONT);
            //fflush(stderr);
            
        }
      }
}

void handle_SIGINT (int signo){}
            

void parser(char **words, int nwords){



  for (size_t i=0; i < nwords; i++){

    // Handle >, where if it is True
    if (strcmp(words[i], ">") == 0){
      i++; // skip over >
      
      if (strcmp(words[i], "") == 0) {
        fprintf(stderr, "Error: nothing after operator >\n");
        exit(1);
      }

      output_file = words[i];

      if (output_file != NULL) {
        write_file = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (write_file < 0){
          perror("Error: Write file");
          exit(1);
        }

        if (dup2(write_file, 1) == -1){
          perror("Error: Write file dup2()");
          exit(1);
        }
        fcntl(write_file, F_SETFD, FD_CLOEXEC);
      }
      

    // Handle >> if True
    } else if (strcmp(words[i], ">>") == 0){
        i++; //skip over >>

        if (strcmp(words[i], "") == 0) {
          fprintf(stderr, "Error: nothing after operator >>\n");
          exit(1);
        }

      append_file = words[i];

      if (append_file != NULL){
        appending = open(append_file, O_WRONLY | O_APPEND | O_CREAT, 0777);
        if (appending < 0){
          perror("Error: Append file");
          exit(1);
        }

        if (dup2(appending, 1) == -1){
          perror("Error: Append file dup2()");
          exit(1);
        }
        fcntl(appending, F_SETFD, FD_CLOEXEC);
      }
      
      
    // Handle <, if True
    } else if (strcmp(words[i], "<") == 0){
      i++; // skip over <

      if (strcmp(words[i], "") == 0) {
        fprintf(stderr, "Error: nothing after operator <\n");
        exit(1);
      }

      input_file = words[i];

      if (input_file != NULL) {
        read_file = open(input_file, O_RDONLY);
        if (read_file < 0){
          perror("Error: Read file");
          exit(1);
        }
        if (dup2(read_file, 0) == -1){
          perror("Error: Read file dup2()");
          exit(1);
        }
        fcntl(read_file, F_SETFD, FD_CLOEXEC);
      }
      


    // Handle &
    } else if (strcmp(words[i], "&") == 0){
        words[i] = NULL;
        bground = true;
         // Ends loop since at end due to "&" keyword
    } else { // Adds word to array token
        token[token_c++] = words[i];
    }
  }

  // Terminate array with Null pointer
  token[token_c] = NULL;


}

int main(int argc, char *argv[])
{
    // Takes 2 args, [0] = ./smallsh.c, [1] = input_fn
    FILE *input = stdin;
    char *input_fn = "(stdin)";
    if (argc == 2) {
        input_fn = argv[1];
        input = fopen(input_fn, "re");
        if (!input) err(1, "%s", input_fn);
    } else if (argc > 2) {
        errx(1, "too many arguments");
    }

    char *line = NULL;
    size_t n = 0;

    struct sigaction oldINT = {0};
    struct sigaction oldTSTP = {0};

    struct sigaction SIGINT_action = {0};
    struct sigaction SIGTSTP_action = {0};

        //Ctrl + C
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

        
    // CTLR + Z
    SIGTSTP_action.sa_handler = SIG_IGN;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;





    for (;;) {
prompt:;
        // CTRL + C

        sigaction(SIGINT, &SIGINT_action, &oldINT);

        // CTLR + Z

        sigaction(SIGTSTP, &SIGTSTP_action, &oldTSTP);

        background_process();


        if (line != NULL){
          line = NULL;

        }

        for (int i = 0; words[i] != NULL; i++){
          words[i] = NULL; 
        }

      
        bground = false;



        if (input == stdin) {
          char *interact = getenv("PS1");
          if (interact) {
            fprintf(stderr, "%s", interact);
          } else {
              fprintf(stderr, "");
          }
        }

        sigaction(SIGINT, &SIGINT_action, NULL);

        // getline(3) man pages, Acquired line_len
        ssize_t line_len = getline(&line, &n, input);

        if (feof(input)){
          clearerr(input);

          fclose(input);
          exit(fg_status);
        }

        sigaction(SIGINT, &SIGTSTP_action, NULL);
        if (line_len < 0) {
            // Check for interrupted read
            if (errno == EINTR) {
                fprintf(stderr, "\n");
                clearerr(input);
                errno = 0;
                goto prompt;
            
            } else if (errno == 0){
              clearerr(input);
              exit(0);
            }
            else {
              err(1, "%s", input_fn);
            }
        }



        size_t nwords = wordsplit(line);
        for (int i = 0; line[i] != '\0'; i++){
          if (line[i] == '&') {
            bground = true;
          }
        }



        for (size_t i = 0; i < nwords; ++i) {

            char *exp_word = expand(words[i]);
            free(words[i]);
            words[i] = exp_word;
     
        }
        

        // -------  Parse  -------
        // Iterate over wordlist, copy pointers to words into list of child arguments
        // If hit operator, redirect and continue
        // Skip over operators & operands when copying word pointers
        // Sets flags for input, output, append, bground
        


        // ------- BUILT IN ------
        // If no command word present, returns to step 1 & print new prompt message
        if (words[0] == NULL) {
          //printf(": ");
          fflush(stdout);
          goto prompt;
        }


        // If word is 'exit' or 'cd'        
        //if (strcmp(words[0], "exit")) || strcmp(words[0], "cd") == 0){
        else if (strcmp(words[0], "exit") == 0){
            char status[21] = {'\0'};

            // Checks for more than 1 arg, then
            // Checks if arg not provided, then
            // Checks if provided, see if it's an int or not
            // Meets all conditions, then prints exit and exits current status
            
            
            
            
            // Takes one Argument, error if more than 1 provided             
            if (nwords > 2){
              sprintf(status, "%d", fg_status);
              fprintf(stderr, "Error: More than 1 arg provided (exit)\n");
              fflush(stdout);
              exit(0);
              //exit(atoi(status));
            } 

            // If arg not provided, exit status of last fg cmd
            else if (words[1] == NULL && nwords == 1){
              sprintf(status, "%d", fg_status); // turns fg_status to str
         
              fflush(stdout);
              exit(0);                                                            
         
              
            }

            // Arg provided not an int
            else if (!isdigit(*words[1]) && (words[1] != NULL)) {
              fprintf(stderr, "Error: Invalid exit argument\n");


            }
            // If arg provided
            
            else {

              fg_status = atoi(words[1]);

              exit(fg_status);
            
            }


          }
          
          // Takes 1 Arg
          // If not provided, implied to be expansion of HOME environment variable
          // i.e. cd is equivalent to cd $HOME
          else if (strcmp(words[0], "cd") == 0){

            // If more than 1 arg
            if (nwords > 2) {
              fprintf(stderr, "Error: More than 1 arg provided (cd)\n");
              goto prompt;
            }

            // If no args provided, set argument to HOME
            else if (nwords < 2) { //if (words[1] not provided)
              words[1] = getenv("HOME");
              chdir(words[1]);
              goto prompt;
            }
            
            // Change directory to words[1], where words[1] == HOME or Argument provided
            //if (words[1] != NULL){
            else {  
              chdir(words[1]);
              if (chdir(words[1]) != 0){
                fprintf(stderr, "Error: Can't change directory\n");
                goto prompt;
              }
              
              goto prompt;
            } 

            
          }
        else {

        
        
       
        // --------- NON-BUILT-IN CMD ---------

        // I/O redirection inside the child process after calling fork
        // If cmd name does not include /, cmd searched for in system's Path execvp(3)
     
        
        pid_t spawnPID = -5;
        spawnPID = fork();
        //printf("child spawned");
        
                
        switch (spawnPID){
          
        // If call to fork falls, return error
        // Use perror to print error msg to stderr
        //if (spawnPID == -1){
        case -1:
          perror("fork() failed\n");
          exit(1);
          break;
            

        /* When error, child prints error msg to stderr and exit w/ non-zero
         * ALl signals reset when smallsh invoked -- See oldact in SIGACTION(2)
         * If operator found, word following interpreted as path to file
         * No words following/redirection fails, return error
         * <  - Open file for reading on stdin
         * >  - Open file for writing on stdout. Don't exit, create w permissions 0777. File's truncated.
         * >> - Open file for appending on stdout. Don't exit, created w/ permissions 0777.
         * Remove redirection ops, filename args, '&" prior to executing non-built-in
         * Child process fails, return error
         */

        case 0:

            sigaction(SIGINT, &oldINT, NULL);
            sigaction(SIGTSTP, &oldTSTP, NULL);

        // -------  Parse  -------
        // Iterate over wordlist, copy pointers to words into list of child arguments
        // If hit operator, redirect and continue
        // Skip over operators & operands when copying word pointers
        // Sets flags for input, output, append, bground
        
          parser(words, nwords);
                       
          execvp(token[0], token);

          exit(0);
          break;
            

          
        /* perform blocking wait (WAITPID(2)) on fg child process
         * $? set to exit status of waited-for command
         * If waited-for cmd is terminated by signal, $? shall be set to value 128 + [n]
         * [n] is num of signal causing child to terminate
        
         * If while waiting on fg child process & it's stopped, smallsh sends to SIGCONT signal and prints msg (kill(2))
         * Shell $? updated to pid of child process, as if had been bg cmd.
         * Smallsh no longer block wait on process & continues to run in bg
         */
         //} else 
        default:
          
           // If background
         if (bground == true) {

           last_bg_pid = spawnPID;
           goto prompt; 
           

           }

            // If foreground, perform blocking WAITPID(2)
            else {

              if (bground == false) {
             
                // Wait for child
             
             spawnPID = waitpid(spawnPID, &child_status, WUNTRACED);
          

             if (spawnPID == -1){
               perror("Error: spawnPID wait error\n");
             }


             if (WIFEXITED(child_status)) {

               fg_status = WEXITSTATUS(child_status);
           

              } 

            // Child process terminating by signal
            // Don't run WEXITSTATUS since has no exit status
            // Set $? to child_status

             else if (WIFSIGNALED(child_status)) {
               fg_status = 128 + WTERMSIG(child_status);

             }
       
             
          
             // If stopped
              else if (WIFSTOPPED(child_status)) {
                kill(spawnPID, SIGCONT);
                fprintf(stderr, "Child process %d stopped. Continuing.\n", (int) spawnPID);
                
                last_bg_pid = spawnPID;
                bground = true;
                
                
              }
               else {
                //
              last_bg_pid = spawnPID;
              bground = true;
              
     
               }
             break;

           }
        }
        }
        }
    }
    free(line);
    fclose(input);
    return 0;

}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
    size_t wlen = 0;
    size_t wind = 0;

    char const *c = line;
    for (;*c && isspace(*c); ++c) ; /* discard leading space */

    for (; *c;) {
        if (wind == MAX_WORDS) break;
        /* read a word */
        if (*c == '#') break;


        for (;*c && !isspace(*c); ++c) {
            if (*c == '\\') ++c;
            void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
            if (!tmp) err(1, "realloc");
            words[wind] = tmp;
            words[wind][wlen++] = *c;
            words[wind][wlen] = '\0';
        }
        ++wind;
        wlen = 0;
        for (;*c && isspace(*c); ++c);
    }
    return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointertodos to the start and end of the parameter
 * token.
 */

char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}


/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
    static size_t base_len = 0;
    static char *base = 0;

    if (!start) {
        /* Reset; new base string, return old one */
        char *ret = base;
        base = NULL;
        base_len = 0;
        return ret;
    }
    /* Append [start, end) to base string
     * If end is NULL, append whole start string to base string.
     * Returns a newly allocated string that the caller must free.
     */
    size_t n = end ? end - start : strlen(start);
    size_t newsize = sizeof *base *(base_len + n + 1);
    void *tmp = realloc(base, newsize);
    if (!tmp) err(1, "realloc");
    base = tmp;
    memcpy(base + base_len, start, n);
    base_len += n;
    base[base_len] = '\0';

    return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
    
    char bg_pid_str[21] = {'\0'};

    // If pid is 0, default to ""
    if (last_bg_pid == 0){
      strcpy(bg_pid_str, "");
      //sprintf(bg_pid_str, "%s", bg_pid);
    } else {
      sprintf(bg_pid_str, "%s", bg_pid);
    } 
    
    char const *pos = word;
    char const *start, *end;
    char c = param_scan(pos, &start, &end);
    free(build_str(NULL, NULL));
    build_str(pos, start);
    while (c) {
        if (c == '!') {
          if (last_bg_pid == 0){
            build_str(bg_pid_str, NULL);
          } else {
            sprintf(bg_pid_str, "%d", last_bg_pid);
            build_str(bg_pid_str, NULL);
          }
        }
        else if (c == '$') {
          char pid_str[21];
          sprintf(pid_str, "%d", getpid());
          build_str(pid_str, NULL);
        }
        else if (c == '?') {
          char status_str[21];
          sprintf(status_str, "%d", fg_status);
          build_str(status_str, NULL);
        }
        else if (c == '{') {
            //new_str = getenv(build_str(start + 2, end - 1));
            char new_str[100] = {'\0'};
          
            strncpy(new_str, start + 2, (end - 1) - (start + 2));
            char *env_result = getenv(new_str);
            
            if (env_result == NULL){
              build_str("", NULL);
            } else{
                build_str(env_result, NULL);
            }


        }
        pos = end;
        c = param_scan(pos, &start, &end);
        build_str(pos, start);
    }
    return build_str(start, NULL);
}





