#!/usr/bin/env python
from __future__ import division
import roslib
roslib.load_manifest('avatar')
import sys
import rospy
import math
import tf
import cv
import numpy
from stage.srv import *
from stage.msg import Velocity,Setpoint
from joystick_commands.msg import JoystickCommands
from geometry_msgs.msg import PointStamped

class PositionControl:

    def __init__(self):
        self.initialized = False
        self.control_dt = rospy.get_param("Control_Update_dt")

        self.tf_listener = tf.TransformListener()

        self.joy_sub = rospy.Subscriber("Joystick/Commands", JoystickCommands, self.commands_callback)
        self.vel_pub = rospy.Publisher("stage/command_velocity",Velocity)
        self.setpoint_pub = rospy.Publisher("setpoint",Setpoint)

        self.vel_stage = Velocity()
        self.vel_scale_factor = 100     # mm/s
        self.vel_vector_plate = numpy.array([[0],[0],[0],[1]])
        self.vel_vector_robot = numpy.array([[0],[0],[0],[1]])

        self.control_frame = "Plate"

        self.setpoint = Setpoint()
        self.setpoint.header.frame_id = self.control_frame
        self.setpoint.radius = 20
        self.setpoint.theta = 0
        self.inc_radius = 1
        self.inc_theta = 0.05
        self.setpoint_radius_max = 80
        self.setpoint_radius_min = 20

        self.rate = rospy.Rate(1/self.control_dt)
        self.gain_radius = rospy.get_param("gain_radius","4")
        self.gain_theta = rospy.get_param("gain_theta","40")

        self.radius_velocity = 0
        self.tangent_velocity = 0
        self.tracking = False
        self.initialized = True

    def circle_dist(self,setpoint,angle):
        diff1 = setpoint - angle
        if angle < setpoint:
            diff2 = setpoint - 2*math.pi - angle
        else:
            diff2 = 2*math.pi - angle + setpoint
        abs_min = min(abs(diff1),abs(diff2))
        if abs_min == abs(diff1):
            return diff1
        else:
            return diff2

    def vel_vector_convert(self,vel_vector,frame):
        (trans,q) = self.tf_listener.lookupTransform("Stage",frame,rospy.Time(0))
        rot_matrix = tf.transformations.quaternion_matrix(q)
        vel_vector_stage = numpy.dot(rot_matrix,vel_vector)
        x_vel_stage = vel_vector_stage[0]
        y_vel_stage = vel_vector_stage[1]
        return [x_vel_stage,y_vel_stage]

    def commands_callback(self,data):
        if self.initialized:
            self.control_frame = data.header.frame_id
            self.setpoint.header.frame_id = self.control_frame
            self.setpoint.radius += self.inc_radius*data.radius_inc
            if self.setpoint.radius < self.setpoint_radius_min:
                self.setpoint.radius = self.setpoint_radius_min
            elif self.setpoint_radius_max < self.setpoint.radius:
                self.setpoint.radius = self.setpoint_radius_max
            self.setpoint.theta += self.inc_theta*data.theta_inc
            self.setpoint.theta = math.fmod(self.setpoint.theta,2*math.pi)
            if self.setpoint.theta < 0:
                self.setpoint.theta = 2*math.pi - self.setpoint.theta
            self.setpoint_pub.publish(self.setpoint)
            self.tracking = data.tracking

            if not self.tracking:
                self.radius_velocity = data.radius_velocity*self.vel_scale_factor
                self.tangent_velocity = data.tangent_velocity*self.vel_scale_factor
                self.vel_vector_plate[0,0] = data.x_velocity*self.vel_scale_factor
                self.vel_vector_plate[1,0] = data.y_velocity*self.vel_scale_factor

                try:
                    (self.vel_stage.x_velocity,self.vel_stage.y_velocity) = self.vel_vector_convert(self.vel_vector_plate,"Plate")
                except (tf.LookupException, tf.ConnectivityException):
                    self.vel_stage.x_velocity = self.vel_vector_plate[0,0]
                    self.vel_stage.y_velocity = -self.vel_vector_plate[1,0]

                self.vel_pub.publish(self.vel_stage)

    def control_loop(self):
        while not rospy.is_shutdown():
            if self.tracking:
                try:
                    self.gain_radius = rospy.get_param("gain_radius")
                    self.gain_theta = rospy.get_param("gain_theta")

                    (self.vel_stage.x_velocity,self.vel_stage.y_velocity) = self.vel_vector_convert(self.vel_vector_plate,"Plate")

                    (trans,q) = self.tf_listener.lookupTransform(self.control_frame,'/Robot',rospy.Time(0))
                    x = trans[0]
                    y = trans[1]
                    theta = math.atan2(y,x)
                    radius = math.sqrt(x**2 + y**2)

                    # rospy.logwarn("self.setpoint.radius = \n%s",str(self.setpoint.radius))
                    # rospy.logwarn("radius = \n%s",str(radius))
                    # rospy.logwarn("self.radius_velocity = \n%s",str(self.radius_velocity))

                    self.gain_radius = self.gain_radius + (6/math.pi)*abs(self.circle_dist(self.setpoint.theta,theta))
                    self.radius_velocity = self.gain_radius*(self.setpoint.radius - radius)
                    # rospy.logwarn("self.gain_radius = \n%s",str(self.gain_radius))
                    self.tangent_velocity = self.gain_theta*self.circle_dist(self.setpoint.theta,theta)

                    rot_matrix = tf.transformations.rotation_matrix(theta, (0,0,1))
                    self.vel_vector_robot[0,0] = self.radius_velocity
                    self.vel_vector_robot[1,0] = self.tangent_velocity
                    vel_vector_plate = numpy.dot(rot_matrix,self.vel_vector_robot)
                    self.vel_vector_plate[0,0] = vel_vector_plate[0]
                    self.vel_vector_plate[1,0] = vel_vector_plate[1]

                    (x_vel_stage,y_vel_stage) = self.vel_vector_convert(self.vel_vector_plate,"Plate")
                    self.vel_stage.x_velocity += x_vel_stage
                    self.vel_stage.y_velocity += y_vel_stage

                    self.vel_pub.publish(self.vel_stage)
                except (tf.LookupException, tf.ConnectivityException):
                    pass

            self.rate.sleep()

if __name__ == '__main__':
    rospy.init_node('PositionControl', anonymous=True)
    pc = PositionControl()
    pc.control_loop()
