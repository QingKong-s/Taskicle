export function getStatus(kind, value) {
  if (kind === 'priority') {
    const map = [
      { text: '最低', background: '#f6faff', color: '#9bbcf5', class: 'p-0' },
      { text: '较低', background: '#e9f3ff', color: '#5e9bf2', class: 'p-1' },
      { text: '普通', background: '#dcecff', color: '#1677ff', class: 'p-2' },
      { text: '较高', background: '#fff4e5', color: '#d48806', class: 'p-3' },
      { text: '最高', background: '#ffe8e8', color: '#cf1322', class: 'p-4' },
    ]
    return map[value] ?? { text: '未知', background: '#f3f4f6', color: '#666', class: '' }
  }

  if (kind === 'state') {
    const map = [
      { text: '未开始', background: '#f0f0f0', color: '#666', class: 's-0' },
      { text: '进行中', background: '#e6f4ff', color: '#095cb5', class: 's-1' },
      { text: '已完成', background: '#e7f9ef', color: '#2a8a43', class: 's-2' },
      { text: '关闭', background: '#e7f9ef', color: '#2a8a43', class: 's-3' },
      { text: '挂起', background: '#fff5d6', color: '#ad7a00', class: 's-4' },
      { text: '待验证', background: '#f1e6ff', color: '#6a32c9', class: 's-5' },
    ]
    return map[value] ?? { text: '未知', background: '#f3f4f6', color: '#666', class: '' }
  }

  return { text: '未知', background: '#f3f4f6', color: '#666', class: '' }
}

export function getApiResultMessage(apir) {
  switch (apir) {
    case 0: return '操作成功'
    case 1: return '未知错误'
    case 2: return '携带的数据格式错误'
    case 3: return '缺少必要字段'
    case 4: return '数据类型不匹配'
    case 5: return '至少需要一个字段'
    case 6: return '枚举值无效'
    case 7: return '数据库错误'
    case 8: return '操作无影响'
    case 9: return '文件操作失败'
    case 10: return '找不到目标实体'
    case 11: return '加解密失败'
    case 12: return '访问被拒绝'
    case 13: return '密码错误'
    default: return '未知错误代码'
  }
}

export function showErrorMessage(elMsg, json) {
  const msg = json.err_msg || '未知错误'
  const output = `错误: ${getApiResultMessage(json.r)} (${json.r}, ${json.r2}, ${msg})`;
  elMsg.error(output)
  console.error(output)
}

export function showAxiosErrorMessage(elMsg, error) {
  var output = ''
  if (error.response) {
    output = `请求错误：${error.response.status}(${error.response.statusText})`
  } else if (error.request) {
    output = '请求错误：无响应'
  } else {
    output = `请求错误：${error.message}`
  }
  elMsg.error(output)
  console.error(output)
}

export default getStatus