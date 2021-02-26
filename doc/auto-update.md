# Feeds Service 升级(只支持Ubuntu/Raspbian)

## 1. 代码分支

​		[feat_managedupgrade](https://github.com/elastos-trinity/feeds-service/tree/feat_managedupgrade)

## 2. 安装包目录结构变化

- 升级前：
```
/
├── bin
│   └── feedsd
├── etc
│   ├── feedsd
│   │   └── feedsd.conf
│   └── init.d
│       └── feedsd
└── lib
    ├── libcrystal.so
    ├── libelacarrier.so
    ├── libeladid.so -> libeladid.so.1
    ├── libeladid.so.1 -> libeladid.so.1.0.0
    ├── libeladid.so.1.0.0
    ├── libelasession.so
    └── systemd
        └── system
            └── feedsd.service
```

- 升级后：
```
/
├── bin
│   └── feedsd
├── etc
│   └── init.d
│       └── feedsd
├── lib
│   └── systemd
│       └── system
│           └── feedsd.service
└── var
    └── lib
        └── feedsd
            └── runtime
                ├── 1.x.0
                │   ├── bin
                │   │   ├── feedsd
                │   │   └── feedsd.updatehelper
                │   ├── etc
                │   │   └── feedsd
                │   │       └── feedsd.conf
                │   └── lib
                │       ├── libcrystal.so
                │       ├── libelacarrier.so
                │       ├── libeladid.so -> libeladid.so.1
                │       ├── libeladid.so.1 -> libeladid.so.1.0.0
                │       ├── libeladid.so.1.0.0
                │       └── libelasession.so
                └── current -> 1.x.0
```

## 3. 测试步骤
### 1. Service包准备
- old: 如1.4.0:
  
-  从https://github.com/elastos-trinity/feeds-service/releases/tag/release-v1.4.0下载
  
- new: 如2.0.0:

  ```
  1. git clone https://github.com/elastos-trinity/feeds-service.git
  2. cd feeds-service
  3. CMakeLists.txt
  		根据版本，修改 project(feedsd VERSION x.x.x)
  		改为 project(feedsd VERSION 2.0.0)
  4. src/cmdhandler/ServiceMethod.cpp
  		修改	#undef FEAT_AUTOUPDATE
  		改为  #define FEAT_AUTOUPDATE
  5. mkdir -p build/linux/ && cd build/linux/
  6. cmake ../../
  7. make package
  ```



## 2. 正常测试流程

- 1.4.0 升级到 2.0.0，需要手动操作升级

  ```
  1. dpkg -i feedsd_1.4.0_xxx.deb(如果1.4.0已安装则忽略)
  2. dpkg -i feedsd_2.0.0_xxx.deb
  3. cat /var/lib/feedsd/runtime/current/feedsd.log观察log。
  ```

- 2.0.0 升级到 2.x.0，支持自动升级

  ```
  1. 将feedsd_2.x.0_xxx.deb放到http服务器，如: 		
  			http://192.168.1.111/download/feedsd_2.x.0_xxx.deb
  2. 下载测试工具: git clone https://github.com/klx99/CarrierDemo.git 
  3. 使用AndroidStudio打开CarrierDemo
  4. 修改RPC.java 的类定义 DownloadNewServiceRequest {
  			String new_version = "2.x.0"
  			String base_url = "http://192.168.1.111/download"
  			Tarball ubuntu_1804/ubuntu_2004/raspbian(根据测试server的版本修改) {
  				name = "feedsd_2.x.0_xxx.deb"
  				size = 16467278 (feedsd_1.6.0_xxx.deb的size)
  				md5 = "5e6d60d3c510122cd576579d7ece168e" (feedsd_1.6.0_xxx.deb的md5)
  			}
  		}
  5. 在Android设备上运行CarrierDemo
  		1)添加Service为Carrier好友并连接后
  		2)点击菜单项[Download New Service]
  		3)点击菜单项[Start New Service]
  		4)观察FeedsServer的版本号变化情况
  ```



## 3. 异常测试流程

​	略

