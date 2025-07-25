/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Unit test for Averager
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/Averager.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"

using namespace std;

TEST_CASE("Primitive_Averager")
{
	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Primitive_Averager.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	const size_t depth = 50000000;

	//Deterministic PRNG for repeatable testing
	minstd_rand rng;
	rng.seed(0);
	TestWaveformSource source(rng);

	SECTION("UniformAnalogWaveform")
	{
		//Input waveform
		UniformAnalogWaveform wfm;
		source.GenerateNoisySinewave(cmdBuf, queue, &wfm, 1.0, 0.0, 200000, 20000, depth, 0.1);

		//Add a small DC offset to the waveform so we have a nonzero average
		wfm.PrepareForCpuAccess();
		float offset = 0.314159;
		for(size_t i=0; i<depth; i++)
			wfm.m_samples[i] += offset;
		wfm.MarkModifiedFromCpu();

		//Find the average using the base function
		double start = GetTime();
		float avg = Filter::GetAvgVoltage(&wfm);
		double dt = GetTime() - start;
		LogNotice("CPU: %6.3f ms, average = %.7f\n", dt*1000, avg);

		//Do the GPU version
		//Run twice, second time for score, so we don't count deferred init or allocations in the benchmark
		Averager acomp;
		float gpuavg = acomp.Average(&wfm, cmdBuf, queue);
		start = GetTime();
		gpuavg = acomp.Average(&wfm, cmdBuf, queue);
		double gpudt = GetTime() - start;
		LogNotice("GPU: %6.3f ms, average = %.7f, %.2fx speedup\n", gpudt*1000, gpuavg, dt / gpudt);

		//Verify everything matches
		//Noisy sine should be close to the desired offset
		float epsilon1 = 0.002;
		REQUIRE(fabs(avg - offset) < epsilon1);
		REQUIRE(fabs(gpuavg - offset) < epsilon1);

		//and CPU and GPU versions should closely agree
		float epsilon2 = 0.0001;
		REQUIRE(fabs(gpuavg - avg) < epsilon2);
	}
}
