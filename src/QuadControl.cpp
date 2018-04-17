#include "Common.h"
#include "QuadControl.h"

#include "Utility/SimpleConfig.h"

#include "Utility/StringUtils.h"
#include "Trajectory.h"
#include "BaseController.h"
#include "Math/Mat3x3F.h"

#ifdef __PX4_NUTTX
#include <systemlib/param/param.h>
#endif

void QuadControl::Init()
{
  BaseController::Init();

  // variables needed for integral control
  integratedAltitudeError = 0;
    
#ifndef __PX4_NUTTX
  // Load params from simulator parameter system
  ParamsHandle config = SimpleConfig::GetInstance();
   
  // Load parameters (default to 0)
  kpPosXY = config->Get(_config+".kpPosXY", 0);
  kpPosZ = config->Get(_config + ".kpPosZ", 0);
  KiPosZ = config->Get(_config + ".KiPosZ", 0);
     
  kpVelXY = config->Get(_config + ".kpVelXY", 0);
  kpVelZ = config->Get(_config + ".kpVelZ", 0);

  kpBank = config->Get(_config + ".kpBank", 0);
  kpYaw = config->Get(_config + ".kpYaw", 0);

  kpPQR = config->Get(_config + ".kpPQR", V3F());

  maxDescentRate = config->Get(_config + ".maxDescentRate", 100);
  maxAscentRate = config->Get(_config + ".maxAscentRate", 100);
  maxSpeedXY = config->Get(_config + ".maxSpeedXY", 100);
  maxAccelXY = config->Get(_config + ".maxHorizAccel", 100);

  maxTiltAngle = config->Get(_config + ".maxTiltAngle", 100);

  minMotorThrust = config->Get(_config + ".minMotorThrust", 0);
  maxMotorThrust = config->Get(_config + ".maxMotorThrust", 100);
#else
  // load params from PX4 parameter system
  //TODO
  param_get(param_find("MC_PITCH_P"), &Kp_bank);
  param_get(param_find("MC_YAW_P"), &Kp_yaw);
#endif
}

VehicleCommand QuadControl::GenerateMotorCommands(float collThrustCmd, V3F momentCmd)
{
  // Convert a desired 3-axis moment and collective thrust command to 
  //   individual motor thrust commands
  // INPUTS: 
  //   desCollectiveThrust: desired collective thrust [N]
  //   desMoment: desired rotation moment about each axis [N m]
  // OUTPUT:
  //   set class member variable cmd (class variable for graphing) where
  //   cmd.desiredThrustsN[0..3]: motor commands, in [N]

  // HINTS: 
  // - you can access parts of desMoment via e.g. desMoment.x
  // You'll need the arm length parameter L, and the drag/thrust ratio kappa

  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

  cmd.desiredThrustsN[0] = mass * 9.81f / 4.f; // front left
  cmd.desiredThrustsN[1] = mass * 9.81f / 4.f; // front right
  cmd.desiredThrustsN[2] = mass * 9.81f / 4.f; // rear left
  cmd.desiredThrustsN[3] = mass * 9.81f / 4.f; // rear right
    
    // overwrite original command and retain comment above of rotor relabeling
    float l = L / sqrt(2);
    float c_b = collThrustCmd; // [N]
    float p_b = momentCmd.x / l; // [N]
    float q_b = momentCmd.y / l;
    float r_b = momentCmd.z / -kappa;
    
    float f1 = (c_b + p_b + q_b + r_b) / 4; // solve for torque [N.m]
    float f2 = (c_b - p_b + q_b - r_b) / 4;
    float f3 = (c_b -p_b - q_b + r_b) / 4;
    float f4 = (c_b + p_b - q_b - r_b) / 4;
    
    f1 = CONSTRAIN(f1, minMotorThrust, maxMotorThrust);
    f2 = CONSTRAIN(f2, minMotorThrust, maxMotorThrust);
    f3 = CONSTRAIN(f3, minMotorThrust, maxMotorThrust);
    f4 = CONSTRAIN(f4, minMotorThrust, maxMotorThrust);
    
    
    cmd.desiredThrustsN[0] = f1; // [N]
    cmd.desiredThrustsN[1] = f2;
    cmd.desiredThrustsN[2] = f4;
    cmd.desiredThrustsN[3] = f3;
    
  /////////////////////////////// END STUDENT CODE ////////////////////////////

  return cmd;
}

V3F QuadControl::BodyRateControl(V3F pqrCmd, V3F pqr)
{
  // Calculate a desired 3-axis moment given a desired and current body rate
  // INPUTS: 
  //   pqrCmd: desired body rates [rad/s]
  //   pqr: current or estimated body rates [rad/s]
  // OUTPUT:
  //   return a V3F containing the desired moments for each of the 3 axes

  // HINTS: 
  //  - you can use V3Fs just like scalars: V3F a(1,1,1), b(2,3,4), c; c=a-b;
  //  - you'll need parameters for moments of inertia Ixx, Iyy, Izz
  //  - you'll also need the gain parameter kpPQR (it's a V3F)

  V3F momentCmd;

  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

    V3F pqrErr = pqrCmd - pqr;
    V3F Imoment;
    Imoment.x = Ixx;
    Imoment.y = Iyy;
    Imoment.z = Izz;
    
    momentCmd.x = kpPQR.x * Imoment.x * pqrErr.x;
    momentCmd.y = kpPQR.y * Imoment.y * pqrErr.y;
    momentCmd.z = kpPQR.z * Imoment.z * pqrErr.z;  // [Nm] =  [kg.m.m]* rad/sec
  

  /////////////////////////////// END STUDENT CODE ////////////////////////////

  return momentCmd;
}

// returns a desired roll and pitch rate 
V3F QuadControl::RollPitchControl(V3F accelCmd, Quaternion<float> attitude, float collThrustCmd)
{
  // Calculate a desired pitch and roll angle rates based on a desired global
  //   lateral acceleration, the current attitude of the quad, and desired
  //   collective thrust command
  // INPUTS: 
  //   accelCmd: desired acceleration in global XY coordinates [m/s2]
  //   attitude: current or estimated attitude of the vehicle
  //   collThrustCmd: desired collective thrust of the quad [N]
  // OUTPUT:
  //   return a V3F containing the desired pitch and roll rates. The Z
  //     element of the V3F should be left at its default value (0)

  // HINTS: 
  //  - we already provide rotation matrix R: to get element R[1,2] (python) use R(1,2) (C++)
  //  - you'll need the roll/pitch gain kpBank
  //  - collThrustCmd is a force in Newtons! You'll likely want to convert it to acceleration first

  V3F pqrCmd;
  Mat3x3F R = attitude.RotationMatrix_IwrtB();

  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

    V3D rpy = attitude.ToEulerYPR(); // used to visualize current attitude in world frame
    
    float b_x = R(0, 2);
    float b_x_err = accelCmd.x - b_x;
    float b_x_p_term = kpBank * b_x_err;
    
    float b_y = R(1, 2);
    float b_y_err = accelCmd.y - b_y;
    float b_y_p_term = kpBank * b_y_err;
    
    float b_x_commanded_dot = b_x_p_term;
    float b_y_commanded_dot = b_y_p_term;
    
    V3F rot_rate;
    rot_rate.x = (R(1, 0) * b_x_commanded_dot - R(0, 0) *  b_y_commanded_dot) / R(2, 2);
    rot_rate.y = (R(1, 1) * b_x_commanded_dot - R(0, 1) *  b_y_commanded_dot) / R(2, 2);
    pqrCmd.x = rot_rate.x;
    pqrCmd.y = rot_rate.y;

  /////////////////////////////// END STUDENT CODE ////////////////////////////

  return pqrCmd;
}

float QuadControl::AltitudeControl(float posZCmd, float velZCmd, float posZ, float velZ, Quaternion<float> attitude, float accelZCmd, float dt)
{
  // Calculate desired quad thrust based on altitude setpoint, actual altitude,
  //   vertical velocity setpoint, actual vertical velocity, and a vertical 
  //   acceleration feed-forward command
  // INPUTS: 
  //   posZCmd, velZCmd: desired vertical position and velocity in NED [m]
  //   posZ, velZ: current vertical position and velocity in NED [m]
  //   accelZCmd: feed-forward vertical acceleration in NED [m/s2]
  //   dt: the time step of the measurements [seconds]
  // OUTPUT:
  //   return a collective thrust command in [N]

  // HINTS: 
  //  - we already provide rotation matrix R: to get element R[1,2] (python) use R(1,2) (C++)
  //  - you'll need the gain parameters kpPosZ and kpVelZ
  //  - maxAscentRate and maxDescentRate are maximum vertical speeds. Note they're both >=0!
  //  - make sure to return a force, not an acceleration
  //  - remember that for an upright quad in NED, thrust should be HIGHER if the desired Z acceleration is LOWER

  Mat3x3F R = attitude.RotationMatrix_IwrtB();
  float thrust = 0;

  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

    float b_z = R(2, 2);
    float z_err = posZCmd - posZ; // error is pointing down
    float z_err_dot = velZCmd - velZ;
    float p_term = kpPosZ * z_err;
    // add integrator
    integratedAltitudeError += z_err * dt;

    float i_term = KiPosZ * integratedAltitudeError;
    float d_term = kpVelZ * z_err_dot;
    
    float u_1_bar = p_term + i_term + d_term + accelZCmd;
    float c = (u_1_bar - CONST_GRAVITY) / b_z;
    
    
    thrust = -mass * c; // turn thrust to positive number

  /////////////////////////////// END STUDENT CODE ////////////////////////////
  
  return thrust;
}

// returns a desired acceleration in global frame
V3F QuadControl::LateralPositionControl(V3F posCmd, V3F velCmd, V3F pos, V3F vel, V3F accelCmd)
{
  // Calculate a desired horizontal acceleration based on 
  //  desired lateral position/velocity/acceleration and current pose
  // INPUTS: 
  //   posCmd: desired position, in NED [m]
  //   velCmd: desired velocity, in NED [m/s]
  //   pos: current position, NED [m]
  //   vel: current velocity, NED [m/s]
  //   accelCmd: desired acceleration, NED [m/s2]
  // OUTPUT:
  //   return a V3F with desired horizontal accelerations. 
  //     the Z component should be 0
  // HINTS: 
  //  - use fmodf(foo,b) to constrain float foo to range [0,b]
  //  - use the gain parameters kpPosXY and kpVelXY
  //  - make sure you cap the horizontal velocity and acceleration
  //    to maxSpeedXY and maxAccelXY

  // make sure we don't have any incoming z-component
  accelCmd.z = 0;
  velCmd.z = 0;
  posCmd.z = pos.z;

  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

    float x_err = posCmd.x - pos.x;
    float x_err_dot = velCmd.x - vel.x;
    
    float p_term_x = kpPosXY * x_err;
    float d_term_x = kpVelXY * x_err_dot;
    float x_dot_dot_command = p_term_x + d_term_x + accelCmd.x;
    float c = -1; // not sure what this is
    float b_x_c = x_dot_dot_command / c;
    float b = 2; // constrain limit
    accelCmd.x = fmodf(b_x_c, b);
    
    float y_err = posCmd.y - pos.y;
    float y_err_dot = velCmd.y - vel.y;
    float p_term_y = kpPosXY * y_err;
    float d_term_y = kpVelXY * y_err_dot;
    float y_dot_dot_command = p_term_y + d_term_y + accelCmd.y;
    float b_y_c = y_dot_dot_command / c;
    accelCmd.y = fmodf(b_y_c, b);

  /////////////////////////////// END STUDENT CODE ////////////////////////////

  return accelCmd;
}

// returns desired yaw rate
float QuadControl::YawControl(float yawCmd, float yaw)
{
  // Calculate a desired yaw rate to control yaw to yawCmd
  // INPUTS: 
  //   yawCmd: commanded yaw [rad]
  //   yaw: current yaw [rad]
  // OUTPUT:
  //   return a desired yaw rate [rad/s]
  // HINTS: 
  //  - use fmodf(foo,b) to constrain float foo to range [0,b]
  //  - use the yaw control gain parameter kpYaw

  float yawRateCmd=0;
  ////////////////////////////// BEGIN STUDENT CODE ///////////////////////////

    float psi_err = yawCmd - yaw;
    float b = 1;
    yawRateCmd = kpYaw * psi_err;
    yawRateCmd = fmodf(yawRateCmd,b);
    
  /////////////////////////////// END STUDENT CODE ////////////////////////////

  return yawRateCmd;

}

VehicleCommand QuadControl::RunControl(float dt, float simTime)
{
  curTrajPoint = GetNextTrajectoryPoint(simTime);

  float collThrustCmd = AltitudeControl(curTrajPoint.position.z, curTrajPoint.velocity.z, estPos.z, estVel.z, estAtt, curTrajPoint.accel.z, dt);

  // reserve some thrust margin for angle control
  float thrustMargin = .1f*(maxMotorThrust - minMotorThrust);
  collThrustCmd = CONSTRAIN(collThrustCmd, (minMotorThrust+ thrustMargin)*4.f, (maxMotorThrust-thrustMargin)*4.f);
  
  V3F desAcc = LateralPositionControl(curTrajPoint.position, curTrajPoint.velocity, estPos, estVel, curTrajPoint.accel);
  
  V3F desOmega = RollPitchControl(desAcc, estAtt, collThrustCmd);
  desOmega.z = YawControl(curTrajPoint.attitude.Yaw(), estAtt.Yaw());

  V3F desMoment = BodyRateControl(desOmega, estOmega);

  return GenerateMotorCommands(collThrustCmd, desMoment);
}
