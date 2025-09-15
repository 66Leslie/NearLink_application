export const formatTime = (date: Date) => {
  const year = date.getFullYear()
  const month = date.getMonth() + 1
  const day = date.getDate()
  const hour = date.getHours()
  const minute = date.getMinutes()
  const second = date.getSeconds()

  return (
    [year, month, day].map(formatNumber).join('/') +
    ' ' +
    [hour, minute, second].map(formatNumber).join(':')
  )
}

const formatNumber = (n: number) => {
  const s = n.toString()
  return s[1] ? s : '0' + s
}

/**
 * 将滑块值(0-100)转换为PWM值(500-2500)
 */
export const convertSliderToPWM = (sliderValue: number): number => {
  return Math.round(500 + (sliderValue / 100) * 2000)
}

/**
 * 将PWM值(500-2500)转换为滑块值(0-100)
 */
export const convertPWMToSlider = (pwmValue: number): number => {
  return Math.round(((pwmValue - 500) / 2000) * 100)
}

/**
 * 获取速度档位文本
 */
export const getSpeedText = (speed: number): string => {
  const speedMap = ['停止', '慢速', '中速', '高速']
  return speedMap[speed] || '未知'
}

/**
 * 获取速度档位对应的指令字符
 */
export const getSpeedCommand = (speed: number): string => {
  const commandMap = ['I', 'J', 'K', 'L'] // 停止, 慢速, 中速, 高速
  return commandMap[speed] || 'I'
}

/**
 * 获取功能按钮对应的指令字符
 */
export const getFunctionCommand = (functionId: string): string => {
  const commandMap: Record<string, string> = {
    'robot_up': 'H',        // 收回
    'start_work': 'M',      // 开始工作
    'pause_work': 'N',      // 暂停工作
    'emergency_stop': 'E'   // 紧急停止
  }
  return commandMap[functionId] || ''
}

/**
 * 构建主板TCP指令格式 (ffXYZZ)
 */
export const buildTCPCommand = (command: string, data: number = 0): string => {
  const hexData = data.toString(16).padStart(4, '0').toUpperCase()
  return `ff${command}${hexData}`
}

/**
 * 防抖函数
 */
export const debounce = (func: Function, wait: number) => {
  let timeout: number | null = null
  return function(this: any, ...args: any[]) {
    if (timeout) clearTimeout(timeout)
    timeout = setTimeout(() => func.apply(this, args), wait)
  }
}

/**
 * 节流函数
 */
export const throttle = (func: Function, limit: number) => {
  let inThrottle = false
  return function(this: any, ...args: any[]) {
    if (!inThrottle) {
      func.apply(this, args)
      inThrottle = true
      setTimeout(() => inThrottle = false, limit)
    }
  }
}

/**
 * 存储键名常量
 */
export const STORAGE_KEYS = {
  SERVO_PRESETS: 'servo_presets',
  CUSTOM_PRESET: 'custom_preset',
  APP_SETTINGS: 'app_settings',
  DEBUG_LOGS: 'debug_logs'
}