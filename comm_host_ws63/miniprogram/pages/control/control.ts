// pages/control/control.ts
import { UDP_CONFIG } from '../../config/index'

interface ServoItem {
  id: number
  name: string
  pwm: number
}

interface DebugMessage {
  id: number
  time: string
  message: string
}

// 页面数据接口
interface ControlPageData {
  udpConnected: boolean
  deviceIP: string
  devicePort: number
  servoList: ServoItem[]
  currentSpeed: number
  debugMessages: DebugMessage[]
  debugScrollTop: number
  currentLineId: number
  lineIdInput: string
  showSuccessPopup: boolean
  successMessage: string
  buttonLocked: boolean  // 添加按钮锁定状态
  demoMode: boolean      // 体验模式标志
}

Page({
  /**
   * 页面的初始数据
   */
  data: {
    udpConnected: false,
    // 默认通信机IP和端口
    deviceIP: UDP_CONFIG.DEFAULT_IP,
    devicePort: UDP_CONFIG.DEFAULT_PORT,
    // 控制设备列表
    servoList: [
      { id: 0, name: '阻拦器', pwm: 1500 },
      { id: 1, name: '弹出器1', pwm: 1500 },
      { id: 2, name: '弹出器2', pwm: 1500 }
    ],
    // 当前速度档位
    currentSpeed: 0,
    // 调试信息
    debugMessages: [],
    debugScrollTop: 0,
    // 当前流水线编号
    currentLineId: 0,
    // 流水线编号输入框
    lineIdInput: '0',
    // 成功弹窗控制
    showSuccessPopup: false,
    // 成功提示消息
    successMessage: '',
    // 按钮锁定状态
    buttonLocked: false,
    // 体验模式标志
    demoMode: false
  } as ControlPageData,

  debugIdCounter: 0,

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad() {
    // 检查是否为体验模式
    const demoMode = wx.getStorageSync('demo_mode');
    if (demoMode) {
      // 显示体验模式提示
      wx.showToast({
        title: '体验模式已开启',
        icon: 'none',
        duration: 3000
      });

      // 模拟一些演示数据
      this.setData({
        demoMode: true,
        udpConnected: false,
        currentSpeed: 2,
        debugMessages: [
          { id: 1, time: new Date().toLocaleTimeString(), message: '[体验模式] 系统已启动' },
          { id: 2, time: new Date().toLocaleTimeString(), message: '[体验模式] 设备状态正常' },
          { id: 3, time: new Date().toLocaleTimeString(), message: '[体验模式] 等待用户操作...' }
        ]
      });
    } else {
      // 从全局数据更新页面数据
      this.updateDataFromGlobal()
      this.loadDebugMessages()
    }
  },

  /**
   * 退出体验模式
   */
  exitDemoMode() {
    wx.showModal({
      title: '退出体验模式',
      content: '退出后将返回连接页面，您可以连接真实设备获得完整功能。',
      confirmText: '确认退出',
      cancelText: '继续体验',
      success: (res) => {
        if (res.confirm) {
          // 清除体验模式标志
          wx.removeStorageSync('demo_mode');

          // 重置页面状态
          this.setData({
            demoMode: false
          });

          // 返回首页
          wx.redirectTo({
            url: '/pages/index/index',
            success: () => {
              wx.showToast({
                title: '已退出体验模式',
                icon: 'success'
              });
            }
          });
        }
      }
    });
  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow() {
    this.updateDataFromGlobal()
  },

  /**
   * 从全局数据更新页面数据
   */
  updateDataFromGlobal() {
    const app = getApp()
    this.setData({
      udpConnected: app.globalData.udpConnected,
      // 更新连接信息
      deviceIP: app.globalData.udpManager?.boardIP || UDP_CONFIG.DEFAULT_IP,
      devicePort: app.globalData.udpManager?.boardPort || UDP_CONFIG.DEFAULT_PORT,
      // 保持本地的设备列表配置，不被全局数据覆盖
      // servoList: 保持本地配置
      // 更新速度档位
      currentSpeed: app.globalData.currentSpeed || 0
    })
  },

  /**
   * 连接通信机
   */
  async onConnectComm() {
    // 防止重复点击
    if (this.data.buttonLocked) {
      console.log('按钮已锁定，忽略点击')
      return
    }
    this.lockButtons()
    
    const app = getApp()
    try {
      // 注意：不需要在这里调用 wx.showLoading，因为 app.connectToCommModule() 内部会处理
      await app.connectToCommModule()
      this.addDebugMessage('连接通信机成功')
    } catch (error) {
      // 确保隐藏 loading
      try {
        wx.hideLoading()
      } catch (e) {
        console.warn('隐藏loading失败:', e)
      }
      
      this.addDebugMessage(`连接通信机失败: ${error}`)
    }
  },
  
  /**
   * 断开连接
   */
  onDisconnect() {
    // 防止重复点击
    if (this.data.buttonLocked) {
      console.log('按钮已锁定，忽略点击')
      return
    }
    this.lockButtons()
    
    const app = getApp()
    app.disconnectCommModule()
    this.addDebugMessage('已断开与通信机的连接')
  },



  /**
   * 设置速度档位
   */
  async setSpeed(e: any) {
    // 防止重复点击
    if (this.data.buttonLocked) {
      console.log('按钮已锁定，忽略点击')
      return
    }
    this.lockButtons()
    
    const app = getApp()
    const speed = parseInt(e.currentTarget.dataset.speed)
    const speedTexts = ['停止', '慢速', '中速', '高速']

    // 更新当前速度（无论是否连接或成功，都更新UI）
    this.setData({ currentSpeed: speed })

    if (!app.globalData.udpManager || !app.globalData.udpConnected) {
      // 测试模式：即使未连接也显示成功
      const successMessage = `传送带速度已设置为${speedTexts[speed]}`
      this.showSuccessPopup(successMessage)
      this.addDebugMessage(`已将传送带速度设置为${speedTexts[speed]}（测试模式）`)
      return
    }
    
    try {
      // 发送传送带速度命令，格式: _change_speed[speed]
      const command = `_change_speed${speed}`

      // 发送指令并获取响应
      let response = ''
      try {
        await app.globalData.udpManager.sendRawCommand(command)

        // 等待短暂时间，让服务器有足够时间处理并响应
        await new Promise(resolve => setTimeout(resolve, 500))

        // 根据最近一次响应判断成功与否
        response = app.globalData.lastCommandResponse || ''
      } catch (sendError) {
        console.error('发送速度指令失败:', sendError)
        this.addDebugMessage(`传送带速度设置失败: ${sendError}`)
        this.showErrorPopup(`速度设置失败`)
        return
      }

      this.addDebugMessage(`已发送传送带速度设置命令: ${speedTexts[speed]}`)
      
      // 检查响应是否表示成功
      if (response && response.startsWith('SUCCESS:')) {
        // 添加成功调试信息
        this.addDebugMessage(`传送带速度设置为${speedTexts[speed]}成功`)

        // 显示自定义成功弹窗
        const successMessage = `传送带速度已设置为${speedTexts[speed]}`
        this.showSuccessPopup(successMessage)
      } else if (response && response.startsWith('ERROR:')) {
        // 解析错误类型
        let errorMessage = `速度设置失败`
        
        if (response.includes('INVALID_SPEED')) {
          errorMessage = `无效的速度值`
        } else if (response.includes('INVALID_SERVO_ID')) {
          errorMessage = `无效的舵机ID`
        } else if (response.includes('SPEED_COMMAND_ERROR')) {
          errorMessage = `速度指令错误`
        }
        
        this.addDebugMessage(`${errorMessage}: ${response}`)
        this.showErrorPopup(errorMessage)
      } else {
        // 未知响应或没有响应
        this.addDebugMessage(`速度设置结果未知`)
        this.showErrorPopup(`操作结果未知`)
      }
    } catch (error) {
      // 处理过程中发生错误
      this.addDebugMessage(`速度设置失败: ${error}`)
      this.showErrorPopup(`速度设置失败`)
    }
  },

  /**
   * 锁定按钮防止重复点击
   */
  lockButtons() {
    this.setData({ buttonLocked: true })
    setTimeout(() => {
      this.setData({ buttonLocked: false })
    }, 1000) // 1秒后解锁
  },

  /**
   * 控制舵机
   */
  async controlServo(e: any) {
    // 防止重复点击
    if (this.data.buttonLocked) {
      console.log('按钮已锁定，忽略点击')
      return
    }
    this.lockButtons()
    
    const app = getApp()
    const servoId = parseInt(e.currentTarget.dataset.servoId)
    const action = e.currentTarget.dataset.action
    
    if (!app.globalData.udpManager || !app.globalData.udpConnected) {
      // 测试模式：即使未连接也显示成功
      const deviceName = servoId === 0 ? '阻拦器' : `弹出器${servoId}`
      const actionText = servoId === 0 ? (action === 'on' ? '阻拦' : '放行') : (action === 'on' ? '弹出' : '收回')
      const successMessage = `${deviceName}${actionText}成功`
      this.showSuccessPopup(successMessage)
      return
    }
    
    try {
      // 发送弹出/收回指令，格式: _light_on[n] 或 _light_off[n]
      const command = action === 'on' ? `_light_on${servoId}` : `_light_off${servoId}`
      
      // 发送指令并获取响应
      let response = ''
      try {
        await app.globalData.udpManager.sendRawCommand(command)
        
        // 等待短暂时间，让服务器有足够时间处理并响应
        await new Promise(resolve => setTimeout(resolve, 500))
        
        // 根据最近一次响应判断成功与否
        response = app.globalData.lastCommandResponse || ''
      } catch (sendError) {
        console.error('发送指令失败:', sendError)
        const deviceName = servoId === 0 ? '阻拦器' : `弹出器${servoId}`
        const actionText = servoId === 0 ? (action === 'on' ? '阻拦' : '放行') : (action === 'on' ? '弹出' : '收回')
        this.addDebugMessage(`${deviceName}${actionText}指令发送失败: ${sendError}`)
        this.showErrorPopup(`操作失败`)
        return
      }

      const deviceName = servoId === 0 ? '阻拦器' : `弹出器${servoId}`
      const actionText = servoId === 0 ? (action === 'on' ? '阻拦' : '放行') : (action === 'on' ? '弹出' : '收回')
      this.addDebugMessage(`${deviceName}${actionText}指令已发送`)
      
      // 检查响应是否表示成功
      if (response && response.startsWith('SUCCESS:')) {
        // 添加成功调试信息
        const deviceName = servoId === 0 ? '阻拦器' : `弹出器${servoId}`
        const actionText = servoId === 0 ? (action === 'on' ? '阻拦' : '放行') : (action === 'on' ? '弹出' : '收回')
        this.addDebugMessage(`${deviceName}${actionText}成功`)

        // 显示自定义成功弹窗
        const successMessage = `${deviceName}${actionText}成功`
        this.showSuccessPopup(successMessage)
      } else if (response && response.startsWith('ERROR:')) {
        // 解析错误类型
        let errorMessage = `舵机${servoId + 1}${action === 'on' ? '弹出' : '收回'}失败`
        
        if (response.includes('INVALID_DEVICE_ID')) {
          errorMessage = `无效的设备ID`
        } else if (response.includes('OPERATION_FAILED')) {
          errorMessage = `操作失败，请重试`
        } else if (response.includes('COMMAND_ERROR')) {
          errorMessage = `指令格式错误`
        }
        
        this.addDebugMessage(`${errorMessage}: ${response}`)
        this.showErrorPopup(errorMessage)
      } else {
        // 未知响应或没有响应
        this.addDebugMessage(`舵机${servoId + 1}${action === 'on' ? '弹出' : '收回'}结果未知`)
        this.showErrorPopup(`操作结果未知`)
      }
    } catch (error) {
      // 处理过程中发生错误
      this.addDebugMessage(`舵机控制失败: ${error}`)
      this.showErrorPopup(`舵机控制失败`)
    }
  },

  /**
   * 添加调试信息
   */
  addDebugMessage(message: string) {
    const now = new Date()
    const time = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
    
    const debugMessage = {
      id: ++this.debugIdCounter,
      time,
      message
    }
    
    const debugMessages = [debugMessage, ...this.data.debugMessages].slice(0, 50)
    this.setData({
      debugMessages,
      debugScrollTop: 0
    })
    
    // 保存到本地存储
    wx.setStorageSync('control_debug_messages', debugMessages)
  },

  /**
   * 加载调试信息
   */
  loadDebugMessages() {
    try {
      const debugMessages = wx.getStorageSync('control_debug_messages')
      if (debugMessages && Array.isArray(debugMessages)) {
        this.setData({ debugMessages })
        if (debugMessages.length > 0) {
          this.debugIdCounter = debugMessages[0].id
        }
      }
    } catch (error) {
      console.error('加载调试信息失败:', error)
    }
  },

  /**
   * 清空调试信息
   */
  clearDebugInfo() {
    this.setData({
      debugMessages: [],
      debugScrollTop: 0
    })
    wx.setStorageSync('control_debug_messages', [])
  },
  
  /**
   * 显示成功弹窗
   */
  showSuccessPopup(message: string) {
    console.log('显示成功弹窗:', message)
    
    // 确保成功消息不超过最大长度
    const maxLength = 7  // 微信toast最多显示7个汉字
    const displayMessage = message.length > maxLength ? 
      message.substring(0, maxLength) + '...' : message
    
    // 使用系统Toast，确保在所有环境下都能显示
    wx.showToast({
      title: displayMessage,
      icon: 'success',
      duration: 2000,
      mask: true
    })
    
    // 如果成功消息较长，同时在调试信息中显示完整信息
    if (message.length > maxLength) {
      this.addDebugMessage(`成功: ${message}`)
    }
    
    // 同时更新自定义弹窗状态（如果页面上有自定义弹窗的话）
    this.setData({
      successMessage: message,
      showSuccessPopup: false  // 设置为false，因为我们使用系统Toast
    })
  },
  
  /**
   * 显示错误弹窗
   */
  showErrorPopup(message: string) {
    console.error('显示错误弹窗:', message)
    
    // 确保错误消息不超过最大长度
    const maxLength = 7  // 微信toast最多显示7个汉字
    const displayMessage = message.length > maxLength ? 
      message.substring(0, maxLength) + '...' : message
    
    // 使用系统Toast显示错误
    wx.showToast({
      title: displayMessage,
      icon: 'error',
      duration: 2000,
      mask: true  // 添加遮罩防止用户点击
    })
    
    // 如果错误消息较长，同时在调试信息中显示完整信息
    if (message.length > maxLength) {
      this.addDebugMessage(`错误: ${message}`)
    }
  },

  /**
   * 流水线编号输入框变化处理
   */
  onLineIdInput(e: any) {
    const value = e.detail.value
    // 只允许输入0-9的单个数字
    const numericValue = value.replace(/[^0-9]/g, '').slice(0, 1)
    this.setData({ lineIdInput: numericValue })
  },

  /**
   * 设置流水线编号
   */
  async setLineId() {
    // 防止重复点击
    if (this.data.buttonLocked) {
      console.log('按钮已锁定，忽略点击')
      return
    }
    this.lockButtons()

    const app = getApp()
    const lineId = parseInt(this.data.lineIdInput)

    // 检查输入有效性 (流水线编号是0-9的单个数字)
    if (isNaN(lineId) || lineId < 0 || lineId > 9) {
      wx.showToast({
        title: '编号应为0-9',
        icon: 'error'
      })
      return
    }

    // 先更新UI
    this.setData({ currentLineId: lineId })

    if (!app.globalData.udpManager || !app.globalData.udpConnected) {
      this.addDebugMessage(`已将流水线编号设置为${lineId}（测试模式）`)
      this.showSuccessPopup(`编号已设为${lineId}`)
      return
    }

    try {
      // 将数字转换为字符串并通过UDP发送
      const command = lineId.toString()
      await app.globalData.udpManager.sendRawCommand(command)

      // 指令发送成功
      this.addDebugMessage(`已发送流水线编号设置命令: ${lineId}`)
      this.showSuccessPopup(`流水线编号已设为 ${lineId}`)

      // （可选）可以保存到本地存储
      wx.setStorageSync('current_line_id', lineId)

    } catch (error: any) {
      // 处理发送失败的情况
      this.addDebugMessage(`设置流水线编号失败: ${error.message || error}`)
      this.showErrorPopup(`设置失败`)
    }
  },

  /**
   * 全局数据更新回调
   */
  onGlobalDataUpdate(type: string, data: any) {
    const app = getApp()
    
    switch (type) {
      case 'udp_status':
        this.setData({ 
          udpConnected: data.connected,
          // 更新连接信息
          deviceIP: app.globalData.udpManager?.boardIP || UDP_CONFIG.DEFAULT_IP,
          devicePort: app.globalData.udpManager?.boardPort || UDP_CONFIG.DEFAULT_PORT
        })
        break;
      case 'servo_pwm':
        // 更新舵机PWM信息
        this.setData({ servoList: data.servoList })
        break;
      case 'device_status':
        // 更新速度档位
        this.setData({ currentSpeed: app.globalData.currentSpeed || 0 })
        break;
      case 'line_id':
        // 更新流水线编号
        if (data && data.lineId) {
          this.setData({
            currentLineId: data.lineId,
            lineIdInput: String(data.lineId)
          })
        }
        break;
    }
  }
})