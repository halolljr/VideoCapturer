# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 Qt 和 FFmpeg 的视频捕获和录制应用程序，支持同时捕获屏幕、摄像头和音频，并提供实时美颜等功能。

## 构建和开发命令

### 构建项目
- 使用 Qt Creator 打开 `videoCapture.pro` 文件
- 或使用命令行：`qmake videoCapture.pro && make`
- 需要 Qt 5.12.12 和 MinGW 64-bit 编译器

### 依赖库
- FFmpeg (位于 `3rdparty/ffmpeg-amf/`)
- OpenCV (位于 `3rdparty/opencv/`)  
- Qt Widgets 和 OpenGL 模块

## 核心架构

### 事件驱动架构
- **FFEventLoop**: 主事件循环，处理所有系统事件
- **FFEventQueue**: 事件队列，管理事件的排队和分发
- **FFThreadPool**: 线程池，管理多线程任务执行
- 各种事件类型：FFStartEvent, FFPauseEvent, FFStopEvent 等

### 多媒体处理管道
```
输入源 -> 解复用 -> 解码 -> 过滤/处理 -> 编码 -> 复用 -> 输出
```

#### 输入源类型
- **屏幕捕获** (SCREEN): 桌面录制
- **摄像头** (CAMERA): 摄像头视频流
- **视频文件** (VIDEO): 本地视频文件
- **音频设备** (AUDIO): 声卡音频
- **麦克风** (MICROPHONE): 麦克风音频

#### 关键组件
- **FFDemuxer/FFDemuxerThread**: 解复用器和线程
- **FFADecoder/FFVDecoder**: 音频/视频解码器
- **FFAEncoder/FFVEncoder**: 音频/视频编码器
- **FFAFilter/FFVFilter**: 音频/视频滤镜（美颜等）
- **FFMuxer**: 输出文件复用器

### UI 架构
- **FFCapWindow**: 主窗口，无边框可拖拽设计
- **FFRenderWidget**: 视频渲染组件，支持多路视频叠加显示
- **FFGLRenderWidget**: OpenGL 渲染组件
- 支持实时预览和录制控制

### 捕获上下文管理
**FFCaptureContext** 是核心管理类，包含：
- 3个视频解码线程（屏幕、摄像头、视频文件）
- 2个音频解码线程（音频设备、麦克风）  
- 对应的解复用线程和队列
- 编码线程和过滤线程
- 各种帧队列和包队列

### 时钟同步
- **FFGlobalClock**: 全局时钟，负责音视频同步
- **FFTimer**: 定时器管理

## 文件组织
- `ui/`: 用户界面相关
- `capture/`: 捕获核心逻辑
- `thread/`: 多线程管理
- `queue/`: 各种队列实现
- `decoder/`: 解码器
- `encoder/`: 编码器  
- `filter/`: 滤镜和效果处理
- `demuxer/`: 解复用器
- `muxer/`: 复用器
- `render/`: 渲染相关
- `opencv/`: OpenCV 相关功能（人脸检测、美颜等）
- `event/`: 事件系统
- `clock/`: 时钟同步

## 开发注意事项
- 项目使用 C++14 标准
- 大量使用多线程，注意线程安全
- FFmpeg 使用软件解码/编码（已从硬件切换到软件）
- 队列系统负责线程间数据传递
- 事件系统处理用户交互和状态变化