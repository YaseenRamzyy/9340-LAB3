
#ifndef command_h // header guard
#define command_h

struct SimpleCommand // only one command
{
	int _numberOfAvailableArguments; // number of arguments that are allocated
	int _numberOfArguments; // no of arguments that are used
	char **_arguments; // pointer to an array of cahr pointers (array of strings)

	SimpleCommand(); // constructor
	void insertArgument(char *argument);
};

struct Command
{
	int _numberOfAvailableSimpleCommands; // nfs el kalam lei fo2
	int _numberOfSimpleCommands;
	SimpleCommand **_simpleCommands;
	char *_outFile; // pointer to the output file name
	char *_inputFile;
	char *_errFile;
	int  _out_error; // flag for redirect and error file name
	int _background;
	int _append;

	void prompt();
	void print();
	void execute();
	void clear();

	Command();
	void insertSimpleCommand(SimpleCommand *simpleCommand);

	static Command _currentCommand; // static variable 3l4an current command
	static SimpleCommand *_currentSimpleCommand;
};

#endif
