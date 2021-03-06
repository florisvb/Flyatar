#!/usr/bin/env python
from __future__ import division
import roslib
roslib.load_manifest('image_gui')
import sys
import rospy
import cv
import tf
import math
from sensor_msgs.msg import Image
from cv_bridge import CvBridge, CvBridgeError
from geometry_msgs.msg import PointStamped
from stage.msg import Setpoint
from plate_tf.srv import *

class ImageDisplay:

    def __init__(self):
        self.initialized = False
        self.images_initialized = False
        self.tf_listener = tf.TransformListener()
        self.image_frame = "UndistortedImage"
        self.image_sub = rospy.Subscriber(self.image_frame, Image, self.image_callback)
        self.image_pub = rospy.Publisher("DisplayImage",Image)
        self.setpoint_sub = rospy.Subscriber("setpoint",Setpoint, self.setpoint_callback)

        cv.NamedWindow("Display",1)
        self.bridge = CvBridge()

        self.color_max = 255
        self.font = cv.InitFont(cv.CV_FONT_HERSHEY_TRIPLEX,0.5,0.5)

        self.plate_image_origin = PointStamped()
        self.plate_image_origin.header.frame_id = "PlateImage"
        self.plate_image_origin.point.x = 0
        self.plate_image_origin.point.y = 0
        self.plate_image_origin.point.z = 0
        self.fly_image_origin = PointStamped()
        self.fly_image_origin.header.frame_id = "FlyImage"
        self.fly_image_origin.point.x = 0
        self.fly_image_origin.point.y = 0
        self.fly_image_origin.point.z = 0
        self.robot_image_origin = PointStamped()
        self.robot_image_origin.header.frame_id = "RobotImage"
        self.robot_image_origin.point.x = 0
        self.robot_image_origin.point.y = 0
        self.robot_image_origin.point.z = 0
        self.setpoint_camera = PointStamped()
        self.setpoint_camera.header.frame_id = "Camera"
        self.setpoint_frame = PointStamped()

        self.axis_length = 15
        self.axis_line_width = 3
        self.axes_center = PointStamped()
        self.axes_center.point.x = 0
        self.axes_center.point.y = 0
        self.axes_center.point.z = 0
        self.axes_x = PointStamped()
        self.axes_x.point.x = self.axis_length
        self.axes_x.point.y = 0
        self.axes_x.point.z = 0
        self.axes_y = PointStamped()
        self.axes_y.point.x = 0
        self.axes_y.point.y = self.axis_length
        self.axes_y.point.z = 0

        self.axes_center_camera = PointStamped()
        self.axes_center_camera.header.frame_id = "Camera"
        self.axes_x_camera = PointStamped()
        self.axes_x_camera.header.frame_id = "Camera"
        self.axes_y_camera = PointStamped()
        self.axes_y_camera.header.frame_id = "Camera"

        self.setpoint = Setpoint()

        rospy.wait_for_service('plate_to_camera')
        try:
            self.plate_to_camera = rospy.ServiceProxy('plate_to_camera', PlateCameraConversion)
        except rospy.ServiceException, e:
            print "Service call failed: %s"%e

        self.initialized = True

    def initialize_images(self,cv_image):
        self.im_display = cv.CreateImage(cv.GetSize(cv_image),cv.IPL_DEPTH_8U,3)
        self.images_initialized = True

    def setpoint_callback(self,data):
        if not self.initialized:
            return
        self.setpoint.header.frame_id = data.header.frame_id
        self.setpoint.radius = data.radius
        self.setpoint.theta = data.theta

    def draw_axes(self,frame_id):
        try:
            self.axes_center.header.frame_id = frame_id
            self.axes_x.header.frame_id = frame_id
            self.axes_y.header.frame_id = frame_id

            axes_center_plate = self.tf_listener.transformPoint("Plate",self.axes_center)
            axes_x_plate = self.tf_listener.transformPoint("Plate",self.axes_x)
            axes_y_plate = self.tf_listener.transformPoint("Plate",self.axes_y)

            Xsrc = [axes_center_plate.point.x,axes_x_plate.point.x,axes_y_plate.point.x]
            Ysrc = [axes_center_plate.point.y,axes_x_plate.point.y,axes_y_plate.point.y]
            response = self.plate_to_camera(Xsrc,Ysrc)
            self.axes_center_camera.point.x = response.Xdst[0]
            self.axes_center_camera.point.y = response.Ydst[0]
            self.axes_x_camera.point.x = response.Xdst[1]
            self.axes_x_camera.point.y = response.Ydst[1]
            self.axes_y_camera.point.x = response.Xdst[2]
            self.axes_y_camera.point.y = response.Ydst[2]
            axes_center_image = self.tf_listener.transformPoint(self.image_frame,self.axes_center_camera)
            axes_x_image = self.tf_listener.transformPoint(self.image_frame,self.axes_x_camera)
            axes_y_image = self.tf_listener.transformPoint(self.image_frame,self.axes_y_camera)

            cv.Line(self.im_display,
                    (int(axes_center_image.point.x),int(axes_center_image.point.y)),
                    (int(axes_x_image.point.x),int(axes_x_image.point.y)),
                    cv.CV_RGB(self.color_max,0,0), self.axis_line_width)
            cv.Line(self.im_display,
                    (int(axes_center_image.point.x),int(axes_center_image.point.y)),
                    (int(axes_y_image.point.x),int(axes_y_image.point.y)),
                    cv.CV_RGB(0,self.color_max,0), self.axis_line_width)

        except (tf.LookupException, tf.ConnectivityException, rospy.ServiceException):
            pass

    def draw_setpoint(self):
        try:
            self.setpoint_frame.header.frame_id = self.setpoint.header.frame_id
            self.setpoint_frame.point.x = self.setpoint.radius*math.cos(self.setpoint.theta)
            self.setpoint_frame.point.y = self.setpoint.radius*math.sin(self.setpoint.theta)

            setpoint_plate = self.tf_listener.transformPoint("Plate",self.setpoint_frame)
            # rospy.logwarn("setpoint_plate.point.x = \n%s", str(setpoint_plate.point.x))
            # rospy.logwarn("setpoint_plate.point.y = \n%s", str(setpoint_plate.point.y))
            Xsrc = [setpoint_plate.point.x]
            Ysrc = [setpoint_plate.point.y]
            response = self.plate_to_camera(Xsrc,Ysrc)
            self.setpoint_camera.point.x = response.Xdst[0]
            self.setpoint_camera.point.y = response.Ydst[0]
            setpoint_image = self.tf_listener.transformPoint(self.image_frame,self.setpoint_camera)
            setpoint_image_radius = math.sqrt((setpoint_image.point.x - self.setpoint_image_origin.point.x)**2 +
                                              (setpoint_image.point.y - self.setpoint_image_origin.point.y)**2 )

            cv.Line(self.im_display,
                    (int(self.setpoint_image_origin.point.x),int(self.setpoint_image_origin.point.y)),
                    (int(setpoint_image.point.x),int(setpoint_image.point.y)),
                    cv.CV_RGB(self.color_max,0,0), 1)
            cv.Circle(self.im_display,
                      (int(self.setpoint_image_origin.point.x),int(self.setpoint_image_origin.point.y)),
                      int(setpoint_image_radius), cv.CV_RGB(self.color_max,0,0),1)
            cv.Circle(self.im_display,
                      (int(self.setpoint_image_origin.point.x),int(self.setpoint_image_origin.point.y)),
                      3, cv.CV_RGB(self.color_max,0,0), cv.CV_FILLED)
            cv.Circle(self.im_display,
                      (int(setpoint_image.point.x),int(setpoint_image.point.y)),
                      3, cv.CV_RGB(self.color_max,0,0), cv.CV_FILLED)

        except (tf.LookupException, tf.ConnectivityException, rospy.ServiceException):
            pass

        # display_text = "setpoint_camera.x = " + str(setpoint_camera.point.x)
        # cv.PutText(self.im_display,display_text,(25,65),self.font,cv.CV_RGB(self.color_max,0,0))
        # display_text = "setpoint_camera.y= " + str(setpoint_camera.point.y)
        # cv.PutText(self.im_display,display_text,(25,85),self.font,cv.CV_RGB(self.color_max,0,0))

    def image_callback(self,data):
        if not self.initialized:
            return
        # Convert ROS image to OpenCV image
        try:
            cv_image = cv.GetImage(self.bridge.imgmsg_to_cv(data, "passthrough"))
        except CvBridgeError, e:
            print e

        if not self.images_initialized:
            self.initialize_images(cv_image)

        cv.CvtColor(cv_image,self.im_display,cv.CV_GRAY2RGB)

        try:
            plate_image_o = self.tf_listener.transformPoint(self.image_frame,self.plate_image_origin)
            fly_image_o = self.tf_listener.transformPoint(self.image_frame,self.fly_image_origin)
            robot_image_o = self.tf_listener.transformPoint(self.image_frame,self.robot_image_origin)

            if self.setpoint.header.frame_id in "Plate":
                self.setpoint_image_origin = plate_image_o
            else:
                self.setpoint_image_origin = fly_image_o

            self.draw_setpoint()
            self.draw_axes("Plate")
            self.draw_axes("Fly")
            self.draw_axes("Robot")

        except (tf.LookupException, tf.ConnectivityException):
            pass

        # display_text = "radius setpoint = " + str(self.setpoint.radius)
        # cv.PutText(self.im_display,display_text,(25,25),self.font,cv.CV_RGB(self.color_max,0,0))
        # display_text = "theta setpoint = " + str(self.setpoint.theta)
        # cv.PutText(self.im_display,display_text,(25,45),self.font,cv.CV_RGB(self.color_max,0,0))

        cv.ShowImage("Display", self.im_display)
        # Publish processed image
        try:
            self.image_pub.publish(self.bridge.cv_to_imgmsg(self.im_display,"passthrough"))
        except CvBridgeError, e:
            print e
        cv.WaitKey(3)

if __name__ == '__main__':
    rospy.init_node('ImageDisplay', anonymous=True)
    id = ImageDisplay()

    while not rospy.is_shutdown():
        rospy.spin()
