// pages/cloud/cloud.ts
import { UDP_CONFIG } from '../../config/index'
import UDPManager from '../../utils/udp-manager'

interface CloudPageData {
  udpConnected: boolean
  isRunning: boolean
  speedText: string
  deviceIP: string
  devicePort: number
  sortingCounts: {
    position0: number
    position1: number
    position2: number
  }
}

Page({
  data: {
    udpConnected: false,
    isRunning: false,
    speedText: '停止',
    deviceIP: UDP_CONFIG.DEFAULT_IP,
    devicePort: UDP_CONFIG.DEFAULT_PORT,
    sortingCounts: {
      position0: 0,
      position1: 0,
      position2: 0
    }
  } as CloudPageData,

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
    this.setData({
      udpConnected: app.globalData.udpConnected,
      isRunning: app.globalData.isRunning || false,
      speedText: app.globalData.speedText || '停止',
      deviceIP: app.globalData.udpManager?.boardIP || UDP_CONFIG.DEFAULT_IP,
      devicePort: app.globalData.udpManager?.boardPort || UDP_CONFIG.DEFAULT_PORT,
      sortingCounts: app.globalData.sortingCounts || {
        position0: 0,
        position1: 0,
        position2: 0
      }
    })
  },

  /**
   * 刷新分拣统计
   */
  async onRefreshCounts() {
    const app = getApp()
    
    if (!app.globalData.udpManager || !app.globalData.udpConnected) {
      wx.showToast({
        title: '请先连接通信机',
        icon: 'error'
      })
      return
    }

    try {
      await app.globalData.udpManager.sendRawCommand('_refresh')
      
      // 指令发送成功后立即显示成功提示
      wx.showToast({
        title: '刷新成功',
        icon: 'success'
      })
    } catch (error) {
      wx.showToast({
        title: '刷新失败',
        icon: 'error'
      })
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
   * 重置分拣计数
   */
  onResetCounts() {
    wx.showModal({
      title: '确认重置',
      content: '确定要重置所有分拣计数吗？',
      success: (res) => {
        if (res.confirm) {
          const app = getApp()
          // 重置全局数据中的分拣计数
          app.globalData.sortingCounts = {
            position0: 0,
            position1: 0,
            position2: 0
          }

          // 更新页面显示
          this.setData({
            sortingCounts: app.globalData.sortingCounts
          })

          wx.showToast({
            title: '计数已重置',
            icon: 'success'
          })
        }
      }
    })
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
          deviceIP: app.globalData.udpManager?.boardIP || UDP_CONFIG.DEFAULT_IP,
          devicePort: app.globalData.udpManager?.boardPort || UDP_CONFIG.DEFAULT_PORT
        })
        break;
      case 'device_status':
        this.setData({
          isRunning: data.isRunning,
          speedText: data.speedText
        })
        break;
      case 'sorting_counts':
        console.log('Cloud页面收到分拣计数更新:', data);
        this.setData({
          sortingCounts: data
        })
        console.log('Cloud页面更新后的显示数据:', this.data.sortingCounts);
        break;
    }
  }
})
