/*--------------------------------------------------
Copyright 2019, Daniel T. Kovari, Emory University
All rights reserved.
----------------------------------------------------*/
#pragma once





#include "RoiParameterMap.hpp"

//////
// ParticleTracking Includes
// Be sure to add .../+extras/+ParticleTracking to your Include Path
#include <extras/cmex/NumericArray.hpp>
#include <radialcenter/source/radialcenter.hpp>
#include <barycenter/source/barycenter_mex.hpp>



namespace extras { namespace ParticleTracking {

	
	/** Asynchronous Processor for particle tracking in ROIs.
	 *	RoiTracker is intended to be used with the ParamProcessorInterface defined in ParamProcessor.hpp
	 *	Currently, the tracker expects to recieve persistent parameters (via the setParameter() method);
	 *	Parameters should include
	 *		'roiList',mex struct array containing "Window" field
	 *		'xyMethod','radialcenter' or 'barycenter' specifying which tracking routine to use
	 *		'COMmethod','meanabs','normal','gradmag' specifying what type of processing should be applied to the image before processing via radialcenter()
	 *		'DistanceFactor',val: distance factor used by radialcenter()
	 *		'LimFrac',val: Limit Fraction used by barycenter()
	 *
	 * Deriving from RoiTracker:
	 *	If you want to derive a class from RoiTracker (to add functionality)
	 *	you will probably want to start by overriding the ProcessTask(...) method.
	 *
	 *  Parameters are passed to the ProcessTask() method via shared_ptr to a 
	 *  specialized PersistentMxMap (RoiParameterMap).
	 *	RoiParameterMaps allow the end user to pass any name,value argument pairs (like the standard PersistentMxMap)
	 *  however, it intercepts the specialized parameters.
	 *  Therefore if you want to add some ability to ProcessTask() that will need other parameters, you don't necessarily need
	 *  to also define a custom PersistentMxMap.
	*/
	class RoiTracker : public extras::async::ParamProcessor {
	protected:

		std::atomic_bool _IncludeImageInResults = false;

		//! Define ProcessTask method
		extras::cmex::mxArrayGroup ProcessTask(const extras::cmex::mxArrayGroup& TaskArgs, std::shared_ptr<const extras::cmex::ParameterMxMap> Params) {
			using namespace extras::cmex;

			// Cast Param to: shared_ptr<const RoiParameterMap>
			std::shared_ptr<const RoiParameterMap> ParamMap;
			if (!Params) { //Params were not initialized, just use an empty map
				ParamMap = std::make_shared<RoiParameterMap>();
			}
			else {
				ParamMap = std::dynamic_pointer_cast<const RoiParameterMap>(Params);
			}

			// Get Image
			const mxArray* img = nullptr;
			const mxArray* time = nullptr;
			if (mxIsStruct(TaskArgs.getConstArray(0))) {
				img = mxGetField(TaskArgs.getConstArray(0), 0, "ImageData");
				time = mxGetField(TaskArgs.getConstArray(0), 0, "Time");
			}
			else {
				img = TaskArgs.getConstArray(0);
			}

			//Copy Parameter Map into resultStruct
			MxStruct resultsStruct = ParamMap->map2struct();
			resultsStruct.makePersistent(); //make resultStruct persistent so that we don't have issued being in a thread

			// if include image, add to struct
			if (_IncludeImageInResults) {

				if (mxIsStruct(TaskArgs.getConstArray(0))) { //imagedata was already a struct
					resultsStruct(0, "ImageStruct") = TaskArgs.getConstArray(0);
				}
				else { //only image data was passed, construct ImageStruct
					MxStruct IS(1, { "ImageData" });
					IS(0, "ImageData") = img;
					resultsStruct(0, "ImageStruct") = std::move(IS);
				}
			}

			
			MxStruct roiList(resultsStruct(0, "roiList").getmxarray()); //alias to roiList inside resultStruct

			/////////////////////////////
			// Process Image using appropriate method

			switch (ParamMap->get_xyMethod()) {
			case XY_FUNCTION::RADIALCENTER:
			{
				auto rcOut = radialcenter<extras::Array<double>>(
									cmex::DynamicTypeMxArray(img,true), //image converted to DynamicMxArray
									*ParamMap //parameters
							);

				rcOut[0] += 1; //shift for 1-indexing
				rcOut[1] += 1; //shift for 1-indexing

							   //set field values
				for (size_t n = 0; n < ParamMap->WIND->nRows(); ++n) {

					MxStruct CentroidResult(1, { "X","Y","varXY","RWR_N","xyMethod" });

					CentroidResult(0, "X") = rcOut[0][n];
					CentroidResult(0, "Y") = rcOut[1][n];

					NumericArray<double> vxy(2, 1);
					vxy(0) = rcOut[2](n, 0);
					vxy(1) = rcOut[2](n, 1);
					CentroidResult(0, "varXY") = vxy;

					CentroidResult(0, "RWR_N") = rcOut[3][n];
					CentroidResult(0, "xyMethod") = "radialcenter";

					roiList(n, "CentroidResult") = CentroidResult.releaseArray();
				}
			}
				break;
			case XY_FUNCTION::BARYCENTER:
				break;
			default:
				throw("Undefined xyMethod.");
			}

			//////////////////
			// Return result

			mxArrayGroup results(1);
			results.ownArray(0, resultsStruct.releaseArray());

			return results;

		}

	public:


		//! default constructor changes pMap to point to an RoiParameterMap
		RoiTracker() {
			_pMap = std::dynamic_pointer_cast<extras::cmex::ParameterMxMap>(std::make_shared<RoiParameterMap>());
		}

		///////////////////////
		// Parameter Related

		//! add or replace persistent perameters
		//! changed from default.
		//! in this version parameter points to an RoiParameterMap
		virtual void setParameters(size_t nrhs, const mxArray* prhs[]) {
			if (nrhs % 2 != 0) {
				throw(std::runtime_error("ParamProcessor::setParameters() number of args must be even (specified as Name,Value pairs)."));
			}
			std::shared_ptr<RoiParameterMap> newMap = std::make_shared<RoiParameterMap>(); // create new, empty parameter map;
			if (_pMap) { //_pMap is not nullptr
				newMap = std::make_shared<RoiParameterMap>(*std::dynamic_pointer_cast<RoiParameterMap>(_pMap)); // make a copy of the parametermap
			}

			newMap->setParameters(nrhs, prhs);

			_pMap = newMap;
		}

		// clear all parameters
		//! changed from default.
		//! in this version parameter points to an RoiParameterMap
		virtual void clearParameters() {
			_pMap = std::make_shared<RoiParameterMap>(); // create new, empty parameter map;
		}

		//! check if image should be included with results (true=yes, false=no)
		bool includeImageData() const { return _IncludeImageInResults; }

		//! set if image should be included with results (true=yes, false=no)
		bool includeImageData(bool includedImage) {
			_IncludeImageInResults = includedImage;
			return _IncludeImageInResults;
		}

		////////////////////////////////////////////////////////
		// Task Related

		//! push arguments to the task list.
		//! Each call to ProcessTask by the task thread will pop the "pushed" arguments
		//! off the stack and use them as arguments for ProcessTask(___)
		//!
		//! Modified here to assert that task either contains an image or contains a struct with "ImageData" and "Time" field
		virtual void pushTask(size_t nrhs, const mxArray* prhs[])
		{
			using namespace std;
			if (nrhs < 1) { return; }
			if (nrhs > 1) {
				throw(runtime_error("XYtracker::pushTask() only accepts one input. It should be an image, or a struct containing 'ImageData' and 'Time' fields"));
			}

			if (mxIsStruct(prhs[0])) {
				if (mxGetFieldNumber(prhs[0], "ImageData")<0) {
					throw(std::runtime_error("XYtracker::pushTask(), Image structuct must contain 'ImageData' field"));
				}
				if (mxGetFieldNumber(prhs[0], "Time")<0) {
					throw(std::runtime_error("XYtracker::pushTask(), Image structuct must contain 'Time' field"));
				}
			}

			// add task to the TaskList
			extras::async::ParamProcessor::pushTask(nrhs, prhs);
		}

		
	};

	/** Extend the ParamProcessorInterface for use with RoiTracker type objects
	 * Adds IncludeImageData method to the mexInterface
	 * 
	 * Usage: template ObjManager should be a reference to a manager for RoiTracker-class.
	*/
	template<class ObjType, extras::SessionManager::ObjectManager<ObjType>& ObjManager>
	class RoiTrackerInterface :public extras::async::ParamProcessorInterface<ObjType, ObjManager> {
		typedef extras::async::ParamProcessorInterface<ObjType, ObjManager> ParentType;
	protected:
		void IncludeImageData(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
			auto objPtr = ParentType::getObjectPtr(nrhs, prhs);
			if (nrhs > 1) { //setting value
				if (!mxIsScalar(prhs[1])) {
					throw("Cannot set IncludeImageData. Argument must be scalar and convertable to logical.");
				}
				bool res = objPtr->includeImageData(mxGetScalar(prhs[1]));
				plhs[0] = mxCreateLogicalScalar(res);
			}
			else {
				bool res = objPtr->includeImageData();
				plhs[0] = mxCreateLogicalScalar(res);
			}
		}
		
	public:
		RoiTrackerInterface() {
			using namespace std::placeholders;
			ParentType::addFunction("IncludeImageData", std::bind(&RoiTrackerInterface::IncludeImageData, this, _1, _2, _3, _4));
		}
	};

}}