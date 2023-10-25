FROM osrf/ros:noetic-desktop-full

# RUN 
RUN apt update && apt install vim -y
SHELL ["/bin/bash", "-c"]

# setup clion remote development
RUN apt-get update \
  && apt-get install -y ssh \
      build-essential \
      gcc \
      g++ \
      gdb \
      clang \
      make \
      ninja-build \
      cmake \
      autoconf \
      automake \
      locales-all \
      dos2unix \
      rsync \
      tar \
      python \
  && apt-get clean

RUN useradd -m da \
  && yes 123 | passwd da

RUN usermod -s /bin/bash da

# install ceres Dependencies

# CMake
RUN apt-get install cmake -y
# google-glog + gflags
RUN apt-get install libgoogle-glog-dev libgflags-dev -y
# BLAS & LAPACK
RUN apt-get install libatlas-base-dev -y
# SuiteSparse and CXSparse (optional)
RUN apt-get install libsuitesparse-dev -y
# opencv boost eigen3
RUN sudo apt install libopencv-dev libboost-dev libboost-filesystem-dev libeigen3-dev -y
# ros dependencies
RUN sudo apt install ros-noetic-pcl-ros ros-noetic-tf2-sensor-msgs -y
# Clang ninja git
RUN sudo apt install ninja-build clang-10 lldb-10 git -y

# eigen 3.3.4
#WORKDIR /home
#RUN git clone https://gitlab.com/libeigen/eigen.git
#WORKDIR /home/eigen
#RUN git checkout 3.3.4
#RUN mkdir build
#WORKDIR ./build/
#RUN cmake ..
#RUN make install

#install ceres
WORKDIR /home
RUN git clone https://ceres-solver.googlesource.com/ceres-solver
WORKDIR /home/ceres-solver/
RUN git checkout 1.14.0
RUN mkdir build
WORKDIR ./build/
RUN cmake .. -GNinja
  # -DCMAKE_CXX_COMPILER=/usr/bin/clang++-10\
  # -DCMAKE_C_COMPILER=/usr/bin/clang-10
RUN ninja && ninja install

#install brisk
RUN apt-get install wget unzip -y
WORKDIR /home
RUN  wget https://www.doc.ic.ac.uk/~sleutene/software/brisk-2.0.8.zip
RUN  unzip brisk-2.0.8.zip
RUN  cd brisk
RUN  mkdir build
WORKDIR /home/brisk/build/
RUN cmake .. -GNinja\
  -DINSTALL_CMAKE_DIR=/usr/local/lib/cmake/brisk
  # -DCMAKE_CXX_COMPILER=/usr/bin/clang++-10\
  # -DCMAKE_C_COMPILER=/usr/bin/clang-10
RUN ninja && ninja install

#install opengv
WORKDIR /home
RUN  git clone https://github.com/laurentkneip/opengv
WORKDIR /home/opengv
RUN  git checkout cc32b16281aa6eab67cb28a61cf87a2a5c2b0961
RUN mkdir build 
WORKDIR /home/opengv/build/
RUN cmake .. -GNinja
  # -DINSTALL_CMAKE_DIR=/usr/local/lib/cmake/opengv\
  # -DCMAKE_CXX_COMPILER=/usr/bin/clang++-10\
  # -DCMAKE_C_COMPILER=/usr/bin/clang-10
RUN ninja && ninja install

#install SVIN
RUN sudo apt install libomp-dev
# WORKDIR /home
RUN mkdir -p /home/svin_ws/src
 WORKDIR /home/svin_ws/src
 RUN git clone --branch 0.2 https://github.com/DaDa0o0/SVin2.git
 RUN git clone https://github.com/AutonomousFieldRoboticsLab/imagenex831l.git
 RUN git clone --branch ros-noetic https://github.com/AutonomousFieldRoboticsLab/sonar_rviz_plugin.git
#COPY ./src /home/svin_ws/src
WORKDIR /home/svin_ws
RUN source /opt/ros/noetic/setup.bash && catkin_make



# RUN echo "source /opt/ros/noetic/setup.bash" >> /home/da/.bashrc
# RUN ( \
#     echo 'LogLevel DEBUG2'; \
#     echo 'PermitRootLogin yes'; \
#     echo 'PasswordAuthentication yes'; \
#     echo 'Subsystem sftp /usr/lib/openssh/sftp-server'; \
#   ) > /etc/ssh/sshd_config_test_clion \
#   && mkdir /run/sshd
# CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_test_clion"]

#CMD [ "bash" ]

COPY ./docker/run_svin.sh /run_svin.sh
ENTRYPOINT [ "bash","/run_svin.sh" ]
# ENTRYPOINT [ "bash" ]
# CMD [  ]




