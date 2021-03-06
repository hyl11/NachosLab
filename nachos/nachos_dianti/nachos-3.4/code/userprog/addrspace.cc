// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader(NoffHeader *noffH) {
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}
AddrSpace::AddrSpace(Thread* toCopy) {
	numPages = toCopy->space->numPages;
	pageTable = new TranslationEntry[numPages];
	for (int i = 0; i < numPages; i++) {
		if (toCopy->space->pageTable[i].codeData == TRUE) {
			memcpy(&pageTable[i], &(toCopy->space->pageTable[i]),
					sizeof(TranslationEntry));
		}
		pageTable[i].virtualPage = i;
	}
}
//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable) {
	NoffHeader noffH;
	unsigned int i, size;

	executable->ReadAt((char *) &noffH, sizeof(noffH), 0);
	if ((noffH.noffMagic != NOFFMAGIC)
			&& (WordToHost(noffH.noffMagic) == NOFFMAGIC))
		SwapHeader(&noffH);
	ASSERT(noffH.noffMagic == NOFFMAGIC);

	// how big is address space?
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
			+ UserStackSize; // we need to increase the size
							 // to leave room for the stack
	numPages = divRoundUp(size, PageSize);
	size = numPages * PageSize;

	ASSERT(numPages <= NumPhysPages); // check we're not trying
									  // to run anything too big --
									  // at least until we have
									  // virtual memory

	DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages,
			size);
	// first, set up the translation
	pageTable = new TranslationEntry[numPages];
	for (i = 0; i < numPages; i++) {
		pageTable[i].virtualPage = i;
		pageTable[i].valid = FALSE;
		pageTable[i].use = FALSE;
		pageTable[i].dirty = FALSE;
		pageTable[i].readOnly = FALSE;
		pageTable[i].onDisk = FALSE;           // 不在磁盘上有缓存
		pageTable[i].codeData = FALSE;
	}

	// 不清零主存
	// zero out the entire address space, to zero the unitialized data segment
	// and the stack segment
	//   bzero(machine->mainMemory, size);

	if (noffH.code.size > 0) {
		DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
				noffH.code.virtualAddr, noffH.code.size);
		/* 将其加载至虚拟磁盘 */
		unsigned codeVirAddr = (unsigned) noffH.code.virtualAddr;
		unsigned int offset = codeVirAddr % PageSize;  //偏移
		unsigned int vpn = codeVirAddr / PageSize;  //第一个虚拟页号
		int codePages = divRoundUp(noffH.code.size + offset, PageSize); //总计需要的页面数量
		int totalCodeSize = codePages * PageSize;    // 总计的代码所需页面大小
		char* allDisk = new char[totalCodeSize];    //分配一个大块磁盘
		bzero(allDisk, totalCodeSize);
		executable->ReadAt(allDisk + offset, noffH.code.size,
				noffH.code.inFileAddr);   //读取所有代码到磁盘上
		for (int i = 0; i < codePages; i++) {     //将磁盘分割为小块映射到内存页

			char* diskPage = allDisk + PageSize * i;
			pageTable[vpn + i].onDisk = true;
			pageTable[vpn + i].diskAddr = diskPage;
			pageTable[vpn + i].codeData = TRUE;
			machine->disk->Append((void*) diskPage);  //加入虚拟磁
		}
	}
	if (noffH.initData.size > 0) {
		DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
				noffH.initData.virtualAddr, noffH.initData.size);
		/* 将其加载至虚拟磁盘 */
		unsigned dataVirAddr = (unsigned) noffH.initData.virtualAddr;
		unsigned int offset = dataVirAddr % PageSize;  //偏移
		unsigned int vpn = dataVirAddr / PageSize;  //第一个虚拟页号
		int dataPages = divRoundUp(noffH.initData.size + offset, PageSize); //总计需要的页面数量
		int totalDataSize = dataPages * PageSize;
		char* allDisk = new char[totalDataSize];    //分配一个大块磁盘
		bzero(allDisk, totalDataSize);
		executable->ReadAt(allDisk + offset, noffH.initData.size,
				noffH.initData.inFileAddr);   //读取所有数据到磁盘上
		for (int i = 0; i < dataPages; i++) {   //  分割磁盘块
			char* diskPage = allDisk + PageSize * i;
			if (pageTable[vpn + i].onDisk == FALSE) {
				pageTable[vpn + i].onDisk = true;
				pageTable[vpn + i].diskAddr = diskPage;
				pageTable[vpn + i].codeData = TRUE;
				machine->disk->Append((void*) diskPage);  //加入虚拟磁盘
			} else {    //拷贝到原来已经分配好的磁盘块
				for (int j = 0; j < PageSize; j++) {
					if (diskPage[j] != 0) {
						((char*) (pageTable[vpn + i].diskAddr))[j] =
								diskPage[j];
					}
				}
			}

		}
	}
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace() {
	delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters() {
	int i;

	for (i = 0; i < NumTotalRegs; i++)
		machine->WriteRegister(i, 0);

	// Initial program counter -- must be location of "Start"
	machine->WriteRegister(PCReg, 0);

	// Need to also tell MIPS where next instruction is, because
	// of branch delay possibility
	machine->WriteRegister(NextPCReg, 4);

	// Set the stack register to the end of the address space, where we
	// allocated the stack; but subtract off a bit, to make sure we don't
	// accidentally reference off the end!
	machine->WriteRegister(StackReg, numPages * PageSize - 16);
	DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

void AddrSpace::setPC(int func) {
	machine->WriteRegister(PCReg, func);
	machine->WriteRegister(NextPCReg, func + 4);
}
//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() {
	machine->pageTable = pageTable;
	machine->pageTableSize = numPages;
}
