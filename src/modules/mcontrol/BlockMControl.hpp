/**
 * @file BlockMControl.hpp
 * @author Matthias Grob <maetugr@gmail.com>
 *
 * Nonlinear Quadrotor Controller (master thesis)
 */

#pragma once
#include <px4_posix.h>
#include <mathlib/mathlib.h>
#include <matrix/Matrix3.hpp>
#include <controllib/uorb/blocks.hpp>
#include <uORB/topics/control_state.h>
#include <uORB/topics/vehicle_force_setpoint.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/manual_control_setpoint.h>

class BlockMControl : public control::SuperBlock {
public:
	BlockMControl(bool simulation);
	void update();
private:
	uORB::Subscription<control_state_s>				_sub_control_state;
	uORB::Subscription<vehicle_attitude_s>			_sub_vehicle_attitude;
	uORB::Subscription<vehicle_force_setpoint_s>	_sub_force_setpoint;
	uORB::Subscription<manual_control_setpoint_s>	_sub_manual_control_setpoint;
	uORB::Subscription<vehicle_attitude_setpoint_s>	_sub_vehicle_attitude_setpoint;
	uORB::Publication<actuator_controls_s>			_pub_actuator_controls;

	bool _simulation;

	bool poll_control_state();
	px4_pollfd_struct_t _control_state_Poll;						// file descriptors struct to feed the system call poll

	void calculate_dt();
	uint64_t _dt_timeStamp;											// last time the loop ran to calculate dt

	void get_joystick_data();
	float _joystick[4];

	void Controller();
	matrix::Quatf _qd;
	matrix::Matrix3f _Rd;

	matrix::Vector3f ControllerQ();									// quaternion based attitude controller
	template<typename T> int sign(T val) {return (T(0) < val) - (val < T(0));}	// type-safe signum function (http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c)

	matrix::Vector3f ControllerR();									// matrix based attitude controller
	matrix::Matrix3f _Rd_prev;
	matrix::Vector3f _Od_prev;
	matrix::Vector3f _O_prev;

	void rateController_original();									// the original rate controller as a reference
	math::Vector<3> _rates_prev;

	void publishMoment(matrix::Vector3f moment);
};
