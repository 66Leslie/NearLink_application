/**
 * 应用配置文件
 */

// UDP通信配置
export const UDP_CONFIG = {
  DEFAULT_IP: '192.168.137.1',
  DEFAULT_PORT: 5566,
  HEARTBEAT_INTERVAL: 15000, // 改为15秒，避免频繁发送刷新指令
  CONNECTION_TIMEOUT: 10000,
  RECONNECT_DELAY: 1000
}

// 舵机配置
export const SERVO_CONFIG = {
  COUNT: 4,
  PWM_RANGE: {
    MIN: 500,
    MAX: 2500,
    DEFAULT: 1500
  },
  SLIDER_RANGE: {
    MIN: 0,
    MAX: 100,
    DEFAULT: 50
  }
}

// 通信机UDP指令映射
export const BOARD_COMMANDS = {
  SERVO: {
    BASE: '0',
    MID: '1', 
    LVL: '2',
    STAMP: '3'
  },
  
  FUNCTION: {
    ROBOT_UP: 'H',
    ROBOT_DOWN: 'G',
    START_WORK: 'M',
    EMERGENCY_STOP: 'E',
    CONNECT_COMM: 'P'
  },
  
  SPEED: {
    STOP: 'I',
    LOW: 'J',
    MID: 'K',
    HIGH: 'L'
  },
  
  QUERY: {
    STATUS: 'Q',
    COUNTS: 'C'
  },
  
  CONNECTION: {
    REQUEST: 'CONNECT_REQUEST',
    RESPONSE: 'CONNECT_OK',
    HEARTBEAT: 'HEARTBEAT'
  }
}

// 调试配置
export const DEBUG_CONFIG = {
  // 调试信息最大保存条数
  MAX_MESSAGES: 100,
  
  // 是否启用控制台日志
  CONSOLE_LOG: true,
  
  // 是否保存调试日志到本地存储
  SAVE_TO_STORAGE: true,
  
  // 日志级别
  LOG_LEVEL: {
    ERROR: 0,
    WARN: 1,
    INFO: 2,
    DEBUG: 3
  }
}

// UI配置
export const UI_CONFIG = {
  // 主题颜色
  COLORS: {
    PRIMARY: '#007AFF',
    SUCCESS: '#4CAF50',
    WARNING: '#FF9800',
    ERROR: '#F44336',
    INFO: '#2196F3'
  },
  
  // 动画持续时间
  ANIMATION_DURATION: 300,
  
  // Toast显示时间
  TOAST_DURATION: 2000,
  
  // 加载提示文本
  LOADING_TEXTS: {
    CONNECTING: '连接中...',
    SENDING: '发送中...',
    LOADING: '加载中...'
  }
}

// 缓存配置
export const CACHE_CONFIG = {
  // 数据缓存时间（毫秒）
  DATA_CACHE_TIME: 5 * 60 * 1000, // 5分钟
  
  // 缓存键名
  KEYS: {
    SERVO_PRESETS: 'servo_presets',
    USER_SETTINGS: 'user_settings',
    DEBUG_LOGS: 'debug_logs',
    LAST_CONNECTION: 'last_connection'
  }
}
