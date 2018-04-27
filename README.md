# The C++ Project Readme #

This is the readme for the C++ project.

## The Tasks ##

Routines are implemented in QuadControl.cpp and the control gains are in QuadControlPararms.txt.

GenerateMotorCommands required math to be carefully computed to figure out what kappa means.  With NED, a negative sign was required to get the correct orientation during roll and pitch.  The F3/F4 swap from course exercise was difficult to diagnose and helpful to visualize the thrust coming out of the roll.

### Body rate and roll/pitch control (scenario 2) ###
This exercise, again, required understanding of the math.  In this case the Euler rotation matrix R13 and R23 are essential as we are targeting an abstract value inside a rotation matrix.

### Position/velocity and yaw angle control (scenario 3) ###

The lateral controller can lead to drone instability which made this exercise interesting to pass.

### Non-idealities and robustness (scenario 4) ###
The mass error of one of the drone required a high integration gain to correct for this systematic error.  We need to iterate over all the lower controller gains to make sure we didn't break anything each time.

### Tracking trajectories ###

In this section, fine tuning of the altitude and latitude controller was necessary.


### Extra Challenge 1 (Optional) ###

not attempted.


### Extra Challenge 2 (Optional) ###

not attempted.


## Evaluation ##

The scenarios all showed pass.

### Performance Metrics ###

The specific performance metrics are as follows:

 - scenario 2
   - roll should less than 0.025 radian of nominal for 0.75 seconds (3/4 of the duration of the loop)
   - roll rate should less than 2.5 radian/sec for 0.75 seconds

 - scenario 3
   - X position of both drones should be within 0.1 meters of the target for at least 1.25 seconds
   - Quad2 yaw should be within 0.1 of the target for at least 1 second


 - scenario 4
   - position error for all 3 quads should be less than 0.1 meters for at least 1.5 seconds

 - scenario 5
   - position error of the quad should be less than 0.25 meters for at least 3 seconds
