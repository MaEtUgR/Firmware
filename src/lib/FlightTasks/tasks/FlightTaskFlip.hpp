/****************************************************************************
 *
 *   Copyright (c) 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file FlightTaskFlip.hpp
 *
 * Task to make the drone flip in place.
 */

#pragma once

#include "FlightTaskManualPosition.hpp"
#include <float.h>

using namespace matrix;

class FlightTaskFlip : public FlightTaskManualPosition
{
public:
	FlightTaskFlip() = default;

	virtual ~FlightTaskFlip() = default;

protected:
	bool update() override
	{
		if (_flip_done) {
			if (_sticks(1) > 0.9f) {
				// start a flip
				_flip_done = false;
				_flip_angle = 0.0f;
			}

			// get all setpoints from manual position task
			return FlightTaskManualPosition::update();

		} else {
			printf(" Flipping: %.3f \n", (double)_flip_angle);
			_resetSetpoints();
			// continue with the flip
			_flip_angle += 2 * M_PI_F * _deltatime;
			Quatf q_sp = AxisAnglef(0.0f, _flip_angle, 0.0f);
			_thrust_setpoint = q_sp.conjugate(Vector3f(0.0f, 0.0f, -1.0f)) * 1.f;

			if (_flip_angle > 2.f * M_PI_F) {
				_flip_done = true;
			}

			return true;
		}
	}

private:
	bool _flip_done = true;
	float _flip_angle = 0.0f;

};
