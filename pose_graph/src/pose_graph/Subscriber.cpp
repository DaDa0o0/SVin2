#include "pose_graph/Subscriber.h"

#include <ros/console.h>

#include <memory>
#include <utility>
#include <vector>

#include "utils/UtilsOpenCV.h"

Subscriber::Subscriber(ros::NodeHandle& nh, const Parameters& params) : params_(params) {
  // TODO(bjoshi): pass as params from roslaunch file
  kf_image_topic_ = "/okvis_node/keyframe_imageL";
  kf_pose_topic_ = "/okvis_node/keyframe_pose";
  kf_points_topic_ = "/okvis_node/keyframe_points";
  svin_reloc_odom_topic_ = "/okvis_node/relocalization_odometry";
  svin_health_topic_ = "/okvis_node/svin_health";
  primitive_estimator_topic_ = "/aqua_primitive_estimator/odometry";
  last_image_time_ = -1;

  setNodeHandle(nh);
}

void Subscriber::setNodeHandle(ros::NodeHandle& nh) {
  nh_ = &nh;
  if (it_) it_.reset();
  it_ = std::make_unique<image_transport::ImageTransport>(std::move(*nh_));

  keyframe_image_subscriber_.subscribe(*it_, kf_image_topic_, 100);
  keyframe_points_subscriber_.subscribe(*nh_, kf_points_topic_, 100);
  keyframe_pose_subscriber_.subscribe(*nh_, kf_pose_topic_, 100);
  svin_health_subscriber_.subscribe(*nh_, svin_health_topic_, 100);

  static constexpr size_t kMaxKeyframeSynchronizerQueueSize = 100u;
  sync_keyframe_ = std::make_unique<message_filters::Synchronizer<keyframe_sync_policy>>(
      keyframe_sync_policy(kMaxKeyframeSynchronizerQueueSize),
      keyframe_image_subscriber_,
      keyframe_pose_subscriber_,
      keyframe_points_subscriber_,
      svin_health_subscriber_);
  sync_keyframe_->registerCallback(boost::bind(&Subscriber::keyframeCallback, this, _1, _2, _3, _4));

  // sub_orig_image_ =
  // it_->subscribe("/cam0/image_raw", 500, std::bind(&Subscriber::imageCallback, this, std::placeholders::_1));

  if (params_.health_params_.health_monitoring_enabled) {
    sub_primitive_estimator_ =
        nh_->subscribe(primitive_estimator_topic_, 500, &Subscriber::primitiveEstimatorCallback, this);
  }
}

void Subscriber::primitiveEstimatorCallback(const nav_msgs::OdometryConstPtr& msg) {
  std::lock_guard<std::mutex> l(measurement_mutex_);
  prim_estimator_odom_buffer_.push(msg);
  last_primitive_estimator_time_ = msg->header.stamp.toSec();
}

// void Subscriber::svinHealthCallback(const okvis_ros::SvinHealthConstPtr& msg) {
//   std::lock_guard<std::mutex> l(measurement_mutex_);
//   svin_health_buffer_.push(msg);
// }

void Subscriber::imageCallback(const sensor_msgs::ImageConstPtr& msg) {
  std::lock_guard<std::mutex> lock(measurement_mutex_);
  orig_image_buffer_.push(msg);
  // ROS_WARN_STREAM("GOT COLOR IMAGE");
}

const cv::Mat Subscriber::getCorrespondingImage(const uint64_t& ros_stamp) {
  sensor_msgs::ImageConstPtr img_msg;

  uint64_t search_stamp = ros_stamp;
  if (params_.image_delay_ != 0.0) {
    search_stamp = ros_stamp + ros::Duration(params_.image_delay_).toNSec();
  }

  // ROS_WARN_STREAM(ros_stamp << "\t" << search_stamp);

  while (!orig_image_buffer_.empty() && orig_image_buffer_.front()->header.stamp.toNSec() < search_stamp) {
    img_msg = orig_image_buffer_.front();
    orig_image_buffer_.pop();
  }

  uint64_t diff = 0;
  if (img_msg) {
    diff = abs(static_cast<int64_t>(img_msg->header.stamp.toNSec()) - static_cast<int64_t>(search_stamp));
  }
  if (!orig_image_buffer_.empty()) {
    uint64_t upper_diff = abs(static_cast<int64_t>(orig_image_buffer_.front()->header.stamp.toNSec()) -
                              static_cast<int64_t>(search_stamp));
    if (img_msg == nullptr || diff > upper_diff) {
      img_msg = orig_image_buffer_.front();
      orig_image_buffer_.pop();
      diff = upper_diff;
    }
  }

  if (diff > 100000000) {
    ROS_WARN_STREAM("Time difference between keyframe and original image is too large: " << diff << " ns");
    ROS_WARN_STREAM("Image time: " << img_msg->header.stamp.toNSec() << " ns");
    ROS_WARN_STREAM("Keyframe time: " << search_stamp << " ns");
  }

  cv_bridge::CvImageConstPtr cv_ptr;
  try {
    // TODO(Toni): here we should consider using toCvShare...
    cv_ptr = cv_bridge::toCvCopy(img_msg);
  } catch (cv_bridge::Exception& exception) {
    ROS_FATAL("cv_bridge exception: %s", exception.what());
    ros::shutdown();
  }

  const cv::Mat img_const = cv_ptr->image;  // Don't modify shared image in ROS.
  cv::Mat converted_img;
  if (img_msg->encoding == sensor_msgs::image_encodings::RGB8) {
    // LOG_EVERY_N(WARNING, 10) << "Converting image...";
    cv::cvtColor(img_const, converted_img, cv::COLOR_RGB2BGR);
    return converted_img;
  } else {
    return img_const;
  }
}

nav_msgs::OdometryConstPtr Subscriber::getPrimitiveEstimatorPose(const uint64_t& ros_stamp) {
  nav_msgs::OdometryConstPtr prim_estimator_pose = nullptr;
  // 25ms sync period while using primitive estimator publish rate of 20Hz
  while (!prim_estimator_odom_buffer_.empty() &&
         prim_estimator_odom_buffer_.front()->header.stamp.toNSec() < (ros_stamp - 25000000)) {
    prim_estimator_pose = prim_estimator_odom_buffer_.front();
    prim_estimator_odom_buffer_.pop();
  }

  if (!prim_estimator_odom_buffer_.empty()) {
    prim_estimator_pose = prim_estimator_odom_buffer_.front();
    prim_estimator_odom_buffer_.pop();
  }

  return prim_estimator_pose;
}

void Subscriber::getPrimitiveEstimatorPoses(const uint64_t& ros_stamp, std::vector<nav_msgs::OdometryConstPtr>& poses) {
  nav_msgs::OdometryConstPtr prim_estimator_pose = nullptr;
  // 25ms sync period while using primitive estimator publish rate of 20Hz

  while (!prim_estimator_odom_buffer_.empty() &&
         prim_estimator_odom_buffer_.front()->header.stamp.toNSec() < (ros_stamp - 25000000)) {
    prim_estimator_odom_buffer_.pop();
  }

  while (!prim_estimator_odom_buffer_.empty()) {
    poses.emplace_back(prim_estimator_odom_buffer_.front());
    prim_estimator_odom_buffer_.pop();
  }
}

void Subscriber::keyframeCallback(const sensor_msgs::ImageConstPtr& kf_image_msg,
                                  const nav_msgs::OdometryConstPtr& kf_odom,
                                  const sensor_msgs::PointCloudConstPtr& kf_points,
                                  const okvis_ros::SvinHealthConstPtr& svin_health) {
  TrackingInfo tracking_info(kf_odom->header.stamp,
                             svin_health->numTrackedKps,
                             svin_health->newKps,
                             svin_health->kpsPerQuadrant,
                             svin_health->covisibilities,
                             svin_health->responseStrengths,
                             svin_health->quality);

  Eigen::Vector3d translation =
      Eigen::Vector3d(kf_odom->pose.pose.position.x, kf_odom->pose.pose.position.y, kf_odom->pose.pose.position.z);
  Eigen::Matrix3d rotation = Eigen::Quaterniond(kf_odom->pose.pose.orientation.w,
                                                kf_odom->pose.pose.orientation.x,
                                                kf_odom->pose.pose.orientation.y,
                                                kf_odom->pose.pose.orientation.z)
                                 .toRotationMatrix();

  std::vector<cv::Point3f> keyframe_points;
  std::vector<cv::KeyPoint> keypoint_observations;
  std::vector<Eigen::Vector3d> point_ids;
  std::vector<int64_t> landmark_ids;
  std::vector<std::vector<int64_t>> kf_covisibilities;

  int64_t keyframe_index = kf_points->channels[0].values[4];
  cv::Mat kf_image = UtilsOpenCV::readRosImage(kf_image_msg);

  for (unsigned int i = 0; i < kf_points->points.size(); i++) {
    cv::Point3f point_3d(kf_points->points[i].x, kf_points->points[i].y, kf_points->points[i].z);
    keyframe_points.push_back(point_3d);

    // @Reloc landmarkId, poseId or MultiFrameId,  keypointIdx
    int64_t landmark_id = kf_points->channels[i].values[0];
    Eigen::Vector3d point_id(
        kf_points->channels[i].values[0], kf_points->channels[i].values[1], kf_points->channels[i].values[2]);
    point_ids.push_back(point_id);

    cv::KeyPoint p_2d_uv;
    p_2d_uv.pt.x = kf_points->channels[i].values[5];
    p_2d_uv.pt.y = kf_points->channels[i].values[6];
    p_2d_uv.size = kf_points->channels[i].values[7];
    p_2d_uv.angle = kf_points->channels[i].values[8];
    p_2d_uv.octave = kf_points->channels[i].values[9];
    p_2d_uv.response = kf_points->channels[i].values[10];
    p_2d_uv.class_id = kf_points->channels[i].values[11];

    keypoint_observations.push_back(p_2d_uv);

    std::vector<int64_t> covisible_kfs;
    for (size_t sz = 12; sz < kf_points->channels[i].values.size(); sz++) {
      int observed_kf_index = kf_points->channels[i].values[sz];
      if (observed_kf_index != keyframe_index) {
        covisible_kfs.push_back(observed_kf_index);
      }
    }
    kf_covisibilities.push_back(covisible_kfs);
  }

  std::unique_ptr<KeyframeInfo> keyframe_info = std::make_unique<KeyframeInfo>(keyframe_index,
                                                                               kf_image,
                                                                               translation,
                                                                               rotation,
                                                                               tracking_info,
                                                                               keyframe_points,
                                                                               keypoint_observations,
                                                                               point_ids,
                                                                               kf_covisibilities);

  keyframe_callback_(std::move(keyframe_info));
}
