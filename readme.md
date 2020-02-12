# Socket QQ

**一个使用 Winsock 实现的 C++ 多线程聊天室。**

- 支持多窗口显示聊天
- 支持私信、群消息
- 支持未读消息

*项目依赖*：

- `pthread-win32`
- `MariaDB C++ Connector`

## 构建要求、运行环境

需要 VS2013。

构建完成后请将以下两个动态链接库依赖，复制到存到 `Server.exe` 和 `Client.exe` 的目录中：

- `common/pthread_w32/dll/x86/pthreadVCE2.dll`
- `common/mariadb_connector/lib/libmariadb.dll`

> `Server.exe` 依赖于 `pthreadVCE2.dll` 和 `libmariadb.dll`，`Client.exe` 依赖于 `pthreadVCE2.dll`。

## 服务器环境

数据库使用 MySQL（推荐 MariaDB）

1. 导入数据库文件 `socketqq-db.sql`
2. 修改 `Server/ServerData.hpp` L25，指定连接 MySQL 的地址、用户、密码、数据库名等。
