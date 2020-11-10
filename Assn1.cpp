#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/raw_ostream.h"
#include "LoopUtils.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
using namespace llvm;

#define DEBUG_TYPE "assn1"
STATISTIC(FuncCounter, "Counts number of functions greeted");
STATISTIC(InstCounter, "Counts number of instructions greeted");
STATISTIC(LoadCounter, "Counts number of load instructions greeted");
STATISTIC(StoreCounter, "Counts number of store instructions greeted");
STATISTIC(LoopCounter, "Counts number of loops greeted");
STATISTIC(BBCounter, "Counts number of basic blocks greeted");

namespace {
  struct pathElem {
  	StringRef lab;
	Instruction *inst;
  };

  struct LoopData {
  	int initV;
	int stepV;
	int finalV;
	StringRef indVar;
  };

  typedef std::vector<std::vector<pathElem>> pathElems;
  struct Assn1Loop : public FunctionPass {
  		
	  static char ID;
	  Assn1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
    	  }

	  void addToPath(StringRef src, Instruction *inst, std::vector<StringRef> destV, pathElems &allPaths ) {
	  	if(src.empty()) {
			for(StringRef dest: destV) {
				std::vector<pathElem> pathV;
				pathElem elem;
				elem.lab = dest;
				elem.inst = inst;
				pathV.push_back(elem);
				allPaths.push_back(pathV);
			}
		}
		else {
			//search the path vector ending with source label
			//Replicate it for number of destination labels adding destination pathElem node
			for(std::vector<pathElem> &pathV : allPaths) {
				pathElem cmp = pathV.back();
				if(src == cmp.lab) {
					pathElem elem;
					if(destV.empty()) {
						elem.lab = "";
					}
					else {
						elem.lab = destV.back();
						destV.pop_back();
					}
					elem.inst = inst;
					pathV.push_back(elem);
					for(StringRef dest: destV) {
						std::vector<pathElem> cpPathV;
						copy(pathV.begin(), pathV.end() - 1, back_inserter(cpPathV));
						pathElem elem;
						elem.inst = inst;
						elem.lab = dest;
						cpPathV.push_back(elem);
						allPaths.push_back(cpPathV);
					}
				}
			}
		}
	  }

	  void printPaths(pathElems &allPaths) {
		int count = 1;
		for(std::vector<pathElem> pathV : allPaths) {
			errs() << "Path no. " << count << " ";
			for(pathElem elem: pathV) {
				errs() << *(elem.inst) << " dest label is: " << elem.lab << "\t";
			}
			count++;
			errs() << "\n";
		}
	  }
	  StringRef checkAlloc(std::vector<pathElem> &pathV, unsigned int nextpos) {
	  	StringRef alloc;
		while(nextpos < pathV.size()) {
			pathElem elem = pathV[nextpos];
			if(llvm::isa<llvm::AllocaInst> (*elem.inst)) {
				Value *v = cast<Value>(elem.inst);
				alloc = v->getName();
				break;
			}
			nextpos++;
		}


		return alloc;
	  }

	  StringRef checkInd(std::vector<pathElem> &pathV, unsigned int nextpos) {
	  	StringRef ind;
		while(nextpos < pathV.size()) {
			pathElem elem = pathV[nextpos];
			if(llvm::isa<llvm::PHINode>(*elem.inst)) {
				Value *v = cast<Value>(elem.inst);
				ind = v->getName();
				break;
			}
			nextpos++;
		}
		return ind;
	  }

	  void analyzePathLoop(pathElems &allPaths, struct LoopData &loopData, llvm::BasicBlock::iterator &inst) {
	  	StringRef alloc, indVar;
		for(std::vector<pathElem> pathV : allPaths) {
			unsigned int nextpos = 0;
			for(pathElem elem : pathV) {
				nextpos++;
				if(llvm::isa<llvm::GetElementPtrInst> (*elem.inst)) {
					if(elem.inst->getOperand(0)->getName() == elem.lab) {
						alloc = checkAlloc(pathV, nextpos);
					}
					else if(elem.inst->getOperand(2)->getName() == elem.lab) {
						indVar = checkInd(pathV, nextpos);
					}
				}

			}
		}

		if(!alloc.empty() && !indVar.empty()) {
			if(indVar == loopData.indVar) {
				errs() << *inst << " defines a stream accessing the array " << alloc << " starting from location " << loopData.initV << " going upto " << loopData.finalV << " with the step value of " << loopData.stepV << "\n"; 
			}
		}

	  }
	  pathElems reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap) {
	  	  std::queue<StringRef> labQ;
		  pathElems allPaths;
		  std::map <StringRef, bool> visited;
		  std::vector<StringRef> destV;
		  Value *v;
		  if(llvm::isa <llvm::StoreInst> (*inst)) {
		  	v = inst->getOperand(1);
		  }
		  else if(llvm::isa<llvm::LoadInst> (*inst)) {
		  	v = inst->getOperand(0);
		  }
		  labQ.push(v->getName());
		  destV.push_back(v->getName());
		  visited[v->getName()] = true;

		  StringRef empty;
		  addToPath(empty, cast<Instruction>(inst), destV, allPaths);

		  while(!labQ.empty()) {
		  	  StringRef v = labQ.front();
			  //errs() << v << " defined by " << *defsMap[v] << "\n";
			  labQ.pop();
			  Instruction* inst2 = cast<Instruction>(defsMap[v]);
			  destV.clear();
			  for(Use &U : inst2->operands()) {
			  	 Value *v = U.get();
				 if(v->getName().startswith("for") == true || v->getName().empty()) {
					 continue;
				 }
				 if(visited.find(v->getName()) == visited.end()) {
				 	labQ.push(v->getName());
			  		destV.push_back(v->getName());
					visited[v->getName()] = true;
				 }
			  }

			  addToPath(v, inst2, destV, allPaths);
			  
		  }
		  /*errs() << "All labels impacting the address in the instruction " << *inst << " are: ";
		  for(std::pair<StringRef, bool> elem : visited) {
		  	errs() << " " << elem.first;
		  }
		  errs() << "\nAll paths impacting the address in this instruction are\n";
		  printPaths(allPaths);
		  */
		  
		  return allPaths;
	  }
	  bool runOnFunction(Function &F) override {
		errs() << "\nFunction name : " << F.getName() << "\n";
		InstCounter = 0;
		LoadCounter = 0;
		StoreCounter = 0;
		std::map<StringRef, Value*> defsMap;
		for(inst_iterator itr = inst_begin(F), etr = inst_end(F); itr != etr; itr++) {
			InstCounter++;

			for (Use &U : itr->operands()) {
				Value *v = U.get();
				if(v->hasName() == false) {
					continue;
				}
				
				if(v->getName().startswith("for") == true) {
					continue;
				}

				if(defsMap.find(v->getName()) != defsMap.end()) {
					continue;
				}
				defsMap.insert(std::pair<StringRef, Value*>(v->getName(), v));
			}

			if(llvm::isa <llvm::StoreInst> (*itr)) {
				StoreCounter++;
			}
			else if(llvm::isa <llvm::LoadInst> (*itr)) {
				LoadCounter++;
			}

			errs() << "Instruction is " << *itr << "\n";

      		}

		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		PHINode *phinode;
		struct LoopData loopData;
		for(Loop *lit : Li) {
			LoopCounter++;

			ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE(); 

			phinode = lit->getInductionVariable(SE);
			//errs() << *phinode << "\n";

			Value* vInd = cast<Value>(phinode);
			loopData.indVar = vInd->getName();

			llvm::Optional<Loop::LoopBounds> lbs = lit->getBounds(SE);
			Value& vInit = (*lbs).getInitialIVValue();
			ConstantInt *Ci;
			Ci = cast<ConstantInt>(&vInit);
			loopData.initV = Ci->getSExtValue(); 
			//errs() << "Initialization of loop is " << loopData.initV << "\n";

			Value& vFinal = (*lbs).getFinalIVValue();
			Ci = cast<ConstantInt>(&vFinal);
			loopData.finalV = Ci->getSExtValue(); 
			//errs() << "Loop upper bound / goes upto " << loopData.finalV << "\n";

			Value* step = (*lbs).getStepValue();
			Ci = cast<ConstantInt>(step);
			loopData.stepV = Ci->getSExtValue(); 
			//errs() << "Step value of the loop is " << loopData.stepV << "\n";

			
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		//errs() << "basicb name: "<< BB->getName() <<"\n";
				if(BB->getName().startswith("for.body")) {
					for (BasicBlock::iterator itr = BB->begin(), e = BB->end(); itr != e; ++itr) {
						if(llvm::isa <llvm::StoreInst> (*itr) || llvm::isa<llvm::LoadInst> (*itr)) {
							pathElems allPaths = reverseClosure(itr, defsMap);
							analyzePathLoop(allPaths, loopData, itr);
						}
					}
					
				}
                	}
		}


    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		
		return false;
	  }
  };
}

char Assn1Loop::ID = 0;
static RegisterPass<Assn1Loop> Y("assn1loop", "Assignment 1 Pass");
