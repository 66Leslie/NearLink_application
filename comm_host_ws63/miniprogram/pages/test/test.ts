// pages/test/test.ts
import { convertSliderToPWM, throttle } from '../../utils/util'

const testApp = getApp<IAppOption>()

Page({
  /**
   * 页面的初始数据
   */
  data: {
    // 四个舵机的滑块值，范围0-100
    servo1Value: 50,
    servo2Value: 50,
    servo3Value: 50,
    servo4Value: 50,
    
    // 舵机名称和描述
    servoInfo: [
      { name: '舵机1', description: '底座控制', channel: 'BASE' },
      { name: '舵机2', description: '中位控制', channel: 'MID' },
      { name: '舵机3', description: '水平控制', channel: 'LVL' },
      { name: '舵机4', description: '印章控制', channel: 'STAMP' }
    ],
    

    
    // 实时PWM值显示
    pwmValues: [1500, 1500, 1500, 1500], // 对应500-2500的PWM值
    
    // 预设位置
    presetPositions: [
      { name: '初始位置', values: [50, 50, 50, 50] },
      { name: '工作位置', values: [30, 70, 60, 40] },
      { name: '维护位置', values: [80, 20, 30, 90] },
      { name: '校准位置', values: [0, 100, 0, 100] }
    ]
  },

  // 节流发送舵机指令
  throttledSendServoCommand: throttle(function(this: any, servoIndex: number, value: number) {
    const pwmValue = convertSliderToPWM(value)
    // 这里可以添加通过UDP发送指令的逻辑
    console.log(`节流发送舵机指令(UDP): 舵机${servoIndex + 1}, 值:${value}, PWM:${pwmValue}`);
  }, 100), // 100ms节流间隔

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad() {
    this.updateFromGlobalData()
  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow() {
    this.updateFromGlobalData()
  },
  
  /**
   * 从全局数据更新页面状态
   */
  updateFromGlobalData() {
    const globalData = testApp.globalData
    this.setData({
      servo1Value: globalData.servoValues[0],
      servo2Value: globalData.servoValues[1],
      servo3Value: globalData.servoValues[2],
      servo4Value: globalData.servoValues[3]
    })
    this.updatePWMValues()
  },
  
  /**
   * 更新PWM值显示
   */
  updatePWMValues() {
    const pwmValues = [
      convertSliderToPWM(this.data.servo1Value),
      convertSliderToPWM(this.data.servo2Value),
      convertSliderToPWM(this.data.servo3Value),
      convertSliderToPWM(this.data.servo4Value)
    ]
    this.setData({ pwmValues })
  },
  
  /**
   * 舵机1滑块变化（实时）
   */
  onServo1Changing(e: any) {
    console.log('onServo1Changing, value:', e.detail.value);
    const value = parseInt(e.detail.value)
    this.setData({ servo1Value: value })
    this.updatePWMValues()
    this.throttledSendServoCommand(0, value)
  },

  /**
   * 舵机1滑块变化
   */
  onServo1Change(e: any) {
    console.log('onServo1Change, value:', e.detail.value);
    const value = parseInt(e.detail.value)
    this.setData({ servo1Value: value })
    this.updatePWMValues()
    this.sendServoCommand(0, value)
    this.updateGlobalServoValue(0, value)
  },
  
  /**
   * 舵机2滑块变化（实时）
   */
  onServo2Changing(e: any) {
    console.log('onServo2Changing, value:', e.detail.value);
    const value = parseInt(e.detail.value)
    this.setData({ servo2Value: value })
    this.updatePWMValues()
    this.throttledSendServoCommand(1, value)
  },

  /**
   * 舵机2滑块变化
   */
  onServo2Change(e: any) {
    const value = parseInt(e.detail.value)
    this.setData({ servo2Value: value })
    this.updatePWMValues()
    this.sendServoCommand(1, value)
    this.updateGlobalServoValue(1, value)
  },
  
  /**
   * 舵机3滑块变化（实时）
   */
  onServo3Changing(e: any) {
    const value = parseInt(e.detail.value)
    this.setData({ servo3Value: value })
    this.updatePWMValues()
    this.throttledSendServoCommand(2, value)
  },

  /**
   * 舵机3滑块变化
   */
  onServo3Change(e: any) {
    const value = parseInt(e.detail.value)
    this.setData({ servo3Value: value })
    this.updatePWMValues()
    this.sendServoCommand(2, value)
    this.updateGlobalServoValue(2, value)
  },
  
  /**
   * 舵机4滑块变化（实时）
   */
  onServo4Changing(e: any) {
    const value = parseInt(e.detail.value)
    this.setData({ servo4Value: value })
    this.updatePWMValues()
    this.throttledSendServoCommand(3, value)
  },

  /**
   * 舵机4滑块变化
   */
  onServo4Change(e: any) {
    const value = parseInt(e.detail.value)
    this.setData({ servo4Value: value })
    this.updatePWMValues()
    this.sendServoCommand(3, value)
    this.updateGlobalServoValue(3, value)
  },
  
  /**
   * 发送舵机控制指令
   */
  sendServoCommand(servoIndex: number, value: number) {
    const pwmValue = convertSliderToPWM(value)
    // 这里可以添加通过UDP发送指令的逻辑
    console.log(`发送舵机指令(UDP): 舵机${servoIndex + 1}, 值:${value}, PWM:${pwmValue}`);
    wx.showToast({
      title: `舵机${servoIndex + 1}指令已发送`,
      icon: 'none'
    })
  },
  
  /**
   * 更新全局舵机值
   */
  updateGlobalServoValue(index: number, value: number) {
    testApp.globalData.servoValues[index] = value
  },
  
  /**
   * 应用预设位置
   */
  onApplyPreset(e: any) {
    const index = e.currentTarget.dataset.index
    const preset = this.data.presetPositions[index]
    
    wx.showModal({
      title: '应用预设',
      content: `确定要应用"${preset.name}"预设位置吗？`,
      success: (res) => {
        if (res.confirm) {
          this.setData({
            servo1Value: preset.values[0],
            servo2Value: preset.values[1],
            servo3Value: preset.values[2],
            servo4Value: preset.values[3]
          })
          
          // 更新PWM值显示
          this.updatePWMValues()
          
          // 发送所有舵机指令
          preset.values.forEach((value, index) => {
            this.sendServoCommand(index, value)
            this.updateGlobalServoValue(index, value)
          })
          
          wx.showToast({
            title: `已应用${preset.name}`,
            icon: 'success'
          })
        }
      }
    })
  },
  
  /**
   * 保存当前位置为预设
   */
  onSaveAsPreset() {
    wx.showModal({
      title: '保存预设',
      content: '将当前舵机位置保存为自定义预设？',
      success: (res) => {
        if (res.confirm) {
          // 这里可以扩展保存到本地存储的功能
          wx.setStorageSync('custom_preset', {
            name: '自定义位置',
            values: [
              this.data.servo1Value,
              this.data.servo2Value,
              this.data.servo3Value,
              this.data.servo4Value
            ],
            timestamp: Date.now()
          })
          
          wx.showToast({
            title: '预设已保存',
            icon: 'success'
          })
        }
      }
    })
  },
  
  /**
   * 停止所有舵机
   */
  onStopAll() {
    wx.showModal({
      title: '停止舵机',
      content: '确定要将所有舵机设置为中位(50%)吗？',
      success: (res) => {
        if (res.confirm) {
          this.setData({
            servo1Value: 50,
            servo2Value: 50,
            servo3Value: 50,
            servo4Value: 50
          })
          
          this.updatePWMValues()
          
          // 发送停止指令
          for (let i = 0; i < 4; i++) {
            this.sendServoCommand(i, 50)
            this.updateGlobalServoValue(i, 50)
          }
          
          wx.showToast({
            title: '所有舵机已停止',
            icon: 'success'
          })
        }
      }
    })
  }
})