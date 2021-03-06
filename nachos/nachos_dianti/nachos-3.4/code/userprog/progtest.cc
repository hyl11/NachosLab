// progtest.cc
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"
#include "directory.h"
//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void StartProcess(char *filename) {
	OpenFile *executable = fileSystem->Open(filename);
	AddrSpace *space;

	if (executable == NULL) {
		printf("Unable to open file %s\n", filename);
		return;
	}
	space = new AddrSpace(executable);
	currentThread->space = space;

	fileSystem->Close(executable);

	space->InitRegisters(); // set the initial register values
	space->RestoreState();  // load page table register

	machine->Run(); // jump to the user progam
	ASSERT(FALSE);  // machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

void StartForkProcess(int func) {

	currentThread->space->InitRegisters(); // set the initial register values
	currentThread->space->RestoreState();  // load page table register
	currentThread->space->setPC(func);

	machine->Run(); // jump to the user progam
	ASSERT(FALSE);  // machine->Run never returns;

}

/*
 * test vm
 * */
void testProg() {
//	char* filename1 = "/home/lihaiyang/Desktop/NachosLab/nachos/nachos_dianti/nachos-3.4/code/test/sort";
//	char* filename2 = "/home/lihaiyang/Desktop/NachosLab/nachos/nachos_dianti/nachos-3.4/code/test/matmult";
//
//	Thread* thread1 = new Thread("sort");
//	thread1->Fork(StartProcess, filename1);
//    Thread* thread2 = new Thread("matmult");
//    thread2->Fork(StartProcess, filename2);

	char* name = "/home/li/thread1";
	Thread* thread = new Thread("thread1");
	thread->Fork(StartProcess, name);
}
// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) {
	readAvail->V();
}
static void WriteDone(int arg) {
	writeDone->V();
}

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void ConsoleTest(char *in, char *out) {
	char ch;

	console = new Console(in, out, ReadAvail, WriteDone, 0);
	readAvail = new Semaphore("read avail", 0);
	writeDone = new Semaphore("write done", 0);

	for (;;) {
		readAvail->P(); // wait for character to arrive
		ch = console->GetChar();
		console->PutChar(ch); // echo it!
		writeDone->P();       // wait for write to finish
		if (ch == 'q')
			return; // if q, quit
	}
}

/*
 shell test
 */

void handleOneOp(char* op) {
	if (strcmp(op, "run") == 0) {
		char prog[20] = { };
		scanf("%s", prog);

		Thread* thread = new Thread("run thread");
		thread->waitingList->Append((void*) currentThread);
		thread->Fork(StartProcess, prog);

		IntStatus oldLevel = interrupt->SetLevel(IntOff);
		currentThread->Sleep();
		interrupt->SetLevel(oldLevel);

		printf("exit code is %d\n", currentThread->exitCode);
	} else if (strcmp(op, "ls") == 0) {
		char dirPath[20] = {};
		scanf("%s", dirPath);

		OpenFile* dirFile = fileSystem->Open(dirPath);
		Directory* dir = new Directory();
		dir->FetchFrom(dirFile);
		dir->List();
	} else if (strcmp(op, "mkdir") == 0) {
		char dirPath[20] = {};
		scanf("%s", dirPath);

		fileSystem->Create(dirPath, 0, FALSE);
	} else if (strcmp(op, "touch") == 0) {
		char filePath[20] = {};
		scanf("%s", filePath);

		fileSystem->Create(filePath, 0, TRUE);
	} else if (strcmp(op, "rm") == 0) {
		char filePath[20] = {};
		scanf("%s", filePath);

		fileSystem->Remove(filePath);
	} else if(strcmp(op, "exit") == 0){
		interrupt->Halt();
	} else if (strcmp(op, "help") == 0) {
		printf("supported operation:\n");
		printf("1. run prog, run the program in file system\n");
		printf("2. ls dir, list the context of the $dir\n");
		printf("3. mkdir dir, create a dir named $dir\n");
		printf("4. rm file, remove the file or dir named $file\n");
		printf("5. touch file, create a file named $file\n");
		printf("6. exit, exit nachos\n");
		printf("notice : all the path should be absoule path\n");
	} else {
		printf("Wrong opration\n");
	}
}

void testShell() {
	OpenFile* pwd = NULL;
	char* pwdPath = "/home/li";
	while (true) {
		printf("root@nachos:%s$ ", pwdPath);
		char op[100] = { };
		scanf("%s", op);
		handleOneOp(op);
	}
}
