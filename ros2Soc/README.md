# ros2Soc



 ##  Contents

- Why Ros2?
- ROS2 cheat sheet
- Run
- Setup  
  
## Why Ros2?

## Ros2 Cheat Sheet
- ros2 topic list
- ros2 topic echo <topic_name>
- rqt_graph
- colcon build (from work space level: ros2Soc folder)
- ros2 pkg create --build-type ament_python chat_client_py (ament_cmake)

- Publish
	Loop -
	ros2 topic pub /chat message_definitions/msg/ChatMessage "{number: 1, timestamp: {sec: 0, nanosec: 0}, message: 'Hello from CLI'}"
	Once -
	ros2 topic pub /chat message_definitions/msg/ChatMessage "{number: 1, timestamp: {sec: 0, nanosec: 0}, message: 'Hello from CLI'}" -1



## Run

- run alias for workspace: (Do this for every new terminal / build)
source_ros2Soc

- run client
ros2 run chat_client_py chat

- (do this twice for two clients)

- look at message traffic with:
	ros2 topic echo chat 

- look at graph
	rqt_graph

## Setup

  

### Linux ()

- Install Ros 2 Iron

	https://docs.ros.org/en/iron/Installation/Ubuntu-Install-Debians.html

  

	````
	locale # check for UTF-8
	sudo apt update && sudo apt install locales
	sudo locale-gen en_US en_US.UTF-8
	sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
	export LANG=en_US.UTF-8
	locale # verify settings
	sudo apt install software-properties-common
	sudo add-apt-repository universe
	sudo apt update && sudo apt install curl -y
	sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
	echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
	sudo apt update && sudo apt install ros-dev-tools
	sudo apt update
	sudo apt upgrade
	sudo apt install ros-iron-desktop
	````

- git clone this repo 
(example location: ~/ls/OmniSoc/

- Source Ros2 and ros2Soc
	- open .bashrc
	````code .bashrc````
	- add alias to ros2Soc, and source ros2 by adding these two lines to .bashrc:
	
		alias  source_ros2Soc='source ~/ls/OmniSoc/ros2Soc/install/setup.bash'
		source  /opt/ros/iron/setup.bash

	- update current terminal with new .bashrc (all new terminals will automatically update)
	````source ~/.bashrc````

	- build ros2Soc.  navigate to ros2Soc and run 'colcon build --symlink-install'


source  /opt/ros/iron/setup.bash
  

### Windows TBD

  

### Raspi TBD

  

### Arduino TBD

(will be micro ros)

https://www.youtube.com/watch?v=xbWaHARjSmk

  

### Andriod ???

- might be better just to have a ros bridge

  

### Iphone ???

- might be better just to have a ros bridge