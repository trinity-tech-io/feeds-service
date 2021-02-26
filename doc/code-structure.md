# Feeds Service 代码结构说明

```
   src/
   ├── CMakeLists.txt
   ├── auth.c					// 旧版本登陆rpc
   ├── auth.h
   ├── cfg.c					// feedsd 启动配置
   ├── cfg.h
   ├── cmdhandler				// C++ RPC分发和处理目录
   │   ├── CMakeLists.txt
   │   ├── ChannelMethod.cpp		// Channel相关新rpc
   │   ├── ChannelMethod.hpp
   │   ├── CommandHandler.cpp		// rpc分发处理
   │   ├── CommandHandler.hpp
   │   ├── LegacyMethod.cpp		// c版本rpc分发处理
   │   ├── LegacyMethod.hpp
   │   ├── MassData.cpp			// 块数据相关rpc
   │   ├── MassData.hpp
   │   ├── StandardAuth.cpp		// 新版本登陆rpc
   │   └── StandardAuth.hpp
   ├── db.c					// db封装
   ├── db.h
   ├── did.c					// did封装
   ├── did.h
   ├── err.c
   ├── err.h
   ├── feeds.c				// c版本rpc
   ├── feeds.h
   ├── feedsd-ext				// 对C版本的RPC和DB的C++封装
   │   ├── CMakeLists.txt
   │   ├── DataBase.cpp			// C++版本DB封装
   │   ├── DataBase.hpp
   │   ├── MsgPackExtension.hpp		// MsgPack C++扩展
   │   ├── RpcDeclare.hpp			// C++ MsgPack rpc定义
   │   ├── RpcFactory.cpp			// C++ MsgPack rpc打包/解包
   │   └── RpcFactory.hpp
   ├── feedsd.conf.in
   ├── main.cpp
   ├── massdata				// 块数据处理目录
   │   ├── CMakeLists.txt
   │   ├── CarrierSessionHelper.cpp	// Carrier Session 封装
   │   ├── CarrierSessionHelper.hpp
   │   ├── MassDataManager.cpp		// 块数据管理，收取，处理，返回结果
   │   ├── MassDataManager.hpp
   │   ├── MassDataProcessor.cpp		// 块数据处理
   │   ├── MassDataProcessor.hpp
   │   ├── SessionParser.cpp		// session协议解析
   │   └── SessionParser.hpp
   ├── msgq.cpp				// 消息发送
   ├── msgq.h
   ├── obj.h
   ├── rpc.c					// 旧版本（c）的rpc打包/解包
   ├── rpc.h
   ├── utils					// C++工具箱
   │   ├── CMakeLists.txt
   │   ├── DateTime.hpp
   │   ├── ErrCode.cpp
   │   ├── ErrCode.hpp
   │   ├── Log.cpp
   │   ├── Log.hpp
   │   ├── Random.hpp
   │   ├── SafePtr.hpp
   │   ├── Semaphore.hpp
   │   ├── StdFileSystem.hpp
   │   ├── ThreadPool.cpp
   │   ├── ThreadPool.hpp
   │   └── platform
   │       ├── CMakeLists.txt
   │       ├── Platform.hpp
   │       ├── PlatformDarwin.cpp
   │       ├── PlatformDarwin.hpp
   │       ├── PlatformUnixLike.cpp
   │       └── PlatformUnixLike.hpp
   └── ver.h.in
```
