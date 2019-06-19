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
#include <iostream>
#include <unistd.h>
#include "pin.H"

//using namespace std;
namespace WIND{
#include <Windows.h>
}


KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
	"o", "proccount.csv", "specify file name");

//ofstream outFile;
int test_count = 0;
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
	//UINT64 _icount;
	UINT64 _pid;
	pid_t _pid_win;
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

/*// Function to delete the entire linked list
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

}*/
// Function to delete the entire linked list
void deleteList() {

	// Deref head_ref to get the real head_ref
	RTN_COUNT* current = RtnList;
	RTN_COUNT* next;

	while (current != NULL) {
		next = current->_next;
		delete(current);
		current = next;
	}
	// Deref head_ref to affect the real head back in the caller
	RtnList= NULL;
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
	//rc->_icount = 0;
	/*if (v == 0) {
		rc->_pid = getpid();
	}
	else {
		rc->_pid = CHILD_PROCESS_GetId(*(CHILD_PROCESS*)v);
	}*/
	rc->_pid = getpid();
	rc->_pid_win = WIND::GetCurrentProcessId();

	// Add to list of routines
	rc->_next = RtnList;
	RtnList = rc;
	//cout << "New entry: " << RtnList->_name << endl;

	RTN_Open(rtn);

	// Insert a call at the entry point of a routine to increment the call count
	RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_rtnCount), IARG_END);

	RTN_Close(rtn);
}

// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID* v)
{
	test_count++;
	//cout << test_count << "TestCount"  << getpid() << endl;

	ofstream outFile;
	int j;
	char fileName[20];
	j = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());
	//j = sprintf(fileName, "%s", WIND::GetModuleFileName);
	j += sprintf(fileName + j, "%ld", (long)getpid());
	j += sprintf(fileName + j, "%s", ".csv");
	cout << "In Fini_Prarent, count: " << test_count << ", fileName: " << fileName
	 << " getpid(): "  << getpid() << ", GetCurrentProcessId: "<< WIND::GetCurrentProcessId()<< endl;
	outFile.open(fileName);

	outFile << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Pid Win" << ","
		<< "Size" << endl;
		//<< "Instructions" << endl;

	//RTN_COUNT* rc = RtnList;
	//RTN_COUNT* next;
	for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
	{
		//if (rc->_icount > 0) {
		if (rc->_rtnCount > 0){
			outFile << rc->_name << ","
				<< rc->_image << ","
				<< rc->_secName << ","
				<< rc->_secSize << ","
				<< hex << rc->_offset << dec << ","
				<< rc->_rtnCount << ","
				<< rc->_pid << ","
				<< rc->_pid_win << ","
				<< rc->_rtnSize << endl;
				//<< rc->_icount << endl;
		}
	}
	outFile.close();
	//deleteList(&RtnList);
}
/*
VOID Fini_Child(INT32 code, VOID* v)
{
	test_count++;
	//cout << test_count << "TestCount"  << getpid() << endl;
	pid_t pid = (pid_t)v;
	ofstream outFile;
	int j;
	char fileName[20];
	j = sprintf(fileName, "%s", KnobOutputFile.Value().c_str());
	//j = sprintf(fileName, "%s", WIND::GetModuleFileName);
	j += sprintf(fileName + j, "%ld", (long)pid);
	j += sprintf(fileName + j, "%s", ".csv");
	cout << "In Fini_Child, count: " << test_count << ", fileName: " << fileName << endl;
	outFile.open(fileName);

	outFile << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Pid Win" << ","
		<< "Size" << endl;
		//<< "Instructions" << endl;

	//RTN_COUNT* rc = RtnList;
	//RTN_COUNT* next;
	for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
	{
		//if (rc->_icount > 0) {
		if (rc->_rtnCount > 0){
			outFile << rc->_name << ","
				<< rc->_image << ","
				<< rc->_secName << ","
				<< rc->_secSize << ","
				<< hex << rc->_offset << dec << ","
				<< rc->_rtnCount << ","
				<< rc->_pid << ","
				<< rc->_pid_win << ","
				<< rc->_rtnSize << endl;
				//<< rc->_icount << endl;
		}
	}
	outFile.close();
	//deleteList(&RtnList);
}
*/
BOOL FollowChild(CHILD_PROCESS childProcess, VOID* userData) {

	pid_t pid= CHILD_PROCESS_GetId(childProcess);
	//TCHAR szFileName[MAX_PATH + 1];
	//GetModuleFileName(NULL,szFileName,MAX_PATH+1);
	//cout << "FileName: " << szFileName << endl;
	cout << "In Child entry: Child PID: " << pid << ", getpid(): "  << getpid() << ", GetCurrentProcessId: "<< WIND::GetCurrentProcessId()<< endl;

	//deleteList();
	RTN_AddInstrumentFunction(Routine, 0);

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


	RTN_AddInstrumentFunction(Routine, 0);

	PIN_AddFollowChildProcessFunction(FollowChild, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
