# HFT Socket Server

A high-performance, ultra-low latency socket server designed for High-Frequency Trading (HFT) applications, built with C++20 and optimized for sub-20μs latency.

## 特性 / Features

- **超低延迟 / Ultra-Low Latency**: 目标延迟 < 20μs
- **单例架构 / Singleton Architecture**: 确保全局唯一实例
- **服务模式 / Service Pattern**: 模块化消息处理服务
- **高性能网络 / High-Performance Networking**: 基于epoll的事件驱动架构
- **多线程处理 / Multi-threaded Processing**: 可配置的工作线程池
- **零拷贝优化 / Zero-Copy Optimization**: 预分配缓冲区减少内存分配

## 架构 / Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Client Apps   │    │   HFT Server    │    │   Services      │
│                 │◄──►│                 │◄──►│                 │
│ - Order Entry   │    │ - Singleton     │    │ - OrderService  │
│ - Market Data   │    │ - Epoll-based   │    │ - MarketData    │
│ - Analytics     │    │ - Multi-thread  │    │ - AuthService   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 构建要求 / Build Requirements

- **编译器 / Compiler**: GCC 10+ 或 Clang 12+
- **C++标准 / C++ Standard**: C++20
- **操作系统 / OS**: Linux (推荐内核 5.4+)
- **依赖 / Dependencies**: pthread

## 构建步骤 / Build Instructions

### 1. 克隆仓库 / Clone Repository
```bash
git clone <repository-url>
cd hft_cpp
```

### 2. 创建构建目录 / Create Build Directory
```bash
mkdir build && cd build
```

### 3. 配置CMake / Configure CMake
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 4. 编译 / Build
```bash
make -j$(nproc)
```

### 5. 安装 (可选) / Install (Optional)
```bash
sudo make install
```

## 运行 / Running

### 基本用法 / Basic Usage
```bash
# 使用默认配置启动服务器
./bin/hft_server

# 指定IP和端口
./bin/hft_server --ip 0.0.0.0 --port 9999

# 指定工作线程数
./bin/hft_server --threads 8

# 显示帮助信息
./bin/hft_server --help
```

### 性能测试 / Performance Testing
```bash
# 构建性能测试目标
make perf_test

# 运行性能测试
./bin/hft_server --threads 8 --port 8888
```

## 配置选项 / Configuration Options

| 选项 / Option | 默认值 / Default | 描述 / Description |
|---------------|------------------|-------------------|
| `--ip` | 127.0.0.1 | 服务器IP地址 / Server IP address |
| `--port` | 8888 | 服务器端口 / Server port |
| `--threads` | 4 | 工作线程数 / Number of worker threads |
| `--help` | - | 显示帮助信息 / Show help message |

## 性能优化 / Performance Optimizations

### 编译器优化 / Compiler Optimizations
- `-O3`: 最高级别优化
- `-march=native`: 针对本地CPU架构优化
- `-flto`: 链接时优化
- `-fno-exceptions`: 禁用异常处理
- `-fno-rtti`: 禁用RTTI

### 网络优化 / Network Optimizations
- TCP_NODELAY: 禁用Nagle算法
- SO_REUSEADDR: 地址重用
- 非阻塞I/O: 异步处理
- epoll边缘触发: 高效事件通知

### 内存优化 / Memory Optimizations
- 预分配缓冲区: 避免运行时分配
- 零拷贝操作: 减少内存拷贝
- 固定大小结构: 缓存友好的内存布局

## 消息类型 / Message Types

### 订单消息 / Order Messages
- `ORDER_NEW`: 新订单
- `ORDER_CANCEL`: 取消订单
- `ORDER_REPLACE`: 替换订单
- `ORDER_FILL`: 订单成交

### 市场数据 / Market Data
- `MARKET_DATA`: 市场数据更新
- `HEARTBEAT`: 心跳检测

### 系统消息 / System Messages
- `LOGIN`: 登录认证
- `LOGOUT`: 登出
- `ERROR`: 错误信息

## 服务架构 / Service Architecture

### IMessageService 接口
```cpp
class IMessageService {
public:
    virtual void process_message(const Message& msg, Connection& conn) = 0;
    virtual void on_connection_established(Connection& conn) = 0;
    virtual void on_connection_closed(Connection& conn) = 0;
};
```

### 内置服务 / Built-in Services
- **OrderService**: 订单管理服务
- **MarketDataService**: 市场数据服务

### 自定义服务 / Custom Services
```cpp
class CustomService : public IMessageService {
public:
    void process_message(const Message& msg, Connection& conn) override {
        // 实现自定义消息处理逻辑
    }
    
    void on_connection_established(Connection& conn) override {
        // 连接建立时的处理
    }
    
    void on_connection_closed(Connection& conn) override {
        // 连接关闭时的处理
    }
};

// 注册服务
server.register_service(MessageType::CUSTOM, std::make_shared<CustomService>());
```

## 性能监控 / Performance Monitoring

服务器提供实时性能统计信息：

```
=== Server Statistics ===
Total Messages: 1,234,567
Active Connections: 45
Peak Connections: 100
Average Latency: 15.23 μs
✓ Latency target met (< 20μs)
========================
```

## 故障排除 / Troubleshooting

### 常见问题 / Common Issues

1. **编译错误 / Compilation Errors**
   - 确保使用C++20兼容的编译器
   - 检查系统依赖是否完整

2. **性能问题 / Performance Issues**
   - 调整工作线程数
   - 检查系统网络配置
   - 使用性能分析工具

3. **连接问题 / Connection Issues**
   - 检查防火墙设置
   - 验证端口可用性
   - 检查网络配置

### 调试模式 / Debug Mode
```bash
# 使用Debug构建
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## 贡献 / Contributing

欢迎提交Issue和Pull Request来改进项目。

## 许可证 / License

本项目采用MIT许可证。

## 联系方式 / Contact

如有问题或建议，请通过以下方式联系：
- 提交GitHub Issue
- 发送邮件至项目维护者

---

**注意**: 此服务器专为HFT应用设计，在生产环境中使用前请充分测试。 