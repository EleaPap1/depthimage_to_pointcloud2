// Copyright (c) 2008, Willow Garage, Inc.
// All rights reserved.
//
// Software License Agreement (BSD License 2.0)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//  * Neither the name of the Willow Garage nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// This file is originally from:
// https://github.com/ros-perception/image_pipeline/blob/da750d1/depth_image_proc/include/depth_image_proc/depth_conversions.h  // NOLINT

#ifndef DEPTHIMAGE_TO_POINTCLOUD2__DEPTH_CONVERSIONS_HPP_
#define DEPTHIMAGE_TO_POINTCLOUD2__DEPTH_CONVERSIONS_HPP_

#include "depthimage_to_pointcloud2/depth_traits.hpp"

#include <image_geometry/pinhole_camera_model.h>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <limits>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>

namespace depthimage_to_pointcloud2
{

// Handles float or uint16 depths
template<typename T>
void convert(
  const sensor_msgs::msg::Image::ConstSharedPtr & depth_msg,
  sensor_msgs::msg::PointCloud2::SharedPtr & cloud_msg,
  const image_geometry::PinholeCameraModel & model,
  double range_max = 0.0,
  bool use_quiet_nan = false,
 cv_bridge::CvImageConstPtr cv_ptr = nullptr)
{
  // Use correct principal point from calibration
  float center_x = model.cx();
  float center_y = model.cy();

  // Combine unit conversion (if necessary) with scaling by focal length for computing (X,Y)
  double unit_scaling = DepthTraits<T>::toMeters(T(1) );
  float constant_x = unit_scaling / model.fx();
  float constant_y = unit_scaling / model.fy();
  float bad_point = std::numeric_limits<float>::quiet_NaN();

  sensor_msgs::PointCloud2Iterator<float> iter_x(*cloud_msg, "x");
  sensor_msgs::PointCloud2Iterator<float> iter_y(*cloud_msg, "y");
  sensor_msgs::PointCloud2Iterator<float> iter_z(*cloud_msg, "z");
  sensor_msgs::PointCloud2Iterator<float> iter_rgb(*cloud_msg, "rgb");
  const T * depth_row = reinterpret_cast<const T *>(&depth_msg->data[0]);
  int row_step = depth_msg->step / sizeof(T);
  for (int v = 0; v < static_cast<int>(depth_msg->height); v += 2, depth_row += 2*row_step) {
    for (int u = 0; u < static_cast<int>(depth_msg->width); u += 2, ++iter_x, ++iter_y, ++iter_z, ++iter_rgb) {
      T depth = depth_row[u];

      // Missing points denoted by NaNs
      if (!DepthTraits<T>::valid(depth)) {
        if (range_max != 0.0 && !use_quiet_nan) {
          depth = DepthTraits<T>::fromMeters(range_max);
        } else {
          *iter_x = *iter_y = *iter_z = *iter_rgb = bad_point;
          continue;
        }
      } else if (range_max != 0.0) {
        T depth_max = DepthTraits<T>::fromMeters(range_max);
        if (depth > depth_max) {
          if (use_quiet_nan) {
            *iter_x = *iter_y = *iter_z = *iter_rgb = bad_point;
            continue;
          } else {
            depth = depth_max;
          }

        }
      }

      // Fill in XYZ
      *iter_x = (u - center_x) * depth * constant_x;
      *iter_y = (v - center_y) * depth * constant_y;
      *iter_z = DepthTraits<T>::toMeters(depth);
      
      // and RGB
      int rgb = 0x000000;
      if (cv_ptr != nullptr) {
        if (cv_ptr->image.type()==CV_8UC1) {
          //grayscale
          rgb &= cv_ptr->image.at<uchar>(v,u);
        } else if(cv_ptr->image.type()==CV_8UC3) {
          //RGB
          rgb = (int)cv_ptr->image.at<cv::Vec3b>(0, 0)[0];
          
        } else if(cv_ptr->image.type()==CV_8UC3 || cv_ptr->image.type()==CV_8UC4) {
          //RGB or RGBA
          if (cv_ptr->image.rows > v && cv_ptr->image.cols > u){
            rgb |= ((int)cv_ptr->image.at<cv::Vec4b>(v, u)[2]) << 16;
            rgb |= ((int)cv_ptr->image.at<cv::Vec4b>(v, u)[1]) << 8;
            rgb |= ((int)cv_ptr->image.at<cv::Vec4b>(v, u)[0]);
          }
        }
      }
      *iter_rgb = *reinterpret_cast<float*>(&rgb);
    }
  }
}

}  // namespace depthimage_to_pointcloud2

#endif  // DEPTHIMAGE_TO_POINTCLOUD2__DEPTH_CONVERSIONS_HPP_
