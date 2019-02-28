/*
Mex framework for creating C++ object which live past the end of each mex call,
allowing the object to be used across multiple calls.

Couple this the mexDispatch to create a class interface

This is intended to be used with Session.m
*/

#pragma once

#include <unordered_map>
#include <memory>
#include <mex.h>
#include <list>

namespace extras{namespace SessionManager{



	/*
	/// ObjectManager base class
	/// used to setup the static mexAtExit function, calling ObjMan::clearObjects() for all object managers
	struct ObjectManager_Base {
		virtual ~ObjectManager_Base();
		virtual void clearObjects() = 0;
		ObjectManager_Base();
	};

	bool g_MexAtExitSet = false; ///< global flag specifying if mexAtExit has been set
	std::list<ObjectManager_Base*> g_ObjectManagerBaseList; ///< global list of object managers

	/// called at MexAtExit
	/// clears all objectmanagers
	void ExitFn() {

		for (auto pObj : g_ObjectManagerBaseList) {
			pObj->clearObjects();
		}

	}

	ObjectManager_Base::~ObjectManager_Base() {
		// remove self from om list

		g_ObjectManagerBaseList.remove(this);
	}

	ObjectManager_Base::ObjectManager_Base() {
		// add self to static OM list
		g_ObjectManagerBaseList.push_back(this);

		if (!g_MexAtExitSet) {
			//void(*pfn)(void) = ;
			///mexAtExit(&ObjectManager_Base::ExitFn);
			g_MexAtExitSet = true;
		}
	}
	*/


	template <class Obj>
	class ObjectManager{//: virtual public ObjectManager_Base {
	protected:
		bool _lock_mex;
		std::unordered_map<intptr_t, std::shared_ptr<Obj>> ObjectMap;

		static intptr_t getIntPointer(const mxArray* pointer) {
			if (mxIsEmpty(pointer))
				throw(std::runtime_error("ObjectManager:invalidType -> Id is empty."));
			if (sizeof(intptr_t) == 8 && !mxIsInt64(pointer) && !mxIsUint64(pointer))
				throw(std::runtime_error("ObjectManager:invalidType -> Invalid ID type, pointer ID must be INT64 or UINT64."));
			if (sizeof(intptr_t) == 4 && !mxIsInt32(pointer) && !mxIsUint32(pointer))
				throw(std::runtime_error("ObjectManager:invalidType -> Invalid ID type, pointer ID must be INT32 or UINT32."));
			return *reinterpret_cast<intptr_t*>(mxGetData(pointer));
		}

	public:

		/// destruct all managed objects
		void clearObjects() {
#ifdef _DEBUG
			mexPrintf("ObjectManager<%s>::clearObjects()\n", typeid(Obj).name());
			mexEvalString("pause(0.2)");
#endif
			ObjectMap.clear();
#ifdef _DEBUG
			mexPrintf("\t...Objects cleared.\n", typeid(Obj).name());
			mexEvalString("pause(0.2)");
#endif
		}

		ObjectManager(bool LOCK_MEX = true){
			_lock_mex = LOCK_MEX;
#ifdef _DEBUG
			mexPrintf("Creating ObjectManager<%s>\n", typeid(Obj).name());
#endif
		}

		virtual ~ObjectManager(){
#ifdef _DEBUG
			mexPrintf("Destroying ObjectManager<%s>\n",typeid(Obj).name());
			mexEvalString("pause(0.2)");
#endif
			
			clearObjects();
		}

		///Add object to map, creates a shared_ptr from the pointer
		/// call using something like objman.create(new YourObj());
		intptr_t create(Obj* p) { 
#ifdef _DEBUG
			mexPrintf("ObjectManager<%s>::create\n", typeid(Obj).name());
			mexEvalString("pause(0.2)");
#endif
			std::shared_ptr<Obj> newObj(p);
			intptr_t ptr = reinterpret_cast<intptr_t>(p);
			ObjectMap.insert(std::make_pair(ptr, newObj));//add object to map

			if (_lock_mex) {
				mexLock(); //increment mex lock counter;
			}
			
			return ptr;
		}

		/// destroy instance
		void destroy(intptr_t id) {
#ifdef _DEBUG
			mexPrintf("ObjectManager<%s>::destroy(%d)\n", typeid(Obj).name(),id);
			mexEvalString("pause(0.2)");
#endif
			if (!ObjectMap.empty())
			{
				ObjectMap.erase(id);
				if (_lock_mex) { //unlock mex file if needed
					mexUnlock(); //deccrement mex lock counter;
				}
			}
		}

		/// destroy instance specified in mxArray
		void destroy(const mxArray* in) {
			intptr_t id = getIntPointer(in);
			destroy(id);
		}

		std::shared_ptr<Obj> get(intptr_t id) {
#ifdef DAN_DEBUG
			mexPrintf("ObjectManager<%s>::get()\n\tnObjects=%d\n", typeid(Obj).name(),ObjectMap.size());
			mexEvalString("pause(0.2)");
#endif
			auto search = ObjectMap.find(id);
			if (search != ObjectMap.end()) {
				return search->second;
			}
			else {
				throw(std::runtime_error(
					std::string("ObjectManager::get(")+
					std::to_string(id)+
					std::string(") -> Object not found")));
			}
		}

		std::shared_ptr<Obj> get(const mxArray* in) {
			intptr_t id = getIntPointer(in);
#ifdef DAN_DEBUG
			mexPrintf("\t ptr=%d\n", typeid(Obj).name(),id);
			mexEvalString("pause(0.2)");
#endif
			return get(id);
		}
	};
}}
