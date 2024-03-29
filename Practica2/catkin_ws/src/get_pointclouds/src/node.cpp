#include <ros/ros.h>
#include <boost/foreach.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/features/fpfh.h>
#include <pcl_ros/point_cloud.h>
#include <geometry_msgs/Twist.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/keypoints/sift_keypoint.h>
#include <pcl/keypoints/harris_3d.h>
#include <pcl/keypoints/narf_keypoint.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/registration/correspondence_rejection.h>
#include <pcl/registration/correspondence_estimation.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/correspondence_estimation_normal_shooting.h>




ros::Publisher cmd_vel_pub_;


//Función para la neavegación del robot (por teclado)
void driveKeyboard() {

	std::cout << "Type a command and then press enter.  "
	"Use 'w' to move forward, 'a' to turn left, "
	"'d' to turn right, or any other character to do nothing.\n";

	geometry_msgs::Twist base_cmd;

	char cmd[50];
	std::cin.getline(cmd, 50);

	base_cmd.linear.x = base_cmd.linear.y = base_cmd.angular.z = 0;   
	switch (cmd[0]) {
		case 'w':{
			base_cmd.linear.x = 0.3;
			break;
		}
		case 'a':{
			base_cmd.angular.z = 0.2;
			break;
		}
		case 'd':{
			base_cmd.angular.z = -0.2;
			break;
		}
		case 's':{
			base_cmd.linear.x = -0.3;
			break;
		}
		default:{
			break;
		}
			
	}

	//lanzamos el comando creado
	cmd_vel_pub_.publish(base_cmd);   

}

// mapa de puntos 
pcl::PointCloud<pcl::PointXYZRGB>::Ptr visu_pc (new pcl::PointCloud<pcl::PointXYZRGB>);


pcl::PointCloud<pcl::PointWithScale>::Ptr anterior_keypoints;
pcl::PointCloud<pcl::FPFHSignature33>::Ptr anterior_features;
bool primero = true;


void simpleVis ()
{
  	pcl::visualization::CloudViewer viewer ("Simple Cloud Viewer");
	while(!viewer.wasStopped())
	{
	  viewer.showCloud (visu_pc);
	  boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
	}

}


// estimacion de normales
pcl::PointCloud<pcl::PointNormal>::Ptr calculateNormal(pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud){

	pcl::NormalEstimation<pcl::PointXYZRGB, pcl::PointNormal> normalEst; // normales estimadas
 	normalEst.setInputCloud(cloud);

	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree_n(new pcl::search::KdTree<pcl::PointXYZRGB>()); //método de búsqueda utilizado por el descriptor
  	normalEst.setSearchMethod(tree_n);
  	normalEst.setRadiusSearch(0.03);

	pcl::PointCloud<pcl::PointNormal>::Ptr cloud_normals(new pcl::PointCloud<pcl::PointNormal>); // será el resultado
  	normalEst.compute(*cloud_normals);

	return cloud_normals;
}


pcl::PointCloud<pcl::PointWithScale>::Ptr calculateKeyPoints(int dmethod, pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud_filtered, pcl::PointCloud<pcl::PointNormal>::Ptr& cloud_normals){

	for(size_t i = 0; i<cloud_normals->points.size(); ++i)
  	{
    	cloud_normals->points[i].x = cloud_filtered->points[i].x;
    	cloud_normals->points[i].y = cloud_filtered->points[i].y;
    	cloud_normals->points[i].z = cloud_filtered->points[i].z;
  	}
	
	pcl::PointCloud<pcl::PointWithScale>::Ptr result(new pcl::PointCloud<pcl::PointWithScale> ());
	pcl::search::KdTree<pcl::PointNormal>::Ptr tree(new pcl::search::KdTree<pcl::PointNormal> ());

	switch (dmethod){
		// case 0:{
		// 	float r_normal = 0.1;
		// 	float r_keypoint = 0.1;
			
		// 	pcl::HarrisKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::PointNormal>* HK3D = new pcl::HarrisKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::PointNormal>;

		// 	HK3D->setRadius(r_normal);
		// 	HK3D->setRadiusSearch(r_keypoint); 
		// 	HK3D->setInputCloud(cloud_normals);
		// 	HK3D->setNormals(cloud_normals);
		// 	HK3D->setSearchMethod(tree);
		// 	HK3D->compute( *result);


		// 	break;
		// }
		// case 1:{

		// 	break;
		// }

		default:{

			// Parameters for sift computation
			const float min_scale = 0.02f;
			const int n_octaves = 4;
			const int n_scales_per_octave = 8;
			const float min_contrast = 0.001f;
		
		
			// Estimate the sift interest points using Intensity values from RGB values
			pcl::SIFTKeypoint<pcl::PointNormal, pcl::PointWithScale> sift;
			sift.setSearchMethod(tree);
			sift.setScales(min_scale, n_octaves, n_scales_per_octave);
			sift.setMinimumContrast(min_contrast);
			sift.setInputCloud(cloud_normals);
			
			//sift.setRadiusSearch(0.05);
			sift.compute(*result);
			break;
		}
	}
	
	
	return result;

}


pcl::PointCloud<pcl::FPFHSignature33>::Ptr descriptorCarateristicas(pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud_filtered, pcl::PointCloud<pcl::PointNormal>::Ptr& cloud_normals,
										pcl::PointCloud<pcl::PointWithScale>::Ptr& keypoints){

	pcl::PointCloud<pcl::PointXYZRGB>::Ptr keypoints_cloud (new pcl::PointCloud<pcl::PointXYZRGB>);

	pcl::copyPointCloud(*keypoints, *keypoints_cloud);

	cout << "cloud_filtered Size: " << cloud_filtered->size() << endl;
	cout << "keypoints Size: " << keypoints->size() << endl;
	cout << "keypoints_cloud Size: " << keypoints_cloud->size() << endl;


	pcl::PointCloud<pcl::FPFHSignature33>::Ptr result(new pcl::PointCloud<pcl::FPFHSignature33> ());


	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZRGB> ());
	pcl::FPFHEstimation<pcl::PointXYZRGB, pcl::PointNormal, pcl::FPFHSignature33> fpfh;
  	fpfh.setInputCloud (keypoints_cloud);
	//fpfh.setInputCloud (cloud_filtered);
  	fpfh.setInputNormals (cloud_normals);
	fpfh.setSearchSurface (cloud_filtered);
	fpfh.setSearchMethod(tree);
  	fpfh.setRadiusSearch (0.07);
  	fpfh.compute (*result);

	return result;
}

void callback(const pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr& msg)
{
	driveKeyboard();

	// Codigo inicial
	//---------------------------------------------------------------------------------------------------
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGB>(*msg));
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZRGB>);

	cout << "Puntos capturados: " << cloud->size() << endl;

	pcl::VoxelGrid<pcl::PointXYZRGB > vGrid;
	vGrid.setInputCloud (cloud);
	vGrid.setLeafSize (0.01f, 0.01f, 0.01f);
	vGrid.filter (*cloud_filtered);

	cout << "Puntos tras VG: " << cloud_filtered->size() << endl;

	//visu_pc = cloud_filtered;
	//---------------------------------------------------------------------------------------------------
	
	//Se calculan las normales
	pcl::PointCloud<pcl::PointNormal>::Ptr cloud_normals = calculateNormal(cloud_filtered);
	
	//Hallamos los key points
	pcl::PointCloud<pcl::PointWithScale>::Ptr keypoints = calculateKeyPoints(-1, cloud_filtered, cloud_normals);
	
	//Descriptor de características
	pcl::PointCloud<pcl::FPFHSignature33>::Ptr cloud_features = descriptorCarateristicas(cloud_filtered, cloud_normals, keypoints);
	
	
	if(!primero){

		// Buscamos corespondencias ------------------------------------------------------------------
		pcl::registration::CorrespondenceEstimation<pcl::FPFHSignature33, pcl::FPFHSignature33> est;
		est.setInputSource(anterior_features);
		est.setInputTarget(cloud_features);

		pcl::Correspondences all_correspondences;
		est.determineReciprocalCorrespondences(all_correspondences);
		//--------------------------------------------------------------------------------------------

		// Eliminamos las malas corespondencias ------------------------------------------------------
		pcl::CorrespondencesConstPtr correspondences_p(new pcl::Correspondences(all_correspondences));

		pcl::registration::CorrespondenceRejectorSampleConsensus<pcl::PointWithScale> ransac;
		ransac.setInputSource(anterior_keypoints);
		ransac.setInputTarget(keypoints);
		ransac.setInlierThreshold(0.1);
		ransac.setMaximumIterations(100000);
		ransac.setRefineModel(true);
		ransac.setInputCorrespondences(correspondences_p); 

		pcl::Correspondences correspondences_out;
		ransac.getCorrespondences(correspondences_out);

		Eigen::Matrix4f transformation = ransac.getBestTransformation();		
		//--------------------------------------------------------------------------------------------


		pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZRGB>);
		pcl::transformPointCloud(*visu_pc, *transformed_cloud, transformation);

		*visu_pc = *transformed_cloud + *cloud_filtered;


	}else{
		visu_pc = cloud_filtered;
	}

	anterior_keypoints = keypoints;
	anterior_features = cloud_features;

	anterior_features = cloud_features;

	primero = false;

	
}

/*
int main(int argc, char** argv)
{
  //init the ROS node
  ros::init(argc, argv, "robot_driver");
  ros::NodeHandle nh;

  RobotDriver driver(nh);
  driver.driveKeyboard();
}*/

int main(int argc, char** argv)
{
	ros::init(argc, argv, "sub_pcl");
	ros::NodeHandle nh;
	cmd_vel_pub_ = nh.advertise<geometry_msgs::Twist>("/mobile_base/commands/velocity", 1);
	ros::Subscriber sub = nh.subscribe<pcl::PointCloud<pcl::PointXYZRGB> >("/camera/depth/points", 1, callback);
	visu_pc = pcl::PointCloud<pcl::PointXYZRGB>::Ptr(new pcl::PointCloud<pcl::PointXYZRGB>);
	boost::thread t(simpleVis);
	
	while(ros::ok()){
		ros::spinOnce();
	}

}