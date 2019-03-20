/*----------------------------------------------------------------------
Copyright 2018-2019, Daniel T. Kovari & James Merrill, Emory University
All rights reserved.
-----------------------------------------------------------------------*/
#pragma once

#include <cstddef>
#include <cstdio>
#include <mex.h>
#include <list>
#include <string>
#include <vector>
#include <mutex>
#include <cstring>

#include <extras/cmex/mxobject.hpp>
#include <extras/SessionManager/mexInterface.hpp>
#include <extras/string_extras.hpp>

/********************************************************************
COMPRESSION Includes
=====================================================================
This header depends on ZLIB for reading and writing files compressed
with the gz format. Therefore you need to have zlib built/installed
on your system.

A version of ZLIB is included with the ExtrasToolbox located in
.../+extras/external_libs/zlib
Look at that folder for build instructions.
Alternatively, is you are using a *nix-type system, you might have 
better luck using your package manager to install zlib.

When building, be sure to include the location of zlib.h and to link to the 
compiled zlib-lib files.
*********************************************/
#include <zlib.h>
/********************************************/

namespace extras {namespace mxfile {

	class MxFileWriter; //forward declaration


	// Data Structure
	struct SerialData {
		size_t nbytes; // number of bytes of data to write
		const mxArray* data;  //the mxArray to be written to disk
	};

	//Wrapper for fwrite; change as needed later
	size_t write(const void * data, size_t n_bytes, size_t count, FILE * stream) {
		return fwrite(data, n_bytes, count, stream);
	}

	/*/compressed version
	size_t write(const void * data, size_t n_bytes, size_t count, gzFile stream) {
		return gzwrite(stream, data, n_bytes * count);
	}*/

	//! Flattens list of mxArray*s into a serialized list
	//! Cells and struct are decomposed into a serialized list of numeric/char type arrays
	//! resulting serialized data is returned as a list of "SerialData" a simple struct containing a
	//! const mxArray* and nbytes, the size the struct will be on the disk (when uncompressed)
	std::list<SerialData> Serialize(size_t nrhs, const mxArray** prhs) {
		std::list<SerialData> out;
		for (size_t i = 0; i < nrhs; i++) {
			SerialData l;
			switch (mxGetClassID(prhs[i])) {
			case mxCELL_CLASS:
				l.nbytes = sizeof(uint8_t) + sizeof(size_t) + sizeof(size_t) * mxGetNumberOfDimensions(prhs[i]);
				//type, ndim, dims
				l.data = prhs[i];
				out.push_back(l);
				for (size_t j = 0; j < mxGetNumberOfElements(prhs[i]); ++j) {

					const mxArray * c = mxGetCell(prhs[i], j);

					out.splice(out.end(), Serialize(1, &c));

				}
				break;
			case mxSTRUCT_CLASS: {
				l.nbytes = sizeof(uint8_t) + sizeof(size_t) + mxGetNumberOfDimensions(prhs[i]) + sizeof(std::string);
				for (size_t j = 0; j < mxGetNumberOfFields(prhs[i]); ++j) {
					l.nbytes += sizeof(char) * std::string(mxGetFieldNameByNumber(prhs[i], j)).length();
				}
				//type, ndims, dims, nfields, field names
				l.data = prhs[i];
				out.push_back(l);
				for (size_t j = 0; j < mxGetNumberOfElements(prhs[i]); ++j) {
					for (size_t k = 0; k < mxGetNumberOfFields(prhs[i]); ++k) {
						const mxArray* c = mxGetFieldByNumber(prhs[i], j, k);
						out.splice(out.end(), Serialize(1, &c)); //unfold contents of fields
					}
				}
			}
				break;
			default:
				l.nbytes = sizeof(uint8_t) + sizeof(size_t) + sizeof(uint8_t) + sizeof(uint8_t); //type, ndims, isComplex, isInterleaved,
				l.nbytes += mxGetNumberOfDimensions(prhs[i]) * sizeof(size_t) + mxGetNumberOfElements(prhs[i]) * mxGetElementSize(prhs[i]) * (1 + mxIsComplex(prhs[i])); //  dims, data
				l.data = prhs[i];
				out.push_back(l);
			}
		}
		return out;
	}


	/** Helper class for file pointers
		* Provides a generic way to write to a file pointer
		* class can be derived and file-pointer and write method
		* can be redefined
	*/
	class FILE_WritePointer {
	protected:
		FILE* _fp = nullptr;
	public:
		FILE_WritePointer(FILE* fp) :_fp(fp) {};

		//! write data to file pointer
		//! Input:
		//!		data: pointer to data to write
		//!		nbytes: number of bytes to write
		//! Return: number of bytes written
		virtual size_t write(const void* data, size_t nbytes) {
			return std::fwrite(data, 1, nbytes, _fp);
		}

		//! return FILE*
		FILE* getFP() const {
			return _fp;
		}
	};

	//! ZLib Wrapper
	class GZFILE_WritePointer : public FILE_WritePointer {
		friend class MxFileWriter;
	protected:
		gzFile _fp; //zlib file pointer

		//! sets _fp to NULL
		void clearFP() {
			_fp = NULL;
		}
	public:
		GZFILE_WritePointer(gzFile fp) :FILE_WritePointer(nullptr), _fp(fp) {};

		//! write data to file pointer
		//! Input:
		//!		data: pointer to data to write
		//!		nbytes: number of bytes to write
		//! Return: number of bytes written
		virtual size_t write(const void* data, size_t nbytes) {
			return gzwrite(_fp, data, nbytes);
		}

		//! return gzFile hides inherited getFP()
		gzFile getFP() const {
			return _fp;
		}
	};

	//! Loop over all arrays in the serialized list and write to FP
	void writeList(const std::list<SerialData>& dataList, FILE_WritePointer& FP) {
		for (auto& thisData : dataList) { //loop over all items in the list
			const mxArray* thisArray = thisData.data;
			uint8_t type = mxGetClassID(thisArray);
			size_t ndims = mxGetNumberOfDimensions(thisArray);

			size_t bytes_written = 0;

			//write type byte
			bytes_written += FP.write(&type, 1);

			//write ndim
			bytes_written += FP.write(&ndims, sizeof(size_t));

			//write dims
			bytes_written += FP.write((void*)mxGetDimensions(thisArray), sizeof(size_t)*ndims);

			switch (mxGetClassID(thisArray))
			{
			case mxCELL_CLASS:
				//nothing else to write
				break;
			case mxSTRUCT_CLASS:
			{
				//write number of field names
				size_t nfields = mxGetNumberOfFields(thisArray);
				bytes_written += FP.write(&nfields, sizeof(size_t));

				////////////
				// FOR EACH FIELD
				//write length of fieldname and fieldnames
				for (size_t f = 0; f < nfields; ++f) {
					const char* fieldname = mxGetFieldNameByNumber(thisArray, f);
					size_t len = strlen(fieldname)+1; // length including null terminator

					//write length of name
					bytes_written += FP.write(&len, sizeof(size_t));

					//write fieldname, including null terminator
					bytes_written += FP.write(fieldname, len * sizeof(char));
				}
			}
				break;
			default: //all other types
			{
				//write complex
				uint8_t isComplex = (uint8_t)mxIsComplex(thisArray);
				bytes_written += FP.write(&isComplex, sizeof(uint8_t));

				//write interleaved flag
				uint8_t interFlag = 0; //default to not interleaved complex data
#if MX_HAS_INTERLEAVED_COMPLEX
				interFlag = 1;
#endif
				bytes_written += FP.write(&interFlag, sizeof(uint8_t));

				//Write data
				size_t numel = mxGetNumberOfElements(thisArray);
				size_t elsz = mxGetElementSize(thisArray);
				if (!isComplex) { //not complex, simple write
					bytes_written += FP.write(mxGetData(thisArray), numel*elsz);
				}
				else { //is complex
#if MX_HAS_INTERLEAVED_COMPLEX //interleaved data, size is 2x so regular copy works fine
					bytes_written += FP.write(mxGetData(thisArray), numel*elsz);
#else //not interleaved, need to write imag data explicitly
					bytes_written += FP.write(mxGetData(thisArray), numel*elsz);
					bytes_written += FP.write(mxGetImagData(thisArray), numel*elsz);
#endif
				}

			}
			}
		}
	}

	//! Opens file for writing using zlib's gz functions
	GZFILE_WritePointer gzOpenWriter(const char* filepath) {
		gzFile fp = gzopen(filepath, "wb");
		if (fp == NULL) {
			throw(std::runtime_error(std::string("gzOpenWriter(): returned null, file:'") + std::string(filepath) + std::string("' could not be opened.")));
		}
		return GZFILE_WritePointer(fp);
	}

	/**
	 * Class which handles writing MxFile data
	*/
	class MxFileWriter {
	protected:
		std::mutex _WPmutex; //mutex protecting _WritePointer
		GZFILE_WritePointer _WritePointer; //pointer class for gzFile, proteced by locks using _WPmutex
		std::string _filepath;

		//! automatically add ".mxf.gz" file extension if not included
		std::string validateFileExt(std::string fpth) {
			//look for ".mxf.gz" at end of file
			std::string fpth_lower = extras::tolower(fpth);
			const char* fpth_c = fpth_lower.c_str();
			const char* p_ext = strstr(fpth_c,".mxf.gz");
			if (p_ext != nullptr) { //found in fpth, make sure it's at the end
				size_t loc = p_ext - fpth_c; //location in the string
				size_t back = fpth.size() - loc; //loc from back
				if (back == 7) { //found at back, just return fpth
					return fpth;
				}
			}
			//didn't find ".mxf.gz", look for ".mxf"
			p_ext = strstr(fpth_c, ".mxf");
			if (p_ext != nullptr) { //found in fpth, make sure it's at the end
				size_t loc = p_ext - fpth_c; //location in the string
				size_t back = fpth.size() - loc; //loc from back
				if (back == 7) { //found at back, add ".gz"
					fpth += ".gz";
					return fpth;
				}
			}
			//didn't find ".mxf" add ".mxf.gz"
			fpth += ".mxf.gz";
			return fpth;
		}
	public:

		//! return true if file is open
		bool isFileOpen() const {
			return _WritePointer.getFP() != NULL;
		}

		//! default constructor
		MxFileWriter() : _WritePointer(NULL) {};

		//! destructor
		//! close the writePointer
		virtual ~MxFileWriter() {
			closeFile();
		}

		//! close the file
		void closeFile() {
			if (isFileOpen()) {
				std::lock_guard<std::mutex> lock(_WPmutex);
				gzclose(_WritePointer.getFP());
				_WritePointer.clearFP();
			}
		}

		//! open specified file for writing
		//! automatically adds ".mxf.gz" file extension if not included
		void openFile(std::string fpth) {
			closeFile();
			std::lock_guard<std::mutex> lock(_WPmutex);
			auto fp_ext = validateFileExt(fpth);
			_WritePointer = gzOpenWriter(fp_ext.c_str());
			_filepath = fp_ext;
		}

		//!open file for writing (using MATLAB args)
		//! automatically add ".mxf.gz" file extension if not included
		void openFile(size_t nrhs, const mxArray** prhs) {
			if (nrhs < 1) {
				throw("MxFileWriter::openWriter() expected one argument");
			}
			openFile(extras::cmex::getstring(prhs[0]));
		}

		//! returns copy of filepath string
		std::string filepath() const {
			return _filepath;
		}

		//! write the matlab arrays to the file
		virtual void writeArrays(size_t nrhs, const mxArray** prhs) {
			if (!isFileOpen()) {
				throw(std::runtime_error(std::string("MxFileWriter::writeArrays() file:'") + _filepath + std::string("' is not open.")));
			}
			std::lock_guard<std::mutex> lock(_WPmutex);
			writeList(Serialize(nrhs, prhs), _WritePointer);
		}
	};


	//! implement mexInterface for MxFileWriter
	template<class ObjType, extras::SessionManager::ObjectManager<ObjType>& ObjManager> /*ObjType should be a derivative of MxFileWriter*/
	class MxFileWriterInterface : public SessionManager::mexInterface<ObjType, ObjManager> {
		typedef SessionManager::mexInterface<ObjType, ObjManager> ParentType;
	protected:
		void openFile(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
			ParentType::getObjectPtr(nrhs, prhs)->openFile(nrhs - 1, &prhs[1]);
		}
		void closeFile(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
			ParentType::getObjectPtr(nrhs, prhs)->closeFile();
		}
		void filepath(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
			cmex::MxObject fpth = ParentType::getObjectPtr(nrhs, prhs)->filepath();
			plhs[0] = fpth;
		}
		void writeArrays(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
			ParentType::getObjectPtr(nrhs, prhs)->writeArrays(nrhs - 1, &prhs[1]);
		}
		void isFileOpen(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]){
			bool isopen = ParentType::getObjectPtr(nrhs, prhs)->isFileOpen();
			plhs[0] = mxCreateLogicalScalar(isopen);
		}
	public:
		MxFileWriterInterface() {
			using namespace std::placeholders;
			ParentType::addFunction("openFile", std::bind(&MxFileWriterInterface::openFile, this, _1, _2, _3, _4));
			ParentType::addFunction("closeFile", std::bind(&MxFileWriterInterface::closeFile, this, _1, _2, _3, _4));
			ParentType::addFunction("filepath", std::bind(&MxFileWriterInterface::filepath, this, _1, _2, _3, _4));
			ParentType::addFunction("writeArrays", std::bind(&MxFileWriterInterface::writeArrays, this, _1, _2, _3, _4));
			ParentType::addFunction("isFileOpen", std::bind(&MxFileWriterInterface::isFileOpen, this, _1, _2, _3, _4));
		}
	};
}}