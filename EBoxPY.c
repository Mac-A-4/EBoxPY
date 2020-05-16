#include <Python.h>
#include <structmember.h>
#include <Windows.h>
#include <Psapi.h>
#include <Tlhelp32.h>

/*
 *
 * General
 *
 */

#define EBOXPY_OBJECT_ZERO(T, O) memset((void*)((char*)O + sizeof(PyObject)), 0, sizeof(T) - sizeof(PyObject))

/*
 *
 * EBoxPY.Region
 *
 */

PyDoc_STRVAR(EBoxPY_Region__doc__, "EBoxPY Region object, defines a Region of memory in a given Process.");

#define EBOXPY_REGION_READABLE (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_READWRITE)
#define EBOXPY_REGION_WRITABLE (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READWRITE | PAGE_WRITECOPY)
#define EBOXPY_REGION_EXECUTABLE (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)

typedef struct EBoxPY_Region_T {
	//
	PyObject_HEAD
	//
	unsigned long long Allocation_;
	unsigned long long Address_;
	unsigned long long Size_;
	//
	unsigned long Protection_;
	unsigned long State_;
	unsigned long Type_;
	//
	char Backed_;
	//
} EBoxPY_Region, *PEBoxPY_Region;

static PyObject* EBoxPY_Region_repr(PyObject* self);

static PyObject* EBoxPY_Region_IsReadable(PEBoxPY_Region self);
static PyObject* EBoxPY_Region_IsWritable(PEBoxPY_Region self);
static PyObject* EBoxPY_Region_IsExecutable(PEBoxPY_Region self);

static PyObject* EBoxPY_Region_IsCommitted(PEBoxPY_Region self);
static PyObject* EBoxPY_Region_IsReserved(PEBoxPY_Region self);
static PyObject* EBoxPY_Region_IsFree(PEBoxPY_Region self);

static PyMemberDef EBoxPY_Region_Members[] = {
	{"Allocation_", T_ULONGLONG, offsetof(EBoxPY_Region, Allocation_), READONLY, PyDoc_STR("The absolute base of the Allocation that the Region is a member of, could be less than or equal to Address.")},
	{"Address_", T_ULONGLONG, offsetof(EBoxPY_Region, Address_), READONLY, PyDoc_STR("The base Address of the Region.")},
	{"Size_", T_ULONGLONG, offsetof(EBoxPY_Region, Size_), READONLY, PyDoc_STR("The Size of the Region, Address + Size points to the beginning of the next Region.")},
	{"Protection_", T_ULONG, offsetof(EBoxPY_Region, Protection_), READONLY, PyDoc_STR("The memory Protection flags associated with this Region.")},
	{"State_", T_ULONG, offsetof(EBoxPY_Region, State_), READONLY, PyDoc_STR("The memory State flags associated with this Region.")},
	{"Type_", T_ULONG, offsetof(EBoxPY_Region, Type_), READONLY, PyDoc_STR("The memory Type flags associated with this Region.")},
	{"Backed_", T_BOOL, offsetof(EBoxPY_Region, Backed_), READONLY, PyDoc_STR("True if the Region is Backed by physical memory, False if otherwise.")},
	{NULL}
};

static PyMethodDef EBoxPY_Region_Methods[] = {
	{"IsReadable", (PyCFunction)EBoxPY_Region_IsReadable, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsReadable() -> bool\nTrue if the Region's Protection flags indicate a Readable state, False if otherwise.")},
	{"IsWritable", (PyCFunction)EBoxPY_Region_IsWritable, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsWritable() -> bool\nTrue if the Region's Protection flags indicate a Writable state, False if otherwise.")},
	{"IsExecutable", (PyCFunction)EBoxPY_Region_IsExecutable, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsExecutable() -> bool\nTrue if the Region's Protection flags indicate an Executable state, False if otherwise.")},
	{"IsCommitted", (PyCFunction)EBoxPY_Region_IsCommitted, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsCommitted() -> bool\nTrue if the Region's State flags indicate a Committed state, False if otherwise.")},
	{"IsReserved", (PyCFunction)EBoxPY_Region_IsReserved, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsReserved() -> bool\nTrue if the Region's State flags indicate a Reserved state, False if otherwise.")},
	{"IsFree", (PyCFunction)EBoxPY_Region_IsFree, METH_NOARGS, PyDoc_STR("EBoxPY.Region.IsFree() -> bool\nTrue if the Region's State flags indicate a Free state, False if otherwise.")},
	{NULL}
};

static PyTypeObject EBoxPY_Region_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "EBoxPY.Region",
	.tp_basicsize = sizeof(EBoxPY_Region),
	.tp_doc = EBoxPY_Region__doc__,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_members = EBoxPY_Region_Members,
	.tp_repr = EBoxPY_Region_repr,
	.tp_str = EBoxPY_Region_repr,
	.tp_methods = EBoxPY_Region_Methods,
};

static int _EBoxPY_Initialize_Region(PyObject* self) {
	if (PyType_Ready(&EBoxPY_Region_Type) < 0)
		return 0;
	PyModule_AddObject(self, "Region", (PyObject*)&EBoxPY_Region_Type);
	return 1;
}

static PyObject* _EBoxPY_Create_Region(HANDLE _Process, unsigned long long _Address) {
	PyObject* output = EBoxPY_Region_Type.tp_alloc(&EBoxPY_Region_Type, 1);
	if (!output)
		return NULL;
	EBOXPY_OBJECT_ZERO(EBoxPY_Region, output);
	PEBoxPY_Region region = (PEBoxPY_Region)output;
	MEMORY_BASIC_INFORMATION information = {0};
	if (VirtualQueryEx(_Process, (void*)_Address, &information, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
		Py_DECREF(output);
		return NULL;
	}
	region->Allocation_ = (unsigned long long)information.AllocationBase;
	region->Address_ = (unsigned long long)information.BaseAddress;
	region->Size_ = (unsigned long long)information.RegionSize;
	region->Protection_ = (unsigned long)information.Protect;
	region->State_ = (unsigned long)information.State;
	region->Type_ = (unsigned long)information.Type;
	PSAPI_WORKING_SET_EX_INFORMATION set = { 0 };
	set.VirtualAddress = (void*)region->Address_;
	if (!QueryWorkingSetEx(_Process, &set, sizeof(PSAPI_WORKING_SET_EX_INFORMATION))) {
		Py_DECREF(output);
		return NULL;
	}
	region->Backed_ = (char)(set.VirtualAttributes.Valid ? 1 : 0);
	return output;
}

static PyObject* EBoxPY_Region_repr(PyObject* self) {
	PEBoxPY_Region region = (PEBoxPY_Region)self;
	char output[64];
	sprintf(output, "<EBoxPY.Region: (0x%p) (0x%p)>", (void*)region->Address_, (void*)region->Size_);
	return PyUnicode_FromString(output);
}

static PyObject* EBoxPY_Region_IsReadable(PEBoxPY_Region self) {
	if (self->Protection_ & EBOXPY_REGION_READABLE) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Region_IsWritable(PEBoxPY_Region self) {
	if (self->Protection_ & EBOXPY_REGION_WRITABLE) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Region_IsExecutable(PEBoxPY_Region self) {
	if (self->Protection_ & EBOXPY_REGION_EXECUTABLE) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Region_IsCommitted(PEBoxPY_Region self) {
	if (self->State_ == MEM_COMMIT) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Region_IsReserved(PEBoxPY_Region self) {
	if (self->State_ == MEM_RESERVE) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Region_IsFree(PEBoxPY_Region self) {
	if (self->State_ == MEM_FREE) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

/*
 *
 * EBoxPY.Module
 *
 */

PyDoc_STRVAR(EBoxPY_Module__doc__, "EBoxPY Module object, defines a Module in a given Process.");

#define EBOXPY_ARCHITECTURE_AMD64 1
#define EBOXPY_ARCHITECTURE_I386 2

#define EBOXPY_PE_HEADERS_SIZE 0x1000

typedef struct EBoxPY_Module_T {
	//
	PyObject_HEAD
	//
	unsigned long long Address_;
	unsigned long long Size_;
	//
	PyObject* Name_;
	PyObject* Path_;
	//
	PyObject* Sections_;
	//
	unsigned int Architecture_;
	//
} EBoxPY_Module, *PEBoxPY_Module;

static void EBoxPY_Module_dealloc(PyObject* self);
static PyObject* EBoxPY_Module_repr(PyObject* self);

static PyObject* EBoxPY_Module_IsAMD64(PEBoxPY_Module self);
static PyObject* EBoxPY_Module_IsI386(PEBoxPY_Module self);

static PyMemberDef EBoxPY_Module_Members[] = {
	{"Address_", T_ULONGLONG, offsetof(EBoxPY_Module, Address_), READONLY, PyDoc_STR("The base Address of the Module.")},
	{"Size_", T_ULONGLONG, offsetof(EBoxPY_Module, Size_), READONLY, PyDoc_STR("The Size of the Module.")},
	{"Name_", T_OBJECT, offsetof(EBoxPY_Module, Name_), READONLY, PyDoc_STR("The Name of the Module.")},
	{"Path_", T_OBJECT, offsetof(EBoxPY_Module, Path_), READONLY, PyDoc_STR("The Path to the Module on disk.")},
	{"Sections_", T_OBJECT, offsetof(EBoxPY_Module, Sections_), READONLY, PyDoc_STR("Dictionary of Module Sections.")},
	{"Architecture_", T_UINT, offsetof(EBoxPY_Module, Architecture_), READONLY, PyDoc_STR("Architecture flags of the Module.")},
	{NULL}
};

static PyMethodDef EBoxPY_Module_Methods[] = {
	{"IsAMD64", (PyCFunction)EBoxPY_Module_IsAMD64, METH_NOARGS, PyDoc_STR("EBoxPY.Module.IsAMD64() -> bool\nTrue if the Module's Architecture is AMD64, False if otherwise.")},
	{"IsI386", (PyCFunction)EBoxPY_Module_IsI386, METH_NOARGS, PyDoc_STR("EBoxPY.Module.IsI386() -> bool\nTrue if the Module's Architecture is I386, False if otherwise.")},
	{NULL}
};

static PyTypeObject EBoxPY_Module_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "EBoxPY.Module",
	.tp_basicsize = sizeof(EBoxPY_Module),
	.tp_doc = EBoxPY_Module__doc__,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_members = EBoxPY_Module_Members,
	.tp_dealloc = EBoxPY_Module_dealloc,
	.tp_repr = EBoxPY_Module_repr,
	.tp_str = EBoxPY_Module_repr,
	.tp_methods = EBoxPY_Module_Methods,
};

static int _EBoxPY_Initialize_Module(PyObject* self) {
	PyModule_AddIntConstant(self, "AMD64", EBOXPY_ARCHITECTURE_AMD64);
	PyModule_AddIntConstant(self, "I386", EBOXPY_ARCHITECTURE_I386);
	if (PyType_Ready(&EBoxPY_Module_Type) < 0)
		return 0;
	PyModule_AddObject(self, "Module", (PyObject*)&EBoxPY_Module_Type);
	return 1;
}

static PyObject* _EBoxPY_Create_Module(HANDLE _Process, MODULEENTRY32W _Entry) {
	PyObject* output = EBoxPY_Module_Type.tp_alloc(&EBoxPY_Module_Type, 1);
	if (!output)
		return NULL;
	EBOXPY_OBJECT_ZERO(EBoxPY_Module, output);
	PEBoxPY_Module _module = (PEBoxPY_Module)output;
	_module->Address_ = (unsigned long long)_Entry.modBaseAddr;
	_module->Size_ = (unsigned long long)_Entry.modBaseSize;
	_module->Name_ = PyUnicode_FromWideChar(_Entry.szModule, -1);
	if (!_module->Name_) {
		Py_DECREF(output);
		return NULL;
	}
	_module->Path_ = PyUnicode_FromWideChar(_Entry.szExePath, -1);
	if (!_module->Name_) {
		Py_DECREF(output);
		return NULL;
	}
	unsigned char* headers = (unsigned char*)malloc(EBOXPY_PE_HEADERS_SIZE);
	if (!headers) {
		Py_DECREF(output);
		return NULL;
	}
	if (ReadProcessMemory(_Process, (void*)_module->Address_, (void*)headers, EBOXPY_PE_HEADERS_SIZE, NULL) == FALSE) {
		free(headers);
		Py_DECREF(output);
		return NULL;
	}
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)headers;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		free(headers);
		Py_DECREF(output);
		return NULL;
	}
	PIMAGE_FILE_HEADER file = (PIMAGE_FILE_HEADER)(headers + dos->e_lfanew + 0x4);
	switch (file->Machine) {
		case IMAGE_FILE_MACHINE_I386:
			_module->Architecture_ = EBOXPY_ARCHITECTURE_I386;
			break;
		case IMAGE_FILE_MACHINE_IA64:
		case IMAGE_FILE_MACHINE_AMD64:
			_module->Architecture_ = EBOXPY_ARCHITECTURE_AMD64;
			break;
		default:
			free(headers);
			Py_DECREF(output);
			return NULL;
	}
	PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)(headers + dos->e_lfanew + (_module->Architecture_ == EBOXPY_ARCHITECTURE_AMD64 ? sizeof(IMAGE_NT_HEADERS64) : sizeof(IMAGE_NT_HEADERS32)));
	_module->Sections_ = PyDict_New();
	for (unsigned short i = 0; i < file->NumberOfSections; ++i) {
		unsigned long long address = (unsigned long long)section[i].VirtualAddress + _module->Address_;
		unsigned long long size = (unsigned long long)section[i].Misc.VirtualSize;
		PEBoxPY_Region region = (PEBoxPY_Region)_EBoxPY_Create_Region(_Process, address);
		if (!region)
			continue;
		region->Address_ = address;
		region->Size_ = size;
		char name[8 + 1] = {0};
		memcpy((void*)name, (void*)section[i].Name, 8);
		PyObject* key = PyUnicode_FromString(name);
		if (!key) {
			Py_DECREF((PyObject*)region);
			continue;
		}
		PyDict_SetItem(_module->Sections_, key, (PyObject*)region);
		Py_DECREF(key);
		Py_DECREF((PyObject*)region);
	}
	free(headers);
	return output;
}

static void EBoxPY_Module_dealloc(PyObject* self) {
	PEBoxPY_Module _module = (PEBoxPY_Module)self;
	Py_XDECREF(_module->Name_);
	Py_XDECREF(_module->Path_);
	Py_XDECREF(_module->Sections_);
	Py_TYPE(self)->tp_free(self);
}

static PyObject* EBoxPY_Module_repr(PyObject* self) {
	PEBoxPY_Module _module = (PEBoxPY_Module)self;
	PyObject* encoded = PyUnicode_AsEncodedString(_module->Name_, "UTF-8", "strict");
	if (!encoded)
		return NULL;
	char* name = PyBytes_AS_STRING(encoded);
	char output[512];
	sprintf(output, "<EBoxPY.Module: (%s) (0x%p) (0x%p)>", name, (void*)_module->Address_, (void*)_module->Size_);
	Py_DECREF(encoded);
	return PyUnicode_FromString(output);
}

static PyObject* EBoxPY_Module_IsAMD64(PEBoxPY_Module self) {
	if (self->Architecture_ == EBOXPY_ARCHITECTURE_AMD64) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* EBoxPY_Module_IsI386(PEBoxPY_Module self) {
	if (self->Architecture_ == EBOXPY_ARCHITECTURE_I386) {
		Py_INCREF(Py_True);
		return Py_True;
	}
	else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

/*
 *
 * EBoxPY.Bytes
 *
 */

#define EBOXPY_UINT8 0
#define EBOXPY_INT8 1
#define EBOXPY_UINT16 2
#define EBOXPY_INT16 3
#define EBOXPY_UINT32 4
#define EBOXPY_INT32 5
#define EBOXPY_UINT64 6
#define EBOXPY_INT64 7
#define EBOXPY_FLOAT 8
#define EBOXPY_DOUBLE 9

static unsigned long long _EBoxPY_GetNativeSize(unsigned long long _Type) {
	switch (_Type) {
		case EBOXPY_UINT8:
		case EBOXPY_INT8:
			return 1;
		case EBOXPY_UINT16:
		case EBOXPY_INT16:
			return 2;
		case EBOXPY_UINT32:
		case EBOXPY_INT32:
			return 4;
		case EBOXPY_UINT64:
		case EBOXPY_INT64:
			return 8;
		case EBOXPY_FLOAT:
			return 4;
		case EBOXPY_DOUBLE:
			return 8;
		default:
			return 0;
	}
}

static PyObject* _EBoxPY_NativeToPython(void* _Raw, unsigned long long _Type) {
	switch (_Type) {
		case EBOXPY_UINT8:
			return PyLong_FromUnsignedLongLong((unsigned long long)(*((unsigned char*)_Raw)));
		case EBOXPY_INT8:
			return PyLong_FromLongLong((long long)(*((char*)_Raw)));
		case EBOXPY_UINT16:
			return PyLong_FromUnsignedLongLong((unsigned long long)(*((unsigned short*)_Raw)));
		case EBOXPY_INT16:
			return PyLong_FromLongLong((long long)(*((short*)_Raw)));
		case EBOXPY_UINT32:
			return PyLong_FromUnsignedLongLong((unsigned long long)(*((unsigned long*)_Raw)));
		case EBOXPY_INT32:
			return PyLong_FromLongLong((long long)(*((long*)_Raw)));
		case EBOXPY_UINT64:
			return PyLong_FromUnsignedLongLong((unsigned long long)(*((unsigned long long*)_Raw)));
		case EBOXPY_INT64:
			return PyLong_FromLongLong((long long)(*((long long*)_Raw)));
		case EBOXPY_FLOAT:
			return PyFloat_FromDouble((double)(*((float*)_Raw)));
		case EBOXPY_DOUBLE:
			return PyFloat_FromDouble((double)(*((double*)_Raw)));
		default:
			return NULL;
	}
}

static int _EBoxPY_PythonToNative(PyObject* _Python, void* _Raw, unsigned long long _Type) {
	switch (_Type) {
		case EBOXPY_UINT8:
			if (!PyLong_Check(_Python))
				return 0;
			*((unsigned char*)_Raw) = (unsigned char)PyLong_AsUnsignedLongLong(_Python);
			return 1;
		case EBOXPY_INT8:
			if (!PyLong_Check(_Python))
				return 0;
			*((char*)_Raw) = (char)PyLong_AsLongLong(_Python);
			return 1;
		case EBOXPY_UINT16:
			if (!PyLong_Check(_Python))
				return 0;
			*((unsigned short*)_Raw) = (unsigned short)PyLong_AsUnsignedLongLong(_Python);
			return 1;
		case EBOXPY_INT16:
			if (!PyLong_Check(_Python))
				return 0;
			*((short*)_Raw) = (short)PyLong_AsLongLong(_Python);
			return 1;
		case EBOXPY_UINT32:
			if (!PyLong_Check(_Python))
				return 0;
			*((unsigned long*)_Raw) = (unsigned long)PyLong_AsUnsignedLongLong(_Python);
			return 1;
		case EBOXPY_INT32:
			if (!PyLong_Check(_Python))
				return 0;
			*((long*)_Raw) = (long)PyLong_AsLongLong(_Python);
			return 1;
		case EBOXPY_UINT64:
			if (!PyLong_Check(_Python))
				return 0;
			*((unsigned long long*)_Raw) = (unsigned long long)PyLong_AsUnsignedLongLong(_Python);
			return 1;
		case EBOXPY_INT64:
			if (!PyLong_Check(_Python))
				return 0;
			*((long long*)_Raw) = (long long)PyLong_AsLongLong(_Python);
			return 1;
		case EBOXPY_FLOAT:
			if (!PyFloat_Check(_Python))
				return 0;
			*((float*)_Raw) = (float)PyFloat_AsDouble(_Python);
			return 1;
		case EBOXPY_DOUBLE:
			if (!PyFloat_Check(_Python))
				return 0;
			*((double*)_Raw) = (double)PyFloat_AsDouble(_Python);
			return 1;
		default:
			return 0;
	}
}

PyDoc_STRVAR(EBoxPY_Bytes__doc__, "EBoxPY Bytes object, used to convert between Python objects and Native bytes.");

typedef struct EBoxPY_Bytes_T {
	//
	PyObject_HEAD
	//
	unsigned char* Allocation_;
	unsigned long long Size_;
	//
} EBoxPY_Bytes, *PEBoxPY_Bytes;

static int EBoxPY_Bytes_init(PyObject* self, PyObject* args, PyObject* kwds);
static PyObject* EBoxPY_Bytes_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
static void EBoxPY_Bytes_dealloc(PyObject* self);
static PyObject* EBoxPY_Bytes_repr(PyObject* self);

static PyObject* EBoxPY_Bytes_Get(PEBoxPY_Bytes self, PyObject* args);
static PyObject* EBoxPY_Bytes_Set(PEBoxPY_Bytes self, PyObject* args);
static PyObject* EBoxPY_Bytes_CopyTo(PEBoxPY_Bytes self, PyObject* args);
static PyObject* EBoxPY_Bytes_CopyFrom(PEBoxPY_Bytes self, PyObject* args);

static PyMemberDef EBoxPY_Bytes_Members[] = {
	{"Allocation_", T_ULONGLONG, offsetof(EBoxPY_Bytes, Allocation_), READONLY, PyDoc_STR("The Allocation of Bytes.")},
	{"Size_", T_ULONGLONG, offsetof(EBoxPY_Bytes, Size_), READONLY, PyDoc_STR("The Size of the Allocation.")},
	{NULL}
};

static PyMethodDef EBoxPY_Bytes_Methods[] = {
	{"Get", (PyCFunction)EBoxPY_Bytes_Get, METH_VARARGS, PyDoc_STR("EBoxPY.Bytes.Get(_Offset, _Type)\nGets a Native data type from the Bytes at the specified Offset.")},
	{"Set", (PyCFunction)EBoxPY_Bytes_Set, METH_VARARGS, PyDoc_STR("EBoxPY.Bytes.Set(_Offset, _Type, _Value)\nSets a Native data type from the Bytes at the specified Offset.")},
	{"CopyTo", (PyCFunction)EBoxPY_Bytes_CopyTo, METH_VARARGS, PyDoc_STR("EBoxPY.Bytes.CopyTo(_Destination, _SourceIndex, _DestinationIndex, _Size)\nCopies a portion of the Bytes into the Destination.")},
	{"CopyFrom", (PyCFunction)EBoxPY_Bytes_CopyFrom, METH_VARARGS, PyDoc_STR("EBoxPY.Bytes.CopyFrom(_Source, _SourceIndex, _DestinationIndex, _Size)\nCopies a portion of the Bytes from the Source.")},
	{NULL}
};

static PyTypeObject EBoxPY_Bytes_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "EBoxPY.Bytes",
	.tp_basicsize = sizeof(EBoxPY_Bytes),
	.tp_doc = EBoxPY_Bytes__doc__,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_members = EBoxPY_Bytes_Members,
	.tp_dealloc = EBoxPY_Bytes_dealloc,
	.tp_repr = EBoxPY_Bytes_repr,
	.tp_str = EBoxPY_Bytes_repr,
	.tp_methods = EBoxPY_Bytes_Methods,
	.tp_init = EBoxPY_Bytes_init,
	.tp_new = EBoxPY_Bytes_new,
};

static int _EBoxPY_Initialize_Bytes(PyObject* self) {
	PyModule_AddIntConstant(self, "UINT8", EBOXPY_UINT8);
	PyModule_AddIntConstant(self, "INT8", EBOXPY_INT8);
	PyModule_AddIntConstant(self, "UINT16", EBOXPY_UINT16);
	PyModule_AddIntConstant(self, "INT16", EBOXPY_INT16);
	PyModule_AddIntConstant(self, "UINT32", EBOXPY_UINT32);
	PyModule_AddIntConstant(self, "INT32", EBOXPY_INT32);
	PyModule_AddIntConstant(self, "UINT64", EBOXPY_UINT64);
	PyModule_AddIntConstant(self, "INT64", EBOXPY_INT64);
	PyModule_AddIntConstant(self, "FLOAT", EBOXPY_FLOAT);
	PyModule_AddIntConstant(self, "DOUBLE", EBOXPY_DOUBLE);
	if (PyType_Ready(&EBoxPY_Bytes_Type) < 0)
		return 0;
	PyModule_AddObject(self, "Bytes", (PyObject*)&EBoxPY_Bytes_Type);
	return 1;
}

static PyObject* _EBoxPY_Create_Bytes(unsigned long long _Size) {
	PyObject* args = PyTuple_Pack(1, PyLong_FromUnsignedLongLong(_Size));
	if (!args)
		return NULL;
	PyObject* output = EBoxPY_Bytes_Type.tp_new(&EBoxPY_Bytes_Type, args, NULL);
	if (EBoxPY_Bytes_Type.tp_init(output, args, NULL) < 0) {
		Py_DECREF(args);
		Py_DECREF(output);
		return NULL;
	}
	Py_DECREF(args);
	return output;
}

static int EBoxPY_Bytes_init(PyObject* self, PyObject* args, PyObject* kwds) {
	EBOXPY_OBJECT_ZERO(EBoxPY_Bytes, self);
	if (PyTuple_Size(args) != 1) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.__init__ requires 1 argument.");
		return -1;
	}
	if (PyObject_IsInstance(PyTuple_GetItem(args, 0), (PyObject*)&EBoxPY_Bytes_Type)) {
		PEBoxPY_Bytes other = (PEBoxPY_Bytes)PyTuple_GetItem(args, 0);
		PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)self;
		bytes->Size_ = other->Size_;
		bytes->Allocation_ = (unsigned char*)malloc(bytes->Size_);
		if (!bytes->Allocation_) {
			PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.__init__ failed to allocate memory.");
			return -1;
		}
		memcpy((void*)bytes->Allocation_, (void*)other->Allocation_, bytes->Size_);
		return 0;
	}
	else if (PyLong_Check(PyTuple_GetItem(args, 0))) {
		unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args,0));
		if (PyErr_Occurred())
			return -1;
		PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)self;
		bytes->Size_ = size;
		bytes->Allocation_ = (unsigned char*)malloc(bytes->Size_);
		if (!bytes->Allocation_) {
			PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.__init__ failed to allocate memory.");
			return -1;
		}
		memset((void*)bytes->Allocation_, 0, bytes->Size_);
		return 0;
	}
	else {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.__init__ requires either an int or a Bytes object.");
		return -1;
	}
}

static PyObject* EBoxPY_Bytes_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	return type->tp_alloc(type, 1);
}

static void EBoxPY_Bytes_dealloc(PyObject* self) {
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)self;
	if (bytes->Allocation_)
		free((void*)bytes->Allocation_);
	Py_TYPE(self)->tp_free(self);
}

static PyObject* EBoxPY_Bytes_repr(PyObject* self) {
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)self;
	char buffer[64] = {0};
	sprintf(buffer, "<EBoxPY.Bytes: (Address: %p) (Size: %i)>", (void*)bytes->Allocation_, (int)bytes->Size_);
	return PyUnicode_FromString(buffer);
}

static PyObject* EBoxPY_Bytes_Get(PEBoxPY_Bytes self, PyObject* args) {
	if (PyTuple_Size(args) != 2) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Get requires 2 arguments.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 0))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Get requires _Offset to be int.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 1))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Get requires _Type to be int.");
		return NULL;
	}
	unsigned long long offset = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Get requires _Offset >= 0.");
		return NULL;
	}
	unsigned long long type = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 1));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Get requires _Type >= 0.");
		return NULL;
	}
	unsigned long long size = _EBoxPY_GetNativeSize(type);
	if (size == 0) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Get requires a valid _Type.");
		return NULL;
	}
	if (offset + size > self->Size_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Get address out of bounds.");
		return NULL;
	}
	PyObject* output = _EBoxPY_NativeToPython((void*)(self->Allocation_ + offset), type);
	if (!output) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Get failed to convert Native type to Python object.");
		return NULL;
	}
	return output;
}

static PyObject* EBoxPY_Bytes_Set(PEBoxPY_Bytes self, PyObject* args) {
	if (PyTuple_Size(args) != 3) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Set requires 3 arguments.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 0))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Set requires _Offset to be int.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 1))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.Set requires _Type to be int.");
		return NULL;
	}
	unsigned long long offset = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Set requires _Offset >= 0.");
		return NULL;
	}
	unsigned long long type = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 1));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Set requires _Type >= 0.");
		return NULL;
	}
	unsigned long long size = _EBoxPY_GetNativeSize(type);
	if (size == 0) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Set requires a valid _Type.");
		return NULL;
	}
	if (offset + size > self->Size_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Set address out of bounds.");
		return NULL;
	}
	if (_EBoxPY_PythonToNative(PyTuple_GetItem(args, 2), self->Allocation_ + offset, type)) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	else {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.Set failed to convert Python object to Native type.");
		return NULL;
	}
}

static int _EBoxPY_Bytes_Copy(PEBoxPY_Bytes _Source, PEBoxPY_Bytes _Destination, unsigned long long _SourceIndex, unsigned long long _DestinationIndex, unsigned long long _Size) {
	if (_SourceIndex + _Size > _Source->Size_)
		return 0;
	if (_DestinationIndex + _Size > _Destination->Size_)
		return 0;
	memcpy(_Destination->Allocation_ + _DestinationIndex, _Source->Allocation_ + _SourceIndex, _Size);
	return 1;
}

static PyObject* EBoxPY_Bytes_CopyTo(PEBoxPY_Bytes self, PyObject* args) {
	if (PyTuple_Size(args) != 4) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyTo Requires 4 Arguments.");
		return NULL;
	}
	PyObject* destination = PyTuple_GetItem(args, 0);
	if (!PyObject_IsInstance(destination, (PyObject*)&EBoxPY_Bytes_Type)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyTo Requires EBoxPY.Bytes as _Destination.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 1))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyTo Requires int as _SourceIndex.");
		return NULL;
	}
	unsigned long long source_index = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 1));
	if (!PyLong_Check(PyTuple_GetItem(args, 2))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyTo Requires int as _DestinationIndex.");
		return NULL;
	}
	unsigned long long destination_index = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 2));
	if (!PyLong_Check(PyTuple_GetItem(args, 3))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyTo Requires int as _Size.");
		return NULL;
	}
	unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 3));
	if (!_EBoxPY_Bytes_Copy(self, (PEBoxPY_Bytes)destination, source_index, destination_index, size)) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.CopyTo Failed to Copy Bytes.");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Bytes_CopyFrom(PEBoxPY_Bytes self, PyObject* args) {
	if (PyTuple_Size(args) != 4) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyFrom Requires 4 Arguments.");
		return NULL;
	}
	PyObject* source = PyTuple_GetItem(args, 0);
	if (!PyObject_IsInstance(source, (PyObject*)&EBoxPY_Bytes_Type)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyFrom Requires EBoxPY.Bytes as _Source.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 1))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyFrom Requires int as _SourceIndex.");
		return NULL;
	}
	unsigned long long source_index = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 1));
	if (!PyLong_Check(PyTuple_GetItem(args, 2))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyFrom Requires int as _DestinationIndex.");
		return NULL;
	}
	unsigned long long destination_index = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 2));
	if (!PyLong_Check(PyTuple_GetItem(args, 3))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Bytes.CopyFrom Requires int as _Size.");
		return NULL;
	}
	unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 3));
	if (!_EBoxPY_Bytes_Copy((PEBoxPY_Bytes)source, self, source_index, destination_index, size)) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Bytes.CopyFrom Failed to Copy Bytes.");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

/*
 *
 * EBoxPY.Thread
 *
 */

PyDoc_STRVAR(EBoxPY_Thread__doc__, "EBoxPY Thread object, defines a running Thread within a Process.");

typedef struct EBoxPY_Thread_T {
	//
	PyObject_HEAD
	//
	HANDLE Thread_;
	char IsOpen_;
	unsigned long ID_;
	//
	char IsLocked_;
	PyObject* Registers_;
	//
	CONTEXT Save_;
	//
} EBoxPY_Thread, *PEBoxPY_Thread;

static void EBoxPY_Thread_dealloc(PyObject* self);
static PyObject* EBoxPY_Thread_repr(PyObject* self);

static PyObject* EBoxPY_Thread_Open(PEBoxPY_Thread self);
static PyObject* EBoxPY_Thread_Close(PEBoxPY_Thread self);
static PyObject* EBoxPY_Thread_Lock(PEBoxPY_Thread self);
static PyObject* EBoxPY_Thread_Unlock(PEBoxPY_Thread self);

static PyMemberDef EBoxPY_Thread_Members[] = {
	{"Thread_", T_ULONGLONG, offsetof(EBoxPY_Thread, Thread_), READONLY, PyDoc_STR("The Handle to the Thread.")},
	{"IsOpen_", T_BOOL, offsetof(EBoxPY_Thread, IsOpen_), READONLY, PyDoc_STR("True if the Thread is Open, False if not, defines validity of the Thread Handle.")},
	{"ID_", T_ULONG, offsetof(EBoxPY_Thread, ID_), READONLY, PyDoc_STR("The ID of the Thread.")},
	{"IsLocked_", T_BOOL, offsetof(EBoxPY_Thread, IsLocked_), READONLY, PyDoc_STR("True if the Thread is Locked, False if not, defines the validity of the Thread Registers.")},
	{"Registers_", T_OBJECT, offsetof(EBoxPY_Thread, Registers_), READONLY, PyDoc_STR("The Registers of the Thread.")},
	{NULL}
};

static PyMethodDef EBoxPY_Thread_Methods[] = {
	{"Open", (PyCFunction)EBoxPY_Thread_Open, METH_NOARGS, PyDoc_STR("EBoxPY.Thread.Open()\nOpens the Thread if it is not already open.")},
	{"Close", (PyCFunction)EBoxPY_Thread_Close, METH_NOARGS, PyDoc_STR("EBoxPY.Thread.Close()\nCloses the Thread, Closes the Handle.")},
	{"Lock", (PyCFunction)EBoxPY_Thread_Lock, METH_NOARGS, PyDoc_STR("EBoxPY.Thread.Lock()\nLocks the Thread, sets the Registers.")},
	{"Unlock", (PyCFunction)EBoxPY_Thread_Unlock, METH_NOARGS, PyDoc_STR("EBoxPY.Thread.Unlock()\nUnlocks the Thread.")},
	{NULL}
};

static PyTypeObject EBoxPY_Thread_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "EBoxPY.Thread",
	.tp_basicsize = sizeof(EBoxPY_Thread),
	.tp_doc = EBoxPY_Thread__doc__,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_members = EBoxPY_Thread_Members,
	.tp_dealloc = EBoxPY_Thread_dealloc,
	.tp_repr = EBoxPY_Thread_repr,
	.tp_str = EBoxPY_Thread_repr,
	.tp_methods = EBoxPY_Thread_Methods,
};

static int _EBoxPY_Initialize_Thread(PyObject* self) {
	if (PyType_Ready(&EBoxPY_Thread_Type) < 0)
		return 0;
	PyModule_AddObject(self, "Thread", (PyObject*)&EBoxPY_Thread_Type);
	return 1;
}

static PyObject* _EBoxPY_Create_Thread_ID(unsigned long _ID) {
	PyObject* output = EBoxPY_Thread_Type.tp_alloc(&EBoxPY_Thread_Type, 1);
	if (!output)
		return NULL;
	EBOXPY_OBJECT_ZERO(EBoxPY_Thread, output);
	PEBoxPY_Thread thread = (PEBoxPY_Thread)output;
	thread->ID_ = _ID;
	thread->IsOpen_ = 0;
	thread->IsLocked_ = 0;
	thread->Thread_ = NULL;
	thread->Registers_ = PyDict_New();
	if (!thread->Registers_){
		Py_DECREF(output);
		return NULL;
	}
	return output;
}

static PyObject* _EBoxPY_Create_Thread_Handle(HANDLE _Handle) {
	PyObject* output = EBoxPY_Thread_Type.tp_alloc(&EBoxPY_Thread_Type, 1);
	if (!output) {
		CloseHandle(_Handle);
		return NULL;
	}
	EBOXPY_OBJECT_ZERO(EBoxPY_Thread, output);
	PEBoxPY_Thread thread = (PEBoxPY_Thread)output;
	thread->ID_ = GetThreadId(_Handle);
	thread->IsOpen_ = 1;
	thread->IsLocked_ = 0;
	thread->Thread_ = _Handle;
	thread->Registers_ = PyDict_New();
	if (!thread->Registers_){
		Py_DECREF(output);
		return NULL;
	}
	return output;
}

static void EBoxPY_Thread_dealloc(PyObject* self) {
	PEBoxPY_Thread thread = (PEBoxPY_Thread)self;
	Py_XDECREF(thread->Registers_);
	if (thread->IsLocked_)
		ResumeThread(thread->Thread_);
	if (thread->IsOpen_)
		CloseHandle(thread->Thread_);
	Py_TYPE(self)->tp_free(self);
}

static PyObject* EBoxPY_Thread_repr(PyObject* self) {
	char output[64];
	sprintf(output, "<EBoxPY.Thread: (0x%08x)>", ((PEBoxPY_Thread)self)->ID_);
	return PyUnicode_FromString(output);
}

static PyObject* EBoxPY_Thread_Open(PEBoxPY_Thread self) {
	if (self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsOpen_ was already True.");
		return NULL;
	}
	self->Thread_ = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)self->ID_);
	if (!self->Thread_ || self->Thread_ == INVALID_HANDLE_VALUE) {
		self->Thread_ = NULL;
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread failed to open Thread, OpenThread failed.");
		return NULL;
	}
	self->IsOpen_ = 1;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Thread_Close(PEBoxPY_Thread self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsOpen_ was already False.");
		return NULL;
	}
	if (self->IsLocked_) {
		if (!EBoxPY_Thread_Unlock(self)) {
			return NULL;
		}
	}
	CloseHandle(self->Thread_);
	self->Thread_ = NULL;
	self->IsOpen_ = 0;
	Py_INCREF(Py_None);
	return Py_None;
}

typedef struct _EBoxPY_Thread_Register_T {
	const char* Public_;
	unsigned long long Private_;
	unsigned long long Size_;
} _EBoxPY_Thread_Register, *_PEBoxPY_Thread_Register;

#define EBOXPY_THREAD_CREATE_REGISTER(_Public, _Private) { #_Public, (unsigned long long)offsetof(CONTEXT, _Private), sizeof(((CONTEXT*)0)->_Private) }

static _EBoxPY_Thread_Register _EBoxPY_Thread_Register_List[] = {
	EBOXPY_THREAD_CREATE_REGISTER(RAX, Rax),
	EBOXPY_THREAD_CREATE_REGISTER(RBX, Rbx),
	EBOXPY_THREAD_CREATE_REGISTER(RCX, Rcx),
	EBOXPY_THREAD_CREATE_REGISTER(RDX, Rdx),
	EBOXPY_THREAD_CREATE_REGISTER(RSI, Rsi),
	EBOXPY_THREAD_CREATE_REGISTER(RDI, Rdi),
	EBOXPY_THREAD_CREATE_REGISTER(RSP, Rsp),
	EBOXPY_THREAD_CREATE_REGISTER(RBP, Rbp),
	EBOXPY_THREAD_CREATE_REGISTER(R8, R8),
	EBOXPY_THREAD_CREATE_REGISTER(R9, R8),
	EBOXPY_THREAD_CREATE_REGISTER(R10, R10),
	EBOXPY_THREAD_CREATE_REGISTER(R11, R11),
	EBOXPY_THREAD_CREATE_REGISTER(R12, R12),
	EBOXPY_THREAD_CREATE_REGISTER(R13, R13),
	EBOXPY_THREAD_CREATE_REGISTER(R14, R14),
	EBOXPY_THREAD_CREATE_REGISTER(R15, R15),
	EBOXPY_THREAD_CREATE_REGISTER(RIP, Rip),
	EBOXPY_THREAD_CREATE_REGISTER(XMM0, Xmm0),
	EBOXPY_THREAD_CREATE_REGISTER(XMM1, Xmm1),
	EBOXPY_THREAD_CREATE_REGISTER(XMM2, Xmm2),
	EBOXPY_THREAD_CREATE_REGISTER(XMM3, Xmm3),
	EBOXPY_THREAD_CREATE_REGISTER(XMM4, Xmm4),
	EBOXPY_THREAD_CREATE_REGISTER(XMM5, Xmm5),
	EBOXPY_THREAD_CREATE_REGISTER(XMM6, Xmm6),
	EBOXPY_THREAD_CREATE_REGISTER(XMM7, Xmm7),
	EBOXPY_THREAD_CREATE_REGISTER(XMM8, Xmm8),
	EBOXPY_THREAD_CREATE_REGISTER(XMM9, Xmm9),
	EBOXPY_THREAD_CREATE_REGISTER(XMM10, Xmm10),
	EBOXPY_THREAD_CREATE_REGISTER(XMM11, Xmm11),
	EBOXPY_THREAD_CREATE_REGISTER(XMM12, Xmm12),
	EBOXPY_THREAD_CREATE_REGISTER(XMM13, Xmm13),
	EBOXPY_THREAD_CREATE_REGISTER(XMM14, Xmm14),
	EBOXPY_THREAD_CREATE_REGISTER(XMM15, Xmm15),
	EBOXPY_THREAD_CREATE_REGISTER(DR0, Dr0),
	EBOXPY_THREAD_CREATE_REGISTER(DR1, Dr1),
	EBOXPY_THREAD_CREATE_REGISTER(DR2, Dr2),
	EBOXPY_THREAD_CREATE_REGISTER(DR3, Dr3),
	EBOXPY_THREAD_CREATE_REGISTER(DR6, Dr6),
	EBOXPY_THREAD_CREATE_REGISTER(DR7, Dr7),
	{NULL}
};

static int _EBoxPY_Thread_Add_Register(PyObject* _Registers, _PEBoxPY_Thread_Register _Register, CONTEXT* _Context) {
	if (!PyDict_Check(_Registers))
		return 0;
	PyObject* value = _EBoxPY_Create_Bytes(_Register->Size_);
	if (!value)
		return 0;
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)value;
	memcpy(bytes->Allocation_, ((char*)_Context) + _Register->Private_, _Register->Size_);
	if (PyDict_SetItemString(_Registers, _Register->Public_, value) != 0) {
		Py_DECREF(value);
		return 0;
	}
	Py_DECREF(value);
	return 1;
}

static int _EBoxPY_Thread_Get_Register(PyObject* _Registers, _PEBoxPY_Thread_Register _Register, CONTEXT* _Context) {
	if (!PyDict_Check(_Registers))
		return 0;
	PyObject* value = PyDict_GetItemString(_Registers, _Register->Public_);
	if (!value)
		return 0;
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)value;
	if (bytes->Size_ != _Register->Size_)
		return 0;
	memcpy(((char*)_Context) + _Register->Private_, bytes->Allocation_, _Register->Size_);
	return 1;
}

static int _EBoxPY_Thread_Add_Context(PyObject* _Registers, CONTEXT* _Context) {
	PyDict_Clear(_Registers);
	for (_PEBoxPY_Thread_Register i = _EBoxPY_Thread_Register_List; i->Public_ != NULL; ++i) {
		if (!_EBoxPY_Thread_Add_Register(_Registers, i, _Context)) {
			PyDict_Clear(_Registers);
			return 0;
		}
	}
	return 1;
}

static int _EBoxPY_Thread_Get_Context(PyObject* _Registers, CONTEXT* _Context) {
	for (_PEBoxPY_Thread_Register i = _EBoxPY_Thread_Register_List; i->Public_ != NULL; ++i) {
		if (!_EBoxPY_Thread_Get_Register(_Registers, i, _Context)) {
			return 0;
		}
	}
	return 1;
}

static PyObject* EBoxPY_Thread_Lock(PEBoxPY_Thread self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsOpen_ was False.");
		return NULL;
	}
	if (self->IsLocked_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsLocked_ was already True.");
		return NULL;
	}
	if (SuspendThread(self->Thread_) == (DWORD)-1){
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread Failed to Suspend Thread.");
		return NULL;
	}
	self->Save_.ContextFlags = CONTEXT_ALL;
	if (!GetThreadContext(self->Thread_, &self->Save_)) {
		ResumeThread(self->Thread_);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread Failed to Read Thread Context.");
		return NULL;
	}
	if (!_EBoxPY_Thread_Add_Context(self->Registers_, &self->Save_)) {
		ResumeThread(self->Thread_);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread Failed to Add Thread Context.");
		return NULL;
	}
	self->IsLocked_ = 1;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Thread_Unlock(PEBoxPY_Thread self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsOpen_ was False.");
		return NULL;
	}
	if (!self->IsLocked_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread.IsLocked_ was already False.");
		return NULL;
	}
	self->Save_.ContextFlags = CONTEXT_ALL;
	if (!_EBoxPY_Thread_Get_Context(self->Registers_, &self->Save_)) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread Failed to Get Context from Dictionary.");
		return NULL;
	}
	PyDict_Clear(self->Registers_);
	if (!SetThreadContext(self->Thread_, &self->Save_)) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Thread Failed to Set Thread Context.");
		return NULL;
	}
	ResumeThread(self->Thread_);
	self->IsLocked_ = 0;
	Py_INCREF(Py_None);
	return Py_None;
}

/*
 *
 * EBoxPY.Process
 *
 */

PyDoc_STRVAR(EBoxPY_Process__doc__, "EBoxPY Process object, defines a running Process.");

typedef struct EBoxPY_Process_T {
	//
	PyObject_HEAD
	//
	HANDLE Process_;
	char IsOpen_;
	unsigned long ID_;
	//
	PyObject* Name_;
	//
} EBoxPY_Process, *PEBoxPY_Process;

static void EBoxPY_Process_dealloc(PyObject* self);
static PyObject* EBoxPY_Process_repr(PyObject* self);

static PyObject* EBoxPY_Process_Open(PEBoxPY_Process self);
static PyObject* EBoxPY_Process_Close(PEBoxPY_Process self);
static PyObject* EBoxPY_Process_GetModules(PEBoxPY_Process self);
static PyObject* EBoxPY_Process_GetRegion(PEBoxPY_Process self, PyObject* address);
static PyObject* EBoxPY_Process_ReadTo(PEBoxPY_Process self, PyObject* args);
static PyObject* EBoxPY_Process_WriteFrom(PEBoxPY_Process self, PyObject* args);
static PyObject* EBoxPY_Process_GetThreads(PEBoxPY_Process self);
static PyObject* EBoxPY_Process_Allocate(PEBoxPY_Process self, PyObject* args);
static PyObject* EBoxPY_Process_Free(PEBoxPY_Process self, PyObject* allocation);

static PyMemberDef EBoxPY_Process_Members[] = {
	{"Process_", T_ULONGLONG, offsetof(EBoxPY_Process, Process_), READONLY, PyDoc_STR("The Handle to the Process.")},
	{"IsOpen_", T_BOOL, offsetof(EBoxPY_Process, IsOpen_), READONLY, PyDoc_STR("True if the Process is Open, False if not, defines validity of the Process Handle.")},
	{"ID_", T_ULONG, offsetof(EBoxPY_Process, ID_), READONLY, PyDoc_STR("The ID of the Process.")},
	{"Name_", T_OBJECT, offsetof(EBoxPY_Process, Name_), READONLY, PyDoc_STR("The Name of the Process, *.exe etc.")},
	{NULL}
};

static PyMethodDef EBoxPY_Process_Methods[] = {
	{"Open", (PyCFunction)EBoxPY_Process_Open, METH_NOARGS, PyDoc_STR("EBoxPY.Process.Open()\nOpens the Process if it is not already open.")},
	{"Close", (PyCFunction)EBoxPY_Process_Close, METH_NOARGS, PyDoc_STR("EBoxPY.Process.Close()\nCloses the Process, Closes the Handle.")},
	{"GetModules", (PyCFunction)EBoxPY_Process_GetModules, METH_NOARGS, PyDoc_STR("EBoxPY.Process.GetModules() -> { \"*.dll\" : EBoxPY.Module(...), ... }\nRetrieves a dictionary of Modules currently loaded in the Process.")},
	{"GetRegion", (PyCFunction)EBoxPY_Process_GetRegion, METH_O, PyDoc_STR("EBoxPY.Process.GetRegion(_Address) -> EBoxPY.Region\nGets the Region at the specified Address.")},
	{"ReadTo", (PyCFunction)EBoxPY_Process_ReadTo, METH_VARARGS, PyDoc_STR("EBoxPY.Process.ReadTo(_Address, _Bytes, _Start, _Size)\nReads Bytes from Address into specified Bytes object.")},
	{"WriteFrom", (PyCFunction)EBoxPY_Process_WriteFrom, METH_VARARGS, PyDoc_STR("EBoxPY.Process.WriteFrom(_Address, _Bytes, _Start, _Size)\nWrites Bytes to specified Address.")},
	{"GetThreads", (PyCFunction)EBoxPY_Process_GetThreads, METH_NOARGS, PyDoc_STR("EBoxPY.Process.GetThreads() -> [ EBoxPY.Thread(...), ... ]\nRetrieves a list of Threads running in the Process.")},
	{"Allocate", (PyCFunction)EBoxPY_Process_Allocate, METH_VARARGS, PyDoc_STR("EBoxPY.Process.Allocate(_Size, _Address=None, _Range=None) -> int\nAllocates Memory in the Process with a given Size + Location options.")},
	{"Free", (PyCFunction)EBoxPY_Process_Free, METH_O, PyDoc_STR("EBoxPY.Process.Free(_Allocation)\nFrees Memory in a Process.")},
	{NULL}
};

static PyTypeObject EBoxPY_Process_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "EBoxPY.Process",
	.tp_basicsize = sizeof(EBoxPY_Process),
	.tp_doc = EBoxPY_Process__doc__,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_members = EBoxPY_Process_Members,
	.tp_dealloc = EBoxPY_Process_dealloc,
	.tp_repr = EBoxPY_Process_repr,
	.tp_str = EBoxPY_Process_repr,
	.tp_methods = EBoxPY_Process_Methods,
};

static int _EBoxPY_Initialize_Process(PyObject* self) {
	if (PyType_Ready(&EBoxPY_Process_Type) < 0)
		return 0;
	PyModule_AddObject(self, "Process", (PyObject*)&EBoxPY_Process_Type);
	return 1;
}

static PyObject* _EBoxPY_Create_Process(PROCESSENTRY32W _Entry) {
	PyObject* output = EBoxPY_Process_Type.tp_alloc(&EBoxPY_Process_Type, 1);
	if (!output)
		return NULL;
	EBOXPY_OBJECT_ZERO(EBoxPY_Process, output);
	PEBoxPY_Process process = (PEBoxPY_Process)output;
	process->Process_ = NULL;
	process->IsOpen_ = (char)0;
	process->ID_ = (unsigned long)_Entry.th32ProcessID;
	process->Name_ = PyUnicode_FromWideChar(_Entry.szExeFile, -1);
	if (!process->Name_) {
		Py_DECREF(output);
		return NULL;
	}
	return output;
}

static PyObject* _EBoxPY_GetProcessNameFromID(unsigned long _ID) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	PROCESSENTRY32W entry = {0};
	entry.dwSize = sizeof(PROCESSENTRY32W);
	for (BOOL b = Process32FirstW(snapshot, &entry); b == TRUE; b = Process32NextW(snapshot, &entry)) {
		if (entry.th32ProcessID == _ID) {
			CloseHandle(snapshot);
			return PyUnicode_FromWideChar(entry.szExeFile, -1);
		}
	}
	CloseHandle(snapshot);
	return NULL;
}

static PyObject* EBoxPY_StartProcess(PyObject* self, PyObject* args) {
	if (PyTuple_Size(args) != 2) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.StartProcess Takes 2 Arguments.");
		return NULL;
	}
	PyObject* _Executable = PyTuple_GetItem(args, 0);
	PyObject* _Suspended = PyTuple_GetItem(args, 1);
	if (!PyUnicode_Check(_Executable)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.StartProcess Requires _Executable to be str.");
		return NULL;
	}
	if (!PyBool_Check(_Suspended)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.StartProcess Requires _Suspended to be bool.");
		return NULL;
	}
	int suspended = PyObject_IsTrue(_Suspended);
	Py_ssize_t length = 0;
	wchar_t* executable = PyUnicode_AsWideCharString(_Executable, &length);
	if (!executable) {
		return NULL;
	}
	STARTUPINFOW startup = {0};
	startup.cb = sizeof(STARTUPINFOW);
	PROCESS_INFORMATION information = {0};
	if (!CreateProcessW(NULL, executable, NULL, NULL, FALSE, (suspended ? CREATE_SUSPENDED : 0), NULL, NULL, &startup, &information)) {
		PyMem_Free(executable);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.StartProcess Failed to Start Process.");
		return NULL;
	}
	PyMem_Free(executable);
	PyObject* thread = _EBoxPY_Create_Thread_Handle(information.hThread);
	if (!thread){
		CloseHandle(information.hProcess);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.StartProcess Failed to Create Thread.");
		return NULL;
	}
	if (suspended) {
		if (!EBoxPY_Thread_Lock((PEBoxPY_Thread)thread)) {
			CloseHandle(information.hProcess);
			Py_DECREF(thread);
			return NULL;
		}
		ResumeThread(information.hThread); // Because EBoxPY_Thread_Lock suspends!
	}
	PyObject* output = EBoxPY_Process_Type.tp_alloc(&EBoxPY_Process_Type, 1);
	if (!output) {
		CloseHandle(information.hProcess);
		Py_DECREF(thread);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.StartProcess Failed to Allocate EBoxPY.Process.");
		return NULL;
	}
	EBOXPY_OBJECT_ZERO(EBoxPY_Process, output);
	PEBoxPY_Process process = (PEBoxPY_Process)output;
	process->ID_ = (unsigned long)information.dwProcessId;
	process->IsOpen_ = 1;
	process->Process_ = information.hProcess;
	process->Name_ = _EBoxPY_GetProcessNameFromID(process->ID_);
	if (!process->Name_) {
		Py_DECREF(thread);
		Py_DECREF(output);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.StartProcess Failed to Get Process Name from ID.");
		return NULL;
	}
	return PyTuple_Pack(2, output, thread);
}

static void EBoxPY_Process_dealloc(PyObject* self) {
	PEBoxPY_Process process = (PEBoxPY_Process)self;
	Py_XDECREF(process->Name_);
	if (process->IsOpen_)
		CloseHandle(process->Process_);
	Py_TYPE(self)->tp_free(self);
}

static PyObject* EBoxPY_Process_repr(PyObject* self) {
	PEBoxPY_Process process = (PEBoxPY_Process)self;
	PyObject* encoded = PyUnicode_AsEncodedString(process->Name_, "UTF-8", "strict");
	if (!encoded)
		return NULL;
	char* name = PyBytes_AS_STRING(encoded);
	char output[512];
	sprintf(output, "<EBoxPY.Process: (%s)>", name);
	Py_DECREF(encoded);
	return PyUnicode_FromString(output);
}

static PyObject* EBoxPY_Process_Open(PEBoxPY_Process self) {
	if (self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was already True.");
		return NULL;
	}
	self->Process_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)self->ID_);
	if (!self->Process_ || self->Process_ == INVALID_HANDLE_VALUE) {
		self->Process_ = NULL;
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process failed to open Process, OpenProcess failed.");
		return NULL;
	}
	self->IsOpen_ = (char)1;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Process_Close(PEBoxPY_Process self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was already False.");
		return NULL;
	}
	CloseHandle(self->Process_);
	self->Process_ = NULL;
	self->IsOpen_ = (char)0;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Process_GetModules(PEBoxPY_Process self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)self->ID_);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.GetModules failed to Create Toolhelp32 Snapshot, CreateToolhelp32Snapshot failed.");
		return NULL;
	}
	MODULEENTRY32W entry = {0};
	entry.dwSize = sizeof(MODULEENTRY32W);
	PyObject* dictionary = PyDict_New();
	if (!dictionary) {
		CloseHandle(snapshot);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.GetModules failed to Create Dictionary.");
		return NULL;
	}
	for (BOOL b = Module32FirstW(snapshot, &entry); b == TRUE; b = Module32NextW(snapshot, &entry)) {
		PyObject* _module = _EBoxPY_Create_Module(self->Process_, entry);
		if (_module) {
			PyDict_SetItem(dictionary, ((PEBoxPY_Module)_module)->Name_, _module);
			Py_DECREF(_module);
		}
	}
	CloseHandle(snapshot);
	return dictionary;
}

static PyObject* EBoxPY_Process_GetRegion(PEBoxPY_Process self, PyObject* address) {
	if (!PyLong_Check(address)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.GetRegion requires _Address to be of type int.");
		return NULL;
	}
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	return _EBoxPY_Create_Region(self->Process_, PyLong_AsUnsignedLongLong(address));
}

/*
static PyObject* EBoxPY_Process_Read(PEBoxPY_Process self, PyObject* args) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	if (PyTuple_Size(args) != 2) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Read requires 2 arguments.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 0))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Read requires _Address to be int.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 1))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Read requires _Type to be int.");
		return NULL;
	}
	unsigned long long address = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Read requires a valid int _Address.");
		return NULL;
	}
	unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 1));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Read requires a valid int _Size.");
		return NULL;
	}
	PyObject* output = _EBoxPY_Create_Bytes(size);
	if (!output) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.Read failed to create Bytes.");
		return NULL;
	}
	PyObject* _args = PyTuple_Pack(2, PyTuple_GetItem(args, 0), output);
	PyObject* result = EBoxPY_Process_ReadTo(self, _args);
	Py_DECREF(_args);
	if (!result) {
		Py_DECREF(output);
		return NULL;
	}
	else {
		Py_DECREF(result);
		return output;
	}
}
*/

static PyObject* EBoxPY_Process_ReadTo(PEBoxPY_Process self, PyObject* args) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	if (PyTuple_Size(args) != 4) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires 2 arguments.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 0))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires _Address to be int.");
		return NULL;
	}
	if (!PyObject_IsInstance(PyTuple_GetItem(args, 1), (PyObject*)&EBoxPY_Bytes_Type)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires _Bytes to be a Bytes object.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 2))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires _Start to be int.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 3))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires _Size to be int.");
		return NULL;
	}
	unsigned long long address = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires a valid int _Address.");
		return NULL;
	}
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)PyTuple_GetItem(args, 1);
	unsigned long long start = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 2));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires a valid int _Start.");
		return NULL;
	}
	unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 3));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.ReadTo requires a valid int _Size.");
		return NULL;
	}
	if (start + size > bytes->Size_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.ReadTo address out of bounds.");
		return NULL;
	}
	if (ReadProcessMemory(self->Process_, (void*)address, bytes->Allocation_ + start, size, NULL) == FALSE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.ReadTo failed to Read memory.");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Process_WriteFrom(PEBoxPY_Process self, PyObject* args) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	if (PyTuple_Size(args) != 4) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires 2 arguments.");
		return NULL;
	}
	if (!PyLong_Check(PyTuple_GetItem(args, 0))) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires _Address to be int.");
		return NULL;
	}
	if (!PyObject_IsInstance(PyTuple_GetItem(args, 1), (PyObject*)&EBoxPY_Bytes_Type)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires _Bytes to be a Bytes object.");
		return NULL;
	}
	unsigned long long address = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires a valid int _Address.");
		return NULL;
	}
	PEBoxPY_Bytes bytes = (PEBoxPY_Bytes)PyTuple_GetItem(args, 1);
	unsigned long long start = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 2));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires a valid int _Start.");
		return NULL;
	}
	unsigned long long size = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 3));
	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.WriteFrom requires a valid int _Size.");
		return NULL;
	}
	if (start + size > bytes->Size_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.WriteFrom address out of bounds.");
		return NULL;
	}
	if (WriteProcessMemory(self->Process_, (void*)address, bytes->Allocation_ + start, size, NULL) == FALSE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.WriteFrom failed to Write memory.");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* EBoxPY_Process_GetThreads(PEBoxPY_Process self) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	PyObject* output = PyList_New(0);
	if (!output) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.GetThreads Failed to Create output list.");
		return NULL;
	}
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE){
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.GetThreads Failed to Create Snapshot.");
		return NULL;
	}
	THREADENTRY32 entry = {0};
	entry.dwSize = sizeof(THREADENTRY32);
	for (BOOL b = Thread32First(snapshot, &entry); b == TRUE; b = Thread32Next(snapshot, &entry)) {
		if (entry.th32OwnerProcessID == self->ID_) {
			PyObject* thread = _EBoxPY_Create_Thread_ID(entry.th32ThreadID);
			if (!thread)
				continue;
			PyList_Append(output, thread);
			Py_DECREF(thread);
		}
	}
	CloseHandle(snapshot);
	return output;
}

static PyObject* EBoxPY_Process_Allocate(PEBoxPY_Process self, PyObject* args) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	Py_ssize_t length = PyTuple_Size(args);
	if (length < 1 || length > 3){
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Allocate Takes 1-3 Arguments.");
		return NULL;
	}
	PyObject* size = (PyTuple_GetItem(args, 0) == Py_None ? NULL : PyTuple_GetItem(args, 0));
	PyObject* address = (length < 2 ? NULL : (PyTuple_GetItem(args, 1) == Py_None ? NULL : PyTuple_GetItem(args, 1)));
	PyObject* range = (length < 3 ? NULL : (PyTuple_GetItem(args, 2) == Py_None ? NULL : PyTuple_GetItem(args, 2)));
	if (!size || !PyLong_Check(size)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Allocate Takes int as _Size.");
		return NULL;
	}
	unsigned long long _size = PyLong_AsUnsignedLongLong(size);
	long long _address = (!address ? 0 : PyLong_AsLongLong(address));
	long long _range = (!range ? 0 : PyLong_AsLongLong(range));
	if (!address && range) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Allocate Invalid Arguments.");
		return NULL;
	}
	else if (!address && !range) {
		unsigned long long __address = (unsigned long long)VirtualAllocEx(self->Process_, NULL, _size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!__address) {
			PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.Allocate Failed to Allocate Memory.");
			return NULL;
		}
		return PyLong_FromUnsignedLongLong(__address);
	}
	else if (address && !range) {
		unsigned long long __address = (unsigned long long)VirtualAllocEx(self->Process_, (void*)_address, _size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!__address) {
			PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.Allocate Failed to Allocate Memory.");
			return NULL;
		}
		return PyLong_FromUnsignedLongLong(__address);
	}
	else {
		SYSTEM_INFO information;
		GetSystemInfo(&information);
		long long start = _address - _range;
		start &= ~((unsigned long long)(information.dwAllocationGranularity - 1));
		if (start < (_address - _range))
			start += information.dwAllocationGranularity;
		long long end = _address + _range - _size;
		end &= ~((unsigned long long)(information.dwAllocationGranularity - 1));
		if (end > (_address + _range - (long long)_size))
			end -= information.dwAllocationGranularity;
		for (long long i = start; i < end; i += 0x1000) {
			unsigned long long __address = (unsigned long long)VirtualAllocEx(self->Process_, (void*)i, _size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (__address)
				return PyLong_FromUnsignedLongLong(__address);
		}
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.Allocate Failed to Allocate Memory.");
		return NULL;
	}
}

static PyObject* EBoxPY_Process_Free(PEBoxPY_Process self, PyObject* allocation) {
	if (!self->IsOpen_) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.IsOpen_ was False.");
		return NULL;
	}
	if (!PyLong_Check(allocation)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.Process.Free requires int as _Allocation.");
		return NULL;
	}
	unsigned long long address = PyLong_AsUnsignedLongLong(allocation);
	if (!VirtualFreeEx(self->Process_, (void*)address, 0, MEM_RELEASE)) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.Process.Free Failed to Free Memory.");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

/*
 *
 * Global
 *
 */

static PyObject* EBoxPY_GetCurrentProcess(PyObject* self) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetCurrentProcess failed to Create Toolhelp32 Snapshot, CreateToolhelp32Snapshot failed.");
		return NULL;
	}
	PROCESSENTRY32W entry = {0};
	entry.dwSize = sizeof(PROCESSENTRY32W);
	for (BOOL b = Process32FirstW(snapshot, &entry); b == TRUE; b = Process32NextW(snapshot, &entry)) {
		if (entry.th32ProcessID == GetCurrentProcessId()) {
			PyObject* process = _EBoxPY_Create_Process(entry);
			CloseHandle(snapshot);
			if (!process) {
				PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetCurrentProcess failed to Create Current Process.");
				return NULL;
			}
			else {
				return process;
			}
		}
	}
	CloseHandle(snapshot);
	PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetCurrentProcess failed to Locate Current Process.");
	return NULL;
}

static PyObject* EBoxPY_GetProcesses(PyObject* self) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetProcesses failed to Create Toolhelp32 Snapshot, CreateToolhelp32Snapshot failed.");
		return NULL;
	}
	PROCESSENTRY32W entry = {0};
	entry.dwSize = sizeof(PROCESSENTRY32W);
	PyObject* list = PyList_New(0);
	if (!list) {
		CloseHandle(snapshot);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetProcesses failed to Create List.");
		return NULL;
	}
	for (BOOL b = Process32FirstW(snapshot, &entry); b == TRUE; b = Process32NextW(snapshot, &entry)) {
		PyObject* process = _EBoxPY_Create_Process(entry);
		if (process) {
			PyList_Append(list, process);
			Py_DECREF(process);
		}
	}
	CloseHandle(snapshot);
	return list;
}

static PyObject* EBoxPY_GetProcessesByName(PyObject* self, PyObject* key) {
	if (!PyUnicode_Check(key)) {
		PyErr_SetString(PyExc_TypeError, "EBoxPY.GetProcesses requires _Name to be of type string.");
		return NULL;
	}
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!snapshot || snapshot == INVALID_HANDLE_VALUE) {
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetProcesses failed to Create Toolhelp32 Snapshot, CreateToolhelp32Snapshot failed.");
		return NULL;
	}
	PROCESSENTRY32W entry = {0};
	entry.dwSize = sizeof(PROCESSENTRY32W);
	PyObject* list = PyList_New(0);
	if (!list) {
		CloseHandle(snapshot);
		PyErr_SetString(PyExc_RuntimeError, "EBoxPY.GetProcesses failed to Create List.");
		return NULL;
	}
	for (BOOL b = Process32FirstW(snapshot, &entry); b == TRUE; b = Process32NextW(snapshot, &entry)) {
		PyObject* process = _EBoxPY_Create_Process(entry);
		if (process) {
			if (PyUnicode_Compare(((PEBoxPY_Process)process)->Name_, key) == 0)
				PyList_Append(list, process);
			Py_DECREF(process);
		}
	}
	CloseHandle(snapshot);
	return list;
}

static PyMethodDef EBoxPY_Global_Methods[] = {
	{"GetCurrentProcess", (PyCFunction)EBoxPY_GetCurrentProcess, METH_NOARGS, PyDoc_STR("EBoxPY.GetCurrentProcess() -> EBoxPY.Process(...)\n.")},
	{"GetProcesses", (PyCFunction)EBoxPY_GetProcesses, METH_NOARGS, PyDoc_STR("EBoxPY.GetProcesses() -> [EBoxPY.Process(...), ...]\nRetrieves a list of Processes, the Processes are not yet Opened.")},
	{"GetProcessesByName", (PyCFunction)EBoxPY_GetProcessesByName, METH_O, PyDoc_STR("EBoxPY.GetProcessesByName(_Name) -> [EBoxPY.Process(...), ...]\nRetrieves a list of Processes with EBoxPY.Process.Name_ == _Name, the Processes are not yet Opened.")},
	{"StartProcess", (PyCFunction)EBoxPY_StartProcess, METH_VARARGS, PyDoc_STR("EBoxPY.StartProcess(_Executable, _Suspended) -> EBoxPY.Process\nLaunches a new Process.")},
	{NULL}
};

static struct PyModuleDef EBoxPY_Module_Definition = {
	PyModuleDef_HEAD_INIT,
	"EBoxPY",
	"EBoxPY - External Memory Hacking Library for Python x64",
	-1,
	EBoxPY_Global_Methods,
	NULL,
	NULL,
	NULL,
	NULL,
};

PyMODINIT_FUNC PyInit_EBoxPY(void) {
	PyObject* _module = PyModule_Create(&EBoxPY_Module_Definition);
	if (!_module)
		return NULL;
	_EBoxPY_Initialize_Region(_module);
	_EBoxPY_Initialize_Module(_module);
	_EBoxPY_Initialize_Bytes(_module);
	_EBoxPY_Initialize_Thread(_module);
	_EBoxPY_Initialize_Process(_module);
	return _module;
}