#!/usr/bin/env python
import roslib
roslib.load_manifest('plate_tf')
import rospy

import tf
from geometry_msgs.msg import PoseStamped
from plate_tf.srv import *

class PoseTFConversion:
    def __init__(self):
        self.tf_listener = tf.TransformListener()
        self.tf_broadcaster = tf.TransformBroadcaster()
        self.robot_image_pose_sub = rospy.Subscriber('RobotImagePose',PoseStamped,self.handle_robot_image_pose)
        self.fly_image_pose_sub = rospy.Subscriber('FlyImagePose',PoseStamped,self.handle_fly_image_pose)

        rospy.wait_for_service('camera_to_plate')
        try:
            self.camera_to_plate = rospy.ServiceProxy('camera_to_plate', PlateCameraConversion)
        except rospy.ServiceException, e:
            print "Service call failed: %s"%e

    def handle_robot_image_pose(self,msg):
        try:
            Xsrc = [msg.pose.position.x]
            Ysrc = [msg.pose.position.y]
            self.tf_broadcaster.sendTransform((msg.pose.position.x, msg.pose.position.y, 0),
                                  tuple(msg.pose.orientation),
                                  rospy.Time.now(),
                                  "RobotImage",
                                  "Camera")

            response = self.camera_to_plate(Xsrc,Ysrc)
            robot_plate_x = response.Xdst[0]
            robot_plate_y = response.Ydst[0]
            self.tf_broadcaster.sendTransform((robot_plate_x, robot_plate_y, 0),
                                  tf.transformations.quaternion_from_euler(0, 0, 0),
                                  rospy.Time.now(),
                                  "Robot",
                                  "Plate")
        except (tf.LookupException, tf.ConnectivityException):
            pass

    def handle_fly_image_pose(self,msg):
        try:
            Xsrc = [msg.pose.position.x]
            Ysrc = [msg.pose.position.y]
            self.tf_broadcaster.sendTransform((msg.pose.position.x, msg.pose.position.y, 0),
                                  tf.transformations.quaternion_from_euler(0, 0, 0),
                                  rospy.Time.now(),
                                  "FlyImage",
                                  "Camera")

            response = self.camera_to_plate(Xsrc,Ysrc)
            fly_plate_x = response.Xdst[0]
            fly_plate_y = response.Ydst[0]
            self.tf_broadcaster.sendTransform((fly_plate_x, fly_plate_y, 0),
                                  tf.transformations.quaternion_from_euler(0, 0, 0),
                                  rospy.Time.now(),
                                  "Fly",
                                  "Plate")
        except (tf.LookupException, tf.ConnectivityException):
            pass

if __name__ == '__main__':
    rospy.init_node('plate_tf_broadcaster')
    ptc = PoseTFConversion()
    while not rospy.is_shutdown():
        rospy.spin()
