// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Optimizer.hpp"

#include "src/IceCfg.h"
#include "src/IceCfgNode.h"

#include <vector>

namespace
{
	class Optimizer
	{
	public:
		void run(Ice::Cfg *function);

	private:
		void analyzeUses(Ice::Cfg *function);
		void eliminateDeadCode();
		void eliminateUnitializedLoads();
		void eliminateLoadsFollowingSingleStore();
		void optimizeStoresInSingleBasicBlock();

		void replace(Ice::Inst *instruction, Ice::Operand *newValue);
		void deleteInstruction(Ice::Inst *instruction);
		bool isDead(Ice::Inst *instruction);

		static const Ice::InstIntrinsicCall *asLoadSubVector(const Ice::Inst *instruction);
		static const Ice::InstIntrinsicCall *asStoreSubVector(const Ice::Inst *instruction);
		static bool isLoad(const Ice::Inst &instruction);
		static bool isStore(const Ice::Inst &instruction);
		static Ice::Operand *storeAddress(const Ice::Inst *instruction);
		static Ice::Operand *loadAddress(const Ice::Inst *instruction);
		static Ice::Operand *storeData(const Ice::Inst *instruction);
		static std::size_t storeSize(const Ice::Inst *instruction);
		static bool loadTypeMatchesStore(const Ice::Inst *load, const Ice::Inst *store);

		Ice::Cfg *function;
		Ice::GlobalContext *context;

		struct Uses : std::vector<Ice::Inst*>
		{
			bool areOnlyLoadStore() const;
			void insert(Ice::Operand *value, Ice::Inst *instruction);
			void erase(Ice::Inst *instruction);

			std::vector<Ice::Inst*> loads;
			std::vector<Ice::Inst*> stores;
		};

		struct LoadStoreInst
		{
			LoadStoreInst(Ice::Inst* inst, bool isStore)
			  : inst(inst),
			    address(isStore ? storeAddress(inst) : loadAddress(inst)),
			    isStore(isStore)
			{
			}

			Ice::Inst* inst;
			Ice::Operand *address;
			bool isStore;
		};

		Optimizer::Uses* getUses(Ice::Operand*);
		void setUses(Ice::Operand*, Optimizer::Uses*);
		bool hasUses(Ice::Operand*) const;

		Ice::CfgNode* getNode(Ice::Inst*);
		void setNode(Ice::Inst*, Ice::CfgNode*);

		Ice::Inst* getDefinition(Ice::Variable*);
		void setDefinition(Ice::Variable*, Ice::Inst*);

		const std::vector<LoadStoreInst>& getLoadStoreInsts(Ice::CfgNode*);
		void setLoadStoreInsts(Ice::CfgNode*, std::vector<LoadStoreInst>*);
		bool hasLoadStoreInsts(Ice::CfgNode* node) const;

		std::vector<Optimizer::Uses*> allocatedUses;
	};

	void Optimizer::run(Ice::Cfg *function)
	{
		this->function = function;
		this->context = function->getContext();

		analyzeUses(function);

		eliminateDeadCode();
		eliminateUnitializedLoads();
		eliminateLoadsFollowingSingleStore();
		optimizeStoresInSingleBasicBlock();
		eliminateDeadCode();

		for(auto uses : allocatedUses)
		{
			delete uses;
		}
		allocatedUses.clear();
	}

	void Optimizer::eliminateDeadCode()
	{
		bool modified;
		do
		{
			modified = false;
			for(Ice::CfgNode *basicBlock : function->getNodes())
			{
				for(Ice::Inst &inst : Ice::reverse_range(basicBlock->getInsts()))
				{
					if(inst.isDeleted())
					{
						continue;
					}

					if(isDead(&inst))
					{
						deleteInstruction(&inst);
						modified = true;
					}
				}
			}
		}
		while(modified);
	}

	void Optimizer::eliminateUnitializedLoads()
	{
		Ice::CfgNode *entryBlock = function->getEntryNode();

		for(Ice::Inst &alloca : entryBlock->getInsts())
		{
			if(alloca.isDeleted())
			{
				continue;
			}

			if(!llvm::isa<Ice::InstAlloca>(alloca))
			{
				break;   // Allocas are all at the top
			}

			Ice::Operand *address = alloca.getDest();

			if(!hasUses(address))
			{
				continue;
			}

			const auto &addressUses = *getUses(address);

			if(!addressUses.areOnlyLoadStore())
			{
				continue;
			}

			if(addressUses.stores.empty())
			{
				for(Ice::Inst *load : addressUses.loads)
				{
					Ice::Variable *loadData = load->getDest();

					if(hasUses(loadData))
					{
						for(Ice::Inst *use : *getUses(loadData))
						{
							for(Ice::SizeT i = 0; i < use->getSrcSize(); i++)
							{
								if(use->getSrc(i) == loadData)
								{
									auto *undef = context->getConstantUndef(loadData->getType());

									use->replaceSource(i, undef);
								}
							}
						}

						setUses(loadData, nullptr);
					}

					load->setDeleted();
				}

				alloca.setDeleted();
				setUses(address, nullptr);
			}
		}
	}

	void Optimizer::eliminateLoadsFollowingSingleStore()
	{
		Ice::CfgNode *entryBlock = function->getEntryNode();

		for(Ice::Inst &alloca : entryBlock->getInsts())
		{
			if(alloca.isDeleted())
			{
				continue;
			}

			if(!llvm::isa<Ice::InstAlloca>(alloca))
			{
				break;   // Allocas are all at the top
			}

			Ice::Operand *address = alloca.getDest();

			if(!hasUses(address))
			{
				continue;
			}

			auto &addressUses = *getUses(address);

			if(!addressUses.areOnlyLoadStore())
			{
				continue;
			}

			if(addressUses.stores.size() == 1)
			{
				Ice::Inst *store = addressUses.stores[0];
				Ice::Operand *storeValue = storeData(store);

				for(Ice::Inst *load = &*++store->getIterator(), *next = nullptr; load != next; next = load, load = &*++store->getIterator())
				{
					if(load->isDeleted() || !isLoad(*load))
					{
						continue;
					}

					if(loadAddress(load) != address)
					{
						continue;
					}

					if(!loadTypeMatchesStore(load, store))
					{
						continue;
					}

					replace(load, storeValue);

					for(size_t i = 0; i < addressUses.loads.size(); i++)
					{
						if(addressUses.loads[i] == load)
						{
							addressUses.loads[i] = addressUses.loads.back();
							addressUses.loads.pop_back();
							break;
						}
					}

					for(size_t i = 0; i < addressUses.size(); i++)
					{
						if(addressUses[i] == load)
						{
							addressUses[i] = addressUses.back();
							addressUses.pop_back();
							break;
						}
					}

					if(addressUses.size() == 1)
					{
						assert(addressUses[0] == store);

						alloca.setDeleted();
						store->setDeleted();
						setUses(address, nullptr);

						if(hasUses(storeValue))
						{
							auto &valueUses = *getUses(storeValue);

							for(size_t i = 0; i < valueUses.size(); i++)
							{
								if(valueUses[i] == store)
								{
									valueUses[i] = valueUses.back();
									valueUses.pop_back();
									break;
								}
							}

							if(valueUses.empty())
							{
								setUses(storeValue, nullptr);
							}
						}

						break;
					}
				}
			}
		}
	}

	void Optimizer::optimizeStoresInSingleBasicBlock()
	{
		Ice::CfgNode *entryBlock = function->getEntryNode();

		std::vector<std::vector<LoadStoreInst>* > allocatedVectors;

		for(Ice::Inst &alloca : entryBlock->getInsts())
		{
			if(alloca.isDeleted())
			{
				continue;
			}

			if(!llvm::isa<Ice::InstAlloca>(alloca))
			{
				break;   // Allocas are all at the top
			}

			Ice::Operand *address = alloca.getDest();

			if(!hasUses(address))
			{
				continue;
			}

			const auto &addressUses = *getUses(address);

			if(!addressUses.areOnlyLoadStore())
			{
				continue;
			}

			Ice::CfgNode *singleBasicBlock = getNode(addressUses.stores[0]);

			for(size_t i = 1; i < addressUses.stores.size(); i++)
			{
				Ice::Inst *store = addressUses.stores[i];
				if(getNode(store) != singleBasicBlock)
				{
					singleBasicBlock = nullptr;
					break;
				}
			}

			if(singleBasicBlock)
			{
				if(!hasLoadStoreInsts(singleBasicBlock))
				{
					std::vector<LoadStoreInst>* loadStoreInstVector = new std::vector<LoadStoreInst>();
					setLoadStoreInsts(singleBasicBlock, loadStoreInstVector);
					allocatedVectors.push_back(loadStoreInstVector);
					for(Ice::Inst &inst : singleBasicBlock->getInsts())
					{
						if(inst.isDeleted())
						{
							continue;
						}

						bool isStoreInst = isStore(inst);
						bool isLoadInst = isLoad(inst);

						if(isStoreInst || isLoadInst)
						{
							loadStoreInstVector->push_back(LoadStoreInst(&inst, isStoreInst));
						}
					}
				}

				Ice::Inst *store = nullptr;
				Ice::Operand *storeValue = nullptr;
				bool unmatchedLoads = false;

				for (auto& loadStoreInst : getLoadStoreInsts(singleBasicBlock))
				{
					Ice::Inst* inst = loadStoreInst.inst;

					if((loadStoreInst.address != address) || inst->isDeleted())
					{
						continue;
					}

					if(loadStoreInst.isStore)
					{
						// New store found. If we had a previous one, try to eliminate it.
						if(store && !unmatchedLoads)
						{
							// If the previous store is wider than the new one, we can't eliminate it
							// because there could be a wide load reading its non-overwritten data.
							if(storeSize(inst) >= storeSize(store))
							{
								deleteInstruction(store);
							}
						}

						store = inst;
						storeValue = storeData(store);
						unmatchedLoads = false;
					}
					else
					{
						if(!loadTypeMatchesStore(inst, store))
						{
							unmatchedLoads = true;
							continue;
						}

						replace(inst, storeValue);
					}
				}
			}
		}

		for(auto loadStoreInstVector : allocatedVectors)
		{
			delete loadStoreInstVector;
		}
	}

	void Optimizer::analyzeUses(Ice::Cfg *function)
	{
		for(Ice::CfgNode *basicBlock : function->getNodes())
		{
			for(Ice::Inst &instruction : basicBlock->getInsts())
			{
				if(instruction.isDeleted())
				{
					continue;
				}

				setNode(&instruction, basicBlock);
				if(instruction.getDest())
				{
					setDefinition(instruction.getDest(), &instruction);
				}

				for(Ice::SizeT i = 0; i < instruction.getSrcSize(); i++)
				{
					Ice::SizeT unique = 0;
					for(; unique < i; unique++)
					{
						if(instruction.getSrc(i) == instruction.getSrc(unique))
						{
							break;
						}
					}

					if(i == unique)
					{
						Ice::Operand *src = instruction.getSrc(i);
						getUses(src)->insert(src, &instruction);
					}
				}
			}
		}
	}

	void Optimizer::replace(Ice::Inst *instruction, Ice::Operand *newValue)
	{
		Ice::Variable *oldValue = instruction->getDest();

		if(!newValue)
		{
			newValue = context->getConstantUndef(oldValue->getType());
		}

		if(hasUses(oldValue))
		{
			for(Ice::Inst *use : *getUses(oldValue))
			{
				assert(!use->isDeleted());   // Should have been removed from uses already

				for(Ice::SizeT i = 0; i < use->getSrcSize(); i++)
				{
					if(use->getSrc(i) == oldValue)
					{
						use->replaceSource(i, newValue);
					}
				}

				getUses(newValue)->insert(newValue, use);
			}

			setUses(oldValue, nullptr);
		}

		deleteInstruction(instruction);
	}

	void Optimizer::deleteInstruction(Ice::Inst *instruction)
	{
		if(!instruction || instruction->isDeleted())
		{
			return;
		}

		instruction->setDeleted();

		for(Ice::SizeT i = 0; i < instruction->getSrcSize(); i++)
		{
			Ice::Operand *src = instruction->getSrc(i);

			if(hasUses(src))
			{
				auto &srcUses = *getUses(src);

				srcUses.erase(instruction);

				if(srcUses.empty())
				{
					setUses(src, nullptr);

					if(Ice::Variable *var = llvm::dyn_cast<Ice::Variable>(src))
					{
						deleteInstruction(getDefinition(var));
					}
				}
			}
		}
	}

	bool Optimizer::isDead(Ice::Inst *instruction)
	{
		Ice::Variable *dest = instruction->getDest();

		if(dest)
		{
			return (!hasUses(dest) || getUses(dest)->empty()) && !instruction->hasSideEffects();
		}
		else if(isStore(*instruction))
		{
			if(Ice::Variable *address = llvm::dyn_cast<Ice::Variable>(storeAddress(instruction)))
			{
				Ice::Inst *def = getDefinition(address);

				if(def && llvm::isa<Ice::InstAlloca>(def))
				{
					if(hasUses(address))
					{
						Optimizer::Uses* uses = getUses(address);
						return uses->size() == uses->stores.size();   // Dead if all uses are stores
					}
					else
					{
						return true; // No uses
					}
				}
			}
		}

		return false;
	}

	const Ice::InstIntrinsicCall *Optimizer::asLoadSubVector(const Ice::Inst *instruction)
	{
		if(auto *instrinsic = llvm::dyn_cast<Ice::InstIntrinsicCall>(instruction))
		{
			if(instrinsic->getIntrinsicInfo().ID == Ice::Intrinsics::LoadSubVector)
			{
				return instrinsic;
			}
		}

		return nullptr;
	}

	const Ice::InstIntrinsicCall *Optimizer::asStoreSubVector(const Ice::Inst *instruction)
	{
		if(auto *instrinsic = llvm::dyn_cast<Ice::InstIntrinsicCall>(instruction))
		{
			if(instrinsic->getIntrinsicInfo().ID == Ice::Intrinsics::StoreSubVector)
			{
				return instrinsic;
			}
		}

		return nullptr;
	}

	bool Optimizer::isLoad(const Ice::Inst &instruction)
	{
		if(llvm::isa<Ice::InstLoad>(&instruction))
		{
			return true;
		}

		return asLoadSubVector(&instruction) != nullptr;
	}

	bool Optimizer::isStore(const Ice::Inst &instruction)
	{
		if(llvm::isa<Ice::InstStore>(&instruction))
		{
			return true;
		}

		return asStoreSubVector(&instruction) != nullptr;
	}

	Ice::Operand *Optimizer::storeAddress(const Ice::Inst *instruction)
	{
		assert(isStore(*instruction));

		if(auto *store = llvm::dyn_cast<Ice::InstStore>(instruction))
		{
			return store->getAddr();
		}

		if(auto *storeSubVector = asStoreSubVector(instruction))
		{
			return storeSubVector->getSrc(2);
		}

		return nullptr;
	}

	Ice::Operand *Optimizer::loadAddress(const Ice::Inst *instruction)
	{
		assert(isLoad(*instruction));

		if(auto *load = llvm::dyn_cast<Ice::InstLoad>(instruction))
		{
			return load->getSourceAddress();
		}

		if(auto *loadSubVector = asLoadSubVector(instruction))
		{
			return loadSubVector->getSrc(1);
		}

		return nullptr;
	}

	Ice::Operand *Optimizer::storeData(const Ice::Inst *instruction)
	{
		assert(isStore(*instruction));

		if(auto *store = llvm::dyn_cast<Ice::InstStore>(instruction))
		{
			return store->getData();
		}

		if(auto *storeSubVector = asStoreSubVector(instruction))
		{
			return storeSubVector->getSrc(1);
		}

		return nullptr;
	}

	std::size_t Optimizer::storeSize(const Ice::Inst *store)
	{
		assert(isStore(*store));

		if(auto *instStore = llvm::dyn_cast<Ice::InstStore>(store))
		{
			return Ice::typeWidthInBytes(instStore->getData()->getType());
		}

		if(auto *storeSubVector = asStoreSubVector(store))
		{
			return llvm::cast<Ice::ConstantInteger32>(storeSubVector->getSrc(3))->getValue();
		}

		return 0;
	}

	bool Optimizer::loadTypeMatchesStore(const Ice::Inst *load, const Ice::Inst *store)
	{
		if(!load || !store)
		{
			return false;
		}

		assert(isLoad(*load) && isStore(*store));
		assert(loadAddress(load) == storeAddress(store));

		if(auto *instStore = llvm::dyn_cast<Ice::InstStore>(store))
		{
			if(auto *instLoad = llvm::dyn_cast<Ice::InstLoad>(load))
			{
				return instStore->getData()->getType() == instLoad->getDest()->getType();
			}
		}

		if(auto *storeSubVector = asStoreSubVector(store))
		{
			if(auto *loadSubVector = asLoadSubVector(load))
			{
				// Check for matching type and sub-vector width.
				return storeSubVector->getSrc(1)->getType() == loadSubVector->getDest()->getType() &&
				       llvm::cast<Ice::ConstantInteger32>(storeSubVector->getSrc(3))->getValue() ==
				       llvm::cast<Ice::ConstantInteger32>(loadSubVector->getSrc(2))->getValue();
			}
		}

		return false;
	}

	Optimizer::Uses* Optimizer::getUses(Ice::Operand* operand)
	{
		Optimizer::Uses* uses = (Optimizer::Uses*)operand->Ice::Operand::getExternalData();
		if(!uses)
		{
			uses = new Optimizer::Uses;
			setUses(operand, uses);
			allocatedUses.push_back(uses);
		}
		return uses;
	}

	void Optimizer::setUses(Ice::Operand* operand, Optimizer::Uses* uses)
	{
		operand->Ice::Operand::setExternalData(uses);
	}

	bool Optimizer::hasUses(Ice::Operand* operand) const
	{
		return operand->Ice::Operand::getExternalData() != nullptr;
	}

	Ice::CfgNode* Optimizer::getNode(Ice::Inst* inst)
	{
		return (Ice::CfgNode*)inst->Ice::Inst::getExternalData();
	}

	void Optimizer::setNode(Ice::Inst* inst, Ice::CfgNode* node)
	{
		inst->Ice::Inst::setExternalData(node);
	}

	Ice::Inst* Optimizer::getDefinition(Ice::Variable* var)
	{
		return (Ice::Inst*)var->Ice::Variable::getExternalData();
	}

	void Optimizer::setDefinition(Ice::Variable* var, Ice::Inst* inst)
	{
		var->Ice::Variable::setExternalData(inst);
	}

	const std::vector<Optimizer::LoadStoreInst>& Optimizer::getLoadStoreInsts(Ice::CfgNode* node)
	{
		return *((const std::vector<LoadStoreInst>*)node->Ice::CfgNode::getExternalData());
	}

	void Optimizer::setLoadStoreInsts(Ice::CfgNode* node, std::vector<LoadStoreInst>* insts)
	{
		node->Ice::CfgNode::setExternalData(insts);
	}

	bool Optimizer::hasLoadStoreInsts(Ice::CfgNode* node) const
	{
		return node->Ice::CfgNode::getExternalData() != nullptr;
	}

	bool Optimizer::Uses::areOnlyLoadStore() const
	{
		return size() == (loads.size() + stores.size());
	}

	void Optimizer::Uses::insert(Ice::Operand *value, Ice::Inst *instruction)
	{
		push_back(instruction);

		if(isLoad(*instruction))
		{
			if(value == loadAddress(instruction))
			{
				loads.push_back(instruction);
			}
		}
		else if(isStore(*instruction))
		{
			if(value == storeAddress(instruction))
			{
				stores.push_back(instruction);
			}
		}
	}

	void Optimizer::Uses::erase(Ice::Inst *instruction)
	{
		auto &uses = *this;

		for(size_t i = 0; i < uses.size(); i++)
		{
			if(uses[i] == instruction)
			{
				uses[i] = back();
				pop_back();

				for(size_t i = 0; i < loads.size(); i++)
				{
					if(loads[i] == instruction)
					{
						loads[i] = loads.back();
						loads.pop_back();
						break;
					}
				}

				for(size_t i = 0; i < stores.size(); i++)
				{
					if(stores[i] == instruction)
					{
						stores[i] = stores.back();
						stores.pop_back();
						break;
					}
				}

				break;
			}
		}
	}
}

namespace rr
{
	void optimize(Ice::Cfg *function)
	{
		Optimizer optimizer;

		optimizer.run(function);
	}
}