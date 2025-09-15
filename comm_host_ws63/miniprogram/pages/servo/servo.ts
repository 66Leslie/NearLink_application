import { SERVO_CONFIG, BOARD_COMMANDS } from '../../config/index'

interface IAppOption {
  globalData: {
    udpConnected: boolean;
    servoValues: number[];
    udpSocket: WechatMiniprogram.UDPSocket | null;
  }
  notifyPagesUpdate: (type: string, data: any) => void;
}

const app = getApp<IAppOption>()

Page({
  data: {
    udpConnected: false,
    servoList: [
      { id: 0, name: '底座控制', value: 50, pwm: 1500 },
      { id: 1, name: '中位控制', value: 50, pwm: 1500 },
      { id: 2, name: '水平控制', value: 50, pwm: 1500 },
      { id: 3, name: '印章控制', value: 50, pwm: 1500 }
    ],
    savedPreset: {
      servo0: 50,
      servo1: 50,
      servo2: 50,
      servo3: 50
    },
    debugMessages: [] as Array<{time: string, message: string}>
  },

  onLoad() {
    // 从本地存储加载预设
    try {
      const savedPreset = wx.getStorageSync('servo_preset')
      if (savedPreset) {
        this.setData({ savedPreset })
      }
    } catch (error) {
      console.error('加载预设失败:', error)
    }

    this.setData({
      udpConnected: app.globalData.udpConnected,
      servoList: this.updateServoList(app.globalData.servoValues)
    })
    
    this.addDebugMessage('机械测试页面已加载')
  },

  onShow() {
    this.setData({
      udpConnected: app.globalData.udpConnected
    })
  },

  // 更新舵机列表数据
  updateServoList(values: number[]) {
    return this.data.servoList.map((servo, index) => ({
      ...servo,
      value: values[index] || 50,
      pwm: this.valueToPwm(values[index] || 50)
    }))
  },

  // 将滑块值(0-100)转换为PWM值(500-2500)
  valueToPwm(value: number): number {
    const { MIN, MAX } = SERVO_CONFIG.PWM_RANGE
    return Math.round(MIN + (value / 100) * (MAX - MIN))
  },

  // 舵机滑块变化
  onServoChange(e: any) {
    const servoId = parseInt(e.currentTarget.dataset.servoId)
    const value = e.detail.value
    const pwm = this.valueToPwm(value)
    
    // 更新本地数据
    const servoList = this.data.servoList.map(servo => 
      servo.id === servoId ? { ...servo, value, pwm } : servo
    )
    this.setData({ servoList })
    
    // 更新全局数据
    app.globalData.servoValues[servoId] = value
    
    // 发送UDP指令
    this.sendServoCommand(servoId, value, pwm)
  },

  // 预设按钮点击
  onPresetTap(e: any) {
    const servoId = parseInt(e.currentTarget.dataset.servoId)
    const value = parseInt(e.currentTarget.dataset.value)
    const pwm = this.valueToPwm(value)
    
    // 更新本地数据
    const servoList = this.data.servoList.map(servo => 
      servo.id === servoId ? { ...servo, value, pwm } : servo
    )
    this.setData({ servoList })
    
    // 更新全局数据
    app.globalData.servoValues[servoId] = value
    
    // 发送UDP指令
    this.sendServoCommand(servoId, value, pwm)
  },

  // 发送舵机控制指令
  sendServoCommand(servoId: number, value: number, pwm: number) {
    if (!app.globalData.udpConnected || !app.globalData.udpSocket) {
      this.addDebugMessage(`❌ 发送失败：通信机未连接`)
      return
    }

    // 根据BOARD_COMMANDS发送单字符指令
    const command = servoId.toString() // '0', '1', '2', '3'
    
    try {
      // 创建指令缓冲区
      const buffer = new ArrayBuffer(command.length)
      const view = new Uint8Array(buffer)
      for (let i = 0; i < command.length; i++) {
        view[i] = command.charCodeAt(i)
      }

      app.globalData.udpSocket.send({
        address: '192.168.137.1', // 这里应该从配置中获取
        port: 5566,
        message: buffer,
        success: () => {
          this.addDebugMessage(`✓ 舵机${servoId}: ${value}% (${pwm}μs)`)
        },
        fail: (err: any) => {
          this.addDebugMessage(`❌ 舵机${servoId}指令发送失败: ${err.errMsg}`)
        }
      })
    } catch (error) {
      this.addDebugMessage(`❌ 舵机${servoId}指令发送异常: ${error}`)
    }
  },

  // 停止所有舵机
  onStopAll() {
    if (!app.globalData.udpConnected) {
      wx.showToast({
        title: '通信机未连接',
        icon: 'error'
      })
      return
    }

    // 将所有舵机设置为中位
    const servoList = this.data.servoList.map(servo => ({
      ...servo,
      value: 50,
      pwm: 1500
    }))
    
    this.setData({ servoList })
    
    // 更新全局数据
    app.globalData.servoValues = [50, 50, 50, 50]
    
    // 发送停止指令到所有舵机
    for (let i = 0; i < 4; i++) {
      this.sendServoCommand(i, 50, 1500)
    }
    
    this.addDebugMessage('🛑 所有舵机已设置为中位')
  },

  // 保存当前预设
  onSavePreset() {
    const preset = {
      servo0: this.data.servoList[0].value,
      servo1: this.data.servoList[1].value,
      servo2: this.data.servoList[2].value,
      servo3: this.data.servoList[3].value
    }
    
    this.setData({ savedPreset: preset })
    
    // 保存到本地存储
    wx.setStorageSync('servo_preset', preset)
    
    this.addDebugMessage(`💾 预设已保存: [${preset.servo0}%, ${preset.servo1}%, ${preset.servo2}%, ${preset.servo3}%]`)
    
    wx.showToast({
      title: '预设已保存',
      icon: 'success'
    })
  },

  // 加载预设位置
  onLoadPreset() {
    if (!app.globalData.udpConnected) {
      wx.showToast({
        title: '通信机未连接',
        icon: 'error'
      })
      return
    }

    const { savedPreset } = this.data
    
    // 更新舵机列表
    const servoList = this.data.servoList.map((servo, index) => {
      const presetKey = `servo${index}` as keyof typeof savedPreset
      const value = savedPreset[presetKey]
      return {
        ...servo,
        value,
        pwm: this.valueToPwm(value)
      }
    })
    
    this.setData({ servoList })
    
    // 更新全局数据
    app.globalData.servoValues = [
      savedPreset.servo0,
      savedPreset.servo1,
      savedPreset.servo2,
      savedPreset.servo3
    ]
    
    // 发送指令到所有舵机
    servoList.forEach((servo, index) => {
      this.sendServoCommand(index, servo.value, servo.pwm)
    })
    
    this.addDebugMessage(`📂 预设已加载: [${savedPreset.servo0}%, ${savedPreset.servo1}%, ${savedPreset.servo2}%, ${savedPreset.servo3}%]`)
    
    wx.showToast({
      title: '预设已加载',
      icon: 'success'
    })
  },

  // 添加调试消息
  addDebugMessage(message: string) {
    const time = new Date().toLocaleTimeString()
    const debugMessages = [{time, message}, ...this.data.debugMessages.slice(0, 49)] // 保留最新50条
    this.setData({ debugMessages })
  }
})