/**
 * @file BlockMControl.hpp
 * @author Matthias Grob <maetugr@gmail.com>
 *
 * Nonlinear Quadrotor Controller (master thesis)
 * quaternion attitude control is implemented mostly according to:
 *   Nonlinear Quadrocopter Attitude Control
 *   Dario Brescianini, markus Hehn, Raffaello D'Andrea
 *   IDSC ETH Zürich
 */

#pragma once
#include <px4_posix.h>
#include <mathlib/mathlib.h>
#include <matrix/Matrix.hpp>
#include <controllib/uorb/blocks.hpp>
#include <uORB/topics/control_state.h>
#include <uORB/topics/vehicle_force_setpoint.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/actuator_outputs.h>
#include "drivers/drv_pwm_output.h"					// for PWM
#include <mathlib/math/filter/LowPassFilter2p.hpp>	// for low pass filtering signals

class BlockMControl : public control::SuperBlock {
public:
	BlockMControl(bool simulation);
	~BlockMControl();
	void update();
private:
	uORB::Subscription<control_state_s>				_sub_control_state;
	uORB::Subscription<vehicle_attitude_s>			_sub_vehicle_attitude;
	uORB::Subscription<vehicle_force_setpoint_s>	_sub_force_setpoint;
	uORB::Subscription<manual_control_setpoint_s>	_sub_manual_control_setpoint; // TODO: joystick input still needed?
	uORB::Subscription<vehicle_attitude_setpoint_s>	_sub_vehicle_attitude_setpoint;
	uORB::Subscription<actuator_armed_s>			_sub_actuator_armed;
	uORB::Subscription<actuator_outputs_s>			_sub_actuator_outputs;
	uORB::Publication<actuator_outputs_s>			_pub_actuator_outputs;
	uORB::Publication<actuator_controls_s>			_pub_actuator_controls;

	uORB::Subscription<sensor_combined_s>	_sub_sensor_combined;

	bool _simulation;

	bool poll_control_state();
	px4_pollfd_struct_t _control_state_Poll;						// file descriptors struct to feed the system call poll

	void calculate_dt();
	uint64_t _dt_timeStamp;											// last time the loop ran to calculate dt

	void get_joystick_data();
	float _joystick[4];

	bool EstimatorInit(matrix::Vector3f A, matrix::Vector3f M);
	bool _estimator_inited;

	void Estimator();
	matrix::Quatf _q;												// the attitude state as quaternion
	matrix::Vector3f _b;											// the gyro bias estimate for the QEKF (QuaternionEKF)
	matrix::SquareMatrix<float,6> _P;								// the covariance matrix for the QEKF

	void Controller();
	matrix::Quatf _qd;
	float _yaw;
	matrix::Vector3f _O_prev;
	bool _rate_mode;

	matrix::Vector3f ControllerQ(matrix::Quatf q, matrix::Quatf qd);									// quaternion based attitude controller
	template<typename T> int sign(T val) {return (T(0) < val) - (val < T(0));}	// type-safe signum function (http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c)

	matrix::Quatf FtoQ(matrix::Vector3f F, float yaw);

	void publishMoment(matrix::Vector3f moment, float thrust);

	void Mixer(matrix::Vector<float,4> moment_thrust);
	matrix::Vector<float,4> _motors;

	void PWM();
	void setMotorPWM(int channel, float power);						// channel 0-3, power 0.0-1.0
	int _pwm_fd;
};
