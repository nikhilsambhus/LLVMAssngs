#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
#include <pthread.h>
#include "Graph.h"
#include "GraphUtils.h"
using namespace llvm;
using namespace std;
struct graph {
	map<Value*, vector<Value*>> adjListMap;
	map<Value*, StringRef> ldstMap;
};

typedef vector<vector<Value*>> pathElems;

class genGraph {
	struct graph DFGbody;
	DAG libGrph;
	void dispVal(Value*);
	void dispChar(const char *);
	void addToPath(StringRef src, Value *val, std::vector<Value*> destV, pathElems &allPaths);
	void computePaths(Value *vl, pathElems &allPaths);
	void printPaths(pathElems &allPaths);
	public:
	void addToGraph(Value*, StringRef, char*);
	void printGraph(string, map<Value*, int>);
	void compStats();
	void loadPaths();
};
