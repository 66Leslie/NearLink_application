// pages/arm/arm.ts
import { SERVO_CONFIG } from '../../config/index'
import UDPManager from '../../utils/udp-manager'

interface ServoItem {
  id: number
  name: string
  value: number
  pwm: number
}

interface ArmPageData {
  udpConnected: boolean
  servoList: ServoItem[]
}

Page({
  data: {
    udpConnected: false,
    servoList: [
      { id: 0, name: '机械臂底座', value: 50, pwm: 1500 },
      { id: 1, name: '机械臂中位', value: 50, pwm: 1500 },
      { id: 2, name: '机械臂水平', value: 50, pwm: 1500 },
      { id: 3, name: '机械臂印章', value: 50, pwm: 1500 }
    ]
  } as ArmPageData,

  udpManager: null as UDPManager | null,

  onLoad() {
    const app = getApp()
    this.udpManager = app.globalData.udpManager
    this.updateDataFromGlobal()
  },

  onShow() {
    this.updateDataFromGlobal()
  },

  /**
   * 从全局数据更新页面数据
   */
  updateDataFromGlobal() {
    const app = getApp()
    const servoValues = app.globalData.servoValues || [50, 50, 50, 50]
    
    const servoList = this.data.servoList.map((servo, index) => ({
      ...servo,
      value: servoValues[index],
      pwm: this.calculatePWM(servoValues[index])
    }))
    
    this.setData({
      udpConnected: app.globalData.udpConnected,
      servoList
    })
  },

  /**
   * 计算PWM值
   */
  calculatePWM(sliderValue: number): number {
    const { MIN, MAX } = SERVO_CONFIG.PWM_RANGE
    return Math.round(MIN + (MAX - MIN) * (sliderValue / 100))
  },

  /**
   * 舵机滑块变化处理
   */
  async onServoChange(e: any) {
    const servoId = parseInt(e.currentTarget.dataset.servoId)
    const value = e.detail.value
    const pwm = this.calculatePWM(value)
    
    // 更新界面
    const servoList = this.data.servoList.map((servo, index) => {
      if (index === servoId) {
        return { ...servo, value, pwm }
      }
      return servo
    })
    
    this.setData({ servoList })
    
    // 更新全局数据
    const app = getApp()
    const servoValues = [...app.globalData.servoValues]
    servoValues[servoId] = value
    app.globalData.servoValues = servoValues
    
    // 发送UDP指令
    if (this.udpManager && this.data.udpConnected) {
      try {
        await this.udpManager.sendServoCommand({
          servo_id: servoId,
          pwm_value: pwm,
          slider_value: value
        })
        
        // 指令发送成功后立即显示成功提示
        wx.showToast({
          title: `舵机${servoId+1}设置成功`,
          icon: 'success',
          duration: 1500
        })
      } catch (error) {
        wx.showToast({
          title: '舵机控制失败',
          icon: 'error'
        })
      }
    }
  },

  /**
   * 连接设备
   */
  async onConnectDevice() {
    const app = getApp()
    await app.connectToCommModule()
  },
  
  /**
   * 断开连接
   */
  onDisconnectDevice() {
    const app = getApp()
    app.disconnectCommModule()
  },

  /**
   * 全局数据更新回调
   */
  onGlobalDataUpdate(type: string, data: any) {
    switch (type) {
      case 'udp_status':
        this.setData({ udpConnected: data.connected })
        break;
      case 'servo_values':
        this.updateDataFromGlobal()
        break;
    }
  }
})
