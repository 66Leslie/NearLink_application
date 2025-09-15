/**
 * UDP通信管理类 - 用于小程序与通信机的UDP通信
 */

import { UDP_CONFIG, BOARD_COMMANDS } from '../config/index'

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

class UDPManager {
  private isConnected = false
  private heartbeatInterval: number | null = null
  private app: any
  public boardIP: string = ''
  public boardPort: number = 0
  private connectionRetries: number = 0
  private maxRetries: number = 3
  private connectionTimeoutId: number | null = null
  private connectionResolve: ((value: boolean) => void) | null = null
  private connectionReject: ((reason: any) => void) | null = null

  constructor(app: any) {
    this.app = app
  }

  /**
   * 连接到通信机
   */
  connect(ip: string, port: number): Promise<boolean> {
    return new Promise((resolve, reject) => {
      // 重置连接状态和重试计数
      this.isConnected = false
      this.boardIP = ip
      this.boardPort = port
      this.connectionRetries = 0
      
      // 确保清除之前的超时计时器
      if (this.connectionTimeoutId !== null) {
        clearTimeout(this.connectionTimeoutId)
        this.connectionTimeoutId = null
      }
      
      // 确保先关闭并重新创建UDP Socket
      this._recreateUDPSocket()
        .then(() => {
          console.log(`尝试连接通信机: ${ip}:${port}`)
          this._attemptConnect(resolve, reject)
        })
        .catch(err => {
          console.error('初始化UDP Socket失败:', err)
          reject(new Error('无法创建UDP Socket'))
        })
    })
  }
  
  /**
   * 重新创建UDP Socket
   */
  private _recreateUDPSocket(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // 关闭现有Socket
        if (this.app.globalData.udpSocket) {
          try {
            this.app.globalData.udpSocket.close()
            this.app.globalData.udpSocket = null
          } catch (e) {
            console.warn('关闭现有UDP Socket失败:', e)
          }
        }
        
        // 创建延迟，确保Socket完全关闭
        setTimeout(() => {
          try {
            // 创建新Socket
            this.app.globalData.udpSocket = wx.createUDPSocket()
            
            // 设置消息监听
            this.app.globalData.udpSocket.onMessage((result: any) => {
              this.app.handleUDPMessage(result)
            })
            
            // 绑定端口
            try {
              const localPort = this.app.globalData.udpSocket.bind()
              console.log('UDP Socket绑定到端口:', localPort)
              resolve()
            } catch (bindErr) {
              console.warn('UDP Socket绑定失败:', bindErr)
              // 即使绑定失败也继续使用，某些环境下可能不需要明确绑定
              resolve()
            }
          } catch (err) {
            reject(new Error('无法创建UDP Socket'))
          }
        }, 300) // 等待300ms确保之前的socket完全关闭
      } catch (err) {
        reject(err)
      }
    })
  }
  
  /**
   * 尝试连接
   */
  private _attemptConnect(resolve: (value: boolean) => void, reject: (reason: any) => void) {
    // 保存resolve和reject函数，以便在收到响应时调用
    this.connectionResolve = resolve
    this.connectionReject = reject

    // 设置连接超时
    this.connectionTimeoutId = setTimeout(() => {
      console.log(`连接超时，当前重试次数: ${this.connectionRetries}/${this.maxRetries}`)

      if (this.connectionRetries < this.maxRetries) {
        // 增加重试次数
        this.connectionRetries++
        console.log(`连接超时，重连 (${this.connectionRetries}/${this.maxRetries})...`)

        // 重新尝试连接
        this._attemptConnect(resolve, reject)
      } else {
        // 超过最大重试次数
        this.isConnected = false

        // 清除保存的Promise函数
        this.connectionResolve = null
        this.connectionReject = null

        // 通知UI更新
        if (this.app.notifyPagesUpdate) {
          this.app.notifyPagesUpdate('udp_status', { connected: false })
        }

        // 隐藏加载提示并显示错误
        try {
          wx.hideLoading()
        } catch (e) {}

        wx.showToast({
          title: '连接超时',
          icon: 'error'
        })

        reject(new Error('连接超时'))
      }
    }, UDP_CONFIG.CONNECTION_TIMEOUT)

    // 发送连接请求
    this.sendRawCommand(BOARD_COMMANDS.CONNECTION.REQUEST)
      .then(() => {
        // 保存连接信息
        wx.setStorageSync('last_connection', {
          ip: this.boardIP,
          port: this.boardPort,
          timestamp: Date.now()
        })

        console.log('连接请求已发送，等待响应...')
        // 注意：连接状态在接收到CONNECT_SUCCESS/CONNECT_OK消息后在handleUDPMessage中更新
        // 这里只是发送了连接请求，不立即resolve，等待超时或收到响应
      })
      .catch(err => {
        // 清除超时计时器
        if (this.connectionTimeoutId !== null) {
          clearTimeout(this.connectionTimeoutId)
          this.connectionTimeoutId = null
        }

        // 清除保存的Promise函数
        this.connectionResolve = null
        this.connectionReject = null

        this.isConnected = false
        if (this.app.notifyPagesUpdate) {
          this.app.notifyPagesUpdate('udp_status', { connected: false })
        }
        reject(err)
      })
  }

  /**
   * 发送原始字符串指令到通信机
   */
  sendRawCommand(command: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      const udp = this.app.globalData.udpSocket
      if (!udp) {
        reject(new Error('UDP Socket未初始化'))
        return
      }

      if (!this.boardIP || !this.boardPort) {
        const lastConnection = wx.getStorageSync('last_connection') || {}
        if (lastConnection.ip && lastConnection.port) {
          this.boardIP = lastConnection.ip
          this.boardPort = lastConnection.port
        } else {
          this.boardIP = UDP_CONFIG.DEFAULT_IP
          this.boardPort = UDP_CONFIG.DEFAULT_PORT
        }
      }

      if (!this.boardIP || !this.boardPort) {
        reject(new Error('无法获取通信机地址'))
        return
      }

      const buffer = new ArrayBuffer(command.length)
      const view = new Uint8Array(buffer)
      for (let i = 0; i < command.length; i++) {
        view[i] = command.charCodeAt(i)
      }

      try {
        udp.send({
          address: this.boardIP,
          port: this.boardPort,
          message: buffer,
          success: () => {
            resolve(true)
          },
          fail: (err: any) => {
            reject(new Error(err.errMsg))
          }
        })
      } catch (error) {
        reject(error)
      }
    })
  }

  /**
   * 发送舵机控制指令
   */
  async sendServoCommand(command: ServoCommand): Promise<boolean> {
    const servoChar = command.servo_id.toString()
    if (!['0', '1', '2', '3'].includes(servoChar)) {
      throw new Error('无效的舵机ID')
    }

    const formattedCommand = `_change_position${command.servo_id}_${command.slider_value}_`
    return await this.sendRawCommand(formattedCommand)
  }

  /**
   * 发送速度控制指令
   */
  async sendSpeedCommand(command: SpeedCommand): Promise<boolean> {
    const speedCommands = ['I', 'J', 'K', 'L'] 
    if (command.speed < 0 || command.speed >= speedCommands.length) {
      throw new Error('无效的速度档位')
    }

    const formattedCommand = `_change_speed${command.speed}`
    return await this.sendRawCommand(formattedCommand)
  }

  /**
   * 发送功能指令
   */
  async sendFunctionCommand(command: FunctionCommand): Promise<boolean> {
    let functionChar = ''
    
    switch (command.function) {
      case 'robot_up':
        functionChar = BOARD_COMMANDS.FUNCTION.ROBOT_UP
        break
      case 'robot_down':
        functionChar = BOARD_COMMANDS.FUNCTION.ROBOT_DOWN
        break
      case 'start_work':
        functionChar = BOARD_COMMANDS.FUNCTION.START_WORK
        break
      case 'emergency_stop':
        functionChar = BOARD_COMMANDS.FUNCTION.EMERGENCY_STOP
        break
      case 'connect_comm':
        functionChar = BOARD_COMMANDS.FUNCTION.CONNECT_COMM || 'P'
        break
      default:
        throw new Error('未知功能指令: ' + command.function)
    }

    return await this.sendRawCommand(functionChar)
  }

  /**
   * 查询设备状态
   */
  async queryStatus(): Promise<boolean> {
    return await this.sendRawCommand(BOARD_COMMANDS.QUERY.STATUS)
  }

  /**
   * 查询分拣计数
   */
  async queryCounts(): Promise<boolean> {
    return await this.sendRawCommand(BOARD_COMMANDS.QUERY.COUNTS)
  }

  /**
   * 开始心跳
   */
  private startHeartbeat() {
    this.stopHeartbeat()
    
    if (this.isConnected && this.boardIP && this.boardPort) {
      // 使用刷新指令作为初始心跳，通信机会响应分拣数量
      this.sendRawCommand('_refresh')
        .catch(err => console.warn('发送初始心跳失败:', err))
    }
    
    this.heartbeatInterval = setInterval(async () => {
      if (this.isConnected) {
        try {
          if (!this.boardIP || !this.boardPort) {
            const lastConnection = wx.getStorageSync('last_connection') || {}
            if (lastConnection.ip && lastConnection.port) {
              this.boardIP = lastConnection.ip
              this.boardPort = lastConnection.port
            }
          }
          
          // 使用刷新指令作为心跳，通信机会响应分拣数量
          await this.sendRawCommand('_refresh')
          this.app.globalData.heartbeatFailCount = 0
          console.log('心跳成功（刷新指令）')
        } catch (error) {
          if (this.app.globalData.heartbeatFailCount === undefined) {
            this.app.globalData.heartbeatFailCount = 1
          } else {
            this.app.globalData.heartbeatFailCount++
          }
          
          console.warn(`心跳失败 (${this.app.globalData.heartbeatFailCount}/3)`)
          
          if (this.app.globalData.heartbeatFailCount >= 3) {
            this.isConnected = false
            
            if (this.app.notifyPagesUpdate) {
              this.app.notifyPagesUpdate('udp_status', { connected: false })
            }
            
            wx.showToast({
              title: '正在重连...',
              icon: 'loading',
              duration: 2000
            })
            
            // 重新创建Socket并尝试重连
            this._recreateUDPSocket()
              .then(() => {
                setTimeout(() => {
                  if (this.app.attemptReconnect) {
                    this.app.attemptReconnect()
                  } else {
                    this.reconnect()
                      .catch(e => console.error('自动重连失败:', e))
                  }
                }, 500)
              })
              .catch(e => console.error('重新创建Socket失败:', e))
          }
        }
      }
    }, UDP_CONFIG.HEARTBEAT_INTERVAL)
  }

  /**
   * 停止心跳
   */
  private stopHeartbeat() {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval)
      this.heartbeatInterval = null
    }
    
    if (this.app && this.app.globalData) {
      this.app.globalData.heartbeatFailCount = 0
    }
  }

  /**
   * 设置连接状态
   */
  setConnected(connected: boolean) {
    this.isConnected = connected

    // 清除连接超时计时器
    if (this.connectionTimeoutId !== null) {
      clearTimeout(this.connectionTimeoutId)
      this.connectionTimeoutId = null
    }

    if (connected) {
      // 连接成功，调用resolve
      if (this.connectionResolve) {
        this.connectionResolve(true)
        this.connectionResolve = null
        this.connectionReject = null
      }
      this.startHeartbeat()
    } else {
      this.stopHeartbeat()
    }
  }

  /**
   * 获取连接状态
   */
  getConnectionStatus(): boolean {
    return this.isConnected
  }

  /**
   * 断开连接
   */
  disconnect(): { savedIP: string; savedPort: number } {
    // 停止心跳
    this.stopHeartbeat()
    
    // 清除连接超时计时器
    if (this.connectionTimeoutId !== null) {
      clearTimeout(this.connectionTimeoutId)
      this.connectionTimeoutId = null
    }
    
    // 更新连接状态
    this.isConnected = false
    
    // 保存当前连接信息
    const savedIP = this.boardIP
    const savedPort = this.boardPort
    
    // 重置连接信息
    this.boardIP = ''
    this.boardPort = 0
    
    // 关闭Socket
    const udp = this.app.globalData.udpSocket
    if (udp) {
      try {
        udp.close()
        this.app.globalData.udpSocket = null
        
        // 延迟创建新Socket
        setTimeout(() => {
          try {
            this.app.globalData.udpSocket = wx.createUDPSocket()
            
            // 设置消息监听
            this.app.globalData.udpSocket.onMessage((result: any) => {
              this.app.handleUDPMessage(result)
            })
            
            // 绑定端口
            try {
              const localPort = this.app.globalData.udpSocket.bind()
              console.log('断开后重新创建UDP Socket，绑定到端口:', localPort)
            } catch (e) {
              console.warn('断开后重新创建UDP Socket绑定失败:', e)
            }
          } catch (error) {
            console.error('断开后重新创建UDP Socket失败:', error)
          }
        }, 500)
      } catch (error) {
        console.warn('关闭UDP Socket失败:', error)
      }
    }
    
    return { savedIP, savedPort }
  }
  
  /**
   * 重新连接
   */
  async reconnect(): Promise<boolean> {
    // 重置连接状态
    this.isConnected = false
    
    // 获取上次连接信息
    const lastConnection = wx.getStorageSync('last_connection') || {}
    const ip = lastConnection.ip || UDP_CONFIG.DEFAULT_IP
    const port = lastConnection.port || UDP_CONFIG.DEFAULT_PORT
    
    console.log(`尝试重新连接到 ${ip}:${port}`)
    
    try {
      // 确保先重新创建Socket
      await this._recreateUDPSocket()
      
      // 连接
      return await this.connect(ip, port)
    } catch (error) {
      console.error('重新连接失败:', error)
      throw error
    }
  }
}

export default UDPManager
