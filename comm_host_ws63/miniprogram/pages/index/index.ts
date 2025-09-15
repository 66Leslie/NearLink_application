import { UDP_CONFIG } from '../../config/index'

interface IAppOption {
  globalData: {
    isRunning: boolean;
    currentSpeed: number;
    speedText: string;
    sortingCounts: {
      position1: number;
      position2: number;
      position3: number;
    };
    servoValues: number[];
    debugMessages: any[];
    udpConnected: boolean;
    userInfo?: WechatMiniprogram.UserInfo;
    udpSocket: WechatMiniprogram.UDPSocket | null;
    udpManager: any; // 使用 any 类型以简化，实际项目中可定义更精确的类型
  }
  notifyPagesUpdate: (type: string, data: any) => void;
  getSpeedText: (speed: number) => string;
  connectToCommModule: () => Promise<boolean>;
  disconnectCommModule: () => void;
}

const app = getApp<IAppOption>()

Page({
  data: {
    ipAddress: UDP_CONFIG.DEFAULT_IP,
    port: UDP_CONFIG.DEFAULT_PORT,
    connected: false,
    log: [] as string[],
    connectButtonText: '连接通信机',
    connecting: false,
    logPanelVisible: false
  },

  timeoutId: 0 as any,

  onLoad() {
    this.addLog('=== 智慧物流控制系统启动 ===');
    this.addLog('通信模式：UDP直连通信机');
    this.addLog('架构：小程序(UDP客户端) <-> Hi3861通信机(UDP服务器)');

    // 检查是否从体验模式返回，如果是则清除相关状态
    const demoMode = wx.getStorageSync('demo_mode');
    if (demoMode) {
      this.addLog('检测到体验模式状态，已清除');
      wx.removeStorageSync('demo_mode');
    }

    // 页面加载时，检查全局是否存在udpSocket实例
    if (!app.globalData.udpSocket) {
      this.createAndBindSocket();
    } else {
      // 如果已存在，说明可能是从其他页面返回，检查连接状态
      this.setData({
        connected: app.globalData.udpConnected,
        connectButtonText: app.globalData.udpConnected ? '断开连接' : '连接通信机'
      });
      this.addLog('UDP Socket 已存在，状态：' + (app.globalData.udpConnected ? '已连接' : '未连接'));
    }

    // 页面加载时，从全局数据同步一次最新状态
    this.updateDataFromGlobal();

    // 尝试从本地缓存恢复IP和端口显示
    const lastConnection = wx.getStorageSync('last_connection');
    if (lastConnection && lastConnection.ip) {
      this.setData({
        ipAddress: lastConnection.ip,
        port: lastConnection.port
      });
    }
  },

  onShow() {
    // 每次显示页面时，都从全局数据同步一次最新状态
    this.updateDataFromGlobal();
  },

  /**
   * 全局数据更新回调，由 app.ts 在状态变化时调用
   */
  onGlobalDataUpdate(type: string, data: any) {
    if (type === 'udp_status') {
      this.setData({
        connected: data.connected,
        connecting: false // 无论成功或失败，都结束连接中状态
      });

      if (data.connected) {
        this.addLog('🎉 连接成功！');
        // 连接成功后，自动跳转到控制页面
        setTimeout(() => {
          wx.switchTab({
            url: '/pages/control/control',
          });
        }, 1000);
      } else {
        this.addLog('🔌 连接已断开。');
      }
    }
  },

  /**
   * 从全局数据更新页面UI
   */
  updateDataFromGlobal() {
    this.setData({
      connected: app.globalData.udpConnected,
      ipAddress: app.globalData.udpManager?.boardIP || this.data.ipAddress,
      port: app.globalData.udpManager?.boardPort || this.data.port,
    });
  },

  createAndBindSocket() {
    this.addLog('正在创建 UDP Socket...');
    const udp = wx.createUDPSocket();
    app.globalData.udpSocket = udp;

    try {
      const localPort = udp.bind();
      this.addLog(`✓ UDP Socket 已创建并绑定到本地端口: ${localPort}`);
      this.addLog(`准备连接到通信机: ${this.data.ipAddress}:${this.data.port}`);
    } catch (error) {
      this.addLog(`❌ UDP Socket 绑定失败: ${error}`);
      return;
    }

    // 监听来自通信机(开发板)的消息
    udp.onMessage((res: WechatMiniprogram.UDPSocketOnMessageCallbackResult) => {
      clearTimeout(this.timeoutId);

      if (!this.data.connected) {
        wx.hideLoading();
        this.setData({ 
          connected: true, 
          connectButtonText: '断开连接',
          connecting: false
        });
        app.globalData.udpConnected = true;
        this.addLog('🎉 连接成功！已收到通信机响应');

        wx.showToast({
          title: '连接成功',
          icon: 'success',
          duration: 1500
        });

        // 连接成功后，延时短暂时间自动跳转到控制页面
        setTimeout(() => {
          wx.switchTab({
            url: '/pages/control/control',
          });
        }, 1500);
      }

      // 解析来自通信机的数据
      const receivedData = new Uint8Array(res.message);
      const messageStr = String.fromCharCode.apply(null, Array.from(receivedData));
      this.addLog(`📥 收到数据: ${messageStr} [${res.remoteInfo.address}:${res.remoteInfo.port}]`);
      
      // 处理通信机发送的业务数据（如分拣计数、状态等）
      this.handleBoardMessage(messageStr);
    });

    udp.onError((res: { errMsg: string }) => {
      this.addLog(`❌ UDP 错误: ${res.errMsg}`);
      wx.hideLoading();
      clearTimeout(this.timeoutId);
      this.setData({ 
        connected: false, 
        connectButtonText: '连接通信机',
        connecting: false 
      });
      app.globalData.udpConnected = false;
    });

    udp.onClose(() => {
      this.addLog('🔌 UDP Socket 已关闭');
      this.setData({ 
        connected: false, 
        connectButtonText: '连接通信机',
        connecting: false 
      });
      app.globalData.udpConnected = false;
      app.globalData.udpSocket = null;
    });
  },

  // 处理来自通信机的业务消息
  handleBoardMessage(message: string) {
    try {
      // 根据文档，通信机可能发送分拣信息等数据
      // 这里需要根据您的C代码中的实际数据格式来解析
      if (message.startsWith('SORT:')) {
        // 示例：解析分拣计数数据
        // 格式假设：SORT:1,2,3 (表示三个投递处的计数)
        const counts = message.substring(5).split(',').map(Number);
        if (counts.length >= 3) {
          app.globalData.sortingCounts = {
            position1: counts[0] || 0,
            position2: counts[1] || 0,
            position3: counts[2] || 0
          };
          app.notifyPagesUpdate('sortingCount', app.globalData.sortingCounts);
        }
      } else if (message.startsWith('STATUS:')) {
        // 示例：解析设备状态
        // 格式假设：STATUS:1,2 (运行状态,速度档位)
        const parts = message.substring(7).split(',');
        if (parts.length >= 2) {
          app.globalData.isRunning = parts[0] === '1';
          app.globalData.currentSpeed = parseInt(parts[1]) || 0;
          app.globalData.speedText = app.getSpeedText(app.globalData.currentSpeed);
          
          app.notifyPagesUpdate('statusUpdate', {
            isRunning: app.globalData.isRunning,
            currentSpeed: app.globalData.currentSpeed,
            speedText: app.globalData.speedText
          });
        }
      }
    } catch (error) {
      this.addLog(`解析通信机数据失败: ${error}`);
    }
  },

  // 输入框处理
  onIpInput(e: WechatMiniprogram.Input) {
    this.setData({ ipAddress: e.detail.value });
    this.addLog(`通信机IP地址设置为: ${e.detail.value}`);
  },

  onPortInput(e: WechatMiniprogram.Input) {
    this.setData({ port: Number(e.detail.value) });
    this.addLog(`通信机端口设置为: ${e.detail.value}`);
  },

  addLog(log: string) {
    const timestamp = new Date().toLocaleTimeString();
    this.setData({
      log: [`[${timestamp}] ${log}`, ...this.data.log.slice(0, 50)],
    });
  },

  /**
   * 连接/断开按钮点击事件
   */
  async onConnectTap() {
    if (this.data.connecting) {
      return; // 防止重复点击
    }

    if (this.data.connected) {
      // 如果已连接，则执行断开操作
      app.disconnectCommModule();
      this.addLog('正在断开连接...');
    } else {
      // 如果未连接，则执行连接操作
      this.setData({ connecting: true });
      this.addLog(`正在连接到 ${this.data.ipAddress}:${this.data.port}...`);
      
      // 在连接前，将用户输入的IP和端口保存到本地，以便app.ts使用
      wx.setStorageSync('last_connection', {
        ip: this.data.ipAddress,
        port: this.data.port,
        timestamp: Date.now()
      });

      try {
        await app.connectToCommModule();
        // 连接成功或失败的后续处理会由 onGlobalDataUpdate 负责
      } catch (error: any) {
        this.setData({ connecting: false });
        this.addLog(`❌ 连接失败: ${error.message || '未知错误'}`);
        wx.showToast({
          title: `连接失败: ${error.message || '未知错误'}`,
          icon: 'none'
        });
      }
    }
  },

  /**
   * 进入控制系统
   */
  onEnterApp() {
    if (!this.data.connected) {
      wx.showToast({
        title: '请先连接通信机',
        icon: 'error'
      });
      return;
    }

    wx.switchTab({
      url: '/pages/control/control',
      success: () => {
        this.addLog('🚀 进入控制系统');
      }
    });
  },

  /**
   * 直接进入体验模式
   */
  onDirectEnter() {
    this.addLog('🎯 直接进入体验模式');

    wx.showModal({
      title: '体验模式',
      content: '您将进入体验模式，部分功能可能无法正常使用。建议连接设备后获得完整体验。',
      confirmText: '继续体验',
      cancelText: '返回连接',
      success: (res) => {
        if (res.confirm) {
          // 设置体验模式标志
          wx.setStorageSync('demo_mode', true);

          wx.switchTab({
            url: '/pages/control/control',
            success: () => {
              this.addLog('🚀 已进入体验模式');
              wx.showToast({
                title: '体验模式已开启',
                icon: 'success',
                duration: 2000
              });
            }
          });
        }
      }
    });
  },

  // 切换日志面板显示状态
  toggleLogPanel() {
    this.setData({
      logPanelVisible: !this.data.logPanelVisible
    });
  },

  // 清空日志
  onClearLog() {
    this.setData({ log: [] });
    this.addLog('📝 日志已清空');
  },

  onUnload() {
    // 页面卸载时只清除定时器，不关闭全局socket
    clearTimeout(this.timeoutId);
  },
});
