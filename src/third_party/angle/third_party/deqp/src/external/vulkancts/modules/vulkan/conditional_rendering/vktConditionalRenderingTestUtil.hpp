#ifndef _VKTCONDITIONALRENDERINGTESTUTIL_HPP
#define _VKTCONDITIONALRENDERINGTESTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Conditional Rendering Test Utils
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktTestCase.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace conditional
{

struct ConditionalData
{
	bool		conditionInPrimaryCommandBuffer;
	bool		conditionInSecondaryCommandBuffer;
	bool		conditionInverted;
	bool		conditionInherited;
	deUint32	conditionValue;

	bool		expectCommandExecution;
};

static const ConditionalData s_testsData[] =
{
//  Primary	Secondary Inverted Inherited Val  Result
	{ true,		false,	false,	false,	1,	true },
	{ true,		false,	false,	false,	0,	false },
	{ true,		false,	true,	false,	0,	true},
	{ true,		false,	true,	false,	1,	false },
	{ true,		false,	false,	true,	1,	true },
	{ true,		false,	false,	true,	0,	false },
	{ true,		false,	true,	true,	0,	true },
	{ true,		false,	true,	true,	1,	false },

	{ false,	true,	false,	false,	1,	true },
	{ false,	true,	false,	false,	0,	false },
	{ false,	true,	true,	false,	0,	true},
	{ false,	true,	true,	false,	1,	false },

	/* Test that commands are executed correctly when secondary command buffer is constructed
	 * with conditional rendering enabled but conditional rendering is not enabled when
	 * executing it in the primary command buffer.
	 */
	{ false,	false,	false,	true,	1,	true },
};

std::ostream&				operator<< (std::ostream& str, ConditionalData const& c);

void						checkConditionalRenderingCapabilities	(vkt::Context& context, const ConditionalData& data);
de::SharedPtr<Draw::Buffer>	createConditionalRenderingBuffer		(vkt::Context& context, const ConditionalData& data);
void						beginConditionalRendering				(const vk::DeviceInterface& vk,
																	 vk::VkCommandBuffer cmdBuffer,
																	 Draw::Buffer& buffer,
																	 const ConditionalData& data);

} // conditional
} // vkt

#endif // _VKTCONDITIONALRENDERINGTESTUTIL_HPP
