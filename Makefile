CXX = g++
CXXFLAGS = -Wall -g

all: myshell

myshell: command.o tokenizer.o
	$(CXX) $(CXXFLAGS) -o myshell command.o tokenizer.o

command.o: command.cc command.h tokenizer.h
	$(CXX) $(CXXFLAGS) -c command.cc

tokenizer.o: tokenizer.cc tokenizer.h
	$(CXX) $(CXXFLAGS) -c tokenizer.cc

clean:
	rm -f *.o myshell shell.log
