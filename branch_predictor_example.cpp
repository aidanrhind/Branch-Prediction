#include <iostream>
#include <fstream>
#include <array>
#include <cstdlib>
#include "pin.H"
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;

// Simulation will stop when this number of instructions have been executed
//
#define STOP_INSTR_NUM 1000000000 // 1b instrs

// Simulator heartbeat rate
//
#define SIMULATOR_HEARTBEAT_INSTR_NUM 100000000 // 100m instrs

/* Base branch predictor class */
// You are highly recommended to follow this design when implementing your branch predictors
//
class BranchPredictorInterface {
public:
  //This function returns a prediction for a branch instruction with address branchPC
  virtual bool getPrediction(ADDRINT branchPC) = 0;
  
  //This function updates branch predictor's history with outcome of branch instruction with address branchPC
  virtual void train(ADDRINT branchPC, bool branchWasTaken) = 0;
};

// This is a class which implements always taken branch predictor
class AlwaysTakenBranchPredictor : public BranchPredictorInterface {
public:
  AlwaysTakenBranchPredictor(UINT64 numberOfEntries) {}; //no entries here: always taken branch predictor is the simplest predictor
	virtual bool getPrediction(ADDRINT branchPC) {
		return true; // predict taken
	}
	virtual void train(ADDRINT branchPC, bool branchWasTaken) {} //nothing to do here: always taken branch predictor does not have history
};

//------------------------------------------------------------------------------
//##############################################################################
/*
 * Insert your changes below here...
 *
 * Put your branch predictor implementation here
 *
 * For example:
 * class LocalBranchPredictor : public BranchPredictorInterface {
 *
 *   ***put private members for Local branch predictor here
 *
 *   public:
 *	   virtual bool getPrediction(ADDRINT branchPC) {
 *	  	 ***put your implementation here
 *	   }
 *	   virtual void train(ADDRINT branchPC, bool branchWasTaken) {
 *	     ***put your implementation here
 *	   }
 * }
 *
 * You also need to create an object of branch predictor class in main()
 * (i.e. at line 193 in the original unmodified version of this file).
 */
//##############################################################################
//------------------------------------------------------------------------------

class LocalBranchPredictor : public BranchPredictorInterface
{

  UINT64 index;
  UINT64 pclsb;
  UINT64 size;
  UINT64 lhr[128] = {}; // An array of 128 distinct Local History Registers (LHRs), that store the branch history of different branches
  int *pht;             // pattern history table;

  // The 7 Least Significant Bits (LSBs) of the Program Counter (PC) of the branch
  // instruction are used to index into the PHT  with the LHRs and select the relevant
  // LHR to be used for the prediction

public:
  LocalBranchPredictor(UINT64 numberOfEntries)
  { // set up pht and lhr array
    size = numberOfEntries;
    pht = new int[size]; // via CL input
    UINT64 i = {0};
    while (i < size)
    {
      pht[i] = 3; // initialize all pht entries to 3
      i++;
    }
  }

  virtual bool getPrediction(ADDRINT branchPC)
  {
    pclsb = branchPC & 0x7F; // get seven lsb of branchPC
    index = lhr[pclsb];      // seven lsb of branch instruction are used to index into the array of lhrs
    if (pht[index] > 1)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  virtual void train(ADDRINT branchPC, bool branchWasTaken)
  {
    pclsb = branchPC & 0x7F; // get seven lsb of branchPC
    index = lhr[pclsb];      // n lsb of branch instruction are used to index into the array of lhrs

    bool bpred = getPrediction(branchPC);

    if (branchWasTaken)
    {
      lhr[pclsb] = ((lhr[pclsb] >> 1) + 1) % size; // shift lhr by 1 and add 1 or 0
    }
    else
    {
      lhr[pclsb] = (lhr[pclsb] >> 1) % size;
    }
    // update pht values if branch is taken or not taken
    if (bpred == branchWasTaken)
    {
      if (pht[index] < 3)
      {
        pht[index] += 1; // if pht val is 00, 01, or 10 -> increment by 1, else stay at 11
      }
    }
    else
    {
      if (pht[index] > 0)
      { // if pht val is 11, 10, or 01 -> decrement by 1, else stay at 00
        pht[index] += -1;
      }
    }
  }
};

class GshareBranchPredictor : public BranchPredictorInterface
{
  UINT64 index;
  UINT64 pclsb;
  UINT64 size;
  UINT64 ghr;
  int *pht; // pattern history table;

public:
  GshareBranchPredictor(UINT64 numberOfEntries)
  {
    size = numberOfEntries;
    pht = new int[size];
    UINT64 i = {0};
    while (i < size)
    {
      pht[i] = 3; // initialize all pht entries to 3
      i++;
    }
  }
  virtual bool getPrediction(ADDRINT branchPC)
  {
    pclsb = branchPC & 0x3FF; // 10 lsb of PC
    index = pclsb ^ (ghr%size);
    if (pht[index] > 1)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  virtual void train(ADDRINT branchPC, bool branchWasTaken)
  {
    pclsb = branchPC & 0x3FF; // 10 lsb of PC
    index = pclsb ^ (ghr%size);

    bool bpred = getPrediction(branchPC);

    if (branchWasTaken)
    {
      ghr = ((ghr >> 1) + 1) % size; // shift lhr by 1 and add 1 or 0
    }
    else
    {
      ghr = (ghr >> 1) % size;
    }
    // update pht values if branch is taken or not taken
    if (bpred == branchWasTaken)
    {
      if (pht[index] < 3)
      {
        pht[index] += 1; // if pht val is 00, 01, or 10 -> increment by 1, else stay at 11
      }
    }
    else
    {
      if (pht[index] > 0)
      { // if pht val is 11, 10, or 01 -> decrement by 1, else stay at 00
        pht[index] += -1;
      }
    }
  }
};

class TournamentBranchPredictor : public BranchPredictorInterface {
  LocalBranchPredictor *LBP;
  GshareBranchPredictor *GBP;
  UINT64 pclsb;
  UINT64 size;
  int *pht; // pattern history table;
public:

  TournamentBranchPredictor(UINT64 numberOfEntries) {
    size = numberOfEntries;
    GBP = new GshareBranchPredictor(size);
    LBP = new LocalBranchPredictor(size);
    pht = new int[size];
    UINT64 i = {0};
    while (i < size)
    {
      pht[i] = 3; // initialize all pht entries to 3
      i++;
    }
  }

	virtual bool getPrediction(ADDRINT branchPC) {
		pclsb = branchPC & 0x3FF; // 10 lsb of PC
	  if (pht[pclsb] > 1)
    {
      return GBP->getPrediction(branchPC); //
    }
    else
    {
      return LBP->getPrediction(branchPC);
    }
  }

	virtual void train(ADDRINT branchPC, bool branchWasTaken) {
    pclsb = branchPC & 0x3FF;
    bool lpred = LBP->getPrediction(branchPC);
    bool gpred = GBP->getPrediction(branchPC);
    LBP->train(branchPC, branchWasTaken);
    GBP->train(branchPC, branchWasTaken);

    if ((lpred == branchWasTaken) && (gpred != branchWasTaken) && (pht[pclsb] > 1)) //gshare selected but lpred is correct
    {
      pht[pclsb] += -1;
    }
    else if ((lpred != branchWasTaken) && (gpred == branchWasTaken) && (pht[pclsb] < 2)) 
    {
      if (pht[pclsb] < 0) 
      {
        pht[pclsb] += -1;
      }        
      }
    else if ((lpred == branchWasTaken) && (pht[pclsb] < 2)) 
    {
        pht[pclsb] += 1;
      }  
    else if ((gpred == branchWasTaken) && (pht[pclsb] > 1)) 
    {
      if (pht[pclsb] < 3) 
      {
        pht[pclsb] += 1;
      }  
      }    
    }
};
ofstream OutFile;
BranchPredictorInterface *branchPredictor;

// Define the command line arguments that Pin should accept for this tool
//
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "BP_stats.out", "specify output file name");
KNOB<UINT64> KnobNumberOfEntriesInBranchPredictor(KNOB_MODE_WRITEONCE, "pintool",
    "num_BP_entries", "1024", "specify number of entries in a branch predictor");
KNOB<string> KnobBranchPredictorType(KNOB_MODE_WRITEONCE, "pintool",
    "BP_type", "always_taken", "specify type of branch predictor to be used");

// The running counts of branches, predictions and instructions are kept here
//
static UINT64 iCount                          = 0;
static UINT64 correctPredictionCount          = 0;
static UINT64 conditionalBranchesCount        = 0;
static UINT64 takenBranchesCount              = 0;
static UINT64 notTakenBranchesCount           = 0;
static UINT64 predictedTakenBranchesCount     = 0;
static UINT64 predictedNotTakenBranchesCount  = 0;

VOID docount() {
  // Update instruction counter
  iCount++;
  // Print this message every SIMULATOR_HEARTBEAT_INSTR_NUM executed
  if (iCount % SIMULATOR_HEARTBEAT_INSTR_NUM == 0) {
    std::cerr << "Executed " << iCount << " instructions." << endl;
  }
  // Release control of application if STOP_INSTR_NUM instructions have been executed
  if (iCount == STOP_INSTR_NUM) {
    PIN_Detach();
  }
}



VOID TerminateSimulationHandler(VOID *v) {
  OutFile.setf(ios::showbase);
  // At the end of a simulation, print counters to a file
  OutFile << "Prediction accuracy:\t"            << (double)correctPredictionCount / (double)conditionalBranchesCount << endl
          << "Number of conditional branches:\t" << conditionalBranchesCount                                      << endl
          << "Number of correct predictions:\t"  << correctPredictionCount                                        << endl
          << "Number of taken branches:\t"       << takenBranchesCount                                            << endl
          << "Number of non-taken branches:\t"   << notTakenBranchesCount                                         << endl
          ;
  OutFile.close();

  std::cerr << endl << "PIN has been detached at iCount = " << STOP_INSTR_NUM << endl;
  std::cerr << endl << "Simulation has reached its target point. Terminate simulation." << endl;
  std::cerr << "Prediction accuracy:\t" << (double)correctPredictionCount / (double)conditionalBranchesCount << endl;
  std::exit(EXIT_SUCCESS);
}

//
VOID Fini(int code, VOID * v)
{
  TerminateSimulationHandler(v);
}

// This function is called before every conditional branch is executed
//
static VOID AtConditionalBranch(ADDRINT branchPC, BOOL branchWasTaken) {
  /*
	 * This is the place where the predictor is queried for a prediction and trained
	 */

  // Step 1: make a prediction for the current branch PC
  //
	bool wasPredictedTaken = branchPredictor->getPrediction(branchPC);
  
  // Step 2: train the predictor by passing it the actual branch outcome
  //
	branchPredictor->train(branchPC, branchWasTaken);

  // Count the number of conditional branches executed
  conditionalBranchesCount++;
  
  // Count the number of conditional branches predicted taken and not-taken
  if (wasPredictedTaken) {
    predictedTakenBranchesCount++;
  } else {
    predictedNotTakenBranchesCount++;
  }

  // Count the number of conditional branches actually taken and not-taken
  if (branchWasTaken) {
    takenBranchesCount++;
  } else {
    notTakenBranchesCount++;
  }

  // Count the number of correct predictions
	if (wasPredictedTaken == branchWasTaken)
    correctPredictionCount++;
}

// Pin calls this function every time a new instruction is encountered
// Its purpose is to instrument the benchmark binary so that when 
// instructions are executed there is a callback to count the number of
// executed instructions, and a callback for every conditional branch
// instruction that calls our branch prediction simulator (with the PC
// value and the branch outcome).
//
VOID Instruction(INS ins, VOID *v) {
  // Insert a call before every instruction that simply counts instructions executed
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);

  // Insert a call before every conditional branch
  if ( INS_IsBranch(ins) && INS_HasFallThrough(ins) ) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtConditionalBranch, IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_END);
  }
}

// Print Help Message
INT32 Usage() {
  cerr << "This tool simulates different types of branch predictors" << endl;
  cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

int main(int argc, char * argv[]) {
  // Initialize pin
  if (PIN_Init(argc, argv)) return Usage();

  // Create a branch predictor object of requested type
  if (KnobBranchPredictorType.Value() == "always_taken") {
    std::cerr << "Using always taken BP" << std::endl;
    branchPredictor = new AlwaysTakenBranchPredictor(KnobNumberOfEntriesInBranchPredictor.Value());
  }
//------------------------------------------------------------------------------
//##############################################################################
/*
 * Insert your changes below here...
 *
 * In the following cascading if-statements instantiate branch predictor objects
 * using the classes that you have implemented for each of the three types of
 * predictor.
 *
 * The choice of predictor, and the number of entries in its prediction table
 * can be obtained from the command line arguments of this Pin tool using:
 *
 *  KnobNumberOfEntriesInBranchPredictor.Value() 
 *    returns the integer value specified by tool option "-num_BP_entries".
 *
 *  KnobBranchPredictorType.Value() 
 *    returns the value specified by tool option "-BP_type".
 *    The argument of tool option "-BP_type" must be one of the strings: 
 *        "always_taken",  "local",  "gshare",  "tournament"
 *
 *  Please DO NOT CHANGE these strings - they will be used for testing your code
 */
//##############################################################################
//------------------------------------------------------------------------------
  else if (KnobBranchPredictorType.Value() == "local") {
  	 std::cerr << "Using Local BP." << std::endl;
/* Uncomment when you have implemented a Local branch predictor */
      branchPredictor = new LocalBranchPredictor(KnobNumberOfEntriesInBranchPredictor.Value());
  }
  else if (KnobBranchPredictorType.Value() == "gshare") {
  	 std::cerr << "Using Gshare BP."<< std::endl;
/* Uncomment when you have implemented a Gshare branch predictor */
     branchPredictor = new GshareBranchPredictor(KnobNumberOfEntriesInBranchPredictor.Value());
  }
  else if (KnobBranchPredictorType.Value() == "tournament") {
  	 std::cerr << "Using Tournament BP." << std::endl;
/* Uncomment when you have implemented a Tournament branch predictor */
     branchPredictor = new TournamentBranchPredictor(KnobNumberOfEntriesInBranchPredictor.Value());
  }
  else {
    std::cerr << "Error: No such type of branch predictor. Simulation will be terminated." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::cerr << "The simulation will run " << STOP_INSTR_NUM << " instructions." << std::endl;

  OutFile.open(KnobOutputFile.Value().c_str());

  // Pin calls Instruction() when encountering each new instruction executed
  INS_AddInstrumentFunction(Instruction, 0);

  // Function to be called if the program finishes before it completes 10b instructions
  PIN_AddFiniFunction(Fini, 0);

  // Callback functions to invoke before Pin releases control of the application
  PIN_AddDetachFunction(TerminateSimulationHandler, 0);

  // Start the benchmark program. This call never returns...
  PIN_StartProgram();

  return 0;
}