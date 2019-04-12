# adaptive_grasping

A ROS Package for the Touch-Based Adaptive Grasping strategy.

## Authors

* **George Jose Pollayil** - [gpollayil](https://github.com/gpollayil)
* **Mathew Jose Pollayil** - [mpollayil](https://github.com/mpollayil)

## Getting Started

### Prerequisites

This package depends on ROS Kinetic or newer.

Needed Packages (from Centro Piaggio GitHub):
`franka_ros` (branch `soma-devel`), `panda-softhand` (branch `master`), `pisa-iit-soft-hand` (branch `demo-march-2019`), `IMU` (NMMI GitHub), `imu_glove_finger_touch_utils` (branch `demo-march-2019`), `finger_fk` (branch `demo-march-2019`)

### Installing

To install this package just clone into your catkin_ws and catkin_make.

## Running the Adaptive Grasping Package

(If REAL ROBOT remember to set correctly `robot_ip` and set `load_gripper` to `false` in `launchPandaSoftHand.launch`)

(Most of the parameters of Adaptive Grasping can be changed from `adaptive_params.yaml` and `full_grasp_params.yaml`)

### Launch Order (Temporary)

1. `roslaunch adaptive_grasping launchPandaSoftHand.launch` (Launches Robot in RViz + Real Robot)
2. `roslaunch panda_softhand_control launchControlServer.launch ` (Launches a server node which provides services for joint control, pose control, ecc.)
3. (TODO: SHOULD BE GIVEN BY VISION IN FUTURE) `roslaunch panda_softhand_control launchSimulateObjectPose.launch"` (change this file for changing object pose)
4. `roslaunch adaptive_grasping launchRobotCommAdaptiveGrasp.launch ` (The main server for adaptive grasping, and launches robotCommander)
5. `rosservice call /adaptive_task_service "data: true""` (To start the whole adaptive grasping process)
6. `rostopic pub -r 50 /touching_finger_topic std_msgs/Int8 "data: 4"` (From 0 to 5 for fingers from thumb to pinky)



