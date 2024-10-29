// Copyright 2017 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>

#include <depthimage_to_pointcloud2/depth_conversions.hpp>

#include <image_geometry/pinhole_camera_model.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

// #include <limits>
// #include <functional>
// #include <memory>
// #include <string>
// #include <vector>

/* Usage example remapping:
$ ros2 run depthimage_to_pointcloud2 depthimage_to_pointcloud2_node \
--ros-args -r depth:=/my_depth_sensor/image -r depth_camera_info:=/my_depth_sensor/camera_info -r pointcloud2:=/my_output_topic
*/

using std::placeholders::_1;

/* This example creates a subclass of Node and uses std::bind() to register a
* member function as a callback from the timer. */

class Depthimage2Pointcloud2 : public rclcpp::Node
{
  public:
    Depthimage2Pointcloud2(const rclcpp::NodeOptions & options)
    : Node("depthimage_to_pointcloud2_node", options)
    {
      range_max = this->declare_parameter("range_max", 0.0);
      use_quiet_nan = this->declare_parameter("use_quiet_nan", true);
      colorful = this->declare_parameter("colorful", false);

      g_pub_point_cloud = this->create_publisher<sensor_msgs::msg::PointCloud2>("pointcloud2", 10);

      if (colorful){
        image_sub = this->create_subscription<sensor_msgs::msg::Image>(
          "image", 10, std::bind(&Depthimage2Pointcloud2::imageCb, this, _1));
      }

      depthimage_sub = this->create_subscription<sensor_msgs::msg::Image>(
        "depth", 10, std::bind(&Depthimage2Pointcloud2::depthCb, this, _1));
      cam_info_sub = this->create_subscription<sensor_msgs::msg::CameraInfo>(
        "depth_camera_info", 10, std::bind(&Depthimage2Pointcloud2::infoCb, this, _1));
    }

  private:
    void imageCb(const sensor_msgs::msg::Image::SharedPtr msg)
    {

      try
      {
          cv_ptr = cv_bridge::toCvShare(msg, msg->encoding);
      }
      catch (cv_bridge::Exception& e)
      {
          RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
          return;
      }
    }

  private:
    void depthCb(const sensor_msgs::msg::Image::SharedPtr image)
    {
      // The meat of this function is a port of the code from:
      // https://github.com/ros-perception/image_pipeline/blob/92d7f6b/depth_image_proc/src/nodelets/point_cloud_xyz.cpp

      if (nullptr == g_cam_info) {
        // we haven't gotten the camera info yet, so just drop until we do
        RCUTILS_LOG_WARN("No camera info, skipping point cloud conversion");
        return;
      }

      sensor_msgs::msg::PointCloud2::SharedPtr cloud_msg =
        std::make_shared<sensor_msgs::msg::PointCloud2>();
      cloud_msg->header = image->header;
      cloud_msg->height = image->height / 2; //to decimate the point cloud to a quarter of the image resolution
      cloud_msg->width = image->width / 2;  //to decimate the point cloud to a quarter of the image resolution
      cloud_msg->is_dense = false;
      cloud_msg->is_bigendian = false;
      cloud_msg->fields.clear();
      cloud_msg->fields.reserve(2);

      sensor_msgs::PointCloud2Modifier pcd_modifier(*cloud_msg);
      pcd_modifier.setPointCloud2FieldsByString(2, "xyz", "rgb");

      // g_cam_info here is a sensor_msg::msg::CameraInfo::SharedPtr,
      // which we get from the depth_camera_info topic.
      image_geometry::PinholeCameraModel model;
      model.fromCameraInfo(g_cam_info);

      if (image->encoding == sensor_msgs::image_encodings::TYPE_16UC1) {
        depthimage_to_pointcloud2::convert<uint16_t>(image, cloud_msg, model, range_max, use_quiet_nan, cv_ptr);
      } else if (image->encoding == sensor_msgs::image_encodings::TYPE_32FC1) {
        depthimage_to_pointcloud2::convert<float>(image, cloud_msg, model, range_max, use_quiet_nan, cv_ptr);
      } else {
        RCUTILS_LOG_WARN_THROTTLE(RCUTILS_STEADY_TIME, 5000,
          "Depth image has unsupported encoding [%s]", image->encoding.c_str());
        return;
      }

      g_pub_point_cloud->publish(*cloud_msg);
    }

    void infoCb(sensor_msgs::msg::CameraInfo::SharedPtr info)
    {
      g_cam_info = info;
    }

    sensor_msgs::msg::CameraInfo::SharedPtr g_cam_info;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr g_pub_point_cloud;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr depthimage_sub;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub;
    rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr cam_info_sub;
    
    cv_bridge::CvImageConstPtr cv_ptr;
    double range_max;
    bool use_quiet_nan;
    bool colorful;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Depthimage2Pointcloud2>(rclcpp::NodeOptions()));
  rclcpp::shutdown();
  return 0;
}