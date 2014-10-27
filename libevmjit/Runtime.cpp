
#include "Runtime.h"

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Function.h>

#include <libevm/VM.h>

#include "Type.h"

namespace dev
{
namespace eth
{
namespace jit
{

llvm::StructType* RuntimeData::getType()
{
	static llvm::StructType* type = nullptr;
	if (!type)
	{
		llvm::Type* elems[] =
		{
			llvm::ArrayType::get(Type::i256, _size)
		};
		type = llvm::StructType::create(elems, "RuntimeData");
	}
	return type;
}

namespace
{
llvm::Twine getName(RuntimeData::Index _index)
{
	switch (_index)
	{
	default:						return "data";
	case RuntimeData::Gas:			return "gas";
	case RuntimeData::Address:		return "address";
	case RuntimeData::Caller:		return "caller";
	case RuntimeData::Origin:		return "origin";
	case RuntimeData::CallValue:	return "callvalue";
	}
}
}

static Runtime* g_runtime;	// FIXME: Remove

Runtime::Runtime(u256 _gas, ExtVMFace& _ext):
	m_ext(_ext)
{
	assert(!g_runtime);
	g_runtime = this;
	set(RuntimeData::Gas, _gas);
	set(RuntimeData::Address, fromAddress(_ext.myAddress));
	set(RuntimeData::Caller, fromAddress(_ext.caller));
	set(RuntimeData::Origin, fromAddress(_ext.origin));
	set(RuntimeData::CallValue, _ext.value);
}

Runtime::~Runtime()
{
	g_runtime = nullptr;
}

void Runtime::set(RuntimeData::Index _index, u256 _value)
{
	m_data.elems[_index] = eth2llvm(_value);
}


ExtVMFace& Runtime::getExt()
{
	return g_runtime->m_ext;
}

u256 Runtime::getGas() const
{
	return llvm2eth(m_data.elems[RuntimeData::Gas]);
}

extern "C" {
	EXPORT i256 mem_returnDataOffset;	// FIXME: Dis-globalize
	EXPORT i256 mem_returnDataSize;
}

bytesConstRef Runtime::getReturnData() const
{
	// TODO: Handle large indexes
	auto offset = static_cast<size_t>(llvm2eth(mem_returnDataOffset));
	auto size = static_cast<size_t>(llvm2eth(mem_returnDataSize));
	return {m_memory.data() + offset, size};
}


RuntimeManager::RuntimeManager(llvm::IRBuilder<>& _builder): CompilerHelper(_builder)
{
	m_dataPtr = new llvm::GlobalVariable(*getModule(), Type::RuntimePtr, false, llvm::GlobalVariable::PrivateLinkage, llvm::UndefValue::get(Type::RuntimePtr), "rt");

	// Export data
	auto mainFunc = getMainFunction();
	llvm::Value* dataPtr = &mainFunc->getArgumentList().back();
	m_builder.CreateStore(dataPtr, m_dataPtr);
}

llvm::Value* RuntimeManager::getRuntimePtr()
{
	// TODO: If in main function - get it from param
	return m_builder.CreateLoad(m_dataPtr);
}

llvm::Value* RuntimeManager::get(RuntimeData::Index _index)
{
	llvm::Value* idxList[] = {m_builder.getInt32(0), m_builder.getInt32(0), m_builder.getInt32(_index)};
	auto ptr = m_builder.CreateInBoundsGEP(getRuntimePtr(), idxList, getName(_index) + "Ptr");
	return m_builder.CreateLoad(ptr, getName(_index));
}

llvm::Value* RuntimeManager::getGas()
{
	return get(RuntimeData::Gas);
}

void RuntimeManager::setGas(llvm::Value* _gas)
{
	llvm::Value* idxList[] = {m_builder.getInt32(0), m_builder.getInt32(0), m_builder.getInt32(RuntimeData::Gas)};
	auto ptr = m_builder.CreateInBoundsGEP(getRuntimePtr(), idxList, "gasPtr");
	m_builder.CreateStore(_gas, ptr);
}

}
}
}
