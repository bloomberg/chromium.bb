#ifndef _VKTDYNAMICSTATETESTCASEUTIL_HPP
#define _VKTDYNAMICSTATETESTCASEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Dynamic State Tests Test Case Utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "gluShaderUtil.hpp"
#include "vkPrograms.hpp"

#include "deUniquePtr.hpp"

#include <map>

namespace vkt
{
namespace DynamicState
{

struct PositionColorVertex
{
	PositionColorVertex(const tcu::Vec4& position_, const tcu::Vec4& color_)
		: position(position_)
		, color(color_)
	{}
	tcu::Vec4 position;
	tcu::Vec4 color;
};

typedef std::map<glu::ShaderType, const char*> ShaderMap;

template<typename Instance, typename Support = NoSupport0>
class InstanceFactory : public TestCase
{
public:
	InstanceFactory (tcu::TestContext& testCtx, const std::string& name, const std::string& desc,
		const std::map<glu::ShaderType, const char*> shaderPaths)
		: TestCase		(testCtx, name, desc)
		, m_shaderPaths (shaderPaths)
		, m_support		()
	{
	}

	InstanceFactory (tcu::TestContext& testCtx, const std::string& name, const std::string& desc,
		const std::map<glu::ShaderType, const char*> shaderPaths, const Support& support)
		: TestCase		(testCtx, name, desc)
		, m_shaderPaths (shaderPaths)
		, m_support		(support)
	{
	}

	TestInstance*	createInstance	(Context& context) const
	{
		return new Instance(context, m_shaderPaths);
	}

	virtual void	initPrograms	(vk::SourceCollections& programCollection) const
	{
		for (ShaderMap::const_iterator i = m_shaderPaths.begin(); i != m_shaderPaths.end(); ++i)
		{
			programCollection.glslSources.add(i->second) <<
				glu::ShaderSource(i->first, ShaderSourceProvider::getSource(m_testCtx.getArchive(), i->second));
		}
	}

	virtual void	checkSupport	(Context& context) const
	{
		m_support.checkSupport(context);
	}

private:
	const ShaderMap	m_shaderPaths;
	const Support	m_support;
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATETESTCASEUTIL_HPP
