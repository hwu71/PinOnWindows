//#include <fstream>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include "pin.H"

//using namespace std;
//#include <processthreadsapi.h>

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
	//rc->_icount = 0;
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
	/*for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
	{
		// Insert a call to docount to increment the instruction counter for this RTN
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_icount), IARG_END);
	}*/

	RTN_Close(rtn);
}

// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID* v)
{
	test_count++;

	cout << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Size" << endl;

	for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
	{
		if (rc->_rtnCount > 0){
			cout << rc->_name << ","
				<< rc->_image << ","
				<< rc->_secName << ","
				<< rc->_secSize << ","
				<< hex << rc->_offset << dec << ","
				<< rc->_rtnCount << ","
				<< rc->_pid << ","
				<< rc->_rtnSize << endl;
		}
	}
	//deleteList(&RtnList);
}

VOID Fini_Child(INT32 code, VOID* v)
{
	test_count++;

	cout << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Size" << endl;

	for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
	{
		if (rc->_rtnCount > 0) {
			cout << rc->_name << ","
				<< rc->_image << ","
				<< rc->_secName << ","
				<< rc->_secSize << ","
				<< hex << rc->_offset << dec << ","
				<< rc->_rtnCount << ","
				<< rc->_pid << ","
				<< rc->_rtnSize << endl;
		}
	}
}

BOOL FollowChild(CHILD_PROCESS childProcess, VOID* userData) {

	pid_t pid= CHILD_PROCESS_GetId(childProcess);
	cout << "In Child: PID: " << pid << " getpid(): "  << getpid() << endl;

	//RTN_AddInstrumentFunction(Routine, 0);


	// Register Fini to be called when the application exits
  //PIN_AddFiniFunction(Fini_Child,0);
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
