An updated version of SVin2(https://github.com/AutonomousFieldRoboticsLab/SVIn) with opengv lib error fixed and crash issue fixed, and easier docker installation.

### Docker Installation ###
* Docker Engine Installation(if you haven't install Docker Engine)
```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
 ```
* Clone
```bash
mkdir [your_catkin_worksapce]/src -p
cd [your_catkin_worksapce]/src
git clone https://github.com/DaDa0o0/SVin2.git
```
**Replace [your_catkin_worksapce] with the absolute path of your catkin workspace. (Example: /home/user/catkin_ws)**
* Build Docker Image

```bash
cd [your_catkin_worksapce]/src/SVin2/docker
docker build -t svin_run -f ./run_svin.Dockerfile --network host .
``` 

* Run Dcoker Container

```bash
docker run -it --name svin_test -v [your_catkin_worksapce]/src:/home/svin_ws/src --rm --net host svin_run
```
* Visalization

Rviz can run out of the docker for visualization.

```bash
rviz -d [your_catkin_worksapce]/src/SVin2/okvis_ros/config/rviz_svin.rviz
```

* Rosbag

datasets: https://afrl.cse.sc.edu/afrl/resources/datasets/

If you follow "Datasets for Visual-Inertial-Based State Estimation Algorithms" link you will be directed to a google drive directory, under the 'Bus' and 'Cave' you will find ROS bagfile. 

```bash
rosbag play [path_to_your_bag]/cave_loop_w_cam_info.bag --clock -r 0.8
```