/**
 * WebSocket通信管理类
 */
import { getSpeedCommand, getFunctionCommand, buildTCPCommand } from './util'

interface SocketMessage {
  type: string
  data: any
}

interface ServoCommand {
  servo_id: number
  pwm_value: number
  slider_value: number
}

interface SpeedCommand {
  speed: number
}

interface FunctionCommand {
  function: string
}

interface RawCommand {
  command: string
}

class WebSocketManager {
  private isConnected = false
  private reconnectAttempts = 0
  private maxReconnectAttempts = 5
  private reconnectDelay = 3000
  private heartbeatInterval: number | null = null
  private app: any

  constructor(app: any) {
    this.app = app
  }

  /**
   * 初始化WebSocket连接
   */
  connect(url: string) {
    // Flask-SocketIO 要求特定的URL格式，需要添加 /socket.io/ 路径和参数
    const socketIoUrl = `${url}/socket.io/?EIO=4&transport=websocket`;
    console.log('尝试连接WebSocket:', socketIoUrl)
    
    wx.connectSocket({
      url: socketIoUrl,
      success: () => {
        console.log('WebSocket连接发起成功')
      },
      fail: (err) => {
        console.error('WebSocket连接发起失败:', err)
        this.handleConnectionError()
      }
    })

    this.setupEventHandlers()
  }

  /**
   * 设置事件处理器
   */
  private setupEventHandlers() {
    wx.onSocketOpen(() => {
      console.log('WebSocket连接已打开')
      this.isConnected = true
      this.reconnectAttempts = 0
      this.app.globalData.socketConnected = true
      this.startHeartbeat()
      this.notifyConnectionStatus(true)
    })

    wx.onSocketClose(() => {
      console.log('WebSocket连接已关闭')
      this.isConnected = false
      this.app.globalData.socketConnected = false
      this.stopHeartbeat()
      this.notifyConnectionStatus(false)
      this.attemptReconnect()
    })

    wx.onSocketMessage((res) => {
      this.handleMessage(res.data)
    })

    wx.onSocketError((err) => {
      console.error('WebSocket错误:', err)
      this.isConnected = false
      this.app.globalData.socketConnected = false
      this.notifyConnectionStatus(false)
    })
  }

  /**
   * 处理接收到的消息
   */
  private handleMessage(data: any) {
    try {
      const message: SocketMessage = typeof data === 'string' ? JSON.parse(data) : data
      
      switch (message.type) {
        case 'sorting_count':
          this.handleSortingCountUpdate(message.data)
          break
        case 'status_update':
          this.handleStatusUpdate(message.data)
          break
        case 'debug_message':
          this.handleDebugMessage(message.data)
          break
        case 'heartbeat':
          console.log('收到心跳响应')
          break
        default:
          console.log('未知消息类型:', message.type)
      }
    } catch (error) {
      console.error('解析WebSocket消息失败:', error)
    }
  }

  /**
   * 处理分拣计数更新
   */
  private handleSortingCountUpdate(data: any) {
    this.app.globalData.sortingCounts = {
      position1: data.position1 || 0,
      position2: data.position2 || 0,
      position3: data.position3 || 0
    }
    this.app.notifyPagesUpdate('sortingCount', this.app.globalData.sortingCounts)
  }

  /**
   * 处理状态更新
   */
  private handleStatusUpdate(data: any) {
    this.app.globalData.isRunning = data.running
    this.app.globalData.currentSpeed = data.speed
    this.app.globalData.speedText = this.app.getSpeedText(data.speed)
    
    this.app.notifyPagesUpdate('statusUpdate', {
      isRunning: this.app.globalData.isRunning,
      currentSpeed: this.app.globalData.currentSpeed,
      speedText: this.app.globalData.speedText
    })
  }

  /**
   * 处理调试消息
   */
  private handleDebugMessage(data: any) {
    this.app.globalData.debugMessages.unshift({
      time: new Date().toLocaleTimeString(),
      message: data.message
    })
    
    // 限制消息数量
    if (this.app.globalData.debugMessages.length > 100) {
      this.app.globalData.debugMessages = this.app.globalData.debugMessages.slice(0, 100)
    }
    
    this.app.notifyPagesUpdate('debugMessage', this.app.globalData.debugMessages)
  }

  /**
   * 发送消息
   */
  sendMessage(message: SocketMessage): boolean {
    if (!this.isConnected) {
      console.error('WebSocket未连接')
      wx.showToast({
        title: '连接已断开',
        icon: 'error'
      })
      return false
    }

    try {
      wx.sendSocketMessage({
        data: JSON.stringify(message),
        success: () => {
          console.log('消息发送成功:', message)
        },
        fail: (err) => {
          console.error('消息发送失败:', err)
          wx.showToast({
            title: '发送失败',
            icon: 'error'
          })
        }
      })
      return true
    } catch (error) {
      console.error('发送消息时出错:', error)
      return false
    }
  }

  /**
   * 发送舵机控制指令
   */
  sendServoCommand(command: ServoCommand): boolean {
    return this.sendMessage({
      type: 'servo_control',
      data: command
    })
  }

  /**
   * 发送速度控制指令
   */
  sendSpeedCommand(command: SpeedCommand): boolean {
    const speedChar = getSpeedCommand(command.speed)
    return this.sendMessage({
      type: 'speed_control',
      data: {
        speed: command.speed,
        command: speedChar,
        tcp_command: buildTCPCommand(speedChar)
      }
    })
  }

  /**
   * 发送功能指令
   */
  sendFunctionCommand(command: FunctionCommand): boolean {
    const functionChar = getFunctionCommand(command.function)
    if (!functionChar) {
      console.error('未知功能指令:', command.function)
      return false
    }
    
    return this.sendMessage({
      type: 'function_control',
      data: {
        function: command.function,
        command: functionChar,
        tcp_command: buildTCPCommand(functionChar)
      }
    })
  }

  /**
   * 发送原始指令
   */
  sendRawCommand(command: RawCommand): boolean {
    return this.sendMessage({
      type: 'raw_command',
      data: {
        command: command.command,
        tcp_command: buildTCPCommand(command.command)
      }
    })
  }

  /**
   * 发送查询指令
   */
  sendQueryCommand(): boolean {
    return this.sendMessage({
      type: 'query',
      data: {
        action: 'get_status',
        tcp_command: buildTCPCommand('Q')
      }
    })
  }

  /**
   * 开始心跳
   */
  private startHeartbeat() {
    this.stopHeartbeat()
    this.heartbeatInterval = setInterval(() => {
      if (this.isConnected) {
        this.sendMessage({
          type: 'heartbeat',
          data: { timestamp: Date.now() }
        })
      }
    }, 30000) // 30秒心跳间隔
  }

  /**
   * 停止心跳
   */
  private stopHeartbeat() {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval)
      this.heartbeatInterval = null
    }
  }

  /**
   * 尝试重连
   */
  private attemptReconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.log('重连次数已达上限，停止重连')
      return
    }

    this.reconnectAttempts++
    console.log(`尝试第${this.reconnectAttempts}次重连...`)

    setTimeout(() => {
      if (!this.isConnected) {
        this.connect(this.app.globalData.serverUrl)
      }
    }, this.reconnectDelay)
  }

  /**
   * 处理连接错误
   */
  private handleConnectionError() {
    this.isConnected = false
    this.app.globalData.socketConnected = false
    this.notifyConnectionStatus(false)
  }

  /**
   * 通知连接状态变化
   */
  private notifyConnectionStatus(connected: boolean) {
    this.app.notifyPagesUpdate('connectionStatus', { connected })
  }

  /**
   * 手动断开连接
   */
  disconnect() {
    this.stopHeartbeat()
    this.isConnected = false
    this.reconnectAttempts = this.maxReconnectAttempts // 阻止自动重连
    wx.closeSocket()
  }

  /**
   * 获取连接状态
   */
  getConnectionStatus(): boolean {
    return this.isConnected
  }
}

export default WebSocketManager
