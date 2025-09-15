// app.ts

/**
 * 定义App的全局类型，包括globalData和自定义方法
 */
interface IAppOption {
  globalData: {
    isRunning: boolean
    currentSpeed: number
    speedText: string
    sortingCounts: {
      position0: number
      position1: number
      position2: number
    }
    servoValues: number[]
    servoPwmInfo: {
      id: number
      name: string
      pwm: number
    }[]
    debugMessages: any[]
    udpConnected: boolean
    userInfo?: WechatMiniprogram.UserInfo
    udpSocket: WechatMiniprogram.UDPSocket | null
    udpManager: UDPManager | null
    heartbeatFailCount: number
    lastPageName: string
    reconnectAttempts: number
    maxReconnectAttempts: number
    isConnecting: boolean
    lastCommandResponse: string
    lastCommandTimestamp: number
  }
  updateCallbacks?: Map<any, any>
  notifyPagesUpdate: (type: string, data: any) => void
  getSpeedText: (speed: number) => string
  initUDPSocket: () => void
  connectToCommModule: () => Promise<boolean>
  disconnectCommModule: () => void
  handleUDPMessage: (result: any) => void
  tryRestoreConnection: () => void
  attemptReconnect: () => void
}

import { getSpeedText } from './utils/util'
import UDPManager from './utils/udp-manager'
import { UDP_CONFIG } from './config/index'

App<IAppOption>({
  globalData: {
    // 设备运行状态
    isRunning: false,
    currentSpeed: 0, // 0-停止, 1-慢速, 2-中速, 3-高速
    speedText: '停止',
    
    // 分拣计数数据
    sortingCounts: {
      position0: 0,
      position1: 0,
      position2: 0
    },
    
    // 舵机调试数据
    servoValues: [50, 50, 50, 50], // 四个舵机的滑块值，范围0-100
    
    // 控制设备PWM信息
    servoPwmInfo: [
      { id: 0, name: '阻拦器', pwm: 1500 },
      { id: 1, name: '弹出器1', pwm: 1500 },
      { id: 2, name: '弹出器2', pwm: 1500 }
    ],
    
    // 调试信息
    debugMessages: [],

    // UDP连接状态
    udpConnected: false,
    udpSocket: null,
    udpManager: null,
    heartbeatFailCount: 0,
    lastPageName: '',
    reconnectAttempts: 0,
    maxReconnectAttempts: 3,
    isConnecting: false,
    // 添加最后一次命令响应存储
    lastCommandResponse: '',
    lastCommandTimestamp: 0
  },
  
  // 页面更新通知队列
  updateCallbacks: new Map(),

  // 扫描数据去重机制
  lastScanData: {
    id: '',
    timestamp: 0,
    processed: false
  },

  // 计数增加冷却机制 - 每个位置增加后3秒内不允许再次增加
  countingCooldown: {
    position0: 0,
    position1: 0,
    position2: 0
  },

  onLaunch() {
    console.log('智慧物流控制系统启动');
    console.log('通信模式：UDP直连通信机');
    
    // 初始化UDP管理器
    if (!this.globalData.udpManager) {
      this.globalData.udpManager = new UDPManager(this);
    }
    
    // 初始化UDP Socket
    this.initUDPSocket();
    
    // 检查设备信息
    const systemInfo = wx.getSystemInfoSync();
    console.log('设备信息:', systemInfo);
    
    // 尝试恢复上次的连接
    this.tryRestoreConnection();
  },

  /**
   * 检查扫描数据是否重复
   * @param id 物品ID
   * @returns true表示是重复数据，false表示是新数据
   */
  isDuplicateScanData(id: string): boolean {
    const now = Date.now();
    const DUPLICATE_THRESHOLD = 1000; // 1秒内的相同ID视为重复（缩短时间阈值）

    console.log(`去重检查: 当前ID=${id}, 上次ID=${this.lastScanData.id}, 时间间隔=${now - this.lastScanData.timestamp}ms`);

    // 如果是相同的ID且在时间阈值内，视为重复
    if (this.lastScanData.id === id &&
        (now - this.lastScanData.timestamp) < DUPLICATE_THRESHOLD) {
      console.log(`🚫 检测到重复扫描数据: ID=${id}, 时间间隔=${now - this.lastScanData.timestamp}ms`);
      return true;
    }

    // 更新最后扫描数据记录
    console.log(`✅ 记录新的扫描数据: ID=${id}, 时间戳=${now}`);
    this.lastScanData = {
      id: id,
      timestamp: now,
      processed: true
    };

    return false;
  },

  /**
   * 检查计数位置是否在冷却期内
   * @param position 位置名称 ('position0', 'position1', 'position2')
   * @returns true表示在冷却期内，false表示可以增加计数
   */
  isCountingInCooldown(position: 'position0' | 'position1' | 'position2'): boolean {
    const now = Date.now();
    const COOLDOWN_PERIOD = 3000; // 3秒冷却期

    const lastIncrementTime = this.countingCooldown[position];
    const timeSinceLastIncrement = now - lastIncrementTime;

    if (timeSinceLastIncrement < COOLDOWN_PERIOD) {
      console.log(`🚫 ${position} 在冷却期内，距离上次增加 ${timeSinceLastIncrement}ms，需要等待 ${COOLDOWN_PERIOD - timeSinceLastIncrement}ms`);
      return true;
    }

    return false;
  },

  /**
   * 更新计数冷却时间
   * @param position 位置名称
   */
  updateCountingCooldown(position: 'position0' | 'position1' | 'position2'): void {
    const now = Date.now();
    this.countingCooldown[position] = now;
    console.log(`✅ 更新 ${position} 冷却时间: ${now}`);
  },

  /**
   * 初始化UDP Socket
   */
  initUDPSocket() {
    try {
      // 如果已存在，先关闭
      if (this.globalData.udpSocket) {
        try {
          this.globalData.udpSocket.close();
          this.globalData.udpSocket = null;
        } catch (e) {
          console.warn('关闭已有UDP Socket失败:', e);
        }
      }
      
      // 创建新的Socket
      const udpSocket = wx.createUDPSocket();
      this.globalData.udpSocket = udpSocket;
      
      // 绑定本地端口（随机端口）
      try {
        const localPort = udpSocket.bind();
        console.log('UDP Socket绑定到本地端口:', localPort);
      } catch (bindErr) {
        console.error('UDP Socket绑定失败:', bindErr);
        // 即使绑定失败也继续使用，某些环境下可能不需要明确绑定
      }
      
      // 监听UDP消息
      udpSocket.onMessage((result) => {
        this.handleUDPMessage(result);
      });
      
      // 添加错误处理
      udpSocket.onError((error) => {
        console.error('UDP Socket错误:', error);
        
        // 尝试重新创建
        setTimeout(() => {
          this.initUDPSocket();
        }, 1000);
      });
      
      console.log('UDP Socket已初始化');
    } catch (error) {
      console.error('UDP Socket初始化失败:', error);
      wx.showToast({
        title: 'UDP初始化失败',
        icon: 'error'
      });
    }
  },
  
  /**
   * 处理UDP消息
   */
  handleUDPMessage(result: any) {
    try {
      const message = String.fromCharCode(...new Uint8Array(result.message));
      console.log('收到UDP消息:', message, '来自:', result.remoteInfo);

      // 保存最后一次服务器响应
      this.globalData.lastCommandResponse = message;
      this.globalData.lastCommandTimestamp = Date.now();

      // 记录错误响应
      if (message.startsWith('ERROR:')) {
        console.error('服务器返回错误:', message);
        return; // 错误消息不需要进一步处理
      }
      // 记录成功响应
      else if (message.startsWith('SUCCESS:')) {
        console.log('服务器操作成功:', message);
        return; // 成功消息不需要进一步处理
      }
      // 处理分拣信息消息 (格式: sort_info:id=XX,dir=Y) - 这是主要的扫描数据处理逻辑
      else if (message.startsWith('sort_info:id=')) {
        console.log('收到分拣信息:', message);

        // 解析ID和方向
        const parts = message.replace('sort_info:id=', '').split(',');
        const id = parts[0] ? parts[0].replace(/\D/g, '') : '';
        const dirPart = parts[1] ? parts[1].replace('dir=', '').replace(/[^A-Z]/g, '') : '';

        console.log('解析出的物品ID:', id, '方向:', dirPart);

        // 检查是否为重复扫描数据
        if (this.isDuplicateScanData(id)) {
          console.log('忽略重复的分拣信息:', message);
          return; // 忽略重复数据
        }

        // 记录处理前的计数状态
        const beforeCounts = {
          position0: this.globalData.sortingCounts.position0,
          position1: this.globalData.sortingCounts.position1,
          position2: this.globalData.sortingCounts.position2
        };
        console.log('处理前的分拣计数:', beforeCounts);

        // 根据物品ID更新对应的分拣计数
        // 硬件ID映射：00/0 → 物品0, 01/1 → 物品1, 02/2 → 物品2
        if (id === '00' || id === '0') {
          if (this.isCountingInCooldown('position0')) {
            console.log(`🚫 position0 在冷却期内，忽略此次计数增加`);
            return; // 在冷却期内，忽略此次增加
          }
          this.globalData.sortingCounts.position0++;
          this.updateCountingCooldown('position0');
          console.log(`🎯 物品ID=${id} 匹配到position0，计数从 ${beforeCounts.position0} 增加到 ${this.globalData.sortingCounts.position0}`);
        } else if (id === '01' || id === '1') {
          if (this.isCountingInCooldown('position1')) {
            console.log(`🚫 position1 在冷却期内，忽略此次计数增加`);
            return; // 在冷却期内，忽略此次增加
          }
          this.globalData.sortingCounts.position1++;
          this.updateCountingCooldown('position1');
          console.log(`🎯 物品ID=${id} 匹配到position1，计数从 ${beforeCounts.position1} 增加到 ${this.globalData.sortingCounts.position1}`);
        } else if (id === '02' || id === '2') {
          if (this.isCountingInCooldown('position2')) {
            console.log(`🚫 position2 在冷却期内，忽略此次计数增加`);
            return; // 在冷却期内，忽略此次增加
          }
          this.globalData.sortingCounts.position2++;
          this.updateCountingCooldown('position2');
          console.log(`🎯 物品ID=${id} 匹配到position2，计数从 ${beforeCounts.position2} 增加到 ${this.globalData.sortingCounts.position2}`);
        } else {
          console.log(`⚠️ 未识别的物品ID: ${id}`);
        }

        console.log('更新分拣计数:', this.globalData.sortingCounts);
        console.log('分拣计数详情 - 物品0:', this.globalData.sortingCounts.position0, '物品1:', this.globalData.sortingCounts.position1, '物品2:', this.globalData.sortingCounts.position2);

        // 收到分拣信息说明传送带正在运行
        this.globalData.isRunning = true;
        this.globalData.speedText = this.getSpeedText(this.globalData.currentSpeed);

        // 通知所有页面更新分拣计数和设备状态
        this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        this.notifyPagesUpdate('device_status', {
          isRunning: this.globalData.isRunning,
          speedText: this.globalData.speedText
        });
        return; // 处理完成，避免重复处理
      }
      // 处理通信机返回的原始分拣数据（心跳响应或刷新响应）
      else if (message.includes('_refresh') && message.length > 8) {
        console.log('收到心跳响应（分拣数据）:', message);
        // 这是心跳响应，表示连接正常

        // 尝试解析心跳响应中的分拣数据
        // 格式可能是类似 "000" 或其他格式
        const cleanData = message.replace('_refresh', '').trim();
        if (cleanData.length === 3 && /^\d{3}$/.test(cleanData)) {
          // 如果是3位数字，分别对应物品0、1、2的计数
          this.globalData.sortingCounts = {
            position0: parseInt(cleanData[0]) || 0,
            position1: parseInt(cleanData[1]) || 0,
            position2: parseInt(cleanData[2]) || 0
          };

          console.log('从心跳响应解析分拣计数:', this.globalData.sortingCounts);
          // 通知所有页面更新分拣计数
          this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        }
        return; // 处理完成，避免重复处理
      }
      // 处理计数信息消息 (格式: C00X\nC01Y\nC02Z)
      else if (message.includes('C00') && message.includes('C01') && message.includes('C02')) {
        console.log('收到计数信息:', message);

        // 解析计数数据
        const lines = message.split('\n');
        const counts = { position0: 0, position1: 0, position2: 0 };

        lines.forEach(line => {
          if (line.startsWith('C00')) {
            counts.position0 = parseInt(line.replace('C00', '')) || 0;
          } else if (line.startsWith('C01')) {
            counts.position1 = parseInt(line.replace('C01', '')) || 0;
          } else if (line.startsWith('C02')) {
            counts.position2 = parseInt(line.replace('C02', '')) || 0;
          }
        });

        console.log('解析出的计数数据:', counts);

        // 更新全局数据
        this.globalData.sortingCounts = counts;

        // 通知所有页面更新分拣计数
        this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        return; // 处理完成，避免重复处理
      }
      // 处理设备响应消息
      else if (message === 'device_light_on') {
        console.log('设备确认：灯光已开启');
        return; // 处理完成
      }
      else if (message === 'device_light_off') {
        console.log('设备确认：灯光已关闭');
        return; // 处理完成
      }
      // 处理速度变化确认消息
      else if (message.includes('speed') || message.includes('Speed')) {
        console.log('收到速度相关消息:', message);
        return; // 处理完成
      }
      // 处理方向指令 (L=左转, R=右转)
      else if (message.trim() === 'L') {
        console.log('收到左转指令');
        return; // 处理完成
      }
      else if (message.trim() === 'R') {
        console.log('收到右转指令');
        return; // 处理完成
      }
      // 处理纯数字消息（物品ID）- 仅在没有其他格式匹配时处理，避免重复计数
      else if (/^\d+$/.test(message.trim())) {
        console.log('收到数字ID信息:', message);
        const messageStr = message.trim();

        // 特殊处理："000"表示没有检测到物品或刷新响应，不应该增加计数
        if (messageStr === '000') {
          console.log('收到"000"，表示没有检测到物品或刷新响应，不更新计数');
          return;
        }

        // 检查是否为重复扫描数据
        if (this.isDuplicateScanData(messageStr)) {
          console.log('忽略重复的数字ID信息:', message);
          return; // 忽略重复数据
        }

        const statusValue = parseInt(messageStr);

        // 记录处理前的计数状态
        const beforeCounts = {
          position0: this.globalData.sortingCounts.position0,
          position1: this.globalData.sortingCounts.position1,
          position2: this.globalData.sortingCounts.position2
        };
        console.log('数字消息处理前的分拣计数:', beforeCounts);

        // 如果是1-2的数字，增加对应的分拣计数
        if (statusValue >= 1 && statusValue <= 2) {
          if (statusValue === 1) {
            if (this.isCountingInCooldown('position1')) {
              console.log(`🚫 position1 在冷却期内，忽略此次数字ID计数增加`);
              return; // 在冷却期内，忽略此次增加
            }
            this.globalData.sortingCounts.position1++;
            this.updateCountingCooldown('position1');
            console.log(`🔢 数字ID=${statusValue} 匹配到position1，计数从 ${beforeCounts.position1} 增加到 ${this.globalData.sortingCounts.position1}`);
          } else if (statusValue === 2) {
            if (this.isCountingInCooldown('position2')) {
              console.log(`🚫 position2 在冷却期内，忽略此次数字ID计数增加`);
              return; // 在冷却期内，忽略此次增加
            }
            this.globalData.sortingCounts.position2++;
            this.updateCountingCooldown('position2');
            console.log(`🔢 数字ID=${statusValue} 匹配到position2，计数从 ${beforeCounts.position2} 增加到 ${this.globalData.sortingCounts.position2}`);
          }

          console.log('根据数字ID更新分拣计数:', this.globalData.sortingCounts);
          // 通知所有页面更新分拣计数
          this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        }
        // 如果收到单独的"0"（不是"000"），可能表示检测到物品0
        else if (statusValue === 0 && messageStr !== '000') {
          if (this.isCountingInCooldown('position0')) {
            console.log(`🚫 position0 在冷却期内，忽略此次数字ID计数增加`);
            return; // 在冷却期内，忽略此次增加
          }
          this.globalData.sortingCounts.position0++;
          this.updateCountingCooldown('position0');
          console.log(`🔢 数字ID=${statusValue} 匹配到position0，计数从 ${beforeCounts.position0} 增加到 ${this.globalData.sortingCounts.position0}`);
          console.log('根据数字ID更新分拣计数:', this.globalData.sortingCounts);
          this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        }
        return; // 处理完成，避免重复处理
      }
      // 处理状态消息
      else if (message.startsWith('STATUS:')) {
        const parts = message.replace('STATUS:', '').split(',');
        const isRunning = parts[0] === '1';
        const speed = parseInt(parts[1]);

        // 更新全局数据
        this.globalData.isRunning = isRunning;
        this.globalData.currentSpeed = speed;
        this.globalData.speedText = this.getSpeedText(speed);

        // 通知所有页面更新
        this.notifyPagesUpdate('device_status', {
          isRunning,
          speedText: this.globalData.speedText
        });
        return; // 处理完成
      }
      // 处理SORT格式的分拣计数消息
      else if (message.startsWith('SORT:')) {
        const parts = message.replace('SORT:', '').split(',');
        this.globalData.sortingCounts = {
          position0: parseInt(parts[0]) || 0,
          position1: parseInt(parts[1]) || 0,
          position2: parseInt(parts[2]) || 0
        };

        // 通知所有页面更新
        this.notifyPagesUpdate('sorting_counts', this.globalData.sortingCounts);
        return; // 处理完成
      }
      // 处理PWM消息
      else if (message.startsWith('PWM:')) {
        const parts = message.replace('PWM:', '').split(',');

        // 更新舵机PWM信息
        const servoPwmInfo = this.globalData.servoPwmInfo.map((item, index) => {
          if (index < parts.length) {
            return {
              ...item,
              pwm: parseInt(parts[index]) || item.pwm
            };
          }
          return item;
        });

        this.globalData.servoPwmInfo = servoPwmInfo;

        // 通知所有页面更新
        this.notifyPagesUpdate('servo_pwm', { servoList: servoPwmInfo });
        return; // 处理完成
      }
      // 处理连接确认消息
      else if (message === 'CONNECT_OK' || message === 'COMM_CONNECTED' || message === 'CONNECT_SUCCESS') {
        // 接收到连接确认响应
        this.globalData.udpConnected = true;
        this.globalData.isConnecting = false;

        // 重置重连尝试次数
        this.globalData.reconnectAttempts = 0;

        // 确保UDPManager也更新连接状态
        if (this.globalData.udpManager) {
          this.globalData.udpManager.setConnected(true);
        }

        console.log('通信机连接确认:', message);

        // 通知所有页面更新连接状态
        this.notifyPagesUpdate('udp_status', { connected: true });
        
        // 连接成功后隐藏加载提示
        try {
          wx.hideLoading();
        } catch (e) {
          console.warn('隐藏loading失败:', e);
        }
        
        // 显示连接成功的提示
        wx.showToast({
          title: '连接成功',
          icon: 'success',
          duration: 1500
        });
        
        // 发送一次状态查询
        setTimeout(() => {
          if (this.globalData.udpManager && this.globalData.udpConnected) {
            this.globalData.udpManager.queryStatus()
              .catch(e => {
                console.warn('状态查询失败', e);
                this.attemptReconnect();
              });
            this.globalData.udpManager.queryCounts()
              .catch(e => console.warn('计数查询失败', e));
          }
        }, 300);
        return; // 处理完成
      }
      // 处理心跳响应
      else if (message === 'HEARTBEAT_OK') {
        // 心跳响应，确认连接状态
        this.globalData.udpConnected = true;

        // 重置心跳失败计数
        this.globalData.heartbeatFailCount = 0;

        // 确保UDPManager也更新连接状态
        if (this.globalData.udpManager && !this.globalData.udpManager.getConnectionStatus()) {
          this.globalData.udpManager.setConnected(true);
          // 通知所有页面更新连接状态
          this.notifyPagesUpdate('udp_status', { connected: true });
        }
        return; // 处理完成
      }
      // 如果没有匹配到任何已知格式，记录日志但不处理
      else {
        console.log('收到未识别的消息格式:', message);
      }
    } catch (error) {
      console.error('解析UDP消息失败:', error);
    }
  },
  
  /**
   * 尝试恢复上次的连接
   */
  async tryRestoreConnection() {
    // 检查是否有上次连接的记录
    const lastConnection = wx.getStorageSync('last_connection');
    
    if (lastConnection && lastConnection.ip && lastConnection.port) {
      // 检查上次连接时间是否在24小时内
      const now = Date.now();
      const lastTime = lastConnection.timestamp || 0;
      const hoursDiff = (now - lastTime) / (1000 * 60 * 60);
      
      // 如果上次连接在24小时内，尝试自动重连
      if (hoursDiff < 24) {
        console.log(`尝试自动连接到上次的通信机 (${lastConnection.ip}:${lastConnection.port})`);
        
        // 延迟一点执行，确保应用完全启动
        setTimeout(() => {
          wx.showModal({
            title: '自动连接',
            content: `是否连接到上次的通信机 (${lastConnection.ip}:${lastConnection.port})?`,
            success: (res) => {
              if (res.confirm) {
                this.connectToCommModule();
              }
            }
          });
        }, 1000);
      }
    }
  },
  
  /**
   * 尝试重新连接
   */
  attemptReconnect() {
    // 如果已经在连接中，不要重复尝试
    if (this.globalData.isConnecting) {
      console.log('已有连接请求正在进行中，跳过重连');
      return;
    }
    
    if (this.globalData.reconnectAttempts < this.globalData.maxReconnectAttempts) {
      this.globalData.reconnectAttempts++;
      this.globalData.isConnecting = true;
      
      console.log(`尝试重新连接 (${this.globalData.reconnectAttempts}/${this.globalData.maxReconnectAttempts})...`);
      
      // 短暂延迟后尝试重新连接
      setTimeout(() => {
        if (this.globalData.udpManager) {
          this.globalData.udpManager.reconnect()
            .then(() => {
              console.log('重新连接成功');
              // 连接成功后会在handleUDPMessage中收到CONNECT_OK消息，并更新状态
            })
            .catch(err => {
              console.error('重新连接失败:', err);
              this.globalData.isConnecting = false;
              
              if (this.globalData.reconnectAttempts >= this.globalData.maxReconnectAttempts) {
                // 确保隐藏loading
                try {
                  wx.hideLoading();
                } catch (e) {
                  console.warn('隐藏loading失败:', e);
                }
                
                wx.showToast({
                  title: '连接失败',
                  icon: 'error'
                });
                
                // 更新连接状态
                this.globalData.udpConnected = false;
                this.notifyPagesUpdate('udp_status', { connected: false });
              } else {
                // 如果还有重试次数，继续尝试
                setTimeout(() => {
                  this.attemptReconnect();
                }, UDP_CONFIG.RECONNECT_DELAY);
              }
            });
        }
      }, UDP_CONFIG.RECONNECT_DELAY);
    } else {
      // 超过最大重试次数，通知用户
      console.warn('超过最大重试次数，放弃连接');
      this.globalData.udpConnected = false;
      this.globalData.isConnecting = false;
      this.notifyPagesUpdate('udp_status', { connected: false });
      
      // 确保隐藏loading
      try {
        wx.hideLoading();
      } catch (e) {
        console.warn('隐藏loading失败:', e);
      }
      
      wx.showToast({
        title: '连接失败',
        icon: 'error'
      });
    }
  },
  
  /**
   * 连接到通信机
   */
  async connectToCommModule(): Promise<boolean> {
    // 如果已经在连接中，不要重复尝试
    if (this.globalData.isConnecting) {
      console.log('已有连接请求正在进行中，跳过连接');
      return false;
    }
    
    if (!this.globalData.udpManager) {
      console.error('UDP管理器未初始化');
      wx.showToast({
        title: 'UDP管理器未初始化',
        icon: 'error'
      });
      return false;
    }

    // 标记为正在连接
    this.globalData.isConnecting = true;
    
    // 重置重连尝试次数
    this.globalData.reconnectAttempts = 0;
    
    // 创建连接加载对话框的超时ID
    let loadingTimeoutId: number | null = null;
    
    try {
      // 显示加载提示
      try {
        wx.hideLoading();  // 先隐藏可能存在的loading
      } catch (e) {}
      
      wx.showLoading({ 
        title: '连接通信机...',
        mask: true 
      });
      
      // 确保UDP Socket已初始化
      if (!this.globalData.udpSocket) {
        console.log('UDP Socket未初始化，正在重新创建...');
        this.initUDPSocket();
      }
      
      // 获取连接信息 - 优先使用UI输入的值，其次是上次连接记录，最后是默认值
      let ip: string
      let port: number
      
      // 尝试获取上次连接信息
      const lastConnection = wx.getStorageSync('last_connection') || {};
      
      // 尝试从页面数据获取
      const pages = getCurrentPages()
      const currentPage = pages[pages.length - 1]
      if (currentPage && currentPage.data && currentPage.data.deviceIP) {
        ip = currentPage.data.deviceIP
        port = currentPage.data.devicePort || UDP_CONFIG.DEFAULT_PORT
        console.log('从页面获取连接信息:', ip, port)
      } else if (lastConnection.ip && lastConnection.port) {
        ip = lastConnection.ip
        port = lastConnection.port
        console.log('从上次记录获取连接信息:', ip, port)
      } else {
        ip = UDP_CONFIG.DEFAULT_IP
        port = UDP_CONFIG.DEFAULT_PORT
        console.log('使用默认连接信息:', ip, port)
      }
      
      console.log(`尝试连接到通信机 ${ip}:${port}`);
      
      // 重置连接状态
      this.globalData.udpConnected = false;
      if (this.globalData.udpManager) {
        this.globalData.udpManager.setConnected(false);
      }
      
      // 通知页面更新连接状态
      this.notifyPagesUpdate('udp_status', { connected: false });
      
      // 设置超时，确保即使没有收到响应也会隐藏loading
      loadingTimeoutId = setTimeout(() => {
        if (this.globalData.isConnecting) {
          try {
            wx.hideLoading();
          } catch (e) {}
          loadingTimeoutId = null;
        }
      }, UDP_CONFIG.CONNECTION_TIMEOUT + 2000);
      
      // 进行连接
      await this.globalData.udpManager.connect(ip, port);
      
      // 注意：连接状态在接收到CONNECT_OK消息后在handleUDPMessage中更新
      
      return true;
    } catch (error) {
      console.error('连接失败:', error);
      
      // 标记连接结束
      this.globalData.isConnecting = false;
      
      // 隐藏加载提示并显示错误
      try {
        wx.hideLoading();
      } catch (e) {}
      
      wx.showToast({
        title: '连接失败',
        icon: 'error'
      });
      
      // 更新连接状态
      this.globalData.udpConnected = false;
      if (this.globalData.udpManager) {
        this.globalData.udpManager.setConnected(false);
      }
      
      // 通知页面更新连接状态
      this.notifyPagesUpdate('udp_status', { connected: false });
      
      return false;
    } finally {
      // 清除loading超时
      if (loadingTimeoutId !== null) {
        clearTimeout(loadingTimeoutId);
        
        // 确保loading被隐藏
        try {
          wx.hideLoading();
        } catch (e) {}
      }
    }
  },
  
  /**
   * 断开通信机连接
   */
  disconnectCommModule() {
    // 标记连接结束
    this.globalData.isConnecting = false;
    
    if (this.globalData.udpManager) {
      const connectionInfo = this.globalData.udpManager.disconnect();
      
      // 存储上次连接的信息
      if (connectionInfo && connectionInfo.savedIP && connectionInfo.savedPort) {
        wx.setStorageSync('last_connection', {
          ip: connectionInfo.savedIP,
          port: connectionInfo.savedPort,
          timestamp: Date.now()
        });
      }
    }
    
    // 更新全局状态
    this.globalData.udpConnected = false;
    this.globalData.isRunning = false;
    this.globalData.speedText = '停止';
    
    // 通知所有页面更新连接状态
    this.notifyPagesUpdate('udp_status', { connected: false });
    this.notifyPagesUpdate('device_status', { 
      isRunning: false,
      speedText: '停止'
    });
    
    wx.showToast({
      title: '已断开连接',
      icon: 'success'
    });
  },

  // 通知页面更新数据
  notifyPagesUpdate(type: string, data: any) {
    const pages = getCurrentPages()
    pages.forEach(page => {
      if (page.onGlobalDataUpdate && typeof page.onGlobalDataUpdate === 'function') {
        page.onGlobalDataUpdate(type, data)
      }
    })
  },

  // 获取速度文本
  getSpeedText(speed: number): string {
    return getSpeedText(speed)
  },
})
