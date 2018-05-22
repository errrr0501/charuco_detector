/**\file charuco_detector.cpp
 * \brief Detector of ChArUco patterns
 *
 * @version 1.0
 * @author Carlos Miguel Correia da Costa
 */

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <includes>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#include <charuco_detector/charuco_detector.h>
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </includes>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


namespace charuco_detector {

	void ChArUcoDetector::setupConfigurationFromParameterServer(ros::NodeHandlePtr &_node_handle, ros::NodeHandlePtr &_private_node_handle) {
		node_handle_ = _node_handle;
		private_node_handle_ = _private_node_handle;

		detector_parameters_ = cv::aruco::DetectorParameters::create();

		private_node_handle_->param("charuco/adaptiveThreshWinSizeMin", detector_parameters_->adaptiveThreshWinSizeMin, 3);
		private_node_handle_->param("charuco/adaptiveThreshWinSizeMax", detector_parameters_->adaptiveThreshWinSizeMax, 23);
		private_node_handle_->param("charuco/adaptiveThreshWinSizeStep", detector_parameters_->adaptiveThreshWinSizeStep, 10);
		private_node_handle_->param("charuco/adaptiveThreshConstant", detector_parameters_->adaptiveThreshConstant, 7.0);
		private_node_handle_->param("charuco/minMarkerPerimeterRate", detector_parameters_->minMarkerPerimeterRate, 0.03);
		private_node_handle_->param("charuco/maxMarkerPerimeterRate", detector_parameters_->maxMarkerPerimeterRate, 4.0);
		private_node_handle_->param("charuco/polygonalApproxAccuracyRate", detector_parameters_->polygonalApproxAccuracyRate, 0.03);
		private_node_handle_->param("charuco/minCornerDistanceRate", detector_parameters_->minCornerDistanceRate, 0.05);
		private_node_handle_->param("charuco/minDistanceToBorder", detector_parameters_->minDistanceToBorder, 3);
		private_node_handle_->param("charuco/minMarkerDistanceRate", detector_parameters_->minMarkerDistanceRate, 0.05);
		private_node_handle_->param("charuco/cornerRefinementMethod", detector_parameters_->cornerRefinementMethod, 0);
		private_node_handle_->param("charuco/cornerRefinementWinSize", detector_parameters_->cornerRefinementWinSize, 5);
		private_node_handle_->param("charuco/cornerRefinementMaxIterations", detector_parameters_->cornerRefinementMaxIterations, 30);
		private_node_handle_->param("charuco/cornerRefinementMinAccuracy", detector_parameters_->cornerRefinementMinAccuracy, 0.1);
		private_node_handle_->param("charuco/markerBorderBits", detector_parameters_->markerBorderBits, 1);
		private_node_handle_->param("charuco/perspectiveRemovePixelPerCell", detector_parameters_->perspectiveRemovePixelPerCell, 4);
		private_node_handle_->param("charuco/perspectiveRemoveIgnoredMarginPerCell", detector_parameters_->perspectiveRemoveIgnoredMarginPerCell, 0.13);
		private_node_handle_->param("charuco/maxErroneousBitsInBorderRate", detector_parameters_->maxErroneousBitsInBorderRate, 0.35);
		private_node_handle_->param("charuco/minOtsuStdDev", detector_parameters_->minOtsuStdDev, 5.0);
		private_node_handle_->param("charuco/errorCorrectionRate", detector_parameters_->errorCorrectionRate, 0.6);

		private_node_handle_->param("charuco/squaresSidesSizeM", squares_sides_size_m_, 0.0280);
		private_node_handle_->param("charuco/markersSidesSizeM", markers_sides_size_m_, 0.0168);
		private_node_handle_->param("charuco/numberOfBitsForMarkersSides", number_of_bits_for_markers_sides_, 6);
		private_node_handle_->param("charuco/numberOfMarkers", number_of_markers_, 70);
		private_node_handle_->param("charuco/numberOfSquaresInX", number_of_squares_in_x_, 10);
		private_node_handle_->param("charuco/numberOfSquaresInY", number_of_squares_in_y_, 14);
		private_node_handle_->param("charuco/dictionaryId", dictionary_id_, 10);

		private_node_handle_->param("charuco_tf_frame", charuco_tf_frame_, std::string("charuco"));
		private_node_handle_->param("image_topic", image_topic_, std::string("image_raw"));
		private_node_handle_->param("camera_info_topic", camera_info_topic_, std::string("camera_info"));
		private_node_handle_->param("image_analysis_publish_topic", image_results_publish_topic_, image_topic_ + std::string("_charuco_detection"));
		private_node_handle_->param("charuco_pose_publish_topic", charuco_pose_publish_topic_, image_topic_ + std::string("_charuco_pose"));

		if (dictionary_id_ > 0)
			dictionary_ = cv::aruco::getPredefinedDictionary(cv::aruco::PREDEFINED_DICTIONARY_NAME(dictionary_id_));
		else
			dictionary_ = cv::aruco::generateCustomDictionary(number_of_markers_, number_of_bits_for_markers_sides_);

		board_ = cv::aruco::CharucoBoard::create(number_of_squares_in_x_, number_of_squares_in_y_,
												 static_cast<float>(squares_sides_size_m_), static_cast<float>(markers_sides_size_m_), dictionary_);
	}


	void ChArUcoDetector::startDetection() {
		image_transport_ptr_ = std::make_shared<image_transport::ImageTransport>(*node_handle_);
		image_subscriber_ = image_transport_ptr_->subscribe(image_topic_, 1, &ChArUcoDetector::imageCallback, this);

		camera_info_subscriber_ = node_handle_->subscribe(camera_info_topic_, 1, &ChArUcoDetector::cameraInfoCallback, this);

		image_transport_results_ptr_ = std::make_shared<image_transport::ImageTransport>(*node_handle_);
		image_results_publisher_ = image_transport_results_ptr_->advertise(image_results_publish_topic_, 1, true);

		charuco_pose_publisher_ = node_handle_->advertise<geometry_msgs::PoseStamped>(charuco_pose_publish_topic_, 1, true);
	}


	void ChArUcoDetector::imageCallback(const sensor_msgs::ImageConstPtr &_msg) {
		if (camera_info_) {
			try {
				cv::Mat image_grayscale = cv_bridge::toCvCopy(_msg, sensor_msgs::image_encodings::MONO8)->image;

				cv::Vec3d camera_rotation, camera_translation;
				cv::Mat image_results;

				if (detectChArUcoBoard(image_grayscale, camera_intrinsics_matrix, camera_distortion_coefficients_matrix, camera_rotation, camera_translation, image_results, true)) {
					geometry_msgs::PoseStamped charuco_pose;
					charuco_pose.header = _msg->header;
					fillPose(camera_rotation, camera_translation, charuco_pose);
					charuco_pose_publisher_.publish(charuco_pose);

					geometry_msgs::TransformStamped static_transformStamped;
					static_transformStamped.header = _msg->header;
					static_transformStamped.child_frame_id = charuco_tf_frame_;
					static_transformStamped.transform.translation.x = charuco_pose.pose.position.x;
					static_transformStamped.transform.translation.y = charuco_pose.pose.position.y;
					static_transformStamped.transform.translation.z = charuco_pose.pose.position.z;
					static_transformStamped.transform.rotation = charuco_pose.pose.orientation;
					static_tf_broadcaster_.sendTransform(static_transformStamped);

					sensor_msgs::ImagePtr image_results_msg = cv_bridge::CvImage(std_msgs::Header(), "rgb8", image_results).toImageMsg();
					image_results_publisher_.publish(image_results_msg);
				}
			} catch (...) {
				ROS_WARN("Caught exception when analyzing image");
			}
		} else {
			ROS_WARN("Discarded image because a valid CameraInfo was not received yet");
		}
	}


	void ChArUcoDetector::cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr &_msg) {
		bool valid_camera_info = false;
		for (size_t i = 0; i < _msg->K.size(); ++i) {
			if (_msg->K[i] != 0.0) {
				valid_camera_info = true;
				break;
			}
		}

		if (valid_camera_info) {
			camera_info_ = _msg;
			camera_intrinsics_matrix = cv::Mat::zeros(3, 3, CV_64F);
			camera_distortion_coefficients_matrix = cv::Mat::zeros(1, 5, CV_64F);

			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					camera_intrinsics_matrix.at<double>(i, j) = _msg->K[i * 3 + j];
				}
			}

			for (int i = 0; i < 5; i++) {
				camera_distortion_coefficients_matrix.at<double>(0, i) = _msg->D[i];
			}
		} else {
			ROS_WARN("Received invalid camera intrinsics (K all zeros)");
		}
	}


	bool ChArUcoDetector::detectChArUcoBoard(const cv::Mat &_image_grayscale, const cv::Mat &_camera_intrinsics, const cv::Mat &_camera_distortion_coefficients,
											 cv::Vec3d &_camera_rotation_out, cv::Vec3d &_camera_translation_out,
											 cv::InputOutputArray _image_with_detection_results, bool _show_rejected_markers) {
		std::vector<int> _marker_ids, charuco_ids;
		std::vector<std::vector<cv::Point2f> > marker_corners, rejected_markers;
		std::vector<cv::Point2f> charuco_corners;

		cv::aruco::detectMarkers(_image_grayscale, dictionary_, marker_corners, _marker_ids, detector_parameters_, rejected_markers);
		cv::aruco::refineDetectedMarkers(_image_grayscale, board_, marker_corners, _marker_ids, rejected_markers, _camera_intrinsics, _camera_distortion_coefficients);

		int interpolatedCorners = 0;
		if (!_marker_ids.empty())
			interpolatedCorners = cv::aruco::interpolateCornersCharuco(marker_corners, _marker_ids, _image_grayscale, board_, charuco_corners, charuco_ids,
																	   _camera_intrinsics, _camera_distortion_coefficients);

		bool valid_pose = false;
		if (_camera_intrinsics.total() != 0)
			valid_pose = cv::aruco::estimatePoseCharucoBoard(charuco_corners, charuco_ids, board_, _camera_intrinsics, _camera_distortion_coefficients,
															 _camera_rotation_out, _camera_translation_out);

		if (_image_with_detection_results.needed()) {
			cv::cvtColor(_image_grayscale, _image_with_detection_results, cv::COLOR_GRAY2BGR);
			if (!_marker_ids.empty()) {
				cv::aruco::drawDetectedMarkers(_image_with_detection_results, marker_corners);
			}

			if (_show_rejected_markers && !rejected_markers.empty())
				cv::aruco::drawDetectedMarkers(_image_with_detection_results, rejected_markers, cv::noArray(), cv::Scalar(100, 0, 255));

			if (interpolatedCorners > 0) {
				cv::Scalar color(255, 0, 0);
				cv::aruco::drawDetectedCornersCharuco(_image_with_detection_results, charuco_corners, charuco_ids, color);
			}
		}

		if (valid_pose) {
			if (_image_with_detection_results.needed()) {
				float axisLength = 0.5f * (static_cast<float>(std::min(number_of_squares_in_x_, number_of_squares_in_y_) * (squares_sides_size_m_)));
				cv::aruco::drawAxis(_image_with_detection_results, _camera_intrinsics, _camera_distortion_coefficients, _camera_rotation_out, _camera_translation_out, axisLength);
			}

			return true;
		}

		return false;
	}


	void ChArUcoDetector::fillPose(const cv::Vec3d &_camera_rotation, const cv::Vec3d &_camera_translation, geometry_msgs::PoseStamped &_pose_in_out) {
		cv::Mat rotation_matrix;
		cv::Rodrigues(_camera_rotation, rotation_matrix);
		Eigen::Matrix3d eigen_rotation_matrix;
		cv::cv2eigen(rotation_matrix, eigen_rotation_matrix);
		Eigen::Quaterniond q(eigen_rotation_matrix);
		_pose_in_out.pose.position.x = _camera_translation(0);
		_pose_in_out.pose.position.y = _camera_translation(1);
		_pose_in_out.pose.position.z = _camera_translation(2);
		_pose_in_out.pose.orientation.x = q.x();
		_pose_in_out.pose.orientation.y = q.y();
		_pose_in_out.pose.orientation.z = q.z();
		_pose_in_out.pose.orientation.w = q.w();
	}

}
