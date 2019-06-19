#include <fstream>
#include <iostream>
#include <unistd.h>
#include "pin.H"

//using namespace std;
namespace WIND{
#include <Windows.h>
}

//General configuration - Tool full path

KNOB<string> KnobToolsFullPath(KNOB_MODE_WRITEONCE,         "pintool",
                               "tools_path", "./obj-intel64/", "grand parent tool full path");


/*//Parent configuration - Application and PinTool name
KNOB<string> KnobParentApplicationName(KNOB_MODE_WRITEONCE,         "pintool",
                                       "parent_app_name", "win_parent_process", "parent application name");

KNOB<string> KnobParentToolName(KNOB_MODE_WRITEONCE,         "pintool",
                                "parent_tool_name", "parent_tool", "parent tool full path");
*/

//Child configuration - Application and PinTool name
//KNOB<string> KnobChildApplicationName(KNOB_MODE_WRITEONCE,         "pintool",
//                                      "child_app_name", "win_child_process", "child application name");

KNOB<string> KnobChildToolName(KNOB_MODE_WRITEONCE,         "pintool",
                               "child_tool_name", "proccount_cmdl_4gen.dll", "child tool full path");


/*// Current process id received by grand_parent tool
KNOB<OS_PROCESS_ID> KnobCurrentProcessId(KNOB_MODE_WRITEONCE,         "pintool",
                                  "process_id", "0", "current process id");

// Whether to check current process id received in KnobCurrentProcessId
KNOB<BOOL> KnobCheckCurrentProcessId(KNOB_MODE_WRITEONCE,         "pintool",
                                         "check_process_id", "0", "current process id");

// Whether to probe the child
KNOB<BOOL>   KnobProbeChild(KNOB_MODE_WRITEONCE,                "pintool",
                            "probe_child", "0", "probe the child process");
*/
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
	cout << "In Fini_cmdl_4gen, count: " << test_count << ", fileName: " << fileName
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
	deleteList();
}
BOOL FollowChild(CHILD_PROCESS childProcess, VOID * userData)
{
    BOOL res;
    INT appArgc;
    CHAR const * const * appArgv;

    OS_PROCESS_ID pid = CHILD_PROCESS_GetId(childProcess);

    CHILD_PROCESS_GetCommandLine(childProcess, &appArgc, &appArgv);

		cout << "In Child entry(cmdl_4gen): Child PID: " << pid << ", getpid(): "  << getpid() << ", GetCurrentProcessId: "<< WIND::GetCurrentProcessId()<< endl;


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


/*BOOL FollowChild(CHILD_PROCESS childProcess, VOID* userData) {

	pid_t pid= CHILD_PROCESS_GetId(childProcess);
	//TCHAR szFileName[MAX_PATH + 1];
	//GetModuleFileName(NULL,szFileName,MAX_PATH+1);
	//cout << "FileName: " << szFileName << endl;
	cout << "In Child entry: Child PID: " << pid << ", getpid(): "  << getpid() << ", GetCurrentProcessId: "<< WIND::GetCurrentProcessId()<< endl;

	RTN_AddInstrumentFunction(Routine, 0);

	return TRUE;
}*/

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
