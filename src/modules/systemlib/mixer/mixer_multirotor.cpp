/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
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
 * @file mixer_multirotor.cpp
 *
 * Multi-rotor mixers.
 */
#include <px4_config.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <math.h>

#include <px4iofirmware/protocol.h>

#include "mixer.h"

// This file is generated by the multi_tables script which is invoked during the build process
#include "mixer_multirotor.generated.h"

#define debug(fmt, args...)	do { } while(0)
//#define debug(fmt, args...)	do { printf("[mixer] " fmt "\n", ##args); } while(0)
//#include <debug.h>
//#define debug(fmt, args...)	lowsyslog(fmt "\n", ##args)

/*
 * Clockwise: 1
 * Counter-clockwise: -1
 */

namespace
{

float constrain(float val, float min, float max)
{
	return (val < min) ? min : ((val > max) ? max : val);
}

} // anonymous namespace

MultirotorMixer::MultirotorMixer(ControlCallback control_cb,
				 uintptr_t cb_handle,
				 MultirotorGeometry geometry,
				 float roll_scale,
				 float pitch_scale,
				 float yaw_scale,
				 float idle_speed) :
	Mixer(control_cb, cb_handle),
	_roll_scale(roll_scale),
	_pitch_scale(pitch_scale),
	_yaw_scale(yaw_scale),
	_idle_speed(-1.0f + idle_speed * 2.0f),	/* shift to output range here to avoid runtime calculation */
	_limits_pub(),
	_rotor_count(_config_rotor_count[(MultirotorGeometryUnderlyingType)geometry]),
	_rotors(_config_index[(MultirotorGeometryUnderlyingType)geometry])
{
}

MultirotorMixer::~MultirotorMixer()
{
}

MultirotorMixer *
MultirotorMixer::from_text(Mixer::ControlCallback control_cb, uintptr_t cb_handle, const char *buf, unsigned &buflen)
{
	MultirotorGeometry geometry;
	char geomname[8];
	int s[4];
	int used;

	/* enforce that the mixer ends with space or a new line */
	for (int i = buflen - 1; i >= 0; i--) {
		if (buf[i] == '\0') {
			continue;
		}

		/* require a space or newline at the end of the buffer, fail on printable chars */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\r') {
			/* found a line ending or space, so no split symbols / numbers. good. */
			break;

		} else {
			debug("simple parser rejected: No newline / space at end of buf. (#%d/%d: 0x%02x)", i, buflen - 1, buf[i]);
			return nullptr;
		}

	}

	if (sscanf(buf, "R: %s %d %d %d %d%n", geomname, &s[0], &s[1], &s[2], &s[3], &used) != 5) {
		debug("multirotor parse failed on '%s'", buf);
		return nullptr;
	}

	if (used > (int)buflen) {
		debug("OVERFLOW: multirotor spec used %d of %u", used, buflen);
		return nullptr;
	}

	buf = skipline(buf, buflen);

	if (buf == nullptr) {
		debug("no line ending, line is incomplete");
		return nullptr;
	}

	debug("remaining in buf: %d, first char: %c", buflen, buf[0]);

	if (!strcmp(geomname, "4+")) {
		geometry = MultirotorGeometry::QUAD_PLUS;

	} else if (!strcmp(geomname, "4x")) {
		geometry = MultirotorGeometry::QUAD_X;

	} else if (!strcmp(geomname, "4h")) {
		geometry = MultirotorGeometry::QUAD_H;

	} else if (!strcmp(geomname, "4v")) {
		geometry = MultirotorGeometry::QUAD_V;

	} else if (!strcmp(geomname, "4w")) {
		geometry = MultirotorGeometry::QUAD_WIDE;

	} else if (!strcmp(geomname, "4dc")) {
		geometry = MultirotorGeometry::QUAD_DEADCAT;

	} else if (!strcmp(geomname, "6+")) {
		geometry = MultirotorGeometry::HEX_PLUS;

	} else if (!strcmp(geomname, "6x")) {
		geometry = MultirotorGeometry::HEX_X;

	} else if (!strcmp(geomname, "6c")) {
		geometry = MultirotorGeometry::HEX_COX;

	} else if (!strcmp(geomname, "8+")) {
		geometry = MultirotorGeometry::OCTA_PLUS;

	} else if (!strcmp(geomname, "8x")) {
		geometry = MultirotorGeometry::OCTA_X;

	} else if (!strcmp(geomname, "8c")) {
		geometry = MultirotorGeometry::OCTA_COX;

	} else if (!strcmp(geomname, "8cw")) {
		geometry = MultirotorGeometry::OCTA_COX_WIDE;

	} else if (!strcmp(geomname, "2-")) {
		geometry = MultirotorGeometry::TWIN_ENGINE;

	} else if (!strcmp(geomname, "3y")) {
		geometry = MultirotorGeometry::TRI_Y;

	} else {
		debug("unrecognised geometry '%s'", geomname);
		return nullptr;
	}

	debug("adding multirotor mixer '%s'", geomname);

	return new MultirotorMixer(
		       control_cb,
		       cb_handle,
		       geometry,
		       s[0] / 10000.0f,
		       s[1] / 10000.0f,
		       s[2] / 10000.0f,
		       s[3] / 10000.0f);
}

unsigned
MultirotorMixer::mix(float *outputs, unsigned space, uint16_t *status_reg)
{
	/* Summary of mixing strategy:
	1) mix roll, pitch and thrust without yaw.
	2) if some outputs violate range [0,1] then try to shift all outputs to minimize violation ->
		increase or decrease total thrust (boost). The total increase or decrease of thrust is limited
		(max_thrust_diff). If after the shift some outputs still violate the bounds then scale roll & pitch.
		In case there is violation at the lower and upper bound then try to shift such that violation is equal
		on both sides.
	3) mix in yaw and scale if it leads to limit violation.
	4) scale all outputs to range [idle_speed,1]
	*/

	float		roll    = constrain(get_control(0, 0) * _roll_scale, -1.0f, 1.0f);
	float		pitch   = constrain(get_control(0, 1) * _pitch_scale, -1.0f, 1.0f);
	float		yaw     = constrain(get_control(0, 2) * _yaw_scale, -1.0f, 1.0f);
	float		thrust  = constrain(get_control(0, 3), 0.0f, 1.0f);

	if (status_reg != NULL) (*status_reg) = 0;		// clean register for saturation status flags

	// perform initial mix pass yielding unbounded outputs, ignore yaw, just to check if they already exceed the limits
	float		min = 1;
	float		max = 0;

	for(int i = 0; i < _rotor_count; i++) {
		outputs[i] = roll * _rotors[i].roll_scale + pitch * _rotors[i].pitch_scale; // first we mix only the moments for roll and pitch on outputs

		float out = (outputs[i] + thrust) * _rotors[i].out_scale;

		if (out < min) min = out; 			// calculate min and max output values of any rotor
		if (out > max) max = out;
	}

	float boost = 0.0f;						// value added to demanded thrust (can also be negative)
	float limit_scale = 1.0f;				// scale for demanded roll and pitch

	if(max - min > 1) {						// hard case where we can't meet the controller output because it exceeds the maximal difference
		boost = (1 - max - min) / 2;		// from equation: (max - 1) + b = -(min + b) which states that after the application of boost the violation above 1 and below 0 is equal
		limit_scale = 1 / (max - min);		// we want to scale such that roll and pitch produce min = 0 and max = 1 with the boost from above applied
	} else if(min < 0)
		boost = -min;						// easy cases where we just shift the the throttle such that the controller output can be met
	else if (max > 1)
		boost = 1 - max;

	thrust += boost;
	for(int i = 0; i <_rotor_count; i++)
		outputs[i] = (outputs[i] * limit_scale);

	// notify if saturation has occurred
	if(status_reg != NULL) {
		if (min < 0.0f) (*status_reg) |= PX4IO_P_STATUS_MIXER_LOWER_LIMIT;
		if (max > 1.0f) (*status_reg) |= PX4IO_P_STATUS_MIXER_UPPER_LIMIT;
	}

	// mix again but now with thrust boost, scale roll/pitch and also add yaw
	min = 1;
	max = 0;
	float min_term = 0;
	float max_term = 0;

	for(int i = 0; i < _rotor_count; i++) {
		float yaw_term = yaw * _rotors[i].yaw_scale;
		float out = (outputs[i] + yaw_term + thrust) * _rotors[i].out_scale;

		if (out < min) {
			min = out;
			min_term = yaw_term;
		}
		if (out > max) {
			max = out;
			max_term = yaw_term;
		}
	}

	boost = 0.0f;
	float low_limit_scale = 1.0f;			// scale for demanded yaw only
	float high_limit_scale = 1.0f;

	if(max - min > 1) {						// hard case where we can't meet the controller output because it exceeds the maximal difference
		boost = (1 - max - min) / 2;		// from equation: (max - 1) + b = -(min + b) which states that after the application of boost the violation above 1 and below 0 is equal
		if(min < 0 && min_term < 0) {		// we want to scale only the yaw term to fill up the remaining control actuation such that min 0 and max 1 and roll pitch does NOT get rescaled
			limit_scale = (min + boost - min_term) / -(min_term);
			//printf("-");
		}
		if (max > 1 && max_term > 0){
			limit_scale = (1 - (max + boost - max_term)) / max_term;
			//printf("+");
		}
		limit_scale = low_limit_scale < high_limit_scale ? low_limit_scale : high_limit_scale;
		limit_scale = limit_scale > 0 ? limit_scale : 0;
	} else if(min < 0)
		boost = -min;						// easy cases where we just shift the the throttle such that the controller output can be met
	else if (max > 1)
		boost = 1 - max;

	// inform about yaw limit reached
	if(status_reg != NULL) {
		if (min < 0.0f || max > 1.0f) (*status_reg) |= PX4IO_P_STATUS_MIXER_YAW_LIMIT;
	}

	/* add yaw and scale outputs to range idle_speed...1 */
	for (unsigned i = 0; i < _rotor_count; i++) {
		outputs[i] = outputs[i] + yaw * _rotors[i].yaw_scale * limit_scale + thrust + boost;

		outputs[i] = constrain(_idle_speed + (outputs[i] * (1.0f - _idle_speed)), _idle_speed, 1.0f);
	}

	return _rotor_count;
}

void
MultirotorMixer::groups_required(uint32_t &groups)
{
	/* XXX for now, hardcoded to indexes 0-3 in control group zero */
	groups |= (1 << 0);
}

