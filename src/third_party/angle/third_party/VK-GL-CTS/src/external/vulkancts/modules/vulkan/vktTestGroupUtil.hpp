#ifndef _VKTTESTGROUPUTIL_HPP
#define _VKTTESTGROUPUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief TestCaseGroup utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{

class TestGroupHelper0 : public tcu::TestCaseGroup
{
public:
	typedef void (*CreateChildrenFunc) (tcu::TestCaseGroup* testGroup);
	typedef void (*CleanupGroupFunc) (tcu::TestCaseGroup* testGroup);

								TestGroupHelper0	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 CreateChildrenFunc		createChildren,
													 CleanupGroupFunc		cleanupGroup);
								~TestGroupHelper0	(void);

	void						init				(void);
	void						deinit				(void);

private:
	const CreateChildrenFunc	m_createChildren;
	const CleanupGroupFunc		m_cleanupGroup;
};

template<typename Arg0>
class TestGroupHelper1 : public tcu::TestCaseGroup
{
public:
	typedef void (*CreateChildrenFunc) (tcu::TestCaseGroup* testGroup, Arg0 arg0);
	typedef void (*CleanupGroupFunc) (tcu::TestCaseGroup* testGroup, Arg0 arg0);

								TestGroupHelper1	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 CreateChildrenFunc		createChildren,
													 const Arg0&			arg0,
													 CleanupGroupFunc		cleanupGroup)
									: tcu::TestCaseGroup	(testCtx, name.c_str(), description.c_str())
									, m_createChildren		(createChildren)
									, m_cleanupGroup		(cleanupGroup)
									, m_arg0				(arg0)
								{}

	void						init				(void) { m_createChildren(this, m_arg0); }
	void						deinit				(void) { if (m_cleanupGroup) m_cleanupGroup(this, m_arg0); }

private:
	const CreateChildrenFunc	m_createChildren;
	const CleanupGroupFunc		m_cleanupGroup;
	const Arg0					m_arg0;
};

inline tcu::TestCaseGroup* createTestGroup (tcu::TestContext&										testCtx,
											const std::string&										name,
											const std::string&										description,
											TestGroupHelper0::CreateChildrenFunc					createChildren,
											TestGroupHelper0::CleanupGroupFunc						cleanupGroup = DE_NULL)
{
	return new TestGroupHelper0(testCtx, name, description, createChildren, cleanupGroup);
}

template<typename Arg0>
tcu::TestCaseGroup* createTestGroup (tcu::TestContext&										testCtx,
									 const std::string&										name,
									 const std::string&										description,
									 typename TestGroupHelper1<Arg0>::CreateChildrenFunc	createChildren,
									 Arg0													arg0,
									 typename TestGroupHelper1<Arg0>::CleanupGroupFunc		cleanupGroup = DE_NULL)
{
	return new TestGroupHelper1<Arg0>(testCtx, name, description, createChildren, arg0, cleanupGroup);
}

inline void addTestGroup (tcu::TestCaseGroup*					parent,
						  const std::string&					name,
						  const std::string&					description,
						  TestGroupHelper0::CreateChildrenFunc	createChildren)
{
	parent->addChild(createTestGroup(parent->getTestContext(), name, description, createChildren));
}

template<typename Arg0>
void addTestGroup (tcu::TestCaseGroup*									parent,
				   const std::string&									name,
				   const std::string&									description,
				   typename TestGroupHelper1<Arg0>::CreateChildrenFunc	createChildren,
				   Arg0													arg0,
				   typename TestGroupHelper1<Arg0>::CleanupGroupFunc	cleanupGroup = DE_NULL)
{
	parent->addChild(createTestGroup<Arg0>(parent->getTestContext(), name, description, createChildren, arg0, cleanupGroup));
}

} // vkt

#endif // _VKTTESTGROUPUTIL_HPP
