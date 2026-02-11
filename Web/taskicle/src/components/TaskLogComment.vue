<template>
  <div class="task-changelog-container">
    <div v-if="allowAddComment" class="input-area">
      <el-input 
        v-model="newComment" 
        type="textarea" 
        :rows="3" 
        placeholder="输入评论..." 
        @keydown.ctrl.enter="addComment"
      />
      <div class="action-bar">
        <el-button type="primary" size="small" :loading="submitting" @click="addComment">发布</el-button>
      </div>
    </div>

    <div class="scroll-area" ref="scrollRef" @scroll="onScroll">
      <div v-if="!loading && timeline.length === 0" class="empty-state">暂无动态</div>

      <ul v-else class="timeline-list">
        <li v-for="item in timeline" :key="item.uniqueId" class="timeline-item">
          <div class="avatar" :style="{ backgroundColor: item.type === 'comment' ? '#2f6fe4' : '#e6a23c' }">
            {{ avatarChar(item.userName) }}
          </div>
          <div class="body">
            <div class="header">
              <span class="name">{{ item.userName || (item.type === 'log' ? '系统' : '匿名') }}</span>
              <span class="time">{{ formatTime(item.time) }}</span>
            </div>
            <div class="content-box">
              <div v-if="item.type === 'comment'" class="text-content">{{ item.content }}</div>
              <div v-else-if="item.type === 'log'" class="log-content">
                将 <strong>{{ humanName(item.field) }}</strong> 
                从 <span class="old-val">{{ renderVal(item.field, item.oldVal) }}</span>
                改为 <span class="new-val">{{ renderVal(item.field, item.newVal) }}</span>
              </div>
            </div>
          </div>
        </li>
      </ul>

      <div v-if="loadingMore" class="loading-bar">
        <el-icon class="is-loading"><Loading /></el-icon> 加载中...
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, computed, defineProps, defineExpose, nextTick } from 'vue'
import { ElMessage } from 'element-plus'
import { Loading } from '@element-plus/icons-vue'
import api from '../utils/api'
import getStatus, { showErrorMessage } from '../utils/utils'

const props = defineProps({
  taskId: { type: [String, Number], required: true },
  pageSize: { type: Number, default: 50 },
  mode: { type: String, default: 'all' }
})

const scrollRef = ref(null)
const loading = ref(false)
const loadingMore = ref(false)
const submitting = ref(false)
const finished = ref(false)
const page = ref(0)
const newComment = ref('')

const rawLogs = ref([])
const rawComms = ref([])

const allowAddComment = true

const timeline = computed(() => {
  let list = []
  if (props.mode === 'all' || props.mode === 'changelog') {
    rawLogs.value.forEach(log => {
      list.push({
        type: 'log',
        uniqueId: `log_${log.change_at}`,
        time: Number(log.change_at),
        userName: log.user_name,
        field: log.field_name,
        oldVal: log.old_value,
        newVal: log.new_value
      })
    })
  }

  if (props.mode === 'all' || props.mode === 'comment') {
    rawComms.value.forEach(comm => {
      list.push({
        type: 'comment',
        uniqueId: `comm_${comm.comm_id}`,
        time: Number(comm.create_at),
        userName: comm.user_name,
        content: comm.content
      })
    })
  }

  return list.sort((a, b) => b.time - a.time)
})

function avatarChar(name) {
  return name ? String(name).trim().slice(0, 1).toUpperCase() : 'U'
}

function formatTime(ts) {
  if (!ts) return ''
  const num = Number(ts)
  const date = new Date(num < 10000000000 ? num * 1000 : num)
  return date.toLocaleString()
}

function humanName(field) {
  const map = {
    task_name: '任务名称', name: '名称', status: '状态', state: '状态',
    priority: '优先级', description: '描述', project_id: '项目', expire_at: '截止时间'
  }
  return map[field] || field
}

function renderVal(field, val) {
  if (val === null || val === undefined || val === '') return '空'
  if (['status', 'state', 'priority'].includes(field)) {
    const n = Number(val)
    const s = getStatus(field === 'priority' ? 'priority' : 'state', isNaN(n) ? val : n)
    return s?.text ?? String(val)
  }
  if (field === 'expire_at' || field.endsWith('_at')) {
    return val == 0 ? '未设置' : formatTime(val)
  }
  return String(val)
}

async function fetchData(isLoadMore = false) {
  if (props.taskId < 0) return
  if (finished.value && isLoadMore) return
  
  if (isLoadMore) {
    loadingMore.value = true
  } else {
    loading.value = true
    page.value = 0
    finished.value = false
    rawLogs.value = []
    rawComms.value = []
  }

  try {
    const [resLogs, resComms] = await Promise.all([
      api.get('/api/task_log', { params: { task_id: props.taskId, count: props.pageSize, page: page.value } }),
      api.get('/api/task_comm_list', { params: { task_id: props.taskId, count: props.pageSize, page: page.value } })
    ])

    const lData = resLogs.data?.r === 0 ? resLogs.data.data : []
    const cData = resComms.data?.r === 0 ? resComms.data.data : []

    rawLogs.value = [...rawLogs.value, ...lData]
    rawComms.value = [...rawComms.value, ...cData]

    if (lData.length < props.pageSize && cData.length < props.pageSize) {
      finished.value = true
    } else {
      page.value++
    }

    if (!isLoadMore) {
      await nextTick()
      if (scrollRef.value) scrollRef.value.scrollTop = 0
    }
  } catch (e) {
    showErrorMessage(ElMessage, e)
  } finally {
    loading.value = false
    loadingMore.value = false
  }
}

function onScroll(e) {
  const container = e.target
  if (container.scrollTop + container.clientHeight >= container.scrollHeight - 50) {
    if (!loadingMore.value && !loading.value && !finished.value) {
      fetchData(true)
    }
  }
}

async function addComment() {
  if (!newComment.value.trim() || props.taskId < 0) return
  submitting.value = true
  try {
    await api.post('/api/task_comm_insert', { task_id: props.taskId, content: newComment.value })
    newComment.value = ''
    await fetchData(false)
  } catch (e) {
    showErrorMessage(ElMessage, e)
  } finally {
    submitting.value = false
  }
}

watch(() => props.taskId, () => fetchData(false));

watch(() => props.mode, () => {
  nextTick(() => {
    if (scrollRef.value) scrollRef.value.scrollTop = 0
  })
})

onMounted(() => fetchData(false))
defineExpose({ reload: () => fetchData(false) })
</script>

<style scoped>
.task-changelog-container {
  display: flex;
  flex-direction: column;
  height: 100%;
  font-size: 14px;
}

.input-area {
  flex-shrink: 0;
  padding: 0 0 12px 0;
  background: transparent;
}

.action-bar {
  margin-top: 8px;
  text-align: right;
}

.scroll-area {
  flex: 1;
  overflow-y: auto;
  padding-right: 4px;
  display: flex;
  flex-direction: column;
}

.timeline-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.timeline-item {
  display: flex;
  gap: 12px;
  align-items: flex-start;
}

.avatar {
  flex-shrink: 0;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: bold;
  font-size: 14px;
}

.body {
  flex: 1;
  min-width: 0;
}

.header {
  margin-bottom: 4px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.name {
  font-weight: 600;
  color: #333;
}

.time {
  font-size: 12px;
  color: #999;
}

.content-box {
  line-height: 1.5;
}

.text-content {
  color: #333;
  white-space: pre-wrap;
  word-break: break-all;
}

.log-content {
  color: #555;
  padding: 0;
}

.old-val {
  text-decoration: line-through;
  color: #900;
  margin: 0 4px;
}

.new-val {
  color: #2b8a3e;
  font-weight: 500;
}

.loading-bar {
  text-align: center;
  padding: 10px;
  color: #909399;
  font-size: 12px;
}

.empty-state {
  text-align: center;
  color: #888;
  padding-top: 20px;
}
</style>