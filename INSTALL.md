# How to install/upgrade/uninstall released package in terminal?

Currently supports Mac and Linux debian distributions.

## 1 Mac

### 1.1 Install Feeds Service

- 1.1.1 Download service installation package Feeds.Service.app.macos.tar.gz from [the latest version](https://github.com/elastos-trinity/feeds-service/releases/).

- 1.1.2 Open terminal and change work directory to package directory.

- 1.1.3 Unzip the installation package in the  terminal:

		tar xf Feeds.Service.app.macos.tar.gz
		
- 1.1.4 Run the command below in the terminal and then input your password:

		sudo spctl --master disable
		
- 1.1.5 Open Feeds Service in the terminal:

		open "Feeds Service.app"
		
- 1.1.6 If The Feeds Service reports the error ["Feeds Service" cannot be opened because the developer cannot be verified.], you can open [System Preferences]->[Security & Privacy]->[General], click [Open Anyway], and then continue to click [Open] in the pop-up confirmation interface to open the Feeds Service. Finally, click [Allow] on the pop-up request link.

### 1.2 Upgrade Feeds Service

- Same as 1.1.

### 1.3 Uninstall Feeds Service

- Remove the [Feeds Service.app] directly.


## 2 Ubuntu(amd64 debian/ubuntu)

### 2.1 Install Feeds Service

- 2.1.1 Download the service installation package such as feedsd_1.3.0_amd64_ubuntu_2004.deb from [the latest version](https://github.com/elastos-trinity/feeds-service/releases/).

- 2.1.2 Open terminal and change work directory to package directory.

- 2.1.3 Run the install command below in the terminal and then input your password:

		sudo dpkg -i feedsd_1.3.0_amd64_ubuntu_2004.deb

- 2.1.4 Input "Y" and [Enter] on the accept prompt.

- 2.1.5 Run the service start command in the terminal and then input your password:

		sudo service feedsd start
		
### 2.2 Upgrade Feeds Service

- Same as 2.1.

### 2.3 Uninstall Feeds Service
	
- Open the terminal and run:
	
		sudo dpkg -P feedsd

## 3 Ubuntu(arm debian/ubuntu)

### 3.1 Install Feeds Service

- 3.1.1 Download the service installation package such as feedsd_1.3.0_armhf_raspbian.deb from [the latest version](https://github.com/elastos-trinity/feeds-service/releases/).

- 3.1.2-3.1.4 This process is similar to 2.1.2-2.1.4 (x86_64 debian/ubuntu) and will not be repeated here.

		
### 3.2 Upgrade Feeds Service

- Same as 3.1.

### 3.3 Uninstall Feeds Service
	
- Same as 2.3.
