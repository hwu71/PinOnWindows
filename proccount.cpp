/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
//
// This tool counts the number of times a routine is executed and
// the number of instructions executed in a routine
//
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include "pin.H"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
	"o", "proccount.csv", "specify file name");

//ofstream outFile;
//PIN_LOCK lock;

// Holds instruction count for a single procedure
typedef struct RtnCount
{
	string _name;
	string _image;
	string _secName;
	UINT64 _secSize;
	ADDRINT _address;
	ADDRINT _imgLowAddress;
	ADDRINT _offset;
	RTN _rtn;
	UINT64 _rtnCount;
	UINT64 _rtnSize;
	UINT64 _icount;
	UINT64 _pid;
	struct RtnCount* _next;
} RTN_COUNT;

// Linked list of instruction counts for each routine
RTN_COUNT* RtnList = 0;

// This function is called before every instruction is executed
VOID docount(UINT64* counter)
{
	(*counter)++;
}

const char* StripPath(const char* path)
{
	const char* file = strrchr(path, '\\');
	if (file)
		return file + 1;
	else
		return path;
}

// Function to delete the entire linked list
void deleteList(RTN_COUNT** head_ref) {

	// Deref head_ref to get the real head_ref
	RTN_COUNT* current = *head_ref;
	RTN_COUNT* next;

	while (current != NULL) {
		next = current->_next;
		delete(current);
		current = next;
	}
	// Deref head_ref to affect the real head back in the caller
	*head_ref = NULL;
	//cout << "Reset the Linked List." << endl;

}
// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID* v)
{

	// Allocate a counter for this routine
	RTN_COUNT* rc = new RTN_COUNT;

	// The RTN goes away when the image is unloaded, so save it now
	// because we need it in the fini
	rc->_name = RTN_Name(rtn);
	rc->_image = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
	rc->_secName = SEC_Name(RTN_Sec(rtn));
	rc->_secSize = SEC_Size(RTN_Sec(rtn));
	rc->_address = RTN_Address(rtn);
	rc->_imgLowAddress = IMG_LowAddress(SEC_Img(RTN_Sec(rtn)));
	rc->_offset = rc->_address - rc->_imgLowAddress;
	rc->_rtnSize = RTN_Size(rtn);
	rc->_rtnCount = 0;
	rc->_icount = 0;
	rc->_pid = getpid();

	//PIN_GetLock(&lock,getpid());
	// Add to list of routines
	rc->_next = RtnList;
	RtnList = rc;
	//cout << "New entry: " << RtnList->_name << endl;
	//PIN_ReleaseLock(&lock);

	RTN_Open(rtn);

	// Insert a call at the entry point of a routine to increment the call count
	RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_rtnCount), IARG_END);

	// For each instruction of the routine
	for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
	{
		// Insert a call to docount to increment the instruction counter for this RTN
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_icount), IARG_END);
	}

	RTN_Close(rtn);
}

// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID* v)
{
	ofstream outFile;
	int j;
	char fileName[20];
	j = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());
	j += sprintf(fileName + j, "%ld", (long)getpid());
	j += sprintf(fileName + j, "%s", ".csv");
	//cout << "FileName: " << fileName << endl;
	outFile.open(fileName);

	//fprintf(stdout, "In Fini: Linked List Head: %p\n", RtnList);
	//cout << "In Fini: Linked List Head:" << RtnList->_name << endl;
	outFile << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Size" << ","
		<< "Instructions" << endl;

	RTN_COUNT* rc = RtnList;
	RTN_COUNT* next;
	//for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
	while(rc)
	{
		if (rc->_icount > 0) {
			outFile << rc->_name << ","
				<< rc->_image << ","
				<< rc->_secName << ","
				<< rc->_secSize << ","
				<< hex << rc->_offset << dec << ","
				<< rc->_rtnCount << ","
				<< rc->_pid << ","
				<< rc->_rtnSize << ","
				<< rc->_icount << endl;
		}
		next = rc->_next;
		delete(rc);
		rc = next;
	}
	//RtnList = NULL;
	outFile.close();
	//deleteList(&RtnList);
}


BOOL FollowChild(CHILD_PROCESS childProcess, VOID* userData) {

	// Initialize symbol table code, needed for rtn instrumentation
	//PIN_InitSymbols();

	// Reset the Linked List
	//deleteList(&RtnList);
	//cout << "In Child: Linked List Head:" << RtnList << endl;

	/*int j;
	char fileName[20];
	j = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());
	j += sprintf(fileName + j, "%ld", (long)getpid());
	j += sprintf(fileName + j, "%s", ".csv");
	cout << "FileName: " << fileName << endl;
	outFile.open(fileName);*/

	// Register Routine to be called to instrument rtn
	RTN_AddInstrumentFunction(Routine, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);
	return TRUE;
}
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
	cerr << "This Pintool counts the number of times a routine is executed" << endl;
	cerr << "and the number of instructions executed in a routine" << endl;
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
	// Initialize symbol table code, needed for rtn instrumentation
	PIN_InitSymbols();

	// Initialize pin
	if (PIN_Init(argc, argv)) return Usage();
	//char * fileName = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());

	/*int j;
	char fileName[20];
	j = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());
	j += sprintf(fileName + j, "%ld", (long)getpid());
	j += sprintf(fileName + j, "%s", ".csv");
	cout << "FileName: " << fileName << endl;
	outFile.open(fileName);
	*/
	// Register Routine to be called to instrument rtn
	RTN_AddInstrumentFunction(Routine, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	PIN_AddFollowChildProcessFunction(FollowChild, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}