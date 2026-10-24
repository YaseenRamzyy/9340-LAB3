/*
 * Mini shell - command.cc
 *
 * Parts done:
 *   1. parse()    : tokens  -> Command table
 *   2. execute()  : fork / execvp / dup2 / pipe / background
 *   3. Ctrl-C ignored, exit, cd, log file (SIGCHLD)
 *   Bonus: wildcards (* and ?)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

// Set to false if you don't want the command table printed before each run
// (Part 1 of the lab asks you to print it, so it is on by default).
const bool SHOW_TABLE = true;

/* ------------------------------------------------------------------ */
/*                         Helper functions                           */
/* ------------------------------------------------------------------ */

//checks if the token is a command or argument
static bool isWord(Token &t)
{
    return t.type == TOKEN_COMMAND || t.type == TOKEN_ARGUMENT;
}

//print an error and throw away the partially constructed command
static void syntaxError(Command &cmd, const std::string &message)
{
    printf("Syntax error: %s\n", message.c_str());
    cmd.clear();
    free(cmd._simpleCommands);
}

//the tokenizer splits on spaces, so  "Hello, World!"  arrives as two tokens
//this function glues them back together and removes the quotes
//'i' is moved forward to the last token that was used
static std::string readWord(std::vector<Token> &tokens, size_t &i, bool &quoted)
{
    std::string word = tokens[i].value;
    quoted = false;

    char q = word[0];
    if (q == '"' || q == '\'')
    {
        quoted = true;

        // keep adding tokens until we find the closing quote
        while (!(word.size() >= 2 && word.back() == q) &&
               tokens[i + 1].type != TOKEN_EOF)
        {
            i++;
            word += " " + tokens[i].value;
        }

        // remove the quotes
        if (word.size() >= 2 && word.back() == q)
            word = word.substr(1, word.size() - 2);
        else
            word = word.substr(1); // closing quote is missing
    }
    return word;
}

// Add a word to a simple command. (Bonus: expands * and ? using glob)
static void addWord(SimpleCommand *simple, const std::string &word, bool quoted)
{
    if (!quoted && word.find_first_of("*?") != std::string::npos)
    {
        glob_t g;
        if (glob(word.c_str(), 0, NULL, &g) == 0) //glob finds filenames matching the pattern
        {
            // every matching file name becomes one argument
            for (size_t k = 0; k < g.gl_pathc; k++)
                simple->insertArgument(strdup(g.gl_pathv[k]));
            globfree(&g);
            return;
        }
        globfree(&g);
        // nothing matched -> keep the word as it is
    }
    simple->insertArgument(strdup(word.c_str()));
}

//store a file name in a Command field (for redirection) 
static void setFile(char *&slot, const std::string &name)
{
    if (slot)
        free(slot); //free the old one if exists
    slot = strdup(name.c_str());
}

/* ------------------------------------------------------------------ */
/*                              PARSE                                 */
/* ------------------------------------------------------------------ */

void parse(std::vector<Token> &tokens)
{
    //empty line->nothing to do
    if (tokens[0].type == TOKEN_EOF)
        return;

    Command cmd;
    SimpleCommand *simple = new SimpleCommand();
    cmd.insertSimpleCommand(simple);

    for (size_t i = 0; tokens[i].type != TOKEN_EOF; i++)
    {
        TokenType type = tokens[i].type; //type of the token

        //check if command name or argument
        if (type == TOKEN_COMMAND || type == TOKEN_ARGUMENT)
        {
            bool quoted;
            std::string word = readWord(tokens, i, quoted);
            addWord(simple, word, quoted);
        }
        //pipe->start a new simple command
        else if (type == TOKEN_PIPE)
        {
            if (simple->_numberOfArguments == 0)
            {
                syntaxError(cmd, "missing command before '|'");
                return;
            }

            simple = new SimpleCommand();
            cmd.insertSimpleCommand(simple);
        }

        //background->only allowed at the end of the line
        else if (type == TOKEN_BACKGROUND)
        {
            if (tokens[i + 1].type != TOKEN_EOF)
            {
                syntaxError(cmd, "'&' must be the last thing in the command");
                return;
            }
            cmd._background = 1;
        }

        // redirections-> >  >>  <  2>  >>&
        else
        {
            // lazem el file name yeegy b3d el operator 
            if (!isWord(tokens[i + 1]))
            {
                syntaxError(cmd, "missing file name after '" + tokens[i].value + "'");
                return;
            }

            i++; // move to the file name
            bool quoted;
            std::string file = readWord(tokens, i, quoted);

            if (type == TOKEN_REDIRECT) // >
            {
                setFile(cmd._outFile, file);
                cmd._append = 0;
            }

            else if (type == TOKEN_APPEND) // >>
            {
                setFile(cmd._outFile, file);
                cmd._append = 1;
            }

            else if (type == TOKEN_INPUT) // <
            {
                setFile(cmd._inputFile, file);
            }

            else if (type == TOKEN_ERROR) // 2>
            {
                setFile(cmd._errFile, file);
            }

            else if (type == TOKEN_REDIRECT_AND_ERROR) // >>&  (stdout + stderr, append)
            {
                setFile(cmd._outFile, file);
                setFile(cmd._errFile, file);
                cmd._append = 1;
                cmd._out_error = 1;
            }
        }
    }

    // the last simple command must not be empty (example:  "ls |")
    if (simple->_numberOfArguments == 0)
    {
        syntaxError(cmd, "missing command");
        return;
    }

    cmd.execute();
    free(cmd._simpleCommands);
}

/* ------------------------------------------------------------------ */
/*                  SimpleCommand / Command basics                    */
/* ------------------------------------------------------------------ */

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

//insert argumne tfunction implementation
void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1) // lw mafeesh space kefaya
    {
        _numberOfAvailableArguments *= 2; // double it w realloc
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument; // store the arg
    _arguments[_numberOfArguments + 1] = NULL; // 3l4an execvp needs a NULL at the end 

    _numberOfArguments++;
}

Command::Command()
{
    // (bug fix: the original code used _numberOfSimpleCommands before setting it)
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0; // da no of simple commands in the command
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _outFile = 0; // da output filename
    _inputFile = 0;
    _errFile = 0; // da error output filename
    _out_error = 0; // lw feeh output w error in the same file
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2; // bardo double w realloc
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand; // insert it

    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]); // all the args in the simple command
        }

        free(_simpleCommands[i]->_arguments); // free the arg array
        delete _simpleCommands[i]; // delete the simplecommand finally
    }

    if (_outFile) // lw feeh output file
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default", _background ? "YES" : "NO");
    printf("\n\n");
}

// write one line in shell.log for a finished child to reap it
static void logChild(pid_t pid)
{
    FILE *log = fopen("shell.log", "a"); // create if it doesn't exist
    if (log)
    {
        fprintf(log, "Child process %d terminated\n", pid);
        fclose(log);
    }
}

/* ------------------------------------------------------------------ */
/*                             EXECUTE                                */
/* ------------------------------------------------------------------ */

void Command::execute()
{
    if (SHOW_TABLE)
        print();

    int n = _numberOfSimpleCommands;

    // ---- built-in commands (run by the shell itself, NO fork) ----
    if (n == 1)
    {
        char **args = _simpleCommands[0]->_arguments;

        if (strcmp(args[0], "exit") == 0) // lw exit
        {
            printf("Good bye!!\n");
            exit(0);
        }

        if (strcmp(args[0], "cd") == 0) // lw cd
        {
            // lw no argument -> go to the home directory
            const char *dir = args[1] ? args[1] : getenv("HOME");
            if (dir == NULL || chdir(dir) != 0)
                perror("cd");
            clear();
            return;
        } // note that if we used fork in cd case the child will change the directories not the shell
    }

    // ---- normal commands: one child process for each simple command ----
    int prevRead = -1; // read end of the previous pipe (-1 = none)
    pid_t lastPid = -1;

    for (int i = 0; i < n; i++)
    {
        int fds[2]; // fds[0] = read end, fds[1] = write end

        // if this is not the last command, make a pipe to the next one
        if (i < n - 1)
        {
            if (pipe(fds) < 0)
            {
                perror("pipe");
                break;
            }
        }

        fflush(stdout); // empty the buffer so the child doesn't print it again
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            break;
        }

        if (pid == 0)
        {
            // ================= CHILD =================
            signal(SIGINT, SIG_DFL); // children should react to Ctrl-C

            // --- input ---
            if (i > 0)
            {
                dup2(prevRead, 0); // read from the previous command
            }
            else if (_inputFile)
            {
                int fd = open(_inputFile, O_RDONLY);
                if (fd < 0)
                {
                    perror(_inputFile); // missing file error
                    _exit(1);
                }
                dup2(fd, 0);
                close(fd);
            }

            // --- output ---
            if (i < n - 1)
            {
                dup2(fds[1], 1); // write into the pipe
            }
            else if (_outFile)
            {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int fd = open(_outFile, flags, 0644);
                if (fd < 0)
                {
                    perror(_outFile);
                    _exit(1);
                }
                dup2(fd, 1);
                if (_out_error)
                    dup2(fd, 2); // >>&  : errors go to the same file
                close(fd);
            }

            // --- error (2>) --- only for the last command
            if (i == n - 1 && _errFile && !_out_error)
            {
                int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0)
                {
                    perror(_errFile);
                    _exit(1);
                }
                dup2(fd, 2);
                close(fd);
            }

            // close the pipe ends we don't need any more
            if (prevRead != -1)
                close(prevRead);
            if (i < n - 1)
            {
                close(fds[0]);
                close(fds[1]);
            }

            // run the program
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);

            // we only get here if execvp failed
            fprintf(stderr, "%s: command not found\n", _simpleCommands[i]->_arguments[0]);
            _exit(1);
        }

        // ================= PARENT =================
        if (prevRead != -1)
            close(prevRead); // done with the old pipe

        if (i < n - 1)
        {
            close(fds[1]);      // parent never writes to the pipe
            prevRead = fds[0];  // next command will read from here
        }

        lastPid = pid;
    }

    // ---- wait for the last command (unless it is a background job) ----
    if (_background)
    {
        printf("[%d]\n", lastPid);
    }
    else if (lastPid > 0)
    {
        // the SIGCHLD handler may interrupt waitpid, so try again if needed
        pid_t r;
        while ((r = waitpid(lastPid, NULL, 0)) == -1 && errno == EINTR)
            ;
        if (r == lastPid)
            logChild(lastPid); // we collected it ourselves, so log it here
    }

    clear();
}

/* ------------------------------------------------------------------ */
/*                      Prompt loop and main                          */
/* ------------------------------------------------------------------ */

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);

        // Ctrl-D (end of input) closes the shell
        if (!std::getline(std::cin, input))
        {
            printf("\n");
            break;
        }

        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

// Called by the system whenever a child process ends.
// It collects the finished children (so no zombies) and logs each one.
// (If the shell's own waitpid collected a child first, it logs it itself.)
void childHandler(int sig)
{
    int savedErrno = errno;
    int status;
    pid_t pid;

    // WNOHANG = don't block, loop in case several children ended together
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        logChild(pid);
    }

    errno = savedErrno;
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    signal(SIGINT, SIG_IGN);        // ignore Ctrl-C
    signal(SIGCHLD, childHandler);  // log every finished child

    Command::_currentCommand.prompt();
    return 0;
}
