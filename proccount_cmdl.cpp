#include <fstream>
#include <iostream>
#include <unistd.h>
#include "pin.H"

//using namespace std;
//General configuration - Tool full path

KNOB<string> KnobToolsFullPath(KNOB_MODE_WRITEONCE,         "pintool",
                               "tools_path", "./obj-intel64/", "grand parent tool full path");

KNOB<string> KnobChildToolName(KNOB_MODE_WRITEONCE,         "pintool",
                               "child_tool_name", "proccount_cmdl_2gen.dll", "child tool full path");

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
	rc->_offset = RTN_Address(rtn)- IMG_LowAddress(SEC_Img(RTN_Sec(rtn)));
	rc->_rtnSize = RTN_Size(rtn);
	rc->_rtnCount = 0;
	rc->_pid = getpid();
	// Add to list of routines
	rc->_next = RtnList;
	RtnList = rc;

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
	cout << "(1gen) In Fini_Prarent, count: " << test_count << ", fileName: " << fileName << "pid: "  << getpid() << endl;
	outFile.open(fileName);

	outFile << "Procedure" << ","
		<< "Image" << ","
		<< "Section Name" << ","
		<< "Section Size" << ","
		<< "Offset" << ","
		<< "Calls" << ","
		<< "Pid" << ","
		<< "Size" << endl;

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
				<< rc->_rtnSize << endl;
				//<< rc->_icount << endl;
		}
	}
	outFile.close();
	//deleteList();
}
BOOL FollowChild(CHILD_PROCESS childProcess, VOID * userData)
{
    BOOL res;
    INT appArgc;
    CHAR const * const * appArgv;

    OS_PROCESS_ID pid = CHILD_PROCESS_GetId(childProcess);

    CHILD_PROCESS_GetCommandLine(childProcess, &appArgc, &appArgv);

		cout << "(1gen) In Child entry: Child PID: " << pid << ", parent pid: "  << getpid() << endl;


    //Set Pin's command line for child process
    INT pinArgc = 0;
    CHAR const * pinArgv[10];

    pinArgv[pinArgc++] = "pin";
    pinArgv[pinArgc++] = "-follow_execv";
    /*if (KnobProbeChild)
    {
        pinArgv[pinArgc++] = "-probe"; // pin in probe mode
    }*/
    pinArgv[pinArgc++] = "-t";
    string tool = KnobToolsFullPath.Value() + KnobChildToolName.Value();
    pinArgv[pinArgc++] = tool.c_str();
		pinArgv[pinArgc++] = "-o";
		string outFileName = KnobOutputFile.Value();
		pinArgv[pinArgc++] = outFileName.c_str();
    pinArgv[pinArgc++] = "--";
		//cout <<"pin Argc: " << pinArgv[0] << " " << pinArgv[1] << " " << pinArgv[2] << " " << pinArgv[3] << " " << pinArgv[4] << " " << pinArgv[5] << " " << pinArgv[6] << endl;
    CHILD_PROCESS_SetPinCommandLine(childProcess, pinArgc, pinArgv);

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
  cout << "In 1gen: Current process id = " << getpid() << endl << flush;

	RTN_AddInstrumentFunction(Routine, 0);

	PIN_AddFollowChildProcessFunction(FollowChild, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
