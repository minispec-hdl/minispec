# -*- mode: ruby -*-
# vi: set ft=ruby :

def total_cpus
  require 'etc'
  Etc.nprocessors
end

Vagrant.configure("2") do |config|
  config.vm.define "minispec"
  config.vm.hostname = "minispec"
  # Includes gcc 8, but it's very slow to boot
  #config.vm.box = "ubuntu/disco64"
  # For now use Bento 18.04 instead, install gcc-8, and make it the default
  config.vm.box = "bento/ubuntu-18.04"
  config.vm.provider "virtualbox" do |vb|
    vb.name = "minispec"
    vb.memory = "4096"
    vb.cpus = total_cpus / 2
    vb.default_nic_type = "virtio"
    vb.customize ["modifyvm", :id, "--paravirtprovider", "kvm"] # faster for linux guests
  end
  # For jupyter notebooks
  config.vm.network "forwarded_port", guest: 8888, host: 8888
  
  # For JupyterHub
  config.vm.network "forwarded_port", guest: 80, host: 8080
  config.vm.network "forwarded_port", guest: 443, host: 8443
 
  config.vm.provision "shell", inline: <<-SHELL
    # Packages
    export DEBIAN_FRONTEND=noninteractive
    apt-get -y update
    # Basics, direct minispec deps (include bsc deps), antlr deps, antlr runtime build
    apt-get -y install vim  scons git build-essential g++ gcc-8 g++-8 libxft2 libgmp  openjdk-8-jdk-headless  cmake pkg-config uuid-dev

    # Make gcc-8 the default by using update-alternatives (see https://askubuntu.com/a/1028656)
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 700 --slave /usr/bin/g++ g++ /usr/bin/g++-7
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

    # Download bsc
    BSVER=Bluespec-2016.07.beta1
    if [ ! -d ~vagrant/${BSVER} ]; then
      echo "Downloading ${BSVER}"
      sudo -u vagrant wget -nc -nv http://www.bluespec.com/downloads/${BSVER}.tar.gz
      sudo -u vagrant tar xzf ${BSVER}.tar.gz
      sudo -u vagrant cp /usr/lib/x86_64-linux-gnu/libgmp.so.10 ${BSVER}/libgmp.so.3
      echo "# Bluespec config" >> ~vagrant/.bashrc
      echo 'export BSPATH=\$HOME/'${BSVER} >> ~vagrant/.bashrc
      echo 'export BLUESPECDIR=\$BSPATH/lib' >> ~vagrant/.bashrc
      echo 'export LD_LIBRARY_PATH=\$BSPATH:\$LD_LIBRARY_PATH' >> ~vagrant/.bashrc
      echo 'export PATH=\$BSPATH/bin:\$PATH' >> ~vagrant/.bashrc
      echo 'export LM_LICENSE_FILE=<fill in license server info>' >> ~vagrant/.bashrc
    fi

    # Yosys
    apt-get -y install build-essential clang bison flex libreadline-dev gawk tcl-dev libffi-dev pkg-config python3 graphviz 
    if [ ! -d ~vagrant/yosys ]; then
      echo "Downloading yosys"
      sudo -u vagrant wget -nc -nv https://github.com/YosysHQ/yosys/archive/yosys-0.8.tar.gz
      sudo -u vagrant tar xzf yosys-0.8.tar.gz
      sudo -u vagrant mv yosys-yosys-0.8 yosys
      sudo -u vagrant bash -c "cd yosys && make -j4"
      echo "# Yosys config" >> ~vagrant/.bashrc
      echo 'export PATH=\$HOME/yosys:\$PATH' >> ~vagrant/.bashrc
    fi

    # netlistsvg (for synth)
    apt-get -y install npm
    npm install -g netlistsvg
    
    # Jupyter notebook (locally, from source, with Minispec syntax)
    apt-get -y install python-pip
    if [ ! -d ~vagrant/notebook-5.7.8 ]; then
      pip install --upgrade setuptools pip
      sudo -H -u vagrant /vagrant/jupyter/install-jupyter.sh
    fi

    # JupyterHub (NOTE: PROVISIONAL; NEEDS TESTING!)
    if [ ! -d /opt/tljh ]; then
      echo "Installing JupyterHub / TLJH"
      # NOTE: SSL requires host iptables rules for port forwarding 443 -> 8443 and 80 -> 8080.
      # For example, in host, run:
      # sudo iptables -A PREROUTING -t nat -p tcp --dport 80 -j REDIRECT --to-ports 8080
      # sudo iptables -A PREROUTING -t nat -p tcp --dport 443 -j REDIRECT --to-ports 8443
      #
      # NOTE: bootstrap.py is unversioned... relying on master being stable, if
      # this breaks, run bootstrap manually from tlhj commit f952e4e
      curl https://raw.githubusercontent.com/jupyterhub/the-littlest-jupyterhub/master/bootstrap/bootstrap.py | python3 - --admin dnl
      # Enable HTTPS
      tljh-config set https.enabled true
      tljh-config set https.letsencrypt.email daniel+lejh@dsanchez.es
      tljh-config add-item https.letsencrypt.domains brix.csail.mit.edu
      tljh-config reload proxy
      # Copy configs over (includes env vars + OIDC auth)
      cp /vagrant/jupyter/jupyterhub_config.d/* /opt/tljh/config/jupyterhub_config.d/
      tljh-config reload hub

      # Install modified notebook (TODO: within jupyterhub... check this works right)
      bash -c "source /opt/tljh/user/bin/activate && cd ~vagrant/notebook-5.7.8 && python -m pip install ."
      bash -c "source /opt/tljh/user/bin/activate && jupyter kernelspec install --sys-prefix /vagrant/jupyter/kernel/minispec"
      # For some reason jupyterhub doesn't catch the installed minispeckernel.py...
      ln -s /usr/local/lib/python2.7/site-packages/minispeckernel.py /usr/local/lib/python2.7/

      # NOTE: Set secrets in jupyter_config.d/auth.py
    fi

    # Finally, build minispec itself (note: in shared folder... might want to
    # move repo somewhere to make VM fully self-contained)
    if [ ! -d /vagrant/msc ]; then
      sudo -H -u vagrant bash -c "cd /vagrant && scons -j4"
      # vboxsf suffers from horrible racy behavior, so sometimes the antlr4
      # download & install code fails because the untarred antlr4 source takes
      # a bit to become available (yep, it's that inconsistent). So try twice
      sleep 1
      sudo -H -u vagrant bash -c "cd /vagrant && scons -j4"
      echo 'export PATH=/vagrant:/vagrant/synth:\$PATH' >> ~vagrant/.bashrc
    fi

  SHELL
end
