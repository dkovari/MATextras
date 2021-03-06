/*
Example AsyncProcessor Object

This mex file creates a dummy processor which simply copies the task inputs
provided via pushTask(Arg1,Arg2,...) to the results queue
yielding a result
[Arg1,Arg2,...] = popResult();

/*--------------------------------------------------
Copyright 2018-2019, Daniel T. Kovari, Emory University
All rights reserved.
----------------------------------------------------*/

#include <extras/async/PersistentArgsProcessor.hpp>

class ExampleProcessor2 : public extras::async::PersistentArgsProcessor<> {
	typedef extras::async::PersistentArgsProcessor<>::TaskPairType TaskPairType;
protected:
	/// method for Processing Tasks in the task list
	virtual extras::cmex::mxArrayGroup ProcessTask(const TaskPairType& argPair) {

		size_t sz = argPair.first.size() + argPair.second->size();

		std::vector<const mxArray*> vA;
		vA.reserve(sz);

		for (size_t n = 0; n<argPair.first.size(); ++n) {
			vA.push_back(argPair.first.getConstArray(n));
		}
		for (size_t n = 0; n<argPair.second->size(); ++n) {
			vA.push_back(argPair.second->getArray(n));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500)); //let some time pass
		return extras::cmex::mxArrayGroup(sz, vA.data());
	}
};

extras::SessionManager::ObjectManager<ExampleProcessor2> manager;
extras::async::PersistentArgsProcessorInterface<ExampleProcessor2, manager> ep2_interface; //create interface manager for the ExampleProcessor

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
	ep2_interface.mexFunction(nlhs, plhs, nrhs, prhs);
}